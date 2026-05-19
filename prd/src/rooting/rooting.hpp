#ifndef ROOTING_HPP
#define ROOTING_HPP

#include "../persing/persing_conf.hpp"
#include "../persing/persing_request.hpp"

#include <string>
#include <vector>

/*
    routing のフラグ管理をする
    これに基づいてresponce status code を返す
*/
enum RouteStatus {
    ROUTE_OK,
    ROUTE_NOT_FOUND,
    ROUTE_METHOD_NOT_ALLOWED,
    ROUTE_REDIRECT
};

struct RouteResult {
    RouteStatus              status;
    const ServerConfig*      server;
    const LocationConfig*    location;
    std::string              fs_path;
    std::string              redirect_to;
};

const ServerConfig* pick_server(
    const std::vector<ServerConfig>& servers,
    const std::string& host);

const LocationConfig* pick_location(
    const ServerConfig& server,
    const std::string& target);

RouteResult route(
    const std::vector<ServerConfig>& servers,
    const RequestParser& req);

const char* route_status_str(RouteStatus s);

#endif
