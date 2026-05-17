#ifndef ROUTING_HPP
#define ROUTING_HPP

#include <string>
#include <vector>

struct LocationConf {
    std::string dir;
    std::string root;
    std::vector<std::string> methods;
};

struct ServerConf {
    std::string name;
    std::string root;
    std::vector<LocationConf> locations;
};

enum RouteStatus {
    ROUTE_OK,
    ROUTE_NOT_FOUND,
    ROUTE_METHOD_NOT_ALLOWED
};

struct RouteResult {
    RouteStatus status;
    const ServerConf* server;
    const LocationConf* location;
    std::string fs_path;
};

const ServerConf* pick_server(
    const std::vector<ServerConf>& servers,
    const std::string& host);

const LocationConf* pick_location(
    const ServerConf& server,
    const std::string& target);

RouteResult route(
    const std::vector<ServerConf>& servers,
    const std::string& method,
    const std::string& target,
    const std::string& host);

const char* route_status_str(RouteStatus s);

#endif
