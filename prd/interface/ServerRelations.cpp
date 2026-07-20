#include <cstddef>
#include <set>
#include <utility>

#include "../util/IrcUtil.hpp"
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
    Client *client = findClientByFd(fd);

    if (client == NULL)
        return;
    if (message.size() >= 2
        && message.compare(message.size() - 2, 2, "\r\n") == 0)
        client->appendSendBuffer(message);
    else
        client->appendSendBuffer(message + "\r\n");
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
