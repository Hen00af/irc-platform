#ifndef UPLOAD_HANDLER_HPP
#define UPLOAD_HANDLER_HPP

#include "Router.hpp"

class UploadHandler {
  public:
    static Response handle(const Request &request, const RouteResult &route,
                           const ServerConfig &config);

  private:
    UploadHandler();
};

#endif
