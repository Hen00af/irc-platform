#include <csignal>

Server serv;

int main(int ac, char **av, char **envp) {

    Conf    data;


    (void)envp;

    try {
        parsing(ac, av, data);
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return (1);
    }

    // log for config
    {
        logging log;
        log.print_all_data(data);
    }

    return 0;
}
