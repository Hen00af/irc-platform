#include "logging.hpp"
#include "../persing/persing_conf.hpp"

using std::cin;
using std::cout;
using std::endl;

/*
*** logging for config perser.
*/

void print_all_data(const Conf& conf)
{
    const std::vector<ServerConfig>& servers = conf.get_servers();
    std::cout << std::endl;
    for (size_t i = 0; i < servers.size(); ++i)
    {
        std::cout << "--- server " << i << ":" << endl;

        std::cout << "   name = " << servers[i].name << endl;
        std::cout << "   listen = " << servers[i].listen << endl;
        std::cout << "   root = " << servers[i].root << endl;
        std::cout << "   index = " << servers[i].index << endl;
        std::cout << "   body = " << servers[i].body << endl;
        std::cout << "   listing = " << servers[i].listing << endl;

        std::cout << "   method = ";
        for (size_t len = 0; len < servers[i].methods.size(); ++len)
        {
            std::cout << "   ";
            std::cout << servers[i].methods[len] << " ";
        }
        std::cout << std::endl;

        std::cout << "   error pages:" << std::endl;
        std::map<std::string, std::string> copy = servers[i].error_pages;
        for (std::map<std::string, std::string>::iterator it = copy.begin(); it != copy.end(); ++it)
        {
            std::cout << "   ";
            std::cout << "error " << it->first << " = " << it->second << endl;
        }

        for (size_t x = 0; x < servers[i].locations.size(); ++x)
        {
            std::cout << "   - location " << x << ":" << std::endl;
            std::cout << "       dir = " << servers[i].locations[x].dir << std::endl;
            std::cout << "       root = " << servers[i].locations[x].root << std::endl;
            std::cout << "       index = " << servers[i].locations[x].index << std::endl;
            std::cout << "       listing = " << servers[i].locations[x].listing << std::endl;
            std::cout << "       redir = " << servers[i].locations[x].redir << std::endl;
            std::cout << "       methods = ";
            for (size_t len = 0; len < servers[i].locations[x].methods.size(); ++len)
                std::cout << servers[i].locations[x].methods[len] << " ";
            std::cout << std::endl;
        }
    }
}

/*
***
*/