#include "conf.hpp"

#include <string>
#include <vector>

void addServer(Conf& cfg, const ServerConfig& server)
{
    cfg._servers.push_back(server);
}

static ServerConfig defaultServer()
{
    ServerConfig s;

    s.name = "localhost";
    s.listen = "8080";
    s.root = "./www";
    s.index = "index.html";
    s.body = "1048576";
    s.listing = "off";
    s.methods.push_back("GET");
    s.methods.push_back("POST");
    s.methods.push_back("DELETE");

    return s;
}

Conf initConf()
{
    Conf config;

    config._servers.push_back(defaultServer());

    return config;
}

Conf mock_conf = initConf();
