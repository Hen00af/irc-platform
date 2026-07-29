#include "server.hpp"

#include "../src/http/request.hpp"
#include "../src/responsebuilder/responsebuilder/responsebuilder.hpp"
#include "../src/responsebuilder/responseSerializer/responseSerializer.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <iostream>
#include <string>

#define MOCK_BUF 4096

static HttpRequest parseRequestLine(const std::string& raw)
{
    HttpRequest req;

    std::string::size_type sp1 = raw.find(' ');
    if (sp1 == std::string::npos)
        return req;
    std::string::size_type sp2 = raw.find(' ', sp1 + 1);
    if (sp2 == std::string::npos)
        return req;

    req.method = raw.substr(0, sp1);
    req.target = raw.substr(sp1 + 1, sp2 - sp1 - 1);

    std::string::size_type header_end = raw.find("\r\n\r\n");
    if (header_end != std::string::npos)
        req.body = raw.substr(header_end + 4);

    return req;
}

void serve_forever(Conf& conf)
{
    const std::vector<ServerConfig>& servers = conf.get_servers();
    if (servers.empty())
    {
        std::cerr << "mock_server: conf has no servers" << std::endl;
        return;
    }

    int port = std::atoi(servers[0].listen.c_str());

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        std::cerr << "mock_server: socket: " << std::strerror(errno) << std::endl;
        return;
    }

    int enable = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        std::cerr << "mock_server: bind: " << std::strerror(errno) << std::endl;
        close(listen_fd);
        return;
    }

    if (listen(listen_fd, 128) == -1)
    {
        std::cerr << "mock_server: listen: " << std::strerror(errno) << std::endl;
        close(listen_fd);
        return;
    }

    std::cout << "mock_server: listening on port " << port << std::endl;

    while (true)
    {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1)
        {
            std::cerr << "mock_server: accept: " << std::strerror(errno) << std::endl;
            continue;
        }

        char buf[MOCK_BUF];
        std::memset(buf, 0, sizeof(buf));

        ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
        {
            close(client_fd);
            continue;
        }

        HttpRequest req = parseRequestLine(std::string(buf, n));

        ResponseContext ctx;
        ctx.config = &conf;
        ctx.file_path = "";

        HttpResponse res = buildResponse(req, ctx);

        // とりあえず接続を閉じる方針
        res.headers["Connection"] = "close";

        std::string out = createResponse(res);
        send(client_fd, out.c_str(), out.size(), 0);

        close(client_fd);
    }

    close(listen_fd);
}