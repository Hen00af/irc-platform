#include <iostream>
#include <cstdlib>
#include <csignal>
#include <stdexcept>

#include "interface/Server.hpp"

/* ============================================================
 * シグナルハンドラ
 *
 * SIGINT / SIGTERM を受け取ったら server->stop() を呼ぶ。
 * poll() のタイムアウト (100ms) 後にループが終了する。
 * ============================================================ */
static Server* g_server = NULL;

static void signalHandler(int signum)
{
    (void)signum;
    if (g_server)
        g_server->stop();
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }

    /* ポート番号の検証 */
    char* end;
    long port = std::strtol(argv[1], &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535)
    {
        std::cerr << "Error: Invalid port \"" << argv[1]
                  << "\". Use a number between 1 and 65535." << std::endl;
        return 1;
    }

    /* パスワードの検証 */
    std::string password = argv[2];
    if (password.empty())
    {
        std::cerr << "Error: Password cannot be empty." << std::endl;
        return 1;
    }

    /* シグナル設定 */
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGPIPE, SIG_IGN); /* 切断済みクライアントへの write を無視 */

    try
    {
        Server server(static_cast<int>(port), password);
        g_server = &server;
        server.run();
        g_server = NULL;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n[" << argv[0] << "] Server stopped." << std::endl;
    return 0;
}
