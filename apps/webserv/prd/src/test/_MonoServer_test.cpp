#include "../persing/persing_conf.hpp"
#include "../logging/logging.hpp"
#include "../server/server.hpp"
#include <iostream>
#include <fstream>

void tcp(Conf &conf);

int main(int ac, char **av, char **envp) {
    Conf    config;
    const std::vector<ServerConfig> &servers = config.get_servers();
    (void)envp;

    try {
        parsing_args(ac, av, config);
        // print_all_data(config);
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return (1);
    }

    std::cout << "server_name: " << servers[1].name << std::endl;
    std::cout << "listen: " << servers[0].listen << std::endl;
    tcp(config);

    return 0;
}
