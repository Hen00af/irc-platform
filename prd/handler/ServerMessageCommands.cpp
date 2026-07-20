#include <cstddef>
#include <string>
#include <vector>

#include "../interface/Server.hpp"
#include "../util/IrcUtil.hpp"
#include "../util/Numerics.hpp"
#include "../util/Reply.hpp"

/* ============================================================
 * メッセージ系 Command Handler (設計書 04 §11)
 *
 * PRIVMSG を実装する。
 * ============================================================ */

/* ── PRIVMSG (設計書 04 §11) ────────────── */

void Server::handlePrivmsg(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;

    /* target Parameter がなければ 411 (設計書 04 §11) */
    if (message.params.empty())
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NORECIPIENT,
                                         ":No recipient given (PRIVMSG)"));
        return;
    }
    /* text Parameter がない、または空なら 412 (設計書 04 §11) */
    if (message.params.size() < 2 || message.params[1].empty())
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NOTEXTTOSEND,
                                         ":No text to send"));
        return;
    }

    const std::string       &text = message.params[1];
    std::vector<std::string> targets =
        IrcUtil::splitCommaList(message.params[0]);

    /* target 一覧を独立処理する。1 つの失敗が他 target を止めない
       (設計書 04 §11) */
    for (std::size_t i = 0; i < targets.size(); ++i)
    {
        const std::string &target = targets[i];

        if (!target.empty() && target[0] == '#')
        {
            Channel *channel = findChannel(target);

            if (channel == NULL)
            {
                queueToClient(fd, Reply::numeric(
                    _serverName, *client, Numeric::ERR_NOSUCHCHANNEL,
                    target + " :No such channel"));
                continue;
            }
            if (!channel->hasMember(fd))
            {
                queueToClient(fd, Reply::numeric(
                    _serverName, *client, Numeric::ERR_CANNOTSENDTOCHAN,
                    channel->getName() + " :Cannot send to channel"));
                continue;
            }

            /* 送信者以外の全 Member へ配送する (設計書 04 §11)。
               送信者自身には Channel message を返送しない */
            std::string notice = Reply::command(
                Reply::clientPrefix(*client), "PRIVMSG",
                channel->getName() + " :" + text);

            broadcastToChannel(*channel, notice, fd);
        }
        else
        {
            Client *targetClient = findClientByNickname(target);

            if (targetClient == NULL)
            {
                queueToClient(fd, Reply::numeric(
                    _serverName, *client, Numeric::ERR_NOSUCHNICK,
                    target + " :No such nick/channel"));
                continue;
            }

            /* 送信者自身が target でも 1 回 queue する (設計書 04 §11) */
            std::string notice = Reply::command(
                Reply::clientPrefix(*client), "PRIVMSG",
                targetClient->getNickname() + " :" + text);

            queueToClient(targetClient->getFd(), notice);
        }
    }
}
