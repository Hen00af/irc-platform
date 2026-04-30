#include <csignal>

Server serv;

int main(int ac, char **av, char **envp) {
    Conf    data;
    try {
        parsing(ac, av, data);
    }
    catch (const std::exception& e) {
        std::err << e.what() << std::endl;
        return (1);
    }
}
