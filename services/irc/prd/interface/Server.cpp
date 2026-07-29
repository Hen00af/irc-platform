#include <ctime>
#include <unistd.h>

#include "Server.hpp"

Server::Server(int port, const std::string &password)
    : _port(port),
      _password(password),
      _serverName("ircserv.local"),
      _listenFd(-1),
      _running(true)
{
    /* 003 RPL_CREATED 用の開始時刻 (設計書 06 §6)。書式は spec で
       "%Y-%m-%d %H:%M:%S" と規定 */
    std::time_t now = std::time(NULL);
    char        buffer[32];

    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S",
                      std::localtime(&now)) != 0)
        _serverStartTime = buffer;
    else
        _serverStartTime = "unknown";
}

Server::~Server()
{
    /* 開いている全 Client FD と listen FD を close() する。IRC 通知は
       送信しない (設計書 02 §4.3) */
    for (std::map<int, Client>::const_iterator it = _clients.begin();
         it != _clients.end(); ++it)
        close(it->first);
    if (_listenFd >= 0)
        close(_listenFd);
}
