#include "Server.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ============================================================
 * IRC 数値リプライ定数
 * ============================================================ */
static const int RPL_WELCOME        = 1;
static const int RPL_YOURHOST       = 2;
static const int RPL_CREATED        = 3;
static const int RPL_MYINFO         = 4;
static const int RPL_WHOREPLY       = 352;
static const int RPL_ENDOFWHO       = 315;
static const int RPL_CHANNELMODEIS  = 324;
static const int RPL_NOTOPIC        = 331;
static const int RPL_TOPIC          = 332;
static const int RPL_INVITING       = 341;
static const int RPL_NAMREPLY       = 353;
static const int RPL_ENDOFNAMES     = 366;
static const int ERR_NOSUCHNICK     = 401;
static const int ERR_NOSUCHCHANNEL  = 403;
static const int ERR_CANNOTSENDTOCHAN = 404;
static const int ERR_TOOMANYCHANNELS  = 405;
static const int ERR_UNKNOWNCOMMAND = 421;
static const int ERR_NONICKNAMEGIVEN = 431;
static const int ERR_ERRONEUSNICKNAME = 432;
static const int ERR_NICKNAMEINUSE  = 433;
static const int ERR_USERNOTINCHANNEL = 441;
static const int ERR_NOTONCHANNEL   = 442;
static const int ERR_USERONCHANNEL  = 443;
static const int ERR_NEEDMOREPARAMS = 461;
static const int ERR_ALREADYREGISTRED = 462;
static const int ERR_PASSWDMISMATCH = 464;
static const int ERR_CHANNELISFULL  = 471;
static const int ERR_INVITEONLYCHAN = 473;
static const int ERR_BADCHANNELKEY  = 475;
static const int ERR_CHANOPRIVSNEEDED = 482;

const std::string Server::SERVNAME = "ircserv";

/* ============================================================
 * コンストラクタ / デストラクタ
 * ============================================================ */
Server::Server(int port, const std::string& password)
    : _port(port), _password(password), _serverFd(-1), _running(false)
{
    _setupServer();
}

Server::~Server()
{
    /* 全クライアントを切断 */
    for (std::map<int, Client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        close(it->first);
        delete it->second;
    }
    /* 全チャンネルを削除 */
    for (std::map<std::string, Channel*>::iterator it = _channels.begin();
         it != _channels.end(); ++it)
        delete it->second;
    if (_serverFd >= 0)
        close(_serverFd);
}

/* ============================================================
 * サーバーソケットのセットアップ
 * ============================================================ */
void Server::_setupServer()
{
    _serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverFd == -1)
        throw std::runtime_error(std::string("socket: ") + strerror(errno));

    int opt = 1;
    setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    fcntl(_serverFd, F_SETFL, O_NONBLOCK);

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(_port));

    if (bind(_serverFd, reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr)) == -1)
        throw std::runtime_error(std::string("bind: ") + strerror(errno));

    if (listen(_serverFd, SOMAXCONN) == -1)
        throw std::runtime_error(std::string("listen: ") + strerror(errno));

    _addPollFd(_serverFd, POLLIN);
    std::cout << "[" << SERVNAME << "] listening on port " << _port << std::endl;
}

/* ============================================================
 * run() / stop()
 * ============================================================ */
void Server::stop()
{
    _running = false;
}


void Server::run()
{
    _running = true;
    while (_running)
    {
        int ret = poll(&_fds[0], static_cast<nfds_t>(_fds.size()), 100);
        if (ret == -1)
        {
            if (errno == EINTR)
                continue;
            throw std::runtime_error(std::string("poll: ") + strerror(errno));
        }

        /* イベントがあった fd を処理 (ループ中に _fds が変わるのでコピー) */
        std::vector<struct pollfd> fds_snap = _fds;
        for (std::size_t i = 0; i < fds_snap.size(); ++i)
        {
            if (fds_snap[i].revents == 0)
                continue;

            if (fds_snap[i].fd == _serverFd)
            {
                if (fds_snap[i].revents & POLLIN)
                    _acceptClient();
                continue;
            }

            /* クライアント fd */
            if (fds_snap[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                _disconnectClient(fds_snap[i].fd);
                continue;
            }
            if (fds_snap[i].revents & POLLIN)
                _recvData(fds_snap[i].fd);
            if (fds_snap[i].revents & POLLOUT)
                _sendData(fds_snap[i].fd);
        }
    }
}

/* ============================================================
 * 新規クライアントの受け入れ
 * ============================================================ */
void Server::_acceptClient()
{
    struct sockaddr_in addr;
    socklen_t          len = sizeof(addr);

    int fd = accept(_serverFd, reinterpret_cast<struct sockaddr*>(&addr), &len);
    if (fd == -1)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            std::cerr << "accept: " << strerror(errno) << std::endl;
        return;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);

    char host[NI_MAXHOST];
    if (getnameinfo(reinterpret_cast<struct sockaddr*>(&addr), len,
                    host, sizeof(host), NULL, 0, NI_NUMERICHOST) != 0)
        std::strncpy(host, inet_ntoa(addr.sin_addr), sizeof(host) - 1);

    Client* client = new Client(fd, host);
    _clients[fd]   = client;
    _addPollFd(fd, POLLIN);

    std::cout << "[connect] fd=" << fd << " host=" << host << std::endl;
}

/* ============================================================
 * データ受信
 * ============================================================ */
void Server::_recvData(int fd)
{
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);

    if (n <= 0)
    {
        _disconnectClient(fd);
        return;
    }

    buf[n] = '\0';
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    it->second->recvBuf += buf;
    _processBuffer(it->second);
}

/* ============================================================
 * 送信バッファのフラッシュ
 * ============================================================ */
void Server::_sendData(int fd)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    Client* client = it->second;
    if (client->sendBuf.empty())
    {
        _setPollOut(fd, false);
        return;
    }

    ssize_t n = send(fd, client->sendBuf.c_str(), client->sendBuf.size(), 0);
    if (n <= 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            _disconnectClient(fd);
        return;
    }

    client->sendBuf.erase(0, static_cast<std::size_t>(n));
    if (client->sendBuf.empty())
        _setPollOut(fd, false);
}

/* ============================================================
 * クライアント切断
 * ============================================================ */
void Server::_disconnectClient(int fd, const std::string& reason)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    Client* client = it->second;
    std::string msg = ":" + client->prefix() + " QUIT :" +
                      (reason.empty() ? "Connection closed" : reason) + "\r\n";

    /* 全チャンネルから退出させ、メンバーに通知 */
    for (std::map<std::string, Channel*>::iterator ch = _channels.begin();
         ch != _channels.end(); )
    {
        Channel* channel = ch->second;
        if (channel->hasMember(fd))
        {
            channel->broadcast(msg, fd);
            channel->removeMember(fd);
            if (channel->members.empty())
            {
                delete channel;
                _channels.erase(ch++);
                continue;
            }
        }
        ++ch;
    }

    std::cout << "[disconnect] fd=" << fd
              << " nick=" << client->nickname << std::endl;

    _removePollFd(fd);
    close(fd);
    delete client;
    _clients.erase(it);
}

/* ============================================================
 * pollfd 管理
 * ============================================================ */
void Server::_addPollFd(int fd, short events)
{
    struct pollfd pfd;
    pfd.fd      = fd;
    pfd.events  = events;
    pfd.revents = 0;
    _fds.push_back(pfd);
}

void Server::_removePollFd(int fd)
{
    for (std::vector<struct pollfd>::iterator it = _fds.begin();
         it != _fds.end(); ++it)
    {
        if (it->fd == fd)
        {
            _fds.erase(it);
            return;
        }
    }
}

void Server::_setPollOut(int fd, bool enable)
{
    for (std::vector<struct pollfd>::iterator it = _fds.begin();
         it != _fds.end(); ++it)
    {
        if (it->fd == fd)
        {
            if (enable)
                it->events |= POLLOUT;
            else
                it->events &= ~POLLOUT;
            return;
        }
    }
}

/* ============================================================
 * 受信バッファから行を取り出してディスパッチ
 * ============================================================ */
void Server::_processBuffer(Client* client)
{
    std::string& buf = client->recvBuf;
    std::string::size_type pos;

    while ((pos = buf.find('\n')) != std::string::npos)
    {
        std::string line = buf.substr(0, pos);
        buf.erase(0, pos + 1);

        /* \r を除去 */
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (!line.empty())
            _dispatch(client, line);
    }
}

/* ============================================================
 * IRC メッセージのパースとコマンドディスパッチ
 *
 * フォーマット: [:prefix] COMMAND [params...] [:trailing]
 * ============================================================ */
void Server::_dispatch(Client* client, const std::string& line)
{
    std::vector<std::string> args = _parseArgs(line);
    if (args.empty())
        return;

    std::string cmd = _toUpper(args[0]);
    /* prefix (:...) が先頭にある場合は args[0] が prefix, args[1] が command */
    if (cmd[0] == ':')
    {
        if (args.size() < 2)
            return;
        args.erase(args.begin());
        cmd = _toUpper(args[0]);
    }
    /* コマンド自体を args から除去 → args は引数のみ */
    args.erase(args.begin());

    if      (cmd == "PASS")    _cmdPass   (client, args);
    else if (cmd == "NICK")    _cmdNick   (client, args);
    else if (cmd == "USER")    _cmdUser   (client, args);
    else if (cmd == "QUIT")    _cmdQuit   (client, args);
    else if (cmd == "PING")    _cmdPing   (client, args);
    else if (cmd == "PONG")    { /* 無視 */ }
    else if (cmd == "JOIN")    _cmdJoin   (client, args);
    else if (cmd == "PART")    _cmdPart   (client, args);
    else if (cmd == "PRIVMSG") _cmdPrivmsg(client, args);
    else if (cmd == "NOTICE")  _cmdNotice (client, args);
    else if (cmd == "KICK")    _cmdKick   (client, args);
    else if (cmd == "INVITE")  _cmdInvite (client, args);
    else if (cmd == "TOPIC")   _cmdTopic  (client, args);
    else if (cmd == "MODE")    _cmdMode   (client, args);
    else if (cmd == "WHO")     _cmdWho    (client, args);
    else if (cmd == "CAP")     { /* クライアントのネゴシエーション: 無視 */ }
    else
    {
        if (client->isRegistered())
            _numericReply(client, ERR_UNKNOWNCOMMAND,
                          cmd + " :Unknown command");
    }
}

/* ============================================================
 * PASS コマンド
 * ============================================================ */
void Server::_cmdPass(Client* client, const std::vector<std::string>& args)
{
    if (client->isRegistered())
    {
        _numericReply(client, ERR_ALREADYREGISTRED, ":You may not reregister");
        return;
    }
    if (args.empty())
    {
        _numericReply(client, ERR_NEEDMOREPARAMS, "PASS :Not enough parameters");
        return;
    }
    if (args[0] != _password)
    {
        _numericReply(client, ERR_PASSWDMISMATCH, ":Password incorrect");
        _disconnectClient(client->fd, "Bad password");
        return;
    }
    client->passOk = true;
}

/* ============================================================
 * NICK コマンド
 * ============================================================ */
void Server::_cmdNick(Client* client, const std::vector<std::string>& args)
{
    if (args.empty())
    {
        _numericReply(client, ERR_NONICKNAMEGIVEN, ":No nickname given");
        return;
    }

    const std::string& newNick = args[0];

    if (!_validNick(newNick))
    {
        _numericReply(client, ERR_ERRONEUSNICKNAME,
                      newNick + " :Erroneous nickname");
        return;
    }

    /* 重複チェック (大文字小文字を区別しない) */
    for (std::map<int, Client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        if (it->second == client)
            continue;
        if (_toUpper(it->second->nickname) == _toUpper(newNick))
        {
            _numericReply(client, ERR_NICKNAMEINUSE,
                          newNick + " :Nickname is already in use");
            return;
        }
    }

    std::string oldPrefix = client->prefix();

    client->nickname = newNick;
    client->nickOk   = true;

    if (client->isRegistered())
    {
        /* 既登録クライアントのニック変更: 自分と共有チャンネルのメンバーに通知 */
        std::string notice = ":" + oldPrefix + " NICK :" + newNick + "\r\n";
        client->appendSend(notice);

        for (std::map<std::string, Channel*>::iterator ch = _channels.begin();
             ch != _channels.end(); ++ch)
        {
            if (ch->second->hasMember(client->fd))
                ch->second->broadcast(notice, client->fd);
        }
    }
    else if (!client->passOk)
    {
        /* PASS なしで NICK を受けた場合はパスワードエラー */
        _numericReply(client, ERR_PASSWDMISMATCH, ":Password incorrect");
        _disconnectClient(client->fd, "Bad password");
        return;
    }
    else if (client->nickOk && client->userOk)
    {
        _sendWelcome(client);
    }
}

/* ============================================================
 * USER コマンド
 * ============================================================ */
void Server::_cmdUser(Client* client, const std::vector<std::string>& args)
{
    if (client->isRegistered())
    {
        _numericReply(client, ERR_ALREADYREGISTRED, ":You may not reregister");
        return;
    }
    if (args.size() < 4)
    {
        _numericReply(client, ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
        return;
    }

    client->username = args[0];
    /* args[1] = mode, args[2] = unused: 無視 */
    client->realname = args[3];
    client->userOk   = true;

    if (!client->passOk)
    {
        _numericReply(client, ERR_PASSWDMISMATCH, ":Password incorrect");
        _disconnectClient(client->fd, "Bad password");
        return;
    }

    if (client->nickOk && client->userOk)
        _sendWelcome(client);
}

/* ============================================================
 * QUIT コマンド
 * ============================================================ */
void Server::_cmdQuit(Client* client, const std::vector<std::string>& args)
{
    std::string reason = args.empty() ? "Quit" : args[0];
    _disconnectClient(client->fd, reason);
}

/* ============================================================
 * PING コマンド
 * ============================================================ */
void Server::_cmdPing(Client* client, const std::vector<std::string>& args)
{
    std::string token = args.empty() ? SERVNAME : args[0];
    _reply(client, ":" + SERVNAME + " PONG " + SERVNAME + " :" + token + "\r\n");
}

/* ============================================================
 * JOIN コマンド
 * ============================================================ */
void Server::_cmdJoin(Client* client, const std::vector<std::string>& args)
{
    if (!client->isRegistered())
        return;
    if (args.empty())
    {
        _numericReply(client, ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
        return;
    }

    /* JOIN 0 → 全チャンネルから退出 */
    if (args[0] == "0")
    {
        for (std::map<std::string, Channel*>::iterator ch = _channels.begin();
             ch != _channels.end(); )
        {
            Channel* channel = ch->second;
            if (channel->hasMember(client->fd))
            {
                _partChannel(client, channel, "");
                if (channel->members.empty())
                {
                    delete channel;
                    _channels.erase(ch++);
                    continue;
                }
            }
            ++ch;
        }
        return;
    }

    /* チャンネル名とキーをパース (カンマ区切りで複数可) */
    std::vector<std::string> names, keys;
    {
        std::string s = args[0];
        std::istringstream ss(s);
        std::string tok;
        while (std::getline(ss, tok, ','))
            names.push_back(tok);
    }
    if (args.size() >= 2)
    {
        std::string s = args[1];
        std::istringstream ss(s);
        std::string tok;
        while (std::getline(ss, tok, ','))
            keys.push_back(tok);
    }

    for (std::size_t i = 0; i < names.size(); ++i)
    {
        const std::string& chanName = names[i];
        std::string key = (i < keys.size()) ? keys[i] : "";

        if (!_validChannel(chanName))
        {
            _numericReply(client, ERR_NOSUCHCHANNEL,
                          chanName + " :No such channel");
            continue;
        }

        Channel* channel = _getOrCreateChannel(chanName);

        if (channel->hasMember(client->fd))
            continue; /* 既に参加済み */

        /* モードチェック */
        if (channel->userLimit > 0 &&
            static_cast<int>(channel->members.size()) >= channel->userLimit)
        {
            _numericReply(client, ERR_CHANNELISFULL,
                          chanName + " :Cannot join channel (+l)");
            continue;
        }
        if (channel->inviteOnly && !channel->isInvited(client->fd))
        {
            _numericReply(client, ERR_INVITEONLYCHAN,
                          chanName + " :Cannot join channel (+i)");
            continue;
        }
        if (!channel->key.empty() && channel->key != key)
        {
            _numericReply(client, ERR_BADCHANNELKEY,
                          chanName + " :Cannot join channel (+k)");
            continue;
        }

        channel->addMember(client);
        channel->invited.erase(client->fd);

        /* 最初のメンバーはオペレータに */
        if (channel->members.size() == 1)
            channel->addOperator(client->fd);

        std::string joinMsg = ":" + client->prefix() + " JOIN :" + chanName + "\r\n";
        channel->broadcast(joinMsg, client->fd);
        client->appendSend(joinMsg);
        _setPollOut(client->fd, true);

        /* TOPIC */
        if (!channel->topic.empty())
            _numericReply(client, RPL_TOPIC,
                          chanName + " :" + channel->topic);
        else
            _numericReply(client, RPL_NOTOPIC, chanName + " :No topic is set");

        /* NAMES */
        _numericReply(client, RPL_NAMREPLY,
                      "= " + chanName + " :" + channel->getMemberList());
        _numericReply(client, RPL_ENDOFNAMES,
                      chanName + " :End of /NAMES list");
    }
}

/* ============================================================
 * PART コマンド
 * ============================================================ */
void Server::_cmdPart(Client* client, const std::vector<std::string>& args)
{
    if (!client->isRegistered())
        return;
    if (args.empty())
    {
        _numericReply(client, ERR_NEEDMOREPARAMS, "PART :Not enough parameters");
        return;
    }

    std::string reason = (args.size() >= 2) ? args[1] : "";

    std::vector<std::string> names;
    {
        std::istringstream ss(args[0]);
        std::string tok;
        while (std::getline(ss, tok, ','))
            names.push_back(tok);
    }

    for (std::size_t i = 0; i < names.size(); ++i)
    {
        std::map<std::string, Channel*>::iterator it =
            _channels.find(names[i]);
        if (it == _channels.end())
        {
            _numericReply(client, ERR_NOSUCHCHANNEL,
                          names[i] + " :No such channel");
            continue;
        }
        Channel* channel = it->second;
        if (!channel->hasMember(client->fd))
        {
            _numericReply(client, ERR_NOTONCHANNEL,
                          names[i] + " :You're not on that channel");
            continue;
        }

        _partChannel(client, channel, reason);

        if (channel->members.empty())
        {
            delete channel;
            _channels.erase(it);
        }
    }
}

/* ============================================================
 * PRIVMSG コマンド
 * ============================================================ */
void Server::_cmdPrivmsg(Client* client, const std::vector<std::string>& args)
{
    if (!client->isRegistered())
        return;
    if (args.size() < 2)
    {
        _numericReply(client, ERR_NEEDMOREPARAMS, "PRIVMSG :Not enough parameters");
        return;
    }

    const std::string& target = args[0];
    const std::string& text   = args[1];
    std::string msg = ":" + client->prefix() + " PRIVMSG " + target +
                      " :" + text + "\r\n";

    if (target[0] == '#' || target[0] == '&')
    {
        std::map<std::string, Channel*>::iterator it = _channels.find(target);
        if (it == _channels.end())
        {
            _numericReply(client, ERR_NOSUCHCHANNEL,
                          target + " :No such channel");
            return;
        }
        if (!it->second->hasMember(client->fd))
        {
            _numericReply(client, ERR_CANNOTSENDTOCHAN,
                          target + " :Cannot send to channel");
            return;
        }
        it->second->broadcast(msg, client->fd);
        /* broadcast した各クライアントに POLLOUT を立てる */
        for (std::map<int, Client*>::iterator ci = it->second->members.begin();
             ci != it->second->members.end(); ++ci)
        {
            if (ci->first != client->fd)
                _setPollOut(ci->first, true);
        }
    }
    else
    {
        Client* target_client = _getClientByNick(target);
        if (!target_client)
        {
            _numericReply(client, ERR_NOSUCHNICK, target + " :No such nick");
            return;
        }
        target_client->appendSend(msg);
        _setPollOut(target_client->fd, true);
    }
}

/* ============================================================
 * NOTICE コマンド (PRIVMSG と同じだがエラーを返さない)
 * ============================================================ */
void Server::_cmdNotice(Client* client, const std::vector<std::string>& args)
{
    if (!client->isRegistered() || args.size() < 2)
        return;

    const std::string& target = args[0];
    const std::string& text   = args[1];
    std::string msg = ":" + client->prefix() + " NOTICE " + target +
                      " :" + text + "\r\n";

    if (target[0] == '#' || target[0] == '&')
    {
        std::map<std::string, Channel*>::iterator it = _channels.find(target);
        if (it == _channels.end() || !it->second->hasMember(client->fd))
            return;
        it->second->broadcast(msg, client->fd);
        for (std::map<int, Client*>::iterator ci = it->second->members.begin();
             ci != it->second->members.end(); ++ci)
        {
            if (ci->first != client->fd)
                _setPollOut(ci->first, true);
        }
    }
    else
    {
        Client* tc = _getClientByNick(target);
        if (!tc)
            return;
        tc->appendSend(msg);
        _setPollOut(tc->fd, true);
    }
}

/* ============================================================
 * KICK コマンド
 * ============================================================ */
void Server::_cmdKick(Client* client, const std::vector<std::string>& args)
{
    if (!client->isRegistered())
        return;
    if (args.size() < 2)
    {
        _numericReply(client, ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
        return;
    }

    const std::string& chanName = args[0];
    const std::string& targetNick = args[1];
    std::string reason = (args.size() >= 3) ? args[2] : targetNick;

    std::map<std::string, Channel*>::iterator it = _channels.find(chanName);
    if (it == _channels.end())
    {
        _numericReply(client, ERR_NOSUCHCHANNEL,
                      chanName + " :No such channel");
        return;
    }
    Channel* channel = it->second;

    if (!channel->hasMember(client->fd))
    {
        _numericReply(client, ERR_NOTONCHANNEL,
                      chanName + " :You're not on that channel");
        return;
    }
    if (!channel->isOperator(client->fd))
    {
        _numericReply(client, ERR_CHANOPRIVSNEEDED,
                      chanName + " :You're not channel operator");
        return;
    }

    Client* target = _getClientByNick(targetNick);
    if (!target || !channel->hasMember(target->fd))
    {
        _numericReply(client, ERR_USERNOTINCHANNEL,
                      targetNick + " " + chanName + " :They aren't on that channel");
        return;
    }

    std::string kickMsg = ":" + client->prefix() + " KICK " + chanName +
                          " " + targetNick + " :" + reason + "\r\n";
    channel->broadcast(kickMsg);
    _setPollOut(client->fd, true);
    _setPollOut(target->fd, true);

    channel->removeMember(target->fd);
    if (channel->members.empty())
    {
        delete channel;
        _channels.erase(it);
    }
}

/* ============================================================
 * INVITE コマンド
 * ============================================================ */
void Server::_cmdInvite(Client* client, const std::vector<std::string>& args)
{
    if (!client->isRegistered())
        return;
    if (args.size() < 2)
    {
        _numericReply(client, ERR_NEEDMOREPARAMS, "INVITE :Not enough parameters");
        return;
    }

    const std::string& targetNick = args[0];
    const std::string& chanName   = args[1];

    std::map<std::string, Channel*>::iterator it = _channels.find(chanName);
    if (it == _channels.end())
    {
        _numericReply(client, ERR_NOSUCHCHANNEL,
                      chanName + " :No such channel");
        return;
    }
    Channel* channel = it->second;

    if (!channel->hasMember(client->fd))
    {
        _numericReply(client, ERR_NOTONCHANNEL,
                      chanName + " :You're not on that channel");
        return;
    }
    if (channel->inviteOnly && !channel->isOperator(client->fd))
    {
        _numericReply(client, ERR_CHANOPRIVSNEEDED,
                      chanName + " :You're not channel operator");
        return;
    }

    Client* target = _getClientByNick(targetNick);
    if (!target)
    {
        _numericReply(client, ERR_NOSUCHNICK,
                      targetNick + " :No such nick");
        return;
    }
    if (channel->hasMember(target->fd))
    {
        _numericReply(client, ERR_USERONCHANNEL,
                      targetNick + " " + chanName + " :is already on channel");
        return;
    }

    channel->invited.insert(target->fd);

    _numericReply(client, RPL_INVITING, chanName + " " + targetNick);
    std::string invMsg = ":" + client->prefix() + " INVITE " + targetNick +
                         " :" + chanName + "\r\n";
    target->appendSend(invMsg);
    _setPollOut(target->fd, true);
}

/* ============================================================
 * TOPIC コマンド
 * ============================================================ */
void Server::_cmdTopic(Client* client, const std::vector<std::string>& args)
{
    if (!client->isRegistered())
        return;
    if (args.empty())
    {
        _numericReply(client, ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");
        return;
    }

    const std::string& chanName = args[0];
    std::map<std::string, Channel*>::iterator it = _channels.find(chanName);
    if (it == _channels.end())
    {
        _numericReply(client, ERR_NOSUCHCHANNEL,
                      chanName + " :No such channel");
        return;
    }
    Channel* channel = it->second;

    if (!channel->hasMember(client->fd))
    {
        _numericReply(client, ERR_NOTONCHANNEL,
                      chanName + " :You're not on that channel");
        return;
    }

    /* 引数なし → topic 取得 */
    if (args.size() < 2)
    {
        if (channel->topic.empty())
            _numericReply(client, RPL_NOTOPIC, chanName + " :No topic is set");
        else
            _numericReply(client, RPL_TOPIC,
                          chanName + " :" + channel->topic);
        return;
    }

    /* topic 変更 */
    if (channel->topicOp && !channel->isOperator(client->fd))
    {
        _numericReply(client, ERR_CHANOPRIVSNEEDED,
                      chanName + " :You're not channel operator");
        return;
    }

    channel->topic = args[1];
    std::string topicMsg = ":" + client->prefix() + " TOPIC " + chanName +
                           " :" + channel->topic + "\r\n";
    channel->broadcast(topicMsg);
    _setPollOut(client->fd, true);
}

/* ============================================================
 * MODE コマンド
 * ============================================================ */
void Server::_cmdMode(Client* client, const std::vector<std::string>& args)
{
    if (!client->isRegistered())
        return;
    if (args.empty())
    {
        _numericReply(client, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
        return;
    }

    const std::string& target = args[0];

    /* ユーザーモード (自分自身への MODE) */
    if (target == client->nickname)
    {
        /* 基本的な +i などのユーザーモードは無視するが、エラーにしない */
        return;
    }

    /* チャンネルモード */
    std::map<std::string, Channel*>::iterator it = _channels.find(target);
    if (it == _channels.end())
    {
        _numericReply(client, ERR_NOSUCHCHANNEL,
                      target + " :No such channel");
        return;
    }
    Channel* channel = it->second;

    /* モード文字列なし → 現在のモードを返す */
    if (args.size() < 2)
    {
        _numericReply(client, RPL_CHANNELMODEIS,
                      target + " " + channel->getModeStr());
        return;
    }

    if (!channel->isOperator(client->fd))
    {
        _numericReply(client, ERR_CHANOPRIVSNEEDED,
                      target + " :You're not channel operator");
        return;
    }

    const std::string& modeStr = args[1];
    bool adding = true;
    std::size_t paramIdx = 2; /* args のどこからパラメータを取るか */

    for (std::size_t i = 0; i < modeStr.size(); ++i)
    {
        char m = modeStr[i];
        if (m == '+') { adding = true;  continue; }
        if (m == '-') { adding = false; continue; }

        switch (m)
        {
        case 'i':
            channel->inviteOnly = adding;
            break;

        case 't':
            channel->topicOp = adding;
            break;

        case 'k':
            if (adding)
            {
                if (paramIdx >= args.size())
                {
                    _numericReply(client, ERR_NEEDMOREPARAMS,
                                  "MODE :Not enough parameters");
                    continue;
                }
                channel->key = args[paramIdx++];
            }
            else
            {
                channel->key.clear();
                if (paramIdx < args.size())
                    ++paramIdx; /* パラメータを消費 */
            }
            break;

        case 'o':
        {
            if (paramIdx >= args.size())
            {
                _numericReply(client, ERR_NEEDMOREPARAMS,
                              "MODE :Not enough parameters");
                continue;
            }
            const std::string& nick = args[paramIdx++];
            Client* tc = _getClientByNick(nick);
            if (!tc || !channel->hasMember(tc->fd))
            {
                _numericReply(client, ERR_USERNOTINCHANNEL,
                              nick + " " + target +
                              " :They aren't on that channel");
                continue;
            }
            if (adding)
                channel->addOperator(tc->fd);
            else
                channel->removeOperator(tc->fd);
            break;
        }

        case 'l':
            if (adding)
            {
                if (paramIdx >= args.size())
                {
                    _numericReply(client, ERR_NEEDMOREPARAMS,
                                  "MODE :Not enough parameters");
                    continue;
                }
                const std::string& limitStr = args[paramIdx++];
                bool valid = !limitStr.empty();
                for (std::size_t ci = 0; ci < limitStr.size(); ++ci)
                {
                    if (!std::isdigit(static_cast<unsigned char>(limitStr[ci])))
                    {
                        valid = false;
                        break;
                    }
                }
                if (!valid || limitStr.size() > 9)
                {
                    _numericReply(client, ERR_NEEDMOREPARAMS,
                                  "MODE :Invalid limit parameter");
                    continue;
                }
                channel->userLimit = std::atoi(limitStr.c_str());
                if (channel->userLimit <= 0)
                    channel->userLimit = 0;
            }
            else
            {
                channel->userLimit = 0;
            }
            break;

        default:
            _numericReply(client, 472,
                          std::string(1, m) + " :is unknown mode char to me");
            break;
        }
    }

    /* MODE 変更を通知 */
    std::string modeMsg = ":" + client->prefix() + " MODE " + target +
                          " " + modeStr;
    for (std::size_t i = 2; i < paramIdx && i < args.size(); ++i)
        modeMsg += " " + args[i];
    modeMsg += "\r\n";
    channel->broadcast(modeMsg);
    _setPollOut(client->fd, true);
}

/* ============================================================
 * WHO コマンド (簡易実装)
 * ============================================================ */
void Server::_cmdWho(Client* client, const std::vector<std::string>& args)
{
    if (!client->isRegistered())
        return;

    std::string mask = args.empty() ? "*" : args[0];

    if (mask[0] == '#' || mask[0] == '&')
    {
        std::map<std::string, Channel*>::iterator it = _channels.find(mask);
        if (it != _channels.end())
        {
            Channel* ch = it->second;
            for (std::map<int, Client*>::iterator mi = ch->members.begin();
                 mi != ch->members.end(); ++mi)
            {
                Client* m = mi->second;
                std::string flags = ch->isOperator(m->fd) ? "H@" : "H";
                /* 352: <channel> <user> <host> <server> <nick> <flags> :<hops> <realname> */
                _numericReply(client, RPL_WHOREPLY,
                              mask + " " + m->username + " " + m->hostname +
                              " " + SERVNAME + " " + m->nickname +
                              " " + flags + " :0 " + m->realname);
            }
        }
    }
    _numericReply(client, RPL_ENDOFWHO, mask + " :End of /WHO list");
}

/* ============================================================
 * ウェルカムメッセージ (001〜004)
 * ============================================================ */
void Server::_sendWelcome(Client* client)
{
    _numericReply(client, RPL_WELCOME,
                  ":Welcome to the IRC Network " + client->prefix());
    _numericReply(client, RPL_YOURHOST,
                  ":Your host is " + SERVNAME + ", running version 1.0");
    _numericReply(client, RPL_CREATED,
                  ":This server was created today");
    _numericReply(client, RPL_MYINFO,
                  SERVNAME + " 1.0 o itkol");
    _setPollOut(client->fd, true);
}

/* ============================================================
 * ヘルパー
 * ============================================================ */
Channel* Server::_getOrCreateChannel(const std::string& name)
{
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end())
        return it->second;
    Channel* ch = new Channel(name);
    _channels[name] = ch;
    return ch;
}

Client* Server::_getClientByNick(const std::string& nick)
{
    for (std::map<int, Client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        if (_toUpper(it->second->nickname) == _toUpper(nick))
            return it->second;
    }
    return NULL;
}

/* チャンネルから退出し全員に通知 */
void Server::_partChannel(Client* client, Channel* channel,
                           const std::string& reason)
{
    std::string partMsg = ":" + client->prefix() + " PART " + channel->name;
    if (!reason.empty())
        partMsg += " :" + reason;
    partMsg += "\r\n";

    /* 自分を含む全員に送信 */
    channel->broadcast(partMsg);
    client->appendSend(partMsg);
    _setPollOut(client->fd, true);

    channel->removeMember(client->fd);
}

/* IRC メッセージを送信バッファに追記し POLLOUT を立てる */
void Server::_reply(Client* client, const std::string& msg)
{
    client->appendSend(msg);
    _setPollOut(client->fd, true);
}

/* 数値リプライ: ":server NNN nick :msg\r\n" */
void Server::_numericReply(Client* client, int num, const std::string& msg)
{
    std::string nick = client->nickname.empty() ? "*" : client->nickname;
    std::ostringstream oss;
    oss << ":" << SERVNAME << " "
        << (num < 100 ? "0" : "") << (num < 10 ? "0" : "") << num
        << " " << nick << " " << msg << "\r\n";
    _reply(client, oss.str());
}

/* ============================================================
 * 静的ユーティリティ
 * ============================================================ */

/*
 * IRC メッセージを引数リストに分割する。
 *
 * 例: "JOIN #foo #bar key1"  → ["JOIN", "#foo", "#bar", "key1"]
 *     "PRIVMSG #foo :hello world" → ["PRIVMSG", "#foo", "hello world"]
 */
std::vector<std::string> Server::_parseArgs(const std::string& line)
{
    std::vector<std::string> result;
    std::size_t i = 0;
    const std::size_t len = line.size();

    while (i < len)
    {
        /* 空白をスキップ */
        while (i < len && line[i] == ' ')
            ++i;
        if (i >= len)
            break;

        if (line[i] == ':')
        {
            /* trailing パラメータ: 残り全部を1つのトークンとして取り込む */
            result.push_back(line.substr(i + 1));
            break;
        }

        /* 通常のトークン */
        std::size_t start = i;
        while (i < len && line[i] != ' ')
            ++i;
        result.push_back(line.substr(start, i - start));
    }
    return result;
}

/*
 * ニックネームの妥当性検証 (RFC 1459)
 * 先頭: 英字またはアンダースコア、バックスラッシュ、[]{|}^`
 * 残り: 英数字またはアンダースコア、ハイフン、バックスラッシュ、[]{|}^`
 * 長さ: 1〜9文字
 */
bool Server::_validNick(const std::string& nick)
{
    if (nick.empty() || nick.size() > 9)
        return false;

    static const std::string special = "_\\[]{}|^`";

    char c = nick[0];
    if (!std::isalpha(static_cast<unsigned char>(c)) &&
        special.find(c) == std::string::npos)
        return false;

    for (std::size_t i = 1; i < nick.size(); ++i)
    {
        c = nick[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) &&
            special.find(c) == std::string::npos && c != '-')
            return false;
    }
    return true;
}

/* チャンネル名の妥当性検証 */
bool Server::_validChannel(const std::string& name)
{
    if (name.size() < 2 || name.size() > 200)
        return false;
    if (name[0] != '#' && name[0] != '&')
        return false;
    for (std::size_t i = 1; i < name.size(); ++i)
    {
        char c = name[i];
        if (c == ' ' || c == '\a' || c == ',' || c == ':')
            return false;
    }
    return true;
}

/* ASCII 大文字変換 */
std::string Server::_toUpper(const std::string& s)
{
    std::string result = s;
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(result[i])));
    return result;
}

std::string Server::_itoa(int n)
{
    std::ostringstream oss;
    oss << n;
    return oss.str();
}
