#include "../persing/conf.hpp"
#include "../logging/logging.hpp"
#include <iostream>
#include <fstream>

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


    return 0;
}
