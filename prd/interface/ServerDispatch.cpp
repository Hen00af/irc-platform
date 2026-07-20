#include <cstddef>

#include "../util/Numerics.hpp"
#include "../util/Reply.hpp"
#include "Server.hpp"

/* ============================================================
 * Command Dispatcher (設計書 04 §3)
 *
 * Dispatcher 表は private の CommandHandler を参照するため、
 * メンバ関数内の static const 配列として定義する。
 * ============================================================ */
void Server::dispatchCommand(int fd, const Message &message)
{
    struct CommandEntry
    {
        const char             *name;
        Server::CommandHandler  handler;
        bool                    requiresRegistration;  /* 登録前 → 451 */
        bool                    rejectsWhenRegistered; /* 登録後 → 462 */
    };
    /* 設計書 04 §3 の Dispatcher 表と 1 対 1 対応 */
    static const CommandEntry table[] = {
        { "PASS",    &Server::handlePass,    false, true  },
        { "NICK",    &Server::handleNick,    false, false },
        { "USER",    &Server::handleUser,    false, true  },
        { "CAP",     &Server::handleCap,     false, false },
        { "PING",    &Server::handlePing,    false, false },
        { "PONG",    &Server::handlePong,    false, false },
        { "QUIT",    &Server::handleQuit,    false, false },
        { "JOIN",    &Server::handleJoin,    true,  false },
        { "PRIVMSG", &Server::handlePrivmsg, true,  false },
        { "KICK",    &Server::handleKick,    true,  false },
        { "INVITE",  &Server::handleInvite,  true,  false },
        { "TOPIC",   &Server::handleTopic,   true,  false },
        { "MODE",    &Server::handleMode,    true,  false },
        { "PART",    &Server::handlePart,    true,  false },
    };
    static const std::size_t tableSize = sizeof(table) / sizeof(table[0]);

    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;

    const CommandEntry *entry = NULL;

    for (std::size_t i = 0; i < tableSize; ++i)
    {
        if (message.command == table[i].name)
        {
            entry = &table[i];
            break;
        }
    }

    if (entry == NULL)
    {
        /* 登録前の未知 Command は無視する (設計書 04 §3 —
           クライアント固有の初期交渉で登録を妨げないため) */
        if (client->isRegistered())
            queueToClient(fd,
                          Reply::numeric(_serverName, *client,
                                         Numeric::ERR_UNKNOWNCOMMAND,
                                         message.command
                                             + " :Unknown command"));
        return;
    }
    if (!client->isRegistered() && entry->requiresRegistration)
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NOTREGISTERED,
                                         ":You have not registered"));
        return;
    }
    if (client->isRegistered() && entry->rejectsWhenRegistered)
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_ALREADYREGISTRED,
                                         ":Unauthorized command "
                                         "(already registered)"));
        return;
    }
    (this->*(entry->handler))(fd, message);
}

/* ============================================================
 * 全 Handler 実装済み (設計書 02 §4.10)
 *
 * 認証系 (PASS/NICK/USER/CAP/PING/PONG) は handler/
 * ServerAuthCommands.cpp、JOIN/PART/KICK/INVITE/TOPIC/QUIT は handler/
 * ServerChannelCommands.cpp、PRIVMSG は handler/ServerMessageCommands.cpp、
 * MODE は handler/ServerMode.cpp に実装済み。スタブは残っていない。
 * ============================================================ */
