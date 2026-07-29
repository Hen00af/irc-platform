#include "TestRunner.hpp"

#include <cstddef>

#include "prd/domain/Message.hpp"
#include "prd/interface/Server.hpp"

namespace
{
    /* PASS / NICK / USER を揃えて登録済みにする (設計書 04 §5) */
    void registerClient(Server &server, int fd, const std::string &nickname)
    {
        Client *client = server.findClientByFd(fd);

        client->acceptPassword();
        client->setNickname(nickname);
        client->setUser("u", "Real Name");
        client->tryCompleteRegistration();
    }
}

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

    /* ── dispatchCommand: 登録前ゲーティング (設計書 04 §3) ── */

    {
        /* 登録必須 Command は登録前 451 */
        const char *commands[] = { "JOIN", "PRIVMSG", "KICK", "INVITE",
                                   "TOPIC", "MODE", "PART" };

        for (std::size_t i = 0; i < sizeof(commands) / sizeof(commands[0]);
             ++i)
        {
            Server  server(6667, "pass");
            Message message;

            server.addClient(3, "127.0.0.1");
            message.command = commands[i];
            server.dispatchCommand(3, message);

            /* Nickname 未設定のため target は "*" (設計書 06 §4) */
            ASSERT_EQ(std::string("Dispatch: 未登録 ") + commands[i]
                          + " は 451",
                      server.findClientByFd(3)->getSendBuffer(),
                      ":ircserv.local 451 * :You have not registered\r\n");
        }
    }

    {
        /* 登録前に許可される Command はゲートを通過する
           (no parameters で dispatched) */
        struct CommandExpected
        {
            const char *command;
            const char *expected;
        };
        static const CommandExpected cases[] = {
            { "PASS",
              ":ircserv.local 461 * PASS :Not enough parameters\r\n" },
            { "NICK", ":ircserv.local 431 * :No nickname given\r\n" },
            { "USER",
              ":ircserv.local 461 * USER :Not enough parameters\r\n" },
            { "CAP", "" },
            { "PING", ":ircserv.local 409 * :No origin specified\r\n" },
            { "PONG", "" },
            { "QUIT", "" },
        };
        static const std::size_t caseCount =
            sizeof(cases) / sizeof(cases[0]);

        for (std::size_t i = 0; i < caseCount; ++i)
        {
            Server  server(6667, "pass");
            Message message;

            server.addClient(3, "127.0.0.1");
            message.command = cases[i].command;
            server.dispatchCommand(3, message);

            ASSERT_EQ(std::string("Dispatch: 未登録 ") + cases[i].command +
                          " はゲート通過",
                      server.findClientByFd(3)->getSendBuffer(),
                      cases[i].expected);
        }
    }

    {
        /* 未登録の未知 Command は無視 (設計書 04 §3 —
           クライアント固有の初期交渉で登録を妨げない) */
        Server  server(6667, "pass");
        Message message;

        server.addClient(3, "127.0.0.1");
        message.command = "FOO";
        server.dispatchCommand(3, message);

        ASSERT_EQ("Dispatch: 未登録の未知 Command は無視",
                  server.findClientByFd(3)->getSendBuffer(), "");
    }

    /* ── dispatchCommand: 登録後ゲーティング ── */

    {
        /* PASS / USER は登録後 462 */
        const char *commands[] = { "PASS", "USER" };

        for (std::size_t i = 0; i < sizeof(commands) / sizeof(commands[0]);
             ++i)
        {
            Server  server(6667, "pass");
            Message message;

            server.addClient(3, "127.0.0.1");
            registerClient(server, 3, "alice");
            message.command = commands[i];
            server.dispatchCommand(3, message);

            ASSERT_EQ(std::string("Dispatch: 登録済み ") + commands[i]
                          + " は 462",
                      server.findClientByFd(3)->getSendBuffer(),
                      ":ircserv.local 462 alice :Unauthorized command "
                      "(already registered)\r\n");
        }
    }

    {
        /* 登録後の未知 Command は 421。受信した Command 名を返す */
        Server  server(6667, "pass");
        Message message;

        server.addClient(3, "127.0.0.1");
        registerClient(server, 3, "alice");
        message.command = "FOO";
        server.dispatchCommand(3, message);

        ASSERT_EQ("Dispatch: 登録済みの未知 Command は 421",
                  server.findClientByFd(3)->getSendBuffer(),
                  ":ircserv.local 421 alice FOO :Unknown command\r\n");
    }

    {
        /* 登録後の通常 Command はゲートを通過する
           (no parameters で dispatched) */
        struct CommandExpected
        {
            const char *command;
            const char *expected;
        };
        static const CommandExpected cases[] = {
            { "NICK", ":ircserv.local 431 alice :No nickname given\r\n" },
            { "CAP", "" },
            { "PING", ":ircserv.local 409 alice :No origin specified\r\n" },
            { "PONG", "" },
            { "QUIT", "" },
            { "JOIN",
              ":ircserv.local 461 alice JOIN :Not enough parameters\r\n" },
            { "PRIVMSG",
              ":ircserv.local 411 alice :No recipient given (PRIVMSG)\r\n" },
            { "KICK",
              ":ircserv.local 461 alice KICK :Not enough parameters\r\n" },
            { "INVITE",
              ":ircserv.local 461 alice INVITE :Not enough parameters\r\n" },
            { "TOPIC",
              ":ircserv.local 461 alice TOPIC :Not enough parameters\r\n" },
            { "MODE",
              ":ircserv.local 461 alice MODE :Not enough parameters\r\n" },
            { "PART",
              ":ircserv.local 461 alice PART :Not enough parameters\r\n" },
        };
        static const std::size_t caseCount =
            sizeof(cases) / sizeof(cases[0]);

        for (std::size_t i = 0; i < caseCount; ++i)
        {
            Server  server(6667, "pass");
            Message message;

            server.addClient(3, "127.0.0.1");
            registerClient(server, 3, "alice");
            message.command = cases[i].command;
            server.dispatchCommand(3, message);

            ASSERT_EQ(std::string("Dispatch: 登録済み ") + cases[i].command +
                          " はゲート通過",
                      server.findClientByFd(3)->getSendBuffer(),
                      cases[i].expected);
        }
    }

    {
        /* 不在 FD への dispatch は何もしない (設計書 04 §4 処理順 1) */
        Server  server(6667, "pass");
        Message message;

        message.command = "JOIN";
        server.dispatchCommand(99, message);
        ASSERT_TRUE("Dispatch: 不在 FD は無視",
                    server.findClientByFd(99) == NULL);
    }
}
