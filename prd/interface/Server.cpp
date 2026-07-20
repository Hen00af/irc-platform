#include <ctime>

#include "Server.hpp"

Server::Server(int port, const std::string &password)
    : _port(port),
      _password(password),
      _serverName("ircserv.local")
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
