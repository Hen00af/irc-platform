#include <csignal>
#include "source/persing/webserver.hpp"
#include "source/logging/logging.hpp"
#include "source/server/server.hpp"

Server serv;

int main(int ac, char **av, char **envp) {

    Conf    config;
    (void)envp;

    try {
        parsing_args(ac, av, config);
        print_all_data(config);
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return (1);
    }


    server(conf);

    return 0;
}
