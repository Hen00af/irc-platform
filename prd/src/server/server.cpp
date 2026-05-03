#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../persing/persing_conf.hpp"
#include "server.hpp"

#define MEM_LIMIT 4096

class Conf;

void Server::build_connection(const std::vector<ServerConfig> &servers) {
    if (servers.empty()) {
        throw Boot_serverErr();
    }

    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd == -1) {
        std::cerr << "socket error: " << std::strerror(errno) << std::endl;
        throw Boot_serverErr();
    }

    const int enable = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &enable, sizeof(enable)) == -1) {
        std::cerr << "setsockopt error: " << std::strerror(errno) << std::endl;
        close(_server_fd);
        _server_fd = -1;
        throw Boot_serverErr();
    }

    std::memset(&_addr, 0, sizeof(_addr));

    _addr.sin_family = AF_INET;
    _addr.sin_addr.s_addr = INADDR_ANY;

    int port = std::stoi(servers[0].listen);
    _addr.sin_port = htons(port);

    if (bind(_server_fd,
             reinterpret_cast<struct sockaddr*>(&_addr),
             sizeof(_addr)) == -1) {
        std::cerr << "bind error: " << std::strerror(errno) << std::endl;
        close(_server_fd);
        throw Boot_serverErr();
    }

    if (listen(_server_fd, SOMAXCONN) == -1) {
        std::cerr << "listen error: " << std::strerror(errno) << std::endl;
        close(_server_fd);
        throw Boot_serverErr();
    }

}

void Server::boot_server(Conf &conf) {
    const std::vector<ServerConfig> &servers = conf.get_servers();
    
    Server::build_connection(servers);
    std::cout << "server listening on port " << _port << std::endl;
    _client_fd = accept(_server_fd, NULL, NULL);

    while (true) {
        if (_client_fd == -1) {
            std::cerr << "accept error: " << std::strerror(errno) << std::endl;
            continue;
        }
        char buffer[MEM_LIMIT];
        std::memset(buffer, 0, sizeof(buffer));

        ssize_t n = recv(_client_fd, buffer, sizeof(buffer) - 1, 0);
        if (n == -1) {
            std::cerr << "recv failed: " << std::strerror(errno) << std::endl;
            close(_client_fd);
            break;
        }

        if (n == 0) {
            std::cout << "client closed connection" << std::endl;
            close(_client_fd);
            break;
        }

        // std::cout << "----- request -----" << std::endl;
        // std::cout.write(buffer, n);
        // std::cout << std::endl;

        // std::string body = "Hello webserv\n";

        // std::string response =
        //     "HTTP/1.1 200 OK\r\n"
        //     "Content-Type: text/plain\r\n"
        //     "Content-Length: " + std::to_string(body.size()) + "\r\n"
        //     "Connection: Keep-alive\r\n"
        //     "\r\n" +
        //     body;

        // ssize_t sent = send(_client_fd, response.c_str(), response.size(), 0);
        if (sent == -1) {
            std::cerr << "send failed: " << std::strerror(errno) << std::endl;
        }
    }
}

Server::Server() :  _port(0), _server_fd(-1), _client_fd(-1) {}

Server::~Server() 
{
    if (_server_fd != -1) {
        close(_server_fd);
        _server_fd = -1;
    }
    if (_client_fd != -1) {
        close(_client_fd);
        _client_fd = -1;
    }
}

void tcp(Conf &conf) {
    Server serv;
    Request ;
    serv.boot_server(conf);
    serv.
}
