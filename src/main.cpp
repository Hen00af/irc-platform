#include "Webserv.hpp"

static volatile sig_atomic_t g_running = 1;

static void stopServer(int) {
    g_running = 0;
}

bool serverRunning() {
    return g_running != 0;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        std::cerr << "usage: ./webserv [config/default.conf]" << std::endl;
        return 1;
    }
    const std::string path = argc == 2 ? argv[1] : "config/default.conf";
    try {
        std::signal(SIGINT, stopServer);
        std::signal(SIGTERM, stopServer);
        std::signal(SIGPIPE, SIG_IGN);
        Server server(Config::parse(path));
        server.run();
    } catch (const std::exception &error) {
        std::cerr << "webserv: " << error.what() << std::endl;
        return 1;
    }
    return 0;
}
