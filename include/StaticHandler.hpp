#ifndef STATIC_HANDLER_HPP
#define STATIC_HANDLER_HPP

#include "Router.hpp"

class StaticHandler {
  public:
    static Response handle(const Request &request, const RouteResult &route,
                           const ServerConfig &config);

  private:
    StaticHandler();
};

#endif
