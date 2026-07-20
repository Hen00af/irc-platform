#include <cstddef>
#include <utility>

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
