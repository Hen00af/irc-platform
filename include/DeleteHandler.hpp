#ifndef DELETE_HANDLER_HPP
#define DELETE_HANDLER_HPP

#include "Router.hpp"

class DeleteHandler {
  public:
    static Response handle(const RouteResult &route, const ServerConfig &config);

  private:
    DeleteHandler();
};

#endif
