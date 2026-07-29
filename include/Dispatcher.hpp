#ifndef DISPATCHER_HPP
#define DISPATCHER_HPP

#include "Webserv.hpp"

class Dispatcher {
  public:
    static Response dispatch(const Request &request, const ServerConfig &config);

  private:
    Dispatcher();
};

#endif
