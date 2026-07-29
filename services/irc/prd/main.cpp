#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "interface/Server.hpp"

/* ============================================================
 * エントリポイント (設計書 03 §2)
 *
 * ./ircserv <port> <password>
 * ============================================================ */
namespace
{
    void printUsage(const char *programName)
    {
        std::cerr << "Usage: " << programName << " <port> <password>"
                  << std::endl;
    }

    bool isAllDigits(const std::string &value)
    {
        if (value.empty())
            return false;
        for (std::string::size_type i = 0; i < value.size(); ++i)
        {
            if (value[i] < '0' || value[i] > '9')
                return false;
        }
        return true;
    }

    /* 空文字・非数字を拒否したうえで std::strtol() (overflow 安全) で
       変換し、1〜65535 の範囲を確認する (設計書 03 §2 手順 2〜5) */
    bool parsePort(const std::string &value, int &port)
    {
        if (!isAllDigits(value))
            return false;

        char     *end    = NULL;
        long      parsed = std::strtol(value.c_str(), &end, 10);

        if (end == NULL || *end != '\0')
            return false;
        if (parsed < 1 || parsed > 65535)
            return false;
        port = static_cast<int>(parsed);
        return true;
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printUsage(argv[0]);
        return 1;
    }

    int port = 0;

    if (!parsePort(argv[1], port))
    {
        printUsage(argv[0]);
        return 1;
    }

    std::string password = argv[2];

    if (password.empty())
    {
        printUsage(argv[0]);
        return 1;
    }

    try
    {
        Server server(port, password);

        server.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "ircserv: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
