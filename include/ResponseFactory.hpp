#ifndef RESPONSE_FACTORY_HPP
#define RESPONSE_FACTORY_HPP

#include "Webserv.hpp"

class ResponseFactory {
  public:
    static Response error(int status, const ServerConfig &config);

  private:
    ResponseFactory();
};

#endif
