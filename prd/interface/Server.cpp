#include "Server.hpp"

Server::Server(int port, const std::string &password)
    : _port(port),
      _password(password),
      _serverName("ircserv.local")
{
}
