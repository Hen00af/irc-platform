#include "../persing/persing_conf.hpp"
#include "../server/server_multi_io.hpp"
#include <iostream>

void tcp(Conf &conf);

int main(int ac, char **av, char **envp) {
    Conf config;
    (void)envp;

    try {
        parsing_args(ac, av, config);
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    tcp(config);
    return 0;
}
