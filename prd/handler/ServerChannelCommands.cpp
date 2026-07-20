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
 * チャンネル系 Command Handler (設計書 04 §10, §15)
 *
 * JOIN / PART を実装する。所属関係の更新は Server::joinChannel() /
 * Server::leaveChannel() (ServerRelations.cpp) に委譲する。
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
