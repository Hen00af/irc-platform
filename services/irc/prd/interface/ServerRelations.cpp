#include <cstddef>
#include <set>
#include <unistd.h>
#include <utility>

#include "../util/IrcUtil.hpp"
#include "../util/Numerics.hpp"
#include "../util/Reply.hpp"
#include "Server.hpp"

bool Server::addClient(int fd, const std::string &hostname)
{
    if (_clients.find(fd) != _clients.end())
        return false;
    _clients.insert(std::make_pair(fd, Client(fd, hostname)));
    return true;
}

void Server::removeClient(int fd)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    /* 不変条件「Nickname 索引と Client Map が常に一致する」(設計書 02
       §13) を保つため、索引も同時に削除する */
    if (!client->getNickname().empty())
        unregisterNickname(client->getNickname());
    _clients.erase(fd);
}

Client *Server::findClientByFd(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);

    if (it == _clients.end())
        return NULL;
    return &it->second;
}

const Client *Server::findClientByFd(int fd) const
{
    std::map<int, Client>::const_iterator it = _clients.find(fd);

    if (it == _clients.end())
        return NULL;
    return &it->second;
}

void Server::queueToClient(int fd, const std::string &message)
{
    /* 送信待ちが 1 MiB を超える Client は読み取り速度が極端に遅いか
       応答不能と判断し、追加予定 message を破棄して切断予約する。他
       Client と Server は継続する (設計書 03 §16) */
    static const std::size_t MAX_SEND_BUFFER = 1048576;

    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;

    std::string normalized = (message.size() >= 2
                              && message.compare(message.size() - 2, 2,
                                                 "\r\n") == 0)
                                  ? message
                                  : message + "\r\n";

    if (client->getSendBuffer().size() + normalized.size() > MAX_SEND_BUFFER)
    {
        scheduleDisconnect(fd);
        return;
    }
    client->appendSendBuffer(normalized);
    /* pollfd の POLLOUT を有効化する (設計書 03 §14)。_pollFds に該当
       fd が無い (単体テストなど) 場合は何もしない */
    updatePollEvents(fd);
}

Client *Server::findClientByNickname(const std::string &nickname)
{
    std::map<std::string, int>::const_iterator it =
        _nickToFd.find(IrcUtil::ircCaseFold(nickname));

    if (it == _nickToFd.end())
        return NULL;
    return findClientByFd(it->second);
}

const Client *Server::findClientByNickname(const std::string &nickname) const
{
    std::map<std::string, int>::const_iterator it =
        _nickToFd.find(IrcUtil::ircCaseFold(nickname));

    if (it == _nickToFd.end())
        return NULL;
    return findClientByFd(it->second);
}

Channel *Server::findChannel(const std::string &name)
{
    std::map<std::string, Channel>::iterator it =
        _channels.find(IrcUtil::normalizeChannelName(name));

    if (it == _channels.end())
        return NULL;
    return &it->second;
}

const Channel *Server::findChannel(const std::string &name) const
{
    std::map<std::string, Channel>::const_iterator it =
        _channels.find(IrcUtil::normalizeChannelName(name));

    if (it == _channels.end())
        return NULL;
    return &it->second;
}

bool Server::isNicknameAvailable(const std::string &nickname,
                                 int                exceptFd) const
{
    std::map<std::string, int>::const_iterator it =
        _nickToFd.find(IrcUtil::ircCaseFold(nickname));

    if (it == _nickToFd.end())
        return true;
    return it->second == exceptFd;
}

void Server::registerNickname(int fd, const std::string &nickname)
{
    _nickToFd[IrcUtil::ircCaseFold(nickname)] = fd;
}

void Server::unregisterNickname(const std::string &nickname)
{
    _nickToFd.erase(IrcUtil::ircCaseFold(nickname));
}

void Server::broadcastToSharedChannels(const Client      &client,
                                       const std::string &message)
{
    std::set<int>                targets;
    const std::set<std::string> &joined = client.getJoinedChannels();

    for (std::set<std::string>::const_iterator it = joined.begin();
         it != joined.end(); ++it)
    {
        const Channel *channel = findChannel(*it);

        if (channel == NULL)
            continue;

        const std::set<int> &members = channel->getMembers();

        targets.insert(members.begin(), members.end());
    }
    targets.erase(client.getFd());
    for (std::set<int>::const_iterator it = targets.begin();
         it != targets.end(); ++it)
        queueToClient(*it, message);
}

void Server::broadcastToChannel(const Channel      &channel,
                                const std::string &message,
                                int                excludeFd)
{
    const std::set<int> &members = channel.getMembers();

    for (std::set<int>::const_iterator it = members.begin();
         it != members.end(); ++it)
    {
        if (*it == excludeFd)
            continue;
        queueToClient(*it, message);
    }
}

/* ── 所属関係 (設計書 02 §4.7) ──────────────── */

bool Server::joinChannel(int clientFd, const std::string &channelKey)
{
    Client  *client  = findClientByFd(clientFd);
    Channel *channel = findChannel(channelKey);

    if (client == NULL || channel == NULL)
        return false;

    channel->addMember(clientFd);
    client->addJoinedChannel(channel->getKeyName());
    /* どちらか一方の追加が不要だった場合 (既に片側だけ一致していた) も、
       最終的に両側が一致することを確認する (設計書 02 §4.7) */
    return channel->hasMember(clientFd)
        && client->hasJoinedChannel(channel->getKeyName());
}

bool Server::leaveChannel(int                clientFd,
                          const std::string &channelKey,
                          const std::string &reason,
                          bool               sendPart)
{
    Client  *client  = findClientByFd(clientFd);
    Channel *channel = findChannel(channelKey);

    if (client == NULL || channel == NULL || !channel->hasMember(clientFd))
        return false;

    /* Channel から削除する前に通知文字列を組み立てておく。削除後では
       表示名や Member 集合が変化するため (設計書 02 §4.7) */
    std::string keyName = channel->getKeyName();

    if (sendPart)
    {
        std::string notice = Reply::command(
            Reply::clientPrefix(*client), "PART",
            channel->getName() + " :" + reason);

        /* 削除前の全 Member (退出する Client 自身を含む) へ送る
           (本 spec の決定事項。02 §4.7 の記載順と異なり、削除前に
           broadcast することで退出 Client にも PART が届く) */
        broadcastToChannel(*channel, notice, -1);
    }
    channel->removeMember(clientFd);
    channel->removeInvite(clientFd);
    client->removeJoinedChannel(keyName);
    deleteChannelIfEmpty(keyName);
    return true;
}

void Server::deleteChannelIfEmpty(const std::string &channelKey)
{
    Channel *channel = findChannel(channelKey);

    if (channel != NULL && channel->isEmpty())
        _channels.erase(channel->getKeyName());
}

void Server::removeClientFromAllChannels(int                clientFd,
                                         const std::string &quitMessage)
{
    /* QUIT 通知は disconnectClient() が事前に broadcastToSharedChannels()
       で送信済み。ここでは Channel 側の関係整理だけを行う (設計書 03
       §18 手順 6〜9) */
    (void)quitMessage;

    Client *client = findClientByFd(clientFd);

    if (client == NULL)
        return;

    /* 参加中 Channel 名をコピーしてから走査する。ループ内で
       Client::removeJoinedChannel() が _joinedChannels を変更するため、
       コピーを使わないと Iterator が無効化される (本 spec の決定事項) */
    std::set<std::string> joined = client->getJoinedChannels();

    for (std::set<std::string>::const_iterator it = joined.begin();
         it != joined.end(); ++it)
    {
        Channel *channel = findChannel(*it);

        if (channel == NULL)
            continue;
        /* Channel::removeMember() は不変条件「Operator は必ず Member」
           を保つため Operator 集合からも同時に削除する */
        channel->removeMember(clientFd);
        client->removeJoinedChannel(*it);
        deleteChannelIfEmpty(*it);
    }

    /* Client は参加していない Channel からも Invite され得るため、
       残っている全 Channel の Invite 集合から FD を削除する (設計書 03
       §18 手順 7) */
    for (std::map<std::string, Channel>::iterator cit = _channels.begin();
         cit != _channels.end(); ++cit)
        cit->second.removeInvite(clientFd);
}

/* ── 切断 (設計書 03 §17〜§18) ──────────────── */

void Server::scheduleDisconnect(int fd)
{
    /* 同じ FD が複数原因 (QUIT / recv エラーなど) で予約されても Set に
       より 1 回だけ処理される (設計書 03 §17) */
    _pendingDisconnects.insert(fd);
}

void Server::processPendingDisconnects()
{
    /* Set をコピーしてから Member を空にする。disconnectClient() の中で
       _pendingDisconnects が再度変更されても (通常は起きない) 今回の
       走査には影響しない (設計書 03 §17) */
    std::set<int> pending = _pendingDisconnects;

    _pendingDisconnects.clear();
    for (std::set<int>::const_iterator it = pending.begin();
         it != pending.end(); ++it)
    {
        std::map<int, std::string>::const_iterator reasonIt =
            _disconnectReasons.find(*it);
        std::string reason = (reasonIt != _disconnectReasons.end())
                                  ? reasonIt->second
                                  : "Connection closed";

        disconnectClient(*it, reason);
    }
}

void Server::disconnectClient(int fd, const std::string &reason)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;

    /* 共有 Channel の他 Member へ QUIT 通知を 1 回だけ queue する。
       broadcastToSharedChannels() は client 自身を除外し、複数 Channel
       共有時も FD 集合の重複排除で 1 回だけ送る (設計書 03 §18 手順
       2〜5) */
    std::string notice = Reply::command(
        Reply::clientPrefix(*client), "QUIT", ":" + reason);

    broadcastToSharedChannels(*client, notice);

    /* Channel 関係 (Member/Operator/Invite/参加集合) の整理と空 Channel
       の削除 (設計書 03 §18 手順 6〜9) */
    removeClientFromAllChannels(fd, reason);

    /* Nickname 索引の削除 (設計書 03 §18 手順 10) */
    if (!client->getNickname().empty())
        unregisterNickname(client->getNickname());

    /* pollfd 一覧からの削除は close() より前に行う。FD 再利用対策
       (設計書 03 §18 手順 11〜12、FD再利用対策) */
    removeClientPollFd(fd);
    close(fd);

    /* _disconnectReasons の該当エントリと Client Map から削除する
       (設計書 03 §18 手順 13、本 spec の決定事項で理由 Map も削除) */
    _disconnectReasons.erase(fd);
    _clients.erase(fd);
}

/* ── 共通検証 (設計書 04 §4) ────────────────── */

bool Server::requireChannelMember(int fd, const Channel &channel)
{
    if (channel.hasMember(fd))
        return true;

    Client *client = findClientByFd(fd);

    if (client != NULL)
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NOTONCHANNEL,
                                         channel.getName()
                                             + " :You're not on that channel"));
    return false;
}

bool Server::requireChannelOperator(int fd, const Channel &channel)
{
    if (channel.isOperator(fd))
        return true;

    Client *client = findClientByFd(fd);

    if (client != NULL)
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_CHANOPRIVSNEEDED,
                                         channel.getName()
                                             + " :You're not channel operator"));
    return false;
}

void Server::sendNoSuchChannel(int fd, const Client &client,
                               const std::string &channelName)
{
    queueToClient(fd, Reply::numeric(_serverName, client,
                                     Numeric::ERR_NOSUCHCHANNEL,
                                     channelName + " :No such channel"));
}
