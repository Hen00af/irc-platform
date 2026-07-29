#include <cstddef>

#include "../interface/Server.hpp"
#include "../util/IrcUtil.hpp"
#include "../util/Numerics.hpp"
#include "../util/Reply.hpp"

/* ============================================================
 * 認証系 Command Handler (設計書 04 §5〜§9, §17, §18)
 *
 * PASS / NICK / USER の受信順序は固定しない。3 つが揃った時点で
 * tryRegisterClient() が 1 回だけ Welcome Sequence を送る。
 * ============================================================ */

bool Server::requireParams(int fd, const Message &message, std::size_t count)
{
    if (message.params.size() >= count)
        return true;

    Client *client = findClientByFd(fd);

    if (client != NULL)
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NEEDMOREPARAMS,
                                         message.command
                                             + " :Not enough parameters"));
    return false;
}

void Server::tryRegisterClient(int fd)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (client->tryCompleteRegistration())
        sendWelcomeSequence(*client);
}

void Server::sendWelcomeSequence(Client &client)
{
    int fd = client.getFd();

    /* 001〜004, 422 の本文は設計書 06 §6 と完全一致させる */
    queueToClient(fd, Reply::numeric(
        _serverName, client, Numeric::RPL_WELCOME,
        ":Welcome to the Internet Relay Network "
            + Reply::clientPrefix(client)));
    queueToClient(fd, Reply::numeric(
        _serverName, client, Numeric::RPL_YOURHOST,
        ":Your host is " + _serverName + ", running version 1.0"));
    queueToClient(fd, Reply::numeric(
        _serverName, client, Numeric::RPL_CREATED,
        ":This server was created " + _serverStartTime));
    /* User Mode は実装しないため "-" (設計書 06 §6) */
    queueToClient(fd, Reply::numeric(
        _serverName, client, Numeric::RPL_MYINFO,
        _serverName + " 1.0 - itkol"));
    /* MOTD を実装しないことを明示して登録シーケンスを終える */
    queueToClient(fd, Reply::numeric(
        _serverName, client, Numeric::ERR_NOMOTD,
        ":MOTD File is missing"));
}

/* ── PASS (設計書 04 §6) ────────────────── */

void Server::handlePass(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    /* 登録済みの 462 は Dispatcher が処理済み (設計書 04 §3) */
    if (!requireParams(fd, message, 1))
        return;
    if (message.params[0] != _password)
    {
        /* 不一致でも接続は維持し、再 PASS を許す (設計書 04 §6) */
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_PASSWDMISMATCH,
                                         ":Password incorrect"));
        return;
    }
    client->acceptPassword();
    tryRegisterClient(fd);
}

/* ── NICK (設計書 04 §7) ────────────────── */

void Server::handleNick(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (message.params.empty())
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NONICKNAMEGIVEN,
                                         ":No nickname given"));
        return;
    }

    const std::string &nickname = message.params[0];

    if (!IrcUtil::isValidNickname(nickname))
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_ERRONEUSNICKNAME,
                                         nickname + " :Erroneous nickname"));
        return;
    }
    if (!isNicknameAvailable(nickname, fd))
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NICKNAMEINUSE,
                                         nickname
                                             + " :Nickname is already in use"));
        return;
    }

    /* 通知は旧 Nickname の Prefix で送るため、変更前に組み立てる
       (設計書 04 §7) */
    std::string oldPrefix     = Reply::clientPrefix(*client);
    bool        wasRegistered = client->isRegistered();

    if (!client->getNickname().empty())
        unregisterNickname(client->getNickname());
    registerNickname(fd, nickname);
    client->setNickname(nickname);

    if (wasRegistered)
    {
        /* 自分と共有 Channel の Client へ 1 回ずつ (設計書 04 §20) */
        std::string notice = Reply::command(oldPrefix, "NICK",
                                            ":" + nickname);

        queueToClient(fd, notice);
        broadcastToSharedChannels(*client, notice);
    }
    else
        tryRegisterClient(fd);
}

/* ── USER (設計書 04 §8) ────────────────── */

void Server::handleUser(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    /* 未登録でも USER 受信済みなら再実行として 462 (設計書 04 §8)。
       登録済みの 462 は Dispatcher が処理済み */
    if (client->hasUser())
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_ALREADYREGISTRED,
                                         ":Unauthorized command "
                                         "(already registered)"));
        return;
    }
    if (!requireParams(fd, message, 4))
        return;

    const std::string &username = message.params[0];

    /* username 不正の Numeric は設計書 04 §8 に規定がないため、
       Parameter 不備として 461 を返す (spec の決定事項) */
    if (username.empty()
        || username.find_first_of(" @\r\n") != std::string::npos
        || username.find('\0') != std::string::npos)
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NEEDMOREPARAMS,
                                         message.command
                                             + " :Not enough parameters"));
        return;
    }
    /* mode (params[1]) と unused (params[2]) は保存せず無視する。
       realname は空でも許可する (設計書 04 §8) */
    client->setUser(username, message.params[3]);
    tryRegisterClient(fd);
}

/* ── CAP (設計書 04 §9) ─────────────────── */

void Server::handleCap(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL || message.params.empty())
        return;

    /* 提供 Capability は 0 件。実クライアントの Negotiation を最低限
       終了させるだけで、CAP 状態は登録完了条件に含めない */
    std::string subcommand = IrcUtil::toUpperAscii(message.params[0]);

    if (subcommand == "LS")
        queueToClient(fd, Reply::command(Reply::serverPrefix(_serverName),
                                         "CAP", "* LS :"));
    else if (subcommand == "REQ")
    {
        std::string requested;

        if (message.params.size() >= 2)
            requested = message.params[1];
        queueToClient(fd, Reply::command(Reply::serverPrefix(_serverName),
                                         "CAP", "* NAK :" + requested));
    }
    /* END とその他の Subcommand は応答なし */
}

/* ── PING (設計書 04 §17) ───────────────── */

void Server::handlePing(int fd, const Message &message)
{
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    /* 空 token も「origin なし」として 409 (spec の決定事項) */
    if (message.params.empty() || message.params[0].empty())
    {
        queueToClient(fd, Reply::numeric(_serverName, *client,
                                         Numeric::ERR_NOORIGIN,
                                         ":No origin specified"));
        return;
    }
    queueToClient(fd, Reply::command(Reply::serverPrefix(_serverName), "PONG",
                                     _serverName + " :" + message.params[0]));
}

/* ── PONG (設計書 04 §18) ───────────────── */

void Server::handlePong(int fd, const Message &message)
{
    /* Server から定期 PING を送らないため、Parameter の有無に
       かかわらず受信して無視する。将来の keepalive に備え Handler
       だけ独立させている */
    (void)fd;
    (void)message;
}
