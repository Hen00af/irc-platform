#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../interface/Server.hpp"
#include "../util/IrcUtil.hpp"
#include "../util/Numerics.hpp"
#include "../util/Reply.hpp"

/* ============================================================
 * チャンネル系 Command Handler (設計書 04 §10, §12, §13, §14, §15, §16)
 *
 * JOIN / PART / KICK / INVITE / TOPIC / QUIT を実装する。所属関係の
 * 更新は Server::joinChannel() / Server::leaveChannel() / Server::
 * removeClientFromAllChannels() (ServerRelations.cpp) に委譲する。
 * ============================================================ */

/* ── JOIN (設計書 04 §10) ───────────────── */

void Server::handleJoin(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (!requireParams(fd, message, 1))
        return;

    /* JOIN 0 は全 Channel からの退出 (設計書 04 §10 補助形式)。
       reason は自身の Nickname (本 spec の決定事項) */
    if (message.params[0] == "0")
    {
        std::set<std::string> joined = client->getJoinedChannels();
        std::string           reason = client->getNickname();

        for (std::set<std::string>::const_iterator it = joined.begin();
             it != joined.end(); ++it)
            leaveChannel(fd, *it, reason, true);
        return;
    }

    /* Channel 一覧と Key 一覧を同じ位置で対応させる。Key 不足分は
       空文字 (設計書 04 §10) */
    std::vector<std::string> channels =
        IrcUtil::splitCommaList(message.params[0]);
    std::vector<std::string> keys;

    if (message.params.size() >= 2)
        keys = IrcUtil::splitCommaList(message.params[1]);

    /* 1 つの Channel の失敗で他 Channel の処理を中止しない
       (設計書 04 §10) */
    for (std::size_t i = 0; i < channels.size(); ++i)
    {
        const std::string &channelName = channels[i];
        std::string         key = (i < keys.size()) ? keys[i] : "";

        if (!IrcUtil::isValidChannelName(channelName))
        {
            queueToClient(fd, Reply::numeric(_serverName, *client,
                                             Numeric::ERR_NOSUCHCHANNEL,
                                             channelName
                                                 + " :No such channel"));
            continue;
        }

        std::string normalizedKey = IrcUtil::normalizeChannelName(channelName);

        /* 参加済みなら何もせず次へ (設計書 04 §10) */
        if (client->hasJoinedChannel(normalizedKey))
            continue;

        Channel *channel = findChannel(normalizedKey);
        bool     created = false;

        if (channel == NULL)
        {
            /* insert(std::make_pair(...)) を使う (Global Constraints)。
               Key は正規化名、Channel の Constructor には表示名と
               正規化名を渡す */
            _channels.insert(std::make_pair(
                normalizedKey, Channel(channelName, normalizedKey)));
            channel = findChannel(normalizedKey);
            created = true;
        }
        else
        {
            /* 既存 Channel のみ +i → +k → +l の順に検証する
               (設計書 04 §10) */
            if (channel->isInviteOnly() && !channel->isInvited(fd))
            {
                queueToClient(fd, Reply::numeric(
                    _serverName, *client, Numeric::ERR_INVITEONLYCHAN,
                    channel->getName() + " :Cannot join channel (+i)"));
                continue;
            }
            if (!channel->matchesKey(key))
            {
                queueToClient(fd, Reply::numeric(
                    _serverName, *client, Numeric::ERR_BADCHANNELKEY,
                    channel->getName() + " :Cannot join channel (+k)"));
                continue;
            }
            if (channel->isFull())
            {
                queueToClient(fd, Reply::numeric(
                    _serverName, *client, Numeric::ERR_CHANNELISFULL,
                    channel->getName() + " :Cannot join channel (+l)"));
                continue;
            }
        }

        /* 双方向追加 → 新規なら Operator 追加 → Invite 消費
           (設計書 04 §10 の順序) */
        joinChannel(fd, normalizedKey);
        if (created)
            channel->addOperator(fd);
        channel->removeInvite(fd);

        /* 全 Member (参加した本人を含む) へ JOIN 通知 */
        std::string joinNotice = Reply::command(
            Reply::clientPrefix(*client), "JOIN",
            ":" + channel->getName());

        broadcastToChannel(*channel, joinNotice, -1);

        /* 参加 Client へ Topic Reply */
        if (channel->hasTopic())
            queueToClient(fd, Reply::numeric(
                _serverName, *client, Numeric::RPL_TOPIC,
                channel->getName() + " :" + channel->getTopic()));
        else
            queueToClient(fd, Reply::numeric(
                _serverName, *client, Numeric::RPL_NOTOPIC,
                channel->getName() + " :No topic is set"));

        /* 参加 Client へ Names Reply。FD 昇順 (std::set<int> の走査順)、
           Operator には '@' を付ける (設計書 04 §10) */
        std::string           names;
        const std::set<int> &members = channel->getMembers();

        for (std::set<int>::const_iterator mit = members.begin();
             mit != members.end(); ++mit)
        {
            const Client *member = findClientByFd(*mit);

            if (member == NULL)
                continue;
            if (!names.empty())
                names += " ";
            if (channel->isOperator(*mit))
                names += "@";
            names += member->getNickname();
        }
        queueToClient(fd, Reply::numeric(
            _serverName, *client, Numeric::RPL_NAMREPLY,
            "= " + channel->getName() + " :" + names));
        queueToClient(fd, Reply::numeric(
            _serverName, *client, Numeric::RPL_ENDOFNAMES,
            channel->getName() + " :End of NAMES list"));
    }
}

/* ── PART (設計書 04 §15) ───────────────── */

void Server::handlePart(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (!requireParams(fd, message, 1))
        return;

    std::vector<std::string> channels =
        IrcUtil::splitCommaList(message.params[0]);
    /* reason は末尾 Parameter。無ければ Client Nickname (設計書 04 §15) */
    std::string reason = (message.params.size() >= 2)
                              ? message.params[1]
                              : client->getNickname();

    /* Channel ごとに独立処理する (設計書 04 §15) */
    for (std::size_t i = 0; i < channels.size(); ++i)
    {
        const std::string &channelName = channels[i];
        std::string         normalizedKey =
            IrcUtil::normalizeChannelName(channelName);
        Channel *channel = findChannel(normalizedKey);

        if (channel == NULL)
        {
            queueToClient(fd, Reply::numeric(_serverName, *client,
                                             Numeric::ERR_NOSUCHCHANNEL,
                                             channelName
                                                 + " :No such channel"));
            continue;
        }
        if (!requireChannelMember(fd, *channel))
            continue;
        leaveChannel(fd, normalizedKey, reason, true);
    }
}

/* ── KICK (設計書 04 §12) ───────────────── */

void Server::handleKick(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (!requireParams(fd, message, 2))
        return;

    const std::string &channelName = message.params[0];
    const std::string &targetNick  = message.params[1];
    Channel            *channel    = findChannel(channelName);

    if (channel == NULL)
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NOSUCHCHANNEL,
                                         channelName + " :No such channel"));
        return;
    }
    /* 実行者が Member であること → Operator であることの順に確認する
       (設計書 04 §12) */
    if (!requireChannelMember(fd, *channel))
        return;
    if (!requireChannelOperator(fd, *channel))
        return;

    Client *target = findClientByNickname(targetNick);

    if (target == NULL)
    {
        queueToClient(fd, Reply::numeric(
            _serverName, *client, Numeric::ERR_NOSUCHNICK,
            targetNick + " :No such nick/channel"));
        return;
    }
    if (!channel->hasMember(target->getFd()))
    {
        queueToClient(fd, Reply::numeric(
            _serverName, *client, Numeric::ERR_USERNOTINCHANNEL,
            targetNick + " " + channel->getName()
                + " :They aren't on that channel"));
        return;
    }

    /* reason が無ければ実行者 Nickname を使う (設計書 04 §12) */
    std::string reason = (message.params.size() >= 3)
                              ? message.params[2]
                              : client->getNickname();

    /* 削除前の全 Member (対象自身を含む) へ KICK 通知を queue する
       (設計書 04 §12) */
    std::string notice = Reply::command(
        Reply::clientPrefix(*client), "KICK",
        channel->getName() + " " + target->getNickname() + " :" + reason);

    broadcastToChannel(*channel, notice, -1);

    /* 通知済みのため sendPart=false で leaveChannel() を再利用する。
       Operator の自動移譲は行わない (設計書 04 §12) */
    leaveChannel(target->getFd(), channel->getKeyName(), reason, false);
}

/* ── INVITE (設計書 04 §13) ─────────────── */

void Server::handleInvite(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (!requireParams(fd, message, 2))
        return;

    const std::string &targetNick  = message.params[0];
    const std::string &channelName = message.params[1];
    Client             *target     = findClientByNickname(targetNick);

    /* 対象 Client 検索 → Channel 検索の順 (設計書 04 §13。JOIN や
       KICK と検証順が異なる点に注意) */
    if (target == NULL)
    {
        queueToClient(fd, Reply::numeric(
            _serverName, *client, Numeric::ERR_NOSUCHNICK,
            targetNick + " :No such nick/channel"));
        return;
    }

    Channel *channel = findChannel(channelName);

    if (channel == NULL)
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NOSUCHCHANNEL,
                                         channelName + " :No such channel"));
        return;
    }
    /* 課題文でOperator固有Commandとして指定されているため、Channel Mode
       に関係なく Operator だけが実行できる (設計書 04 §13) */
    if (!requireChannelMember(fd, *channel))
        return;
    if (!requireChannelOperator(fd, *channel))
        return;
    if (channel->hasMember(target->getFd()))
    {
        queueToClient(fd, Reply::numeric(
            _serverName, *client, Numeric::ERR_USERONCHANNEL,
            target->getNickname() + " " + channel->getName()
                + " :is already on channel"));
        return;
    }

    /* 戻り値 false (既に Invite 済み) でも成功扱いとし、通知を再送する
       (設計書 04 §13) */
    channel->addInvite(target->getFd());

    queueToClient(fd, Reply::numeric(
        _serverName, *client, Numeric::RPL_INVITING,
        channel->getName() + " " + target->getNickname()));
    queueToClient(target->getFd(),
                  Reply::command(Reply::clientPrefix(*client), "INVITE",
                                 target->getNickname() + " :"
                                     + channel->getName()));
}

/* ── TOPIC (設計書 04 §14) ──────────────── */

void Server::handleTopic(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (!requireParams(fd, message, 1))
        return;

    const std::string &channelName = message.params[0];
    Channel            *channel    = findChannel(channelName);

    if (channel == NULL)
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NOSUCHCHANNEL,
                                         channelName + " :No such channel"));
        return;
    }
    if (!requireChannelMember(fd, *channel))
        return;

    /* topic Parameter が無ければ照会 (設計書 04 §14) */
    if (message.params.size() < 2)
    {
        if (channel->hasTopic())
            queueToClient(fd, Reply::numeric(
                _serverName, *client, Numeric::RPL_TOPIC,
                channel->getName() + " :" + channel->getTopic()));
        else
            queueToClient(fd, Reply::numeric(
                _serverName, *client, Numeric::RPL_NOTOPIC,
                channel->getName() + " :No topic is set"));
        return;
    }

    /* 変更: +t 有効かつ実行者が Operator でなければ 482 (設計書 04 §14) */
    if (channel->isTopicRestricted() && !requireChannelOperator(fd, *channel))
        return;

    const std::string &topic = message.params[1];

    /* 空文字 topic は削除 (Channel::hasTopic() は空文字を「未設定」と
       扱う) (設計書 04 §14) */
    channel->setTopic(topic);

    /* 全 Member (自分含む) へ通知する (設計書 04 §14) */
    std::string notice = Reply::command(
        Reply::clientPrefix(*client), "TOPIC",
        channel->getName() + " :" + topic);

    broadcastToChannel(*channel, notice, -1);
}

/* ── QUIT (設計書 04 §16) ───────────────── */

void Server::handleQuit(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;

    /* message 省略時は "Client Quit" (設計書 04 §16) */
    std::string reason =
        (!message.params.empty()) ? message.params[0] : "Client Quit";

    /* disconnectClient() を即時実行せず、理由を保存して切断予約する
       だけに留める。通知・関係削除・削除は processPendingDisconnects()
       → disconnectClient() が後で行う (設計書 04 §16, 03 §17〜§18) */
    _disconnectReasons[fd] = reason;
    scheduleDisconnect(fd);
}
