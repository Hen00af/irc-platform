#include "TestRunner.hpp"

#include "prd/domain/Message.hpp"
#include "prd/interface/Server.hpp"

void runDispatchTests()
{
    TestRunner::beginSuite("Dispatch");

    /* ── addClient / removeClient / findClientByFd ── */

    {
        Server server(6667, "pass");

        ASSERT_TRUE("Server: addClient 新規", server.addClient(3, "127.0.0.1"));
        ASSERT_FALSE("Server: addClient 重複 FD は拒否",
                     server.addClient(3, "10.0.0.1"));

        Client *client = server.findClientByFd(3);

        ASSERT_TRUE("Server: findClientByFd で取得できる", client != NULL);
        ASSERT_EQ("Server: Client の FD", client->getFd(), 3);
        ASSERT_EQ("Server: Client の Hostname", client->getHostname(),
                  "127.0.0.1");
        ASSERT_TRUE("Server: 不在 FD は NULL",
                    server.findClientByFd(99) == NULL);

        server.removeClient(3);
        ASSERT_TRUE("Server: removeClient 後は NULL",
                    server.findClientByFd(3) == NULL);
    }

    {
        Server server(6667, "pass");

        server.addClient(3, "127.0.0.1");

        const Server &constServer = server;

        ASSERT_TRUE("Server: const 版 findClientByFd",
                    constServer.findClientByFd(3) != NULL);
        ASSERT_TRUE("Server: const 版 findClientByFd 不在は NULL",
                    constServer.findClientByFd(99) == NULL);
    }

    /* ── queueToClient (設計書 02 §4.9) ────── */

    {
        Server server(6667, "pass");

        server.addClient(3, "127.0.0.1");

        server.queueToClient(3, "PING :x");
        ASSERT_EQ("Server: queueToClient は CRLF を付与する",
                  server.findClientByFd(3)->getSendBuffer(), "PING :x\r\n");

        server.queueToClient(3, "PONG :y\r\n");
        ASSERT_EQ("Server: 既に CRLF 付きなら二重付与しない",
                  server.findClientByFd(3)->getSendBuffer(),
                  "PING :x\r\nPONG :y\r\n");

        /* クラッシュしないことの確認 */
        server.queueToClient(99, "IGNORED");
        ASSERT_TRUE("Server: 不在 FD への queue は無視",
                    server.findClientByFd(99) == NULL);
    }
}
