#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../persing/conf.hpp"
#include "server.hpp"

class Conf;


void server(Conf &conf) {
    boot_server(conf);
 
}

void boot_server(Conf &conf) {
    const std::vector<ServerConfig> &server = conf.get_servers();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        throw Boot_serverErr();
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(server_fd);
        throw Boot_serverErr();
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(std::stoi(server[0].listen));

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        close(server_fd);
        throw Boot_serverErr();
    }

    if (listen(server_fd, SOMAXCONN) == -1) {
        close(server_fd);
        throw Boot_serverErr();
    }

    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        close(server_fd);
        throw Boot_serverErr();
    }
    
    while (true) {
        char buffer[4096];
        std::memset(buffer, 0, sizeof(buffer));
    
        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (n == -1) {
            std::cerr << "recv failed: " << std::strerror(errno) << std::endl;
            break;
        }
    
        if (n == 0) {
            // クライアントが接続を閉じた
            std::cout << "client closed connection" << std::endl;
            break;
        }
    
        std::cout << "----- request -----" << std::endl;
        std::cout << buffer << std::endl;
    
        std::string body = "Hello webserv\n";
    
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 14\r\n"
            "Connection: keep-alive\r\n"
            "\r\n" +
            body;
    
        ssize_t sent = send(client_fd, response.c_str(), response.size(), 0);
        if (sent == -1) {
            std::cerr << "send failed: " << std::strerror(errno) << std::endl;
            throw Boot_serverErr();
            break;
        }
    }
    close(client_fd);
    close(server_fd);

}


