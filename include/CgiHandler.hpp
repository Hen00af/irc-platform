#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include "Router.hpp"

struct CgiProcess {
    pid_t pid;
    int inputFd;
    int outputFd;

    CgiProcess();
};

class CgiHandler {
  public:
    static bool matches(const RouteResult &route);
    static bool start(const Request &request, const RouteResult &route,
                      const ServerConfig &config, CgiProcess &process);
    static Response makeResponse(const std::string &output,
                                 const ServerConfig &config);

  private:
    CgiHandler();
};

#endif
