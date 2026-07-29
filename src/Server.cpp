#include "Webserv.hpp"
#include "CgiHandler.hpp"
#include "Dispatcher.hpp"
#include "ResponseFactory.hpp"
#include "Router.hpp"

bool serverRunning();

Server::Client::Client()
    : fd(-1), config(NULL), sent(0), touched(0), cgiPid(-1),
      cgiInputFd(-1), cgiOutputFd(-1), cgiWritten(0), remotePort(0),
      timeoutSeconds(30), phase(CLIENT_READING) {}

static void nonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl failed");
}

Server::Server(const std::vector<ServerConfig> &configs) : _configs(configs) {
    openListeners();
}

Server::~Server() {
    for (std::map<int, Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it) {
        if (it->second.cgiPid > 0)
            kill(it->second.cgiPid, SIGKILL);
        if (it->second.cgiInputFd >= 0)
            close(it->second.cgiInputFd);
        if (it->second.cgiOutputFd >= 0)
            close(it->second.cgiOutputFd);
        close(it->first);
    }
    for (std::map<int, Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
        if (it->second.cgiPid > 0)
            waitpid(it->second.cgiPid, NULL, 0);
    for (size_t i = 0; i < _listeners.size(); ++i)
        close(_listeners[i].fd);
}

void Server::openListeners() {
    for (size_t i = 0; i < _configs.size(); ++i) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            throw std::runtime_error("socket failed");
        int enabled = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
            close(fd);
            throw std::runtime_error("setsockopt failed");
        }
        nonBlocking(fd);
        struct sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(_configs[i].port);
        if (inet_pton(AF_INET, _configs[i].host.c_str(), &address.sin_addr) != 1) {
            close(fd);
            throw std::runtime_error("invalid listen address: " + _configs[i].host);
        }
        if (bind(fd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0 ||
            listen(fd, SOMAXCONN) < 0) {
            close(fd);
            throw std::runtime_error("cannot listen on port " + toString(_configs[i].port));
        }
        Listener listener;
        listener.fd = fd;
        listener.config = &_configs[i];
        _listeners.push_back(listener);
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        _pollfds.push_back(pfd);
        std::cout << "listening on " << _configs[i].host << ":"
                  << _configs[i].port << std::endl;
    }
}

bool Server::isListener(int fd, const ServerConfig **config) const {
    for (size_t i = 0; i < _listeners.size(); ++i) {
        if (_listeners[i].fd == fd) {
            if (config)
                *config = _listeners[i].config;
            return true;
        }
    }
    return false;
}

void Server::acceptClient(size_t index) {
    const ServerConfig *config = NULL;
    isListener(_pollfds[index].fd, &config);
    while (true) {
        struct sockaddr_in peer;
        socklen_t peerLength = sizeof(peer);
        std::memset(&peer, 0, sizeof(peer));
        int fd = accept(_pollfds[index].fd,
                        reinterpret_cast<struct sockaddr *>(&peer), &peerLength);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            return;
        }
        try {
            nonBlocking(fd);
        } catch (...) {
            close(fd);
            continue;
        }
        Client client;
        client.fd = fd;
        client.config = config;
        client.touched = std::time(NULL);
        char address[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &peer.sin_addr, address, sizeof(address)))
            client.remoteAddress = address;
        client.remotePort = ntohs(peer.sin_port);
        _clients[fd] = client;
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        _pollfds.push_back(pfd);
    }
}

bool Server::startCgi(size_t index, const Request &request) {
    const int clientFd = _pollfds[index].fd;
    Client &client = _clients[clientFd];
    const RouteResult route = Router::resolve(request, *client.config);
    if (route.status != ROUTE_READY || !CgiHandler::matches(route))
        return false;

    struct stat scriptInfo;
    if (stat(route.diskPath.c_str(), &scriptInfo) != 0 ||
        !S_ISREG(scriptInfo.st_mode)) {
        client.output = ResponseFactory::error(404, *client.config).serialize();
        client.phase = CLIENT_WRITING;
        _pollfds[index].events = POLLOUT;
        return true;
    }
    CgiProcess process;
    if (!CgiHandler::start(request, route, *client.config, process)) {
        client.output = ResponseFactory::error(500, *client.config).serialize();
        _pollfds[index].events = POLLOUT;
        return true;
    }
    client.cgiPid = process.pid;
    client.cgiInputFd = process.inputFd;
    client.cgiOutputFd = process.outputFd;
    client.cgiInput = request.body;
    client.cgiWritten = 0;
    client.cgiOutput.clear();
    client.touched = std::time(NULL);
    client.timeoutSeconds = route.location->cgiTimeout;
    client.phase = client.cgiInput.empty() ? CLIENT_CGI_READING
                                          : CLIENT_CGI_WRITING;
    _pollfds[index].events = 0;

    struct pollfd outputPfd;
    outputPfd.fd = process.outputFd;
    outputPfd.events = POLLIN;
    outputPfd.revents = 0;
    _pollfds.push_back(outputPfd);
    _cgiOutputOwners[process.outputFd] = clientFd;

    if (client.cgiInput.empty()) {
        close(process.inputFd);
        client.cgiInputFd = -1;
    } else {
        struct pollfd inputPfd;
        inputPfd.fd = process.inputFd;
        inputPfd.events = POLLOUT;
        inputPfd.revents = 0;
        _pollfds.push_back(inputPfd);
        _cgiInputOwners[process.inputFd] = clientFd;
    }
    return true;
}

void Server::readClient(size_t index) {
    const int fd = _pollfds[index].fd;
    Client &client = _clients[fd];
    char buffer[16384];
    const ssize_t count = recv(fd, buffer, sizeof(buffer), 0);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return;
    if (count <= 0) {
        closeClient(fd);
        return;
    }
    client.input.append(buffer, static_cast<size_t>(count));
    client.touched = std::time(NULL);
    Request request;
    const ParseResult result =
        parseRequest(client.input, client.config->maxBody, request);
    if (result == REQUEST_INCOMPLETE)
        return;
    request.remoteAddress = client.remoteAddress;
    request.remotePort = client.remotePort;
    if (result == REQUEST_OK && startCgi(index, request))
        return;

    Response response =
        result == REQUEST_TOO_LARGE
            ? ResponseFactory::error(413, *client.config)
            : (result == REQUEST_VERSION_NOT_SUPPORTED
                   ? ResponseFactory::error(505, *client.config)
            : (result == REQUEST_BAD
                   ? ResponseFactory::error(400, *client.config)
                   : Dispatcher::dispatch(request, *client.config)));
    client.output = response.serialize();
    client.phase = CLIENT_WRITING;
    _pollfds[index].events = POLLOUT;
}

void Server::writeClient(size_t index) {
    const int fd = _pollfds[index].fd;
    Client &client = _clients[fd];
    const ssize_t count =
        send(fd, client.output.data() + client.sent,
             client.output.size() - client.sent, 0);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return;
    if (count <= 0) {
        closeClient(fd);
        return;
    }
    client.sent += static_cast<size_t>(count);
    client.touched = std::time(NULL);
    if (client.sent == client.output.size())
        closeClient(fd);
}

void Server::writeCgi(size_t index) {
    const int pipeFd = _pollfds[index].fd;
    const int clientFd = _cgiInputOwners[pipeFd];
    Client &client = _clients[clientFd];
    const ssize_t count =
        write(pipeFd, client.cgiInput.data() + client.cgiWritten,
              client.cgiInput.size() - client.cgiWritten);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return;
    if (count > 0) {
        client.cgiWritten += static_cast<size_t>(count);
        client.touched = std::time(NULL);
    }
    if (count <= 0 || client.cgiWritten == client.cgiInput.size()) {
        close(pipeFd);
        client.cgiInputFd = -1;
        _cgiInputOwners.erase(pipeFd);
        removePollFd(pipeFd);
        client.phase = CLIENT_CGI_READING;
    }
}

void Server::readCgi(size_t index) {
    const int pipeFd = _pollfds[index].fd;
    const int clientFd = _cgiOutputOwners[pipeFd];
    Client &client = _clients[clientFd];
    char buffer[16384];
    const ssize_t count = read(pipeFd, buffer, sizeof(buffer));
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return;
    if (count > 0) {
        client.cgiOutput.append(buffer, static_cast<size_t>(count));
        client.touched = std::time(NULL);
        if (client.cgiOutput.size() > 16 * 1024 * 1024) {
            kill(client.cgiPid, SIGKILL);
            int status = 0;
            waitpid(client.cgiPid, &status, 0);
            finishCgi(clientFd, status);
        }
        return;
    }
    close(pipeFd);
    client.cgiOutputFd = -1;
    _cgiOutputOwners.erase(pipeFd);
    removePollFd(pipeFd);
    int status = 0;
    const pid_t result = waitpid(client.cgiPid, &status, WNOHANG);
    if (result == client.cgiPid)
        finishCgi(clientFd, status);
}

void Server::finishCgi(int clientFd, int waitStatus) {
    std::map<int, Client>::iterator found = _clients.find(clientFd);
    if (found == _clients.end())
        return;
    Client &client = found->second;
    if (client.cgiInputFd >= 0) {
        close(client.cgiInputFd);
        _cgiInputOwners.erase(client.cgiInputFd);
        removePollFd(client.cgiInputFd);
        client.cgiInputFd = -1;
    }
    if (client.cgiOutputFd >= 0) {
        close(client.cgiOutputFd);
        _cgiOutputOwners.erase(client.cgiOutputFd);
        removePollFd(client.cgiOutputFd);
        client.cgiOutputFd = -1;
    }
    const bool success =
        WIFEXITED(waitStatus) && WEXITSTATUS(waitStatus) == 0;
    const Response response =
        success ? CgiHandler::makeResponse(client.cgiOutput, *client.config)
                : ResponseFactory::error(500, *client.config);
    client.cgiPid = -1;
    client.output = response.serialize();
    client.sent = 0;
    client.touched = std::time(NULL);
    client.phase = CLIENT_WRITING;
    for (size_t i = 0; i < _pollfds.size(); ++i)
        if (_pollfds[i].fd == clientFd) {
            _pollfds[i].events = POLLOUT;
            return;
        }
}

void Server::reapCgi() {
    std::vector<int> clients;
    std::vector<int> statuses;
    for (std::map<int, Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it) {
        Client &client = it->second;
        if (client.cgiPid <= 0 || client.cgiOutputFd >= 0)
            continue;
        int status = 0;
        const pid_t result = waitpid(client.cgiPid, &status, WNOHANG);
        if (result == client.cgiPid) {
            clients.push_back(it->first);
            statuses.push_back(status);
        }
    }
    for (size_t i = 0; i < clients.size(); ++i)
        finishCgi(clients[i], statuses[i]);
}

void Server::removePollFd(int fd) {
    for (size_t i = 0; i < _pollfds.size(); ++i)
        if (_pollfds[i].fd == fd) {
            _pollfds.erase(_pollfds.begin() + i);
            return;
        }
}

void Server::closeClient(int fd) {
    std::map<int, Client>::iterator found = _clients.find(fd);
    if (found == _clients.end())
        return;
    Client &client = found->second;
    if (client.cgiPid > 0) {
        kill(client.cgiPid, SIGKILL);
        waitpid(client.cgiPid, NULL, 0);
    }
    if (client.cgiInputFd >= 0) {
        close(client.cgiInputFd);
        _cgiInputOwners.erase(client.cgiInputFd);
        removePollFd(client.cgiInputFd);
    }
    if (client.cgiOutputFd >= 0) {
        close(client.cgiOutputFd);
        _cgiOutputOwners.erase(client.cgiOutputFd);
        removePollFd(client.cgiOutputFd);
    }
    close(fd);
    removePollFd(fd);
    _clients.erase(found);
}

void Server::expireClients() {
    const std::time_t now = std::time(NULL);
    std::vector<int> expired;
    for (std::map<int, Client>::const_iterator it = _clients.begin();
         it != _clients.end(); ++it)
        if (now - it->second.touched >
            static_cast<std::time_t>(it->second.timeoutSeconds))
            expired.push_back(it->first);
    for (size_t i = 0; i < expired.size(); ++i) {
        std::map<int, Client>::iterator found = _clients.find(expired[i]);
        if (found == _clients.end())
            continue;
        Client &client = found->second;
        if (client.cgiPid > 0) {
            kill(client.cgiPid, SIGKILL);
            waitpid(client.cgiPid, NULL, 0);
            if (client.cgiInputFd >= 0) {
                close(client.cgiInputFd);
                _cgiInputOwners.erase(client.cgiInputFd);
                removePollFd(client.cgiInputFd);
                client.cgiInputFd = -1;
            }
            if (client.cgiOutputFd >= 0) {
                close(client.cgiOutputFd);
                _cgiOutputOwners.erase(client.cgiOutputFd);
                removePollFd(client.cgiOutputFd);
                client.cgiOutputFd = -1;
            }
            client.cgiPid = -1;
            client.output =
                ResponseFactory::error(504, *client.config).serialize();
            client.sent = 0;
            client.touched = now;
            client.phase = CLIENT_WRITING;
            for (size_t j = 0; j < _pollfds.size(); ++j)
                if (_pollfds[j].fd == client.fd)
                    _pollfds[j].events = POLLOUT;
        } else if (client.output.empty()) {
            client.output =
                ResponseFactory::error(408, *client.config).serialize();
            client.sent = 0;
            client.touched = now;
            client.phase = CLIENT_WRITING;
            for (size_t j = 0; j < _pollfds.size(); ++j)
                if (_pollfds[j].fd == client.fd)
                    _pollfds[j].events = POLLOUT;
        } else {
            closeClient(client.fd);
        }
    }
}

void Server::run() {
    while (serverRunning()) {
        int ready = poll(&_pollfds[0], _pollfds.size(), 1000);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("poll failed");
        }
        for (size_t i = 0; i < _pollfds.size() && ready > 0;) {
            const short events = _pollfds[i].revents;
            if (!events) {
                ++i;
                continue;
            }
            --ready;
            const int fd = _pollfds[i].fd;
            if (isListener(fd, NULL)) {
                if (events & POLLIN)
                    acceptClient(i);
                ++i;
            } else if (_cgiOutputOwners.count(fd)) {
                if (events & (POLLIN | POLLHUP | POLLERR))
                    readCgi(i);
                else if (events & POLLNVAL)
                    closeClient(_cgiOutputOwners[fd]);
                if (i < _pollfds.size() && _pollfds[i].fd == fd)
                    ++i;
            } else if (_cgiInputOwners.count(fd)) {
                if (events & POLLOUT)
                    writeCgi(i);
                else {
                    const int clientFd = _cgiInputOwners[fd];
                    closeClient(clientFd);
                }
                if (i < _pollfds.size() && _pollfds[i].fd == fd)
                    ++i;
            } else if (events & (POLLERR | POLLHUP | POLLNVAL)) {
                closeClient(fd);
            } else if (events & POLLIN) {
                readClient(i);
                if (i < _pollfds.size() && _pollfds[i].fd == fd)
                    ++i;
            } else if (events & POLLOUT) {
                writeClient(i);
                if (i < _pollfds.size() && _pollfds[i].fd == fd)
                    ++i;
            } else {
                ++i;
            }
        }
        reapCgi();
        expireClients();
    }
}
