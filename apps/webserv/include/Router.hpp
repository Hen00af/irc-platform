#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "Webserv.hpp"

enum RouteStatus {
    ROUTE_READY,
    ROUTE_METHOD_NOT_ALLOWED,
    ROUTE_REDIRECT
};

struct RouteResult {
    RouteStatus status;
    const Location *location;
    const std::vector<std::string> *methods;
    std::string diskPath;
    std::string redirect;

    RouteResult();
};

class Router {
  public:
    static RouteResult resolve(const Request &request, const ServerConfig &config);

  private:
    Router();
};

#endif
