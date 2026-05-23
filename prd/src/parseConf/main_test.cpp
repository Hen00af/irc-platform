#include "parseConf.hpp"

using namespace std;

void print_all_data(const Conf &conf) {
    const std::vector<ServerConfig> &servers = conf.get_servers();
    for (size_t i = 0; i < servers.size(); ++i) {
        if (i > 0)
            std::cout << std::endl;
        std::cout << "server" << endl;
        std::cout << "server_name " << servers[i].name << endl;
        std::cout << "listen " << servers[i].listen << endl;
        for (size_t m = 0; m < servers[i].methods.size(); ++m)
            std::cout << "allowed_methods " << servers[i].methods[m] << endl;
        const std::map<std::string, std::string> &errs = servers[i].error_pages;
        for (std::map<std::string, std::string>::const_iterator it = errs.begin(); it != errs.end(); ++it)
            std::cout << "error_page " << it->first << " " << it->second << endl;
        std::cout << "root " << servers[i].root << endl;
        std::cout << "index " << servers[i].index << endl;
        std::cout << "client_max_body_size " << servers[i].body << endl;
        std::cout << "dir_listing " << servers[i].listing << endl;

        for (size_t x = 0; x < servers[i].locations.size(); ++x) {
            const LocationConfig &loc = servers[i].locations[x];
            std::cout << std::endl;
            std::cout << "location " << loc.dir << endl;
            std::cout << "root " << loc.root << endl;
            std::cout << "index " << loc.index << endl;
            for (size_t m = 0; m < loc.methods.size(); ++m)
                std::cout << "allowed_methods " << loc.methods[m] << endl;
            std::cout << "dir_listing " << loc.listing << endl;
            if (!loc.redir.empty())
                std::cout << "redir " << loc.redir << endl;
        }
    }
}

int main(int argc, char **argv, char **envp) {
    Conf conf;
    (void)envp;

    try {
        parseArgs(argc, argv, conf);
        print_all_data(conf);
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return (1);
    }

    return 0;
}
