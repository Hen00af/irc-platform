// #include <iostream>
// #include <cstring>
// #include <cerrno>
// #include <unistd.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <fcntl.h>
// #include <poll.h>
// #include <map>
#include <sstream>
// #include "../persing/persing_conf.hpp"
#include "server_multi_io.hpp"

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
    _port = port;
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
    fcntl(_server_fd, F_SETFL, O_NONBLOCK);
}

void Server::boot_server(Conf &conf) {
    const std::vector<ServerConfig> &servers = conf.get_servers();
    Server::build_connection(servers);
    std::cout << "server listening on port " << _port << std::endl;

    std::vector<struct pollfd> fds;
    std::map<int, ClientState> clients;

    struct pollfd server_pfd;
    server_pfd.fd = _server_fd;
    server_pfd.events = POLLIN;
    server_pfd.revents = 0;
    fds.reserve(1);
    fds.push_back(server_pfd);

    while(true) {
        int ready = poll(&fds[0], fds.size(), -1);
        if(ready == -1) {
            std::cerr << "poll error" << std::endl;
            break;
        }
        // FDイベント
        for(size_t i = 0; i < fds.size(); i++) {
            if(fds[i].revents == 0)
                continue;
            if(fds[i].fd == _server_fd) {
                int client_fd = accept(_server_fd, NULL, NULL);
                if(client_fd == -1) {
                    std::cerr << "accept error" << std::endl;
                    continue;
                }
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                struct pollfd client_pfd;
                client_pfd.fd = client_fd;
                client_pfd.events = POLLIN;
                client_pfd.revents = 0;
                fds.push_back(client_pfd);
                clients[client_fd] = ClientState();
                std::cout << "new client: fd = " << client_fd << std::endl;
                continue;
            }
            if(fds[i].revents & POLLIN)  {
                char buf[MEM_LIMIT];
                ssize_t n = recv(fds[i].fd, buf, sizeof(buf), 0);
                
                if(n <= 0) {
                    if(n == 0)
                        std::cout << "client disconnected: fd=" << fds[i].fd << std::endl;
                    else 
                        std::cerr << "recv error" << std::endl;
                    close(fds[i].fd);
                    clients.erase(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }
                ClientState &cs = clients[fds[i].fd];
                cs.read_buffer.append(buf, n);
                if(cs.read_buffer.find("\r\n\r\n") != std::string::npos) {
                    std::cout << "----- request -----" << std::endl;
                    std::cout << cs.read_buffer << std::endl;

                    //レスポンス(仮)　＝　HTTPパーサー部分
                    std::string body = "Hello webserv\n";
                    std::ostringstream oss;
                    oss << body.size();
                    std::string response =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: " + oss.str() + "\r\n"
                        "Connection: close\r\n"
                        "\r\n" + body;

                    // 書き込み監視に切り替え
                    cs.write_buffer = response;
                    fds[i].events = POLLOUT;
                }
            }
            if(fds[i].revents & POLLOUT) {
                ClientState &cs = clients[fds[i].fd];
                std::string &w_buf = cs.write_buffer;
                ssize_t sent = send(fds[i].fd, w_buf.c_str(),w_buf.size(), 0);
                if(sent == -1) {
                    std::cerr << "send error" << std::endl;
                    close(fds[i].fd);
                    clients.erase(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }
                // 遅れた分だけ削除
                w_buf.erase(0, sent);

                //全て送ったら接続終了
                if(w_buf.empty()) {
                    close(fds[i].fd);
                    clients.erase(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                }
            }
        }
    }
}

// void Server::boot_server(Conf &conf) {
//     const std::vector<ServerConfig> &servers = conf.get_servers();

//     Server::build_connection(servers);
//     std::cout << "server listening on port " << _port << std::endl;
//     _client_fd = accept(_server_fd, NULL, NULL);

//     while (true) {
//         if (_client_fd == -1) {
//             std::cerr << "accept error: " << std::strerror(errno) << std::endl;
//             continue;
//         }
//         char buffer[MEM_LIMIT];
//         std::memset(buffer, 0, sizeof(buffer));

//         ssize_t n = recv(_client_fd, buffer, sizeof(buffer) - 1, 0);
//         if (n == -1) {
//             std::cerr << "recv failed: " << std::strerror(errno) << std::endl;
//             close(_client_fd);
//             break;
//         }

//         if (n == 0) {
//             std::cout << "client closed connection" << std::endl;
//             close(_client_fd);
//             break;
//         }

//         std::cout << "----- request -----" << std::endl;
//         std::cout.write(buffer, n);
//         std::cout << std::endl;

//         std::string body = "Hello webserv\n";

//         std::string response =
//             "HTTP/1.1 200 OK\r\n"
//             "Content-Type: text/plain\r\n"
//             "Content-Length: " + std::to_string(body.size()) + "\r\n"
//             "Connection: Keep-alive\r\n"
//             "\r\n" +
//             body;

//         ssize_t sent = send(_client_fd, response.c_str(), response.size(), 0);
//         if (sent == -1) {
//             std::cerr << "send failed: " << std::strerror(errno) << std::endl;
//         }
//     }
// }

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
    serv.boot_server(conf);
}
