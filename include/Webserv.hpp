#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <limits>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

struct Location {
    std::string path;
    std::string root;
    std::string index;
    std::string redirect;
    int redirectStatus;
    std::string uploadDir;
    std::string cgiExtension;
    std::string cgiPath;
    size_t cgiTimeout;
    bool autoindex;
    std::vector<std::string> methods;
    Location();
};

struct ServerConfig {
    std::string host;
    unsigned short port;
    std::string root;
    std::string index;
    size_t maxBody;
    bool autoindex;
    std::vector<std::string> methods;
    std::map<int, std::string> errorPages;
    std::vector<Location> locations;
    ServerConfig();
};

class Config {
  public:
    static std::vector<ServerConfig> parse(const std::string &path);
};

struct Request {
    std::string method;
    std::string target;
    std::string path;
    std::string query;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string remoteAddress;
    unsigned short remotePort;

    Request() : remotePort(0) {}
};

struct Response {
    int status;
    std::map<std::string, std::string> headers;
    std::string body;
    Response(int code);
    std::string serialize() const;
};

enum ParseResult {
    REQUEST_INCOMPLETE,
    REQUEST_OK,
    REQUEST_BAD,
    REQUEST_TOO_LARGE,
    REQUEST_VERSION_NOT_SUPPORTED
};

ParseResult parseRequest(const std::string &raw, size_t maxBody, Request &request);
std::string reasonPhrase(int status);

class Server {
    enum ClientPhase {
        CLIENT_READING,
        CLIENT_CGI_WRITING,
        CLIENT_CGI_READING,
        CLIENT_WRITING
    };
    struct Listener {
        int fd;
        const ServerConfig *config;
    };
    struct Client {
        int fd;
        const ServerConfig *config;
        std::string input;
        std::string output;
        size_t sent;
        std::time_t touched;
        pid_t cgiPid;
        int cgiInputFd;
        int cgiOutputFd;
        std::string cgiInput;
        size_t cgiWritten;
        std::string cgiOutput;
        std::string remoteAddress;
        unsigned short remotePort;
        size_t timeoutSeconds;
        ClientPhase phase;
        Client();
    };
    std::vector<ServerConfig> _configs;
    std::vector<Listener> _listeners;
    std::map<int, Client> _clients;
    std::map<int, int> _cgiInputOwners;
    std::map<int, int> _cgiOutputOwners;
    std::vector<struct pollfd> _pollfds;

    void openListeners();
    void acceptClient(size_t index);
    void readClient(size_t index);
    void writeClient(size_t index);
    void writeCgi(size_t index);
    void readCgi(size_t index);
    void reapCgi();
    void finishCgi(int clientFd, int waitStatus);
    bool startCgi(size_t index, const Request &request);
    void closeClient(int fd);
    void removePollFd(int fd);
    bool isListener(int fd, const ServerConfig **config) const;
    void expireClients();

  public:
    explicit Server(const std::vector<ServerConfig> &configs);
    ~Server();
    void run();
};

std::string trim(const std::string &value);
std::string lower(const std::string &value);
std::string toString(size_t value);

#endif
