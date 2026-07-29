#include <cstddef>

#include "TestRunner.hpp"

#include "prd/domain/Message.hpp"
#include "prd/interface/Server.hpp"
#include "prd/util/Parser.hpp"

namespace
{
    /* 生の IRC 行を Parser 経由で dispatch する。実運用と同じ経路 */
    void dispatchLine(Server &server, int fd, const std::string &line)
    {
        Message message;

        if (Parser::parse(line, message))
            server.dispatchCommand(fd, message);
    }

    /* 送信バッファの内容を取り出して空にする */
    std::string takeOutput(Server &server, int fd)
    {
        Client *client = server.findClientByFd(fd);

        if (client == NULL)
            return "";

        std::string output = client->getSendBuffer();

        client->eraseSendPrefix(output.size());
        return output;
    }

    /* alice を fd で登録済みにし、Welcome を読み捨てる */
    void registerAlice(Server &server, int fd)
    {
        server.addClient(fd, "127.0.0.1");
        dispatchLine(server, fd, "PASS secret");
        dispatchLine(server, fd, "NICK alice");
        dispatchLine(server, fd, "USER u 0 * :Alice Example");
        takeOutput(server, fd);
    }

    const std::string WELCOME_HEAD =
        ":ircserv.local 001 alice :Welcome to the Internet Relay Network "
        "alice!u@127.0.0.1\r\n"
        ":ircserv.local 002 alice :Your host is ircserv.local, "
        "running version 1.0\r\n"
        ":ircserv.local 003 alice :This server was created ";
    const std::string WELCOME_TAIL =
        ":ircserv.local 004 alice ircserv.local 1.0 - itkol\r\n"
        ":ircserv.local 422 alice :MOTD File is missing\r\n";

    /* Welcome 5 行の検証。003 の時刻部分だけ前置一致で許容する */
    bool isWelcome(const std::string &output)
    {
        if (output.compare(0, WELCOME_HEAD.size(), WELCOME_HEAD) != 0)
            return false;
        if (output.size() < WELCOME_TAIL.size())
            return false;
        return output.compare(output.size() - WELCOME_TAIL.size(),
                              WELCOME_TAIL.size(), WELCOME_TAIL) == 0;
    }
}

void runAuthTests()
{
    TestRunner::beginSuite("Auth");

    /* ── 登録順序: 6 順列すべてで揃った瞬間に 1 回だけ Welcome ── */

    {
        const char *orders[6][3] = {
            { "PASS secret", "NICK alice", "USER u 0 * :Alice Example" },
            { "PASS secret", "USER u 0 * :Alice Example", "NICK alice" },
            { "NICK alice", "PASS secret", "USER u 0 * :Alice Example" },
            { "NICK alice", "USER u 0 * :Alice Example", "PASS secret" },
            { "USER u 0 * :Alice Example", "PASS secret", "NICK alice" },
            { "USER u 0 * :Alice Example", "NICK alice", "PASS secret" },
        };

        for (std::size_t i = 0; i < 6; ++i)
        {
            Server server(6667, "secret");

            server.addClient(3, "127.0.0.1");
            dispatchLine(server, 3, orders[i][0]);
            dispatchLine(server, 3, orders[i][1]);
            ASSERT_EQ("Auth: 2 Command では Welcome なし (順列)",
                      takeOutput(server, 3), "");
            dispatchLine(server, 3, orders[i][2]);
            ASSERT_TRUE("Auth: 3 つ揃って Welcome (順列)",
                        isWelcome(takeOutput(server, 3)));
        }
    }

    /* ── PASS ─────────────────────────────── */

    {
        Server server(6667, "secret");

        server.addClient(3, "127.0.0.1");
        dispatchLine(server, 3, "PASS");
        ASSERT_EQ("Auth: PASS Parameter なしは 461", takeOutput(server, 3),
                  ":ircserv.local 461 * PASS :Not enough parameters\r\n");

        dispatchLine(server, 3, "PASS wrong");
        ASSERT_EQ("Auth: PASS 不一致は 464", takeOutput(server, 3),
                  ":ircserv.local 464 * :Password incorrect\r\n");

        /* 不一致後も接続は維持され、正しい PASS で回復できる */
        dispatchLine(server, 3, "PASS secret");
        ASSERT_EQ("Auth: PASS 成功は単独 Reply なし", takeOutput(server, 3),
                  "");

        dispatchLine(server, 3, "NICK alice");
        dispatchLine(server, 3, "USER u 0 * :Alice Example");
        ASSERT_TRUE("Auth: 回復後に登録完了できる",
                    isWelcome(takeOutput(server, 3)));
    }

    /* ── NICK ─────────────────────────────── */

    {
        Server server(6667, "secret");

        server.addClient(3, "127.0.0.1");
        dispatchLine(server, 3, "NICK");
        ASSERT_EQ("Auth: NICK Parameter なしは 431", takeOutput(server, 3),
                  ":ircserv.local 431 * :No nickname given\r\n");

        dispatchLine(server, 3, "NICK 1abc");
        ASSERT_EQ("Auth: 数字始まりの NICK は 432", takeOutput(server, 3),
                  ":ircserv.local 432 * 1abc :Erroneous nickname\r\n");

        dispatchLine(server, 3, "NICK abcdefghij");
        ASSERT_EQ("Auth: 10 文字の NICK は 432", takeOutput(server, 3),
                  ":ircserv.local 432 * abcdefghij :Erroneous nickname\r\n");
    }

    {
        /* 重複は ircCaseFold で比較する (A-Z→a-z, []→{}, \→|, ^→~) */
        Server server(6667, "secret");

        registerAlice(server, 3);
        server.addClient(4, "10.0.0.1");
        dispatchLine(server, 4, "NICK ALICE");
        ASSERT_EQ("Auth: 大文字違いの重複 NICK は 433", takeOutput(server, 4),
                  ":ircserv.local 433 * ALICE :Nickname is already in use\r\n");

        dispatchLine(server, 4, "NICK [b]x");
        takeOutput(server, 4);
        server.addClient(5, "10.0.0.2");
        dispatchLine(server, 5, "NICK {b}x");
        ASSERT_EQ("Auth: []{} を同一視した重複は 433", takeOutput(server, 5),
                  ":ircserv.local 433 * {b}x :Nickname is already in use\r\n");
    }

    {
        /* 自分の Nickname への変更 (大小変更) は重複扱いしない */
        Server server(6667, "secret");

        registerAlice(server, 3);
        dispatchLine(server, 3, "NICK ALICE");
        ASSERT_EQ("Auth: 自分自身への NICK 変更は許可され通知される",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 NICK :ALICE\r\n");
    }

    {
        /* 変更で旧 Nickname が解放される */
        Server server(6667, "secret");

        registerAlice(server, 3);
        dispatchLine(server, 3, "NICK alicia");
        takeOutput(server, 3);

        server.addClient(4, "10.0.0.1");
        dispatchLine(server, 4, "NICK alice");
        ASSERT_EQ("Auth: 変更後の旧 NICK は再利用できる",
                  takeOutput(server, 4), "");

        ASSERT_TRUE("Auth: findClientByNickname が case-fold で引ける",
                    server.findClientByNickname("ALICIA") != NULL);
        ASSERT_EQ("Auth: 索引が変更後の FD を指す",
                  server.findClientByNickname("alicia")->getFd(), 3);
    }

    {
        /* removeClient で Nickname が解放される */
        Server server(6667, "secret");

        registerAlice(server, 3);
        server.removeClient(3);
        ASSERT_TRUE("Auth: removeClient で索引も消える",
                    server.findClientByNickname("alice") == NULL);

        server.addClient(4, "10.0.0.1");
        dispatchLine(server, 4, "NICK alice");
        ASSERT_EQ("Auth: 切断後の NICK は再利用できる",
                  takeOutput(server, 4), "");
    }

    /* ── USER ─────────────────────────────── */

    {
        Server server(6667, "secret");

        server.addClient(3, "127.0.0.1");
        dispatchLine(server, 3, "USER u 0 *");
        ASSERT_EQ("Auth: USER 3 Parameter は 461", takeOutput(server, 3),
                  ":ircserv.local 461 * USER :Not enough parameters\r\n");

        dispatchLine(server, 3, "USER u@h 0 * :Real");
        ASSERT_EQ("Auth: username の @ は 461", takeOutput(server, 3),
                  ":ircserv.local 461 * USER :Not enough parameters\r\n");

        dispatchLine(server, 3, "USER u 0 * :");
        ASSERT_EQ("Auth: 空 realname は許可", takeOutput(server, 3), "");

        dispatchLine(server, 3, "USER v 0 * :Again");
        ASSERT_EQ("Auth: 未登録でも USER 再実行は 462", takeOutput(server, 3),
                  ":ircserv.local 462 * :Unauthorized command "
                  "(already registered)\r\n");
    }

    /* ── CAP ──────────────────────────────── */

    {
        Server server(6667, "secret");

        server.addClient(3, "127.0.0.1");
        dispatchLine(server, 3, "CAP LS 302");
        ASSERT_EQ("Auth: CAP LS は空 Capability 一覧", takeOutput(server, 3),
                  ":ircserv.local CAP * LS :\r\n");

        dispatchLine(server, 3, "CAP REQ :sasl");
        ASSERT_EQ("Auth: CAP REQ は NAK", takeOutput(server, 3),
                  ":ircserv.local CAP * NAK :sasl\r\n");

        dispatchLine(server, 3, "CAP END");
        ASSERT_EQ("Auth: CAP END は応答なし", takeOutput(server, 3), "");

        dispatchLine(server, 3, "CAP");
        ASSERT_EQ("Auth: CAP 単独は応答なし", takeOutput(server, 3), "");
    }

    /* ── PING / PONG ──────────────────────── */

    {
        Server server(6667, "secret");

        server.addClient(3, "127.0.0.1");
        dispatchLine(server, 3, "PING tok123");
        ASSERT_EQ("Auth: 未登録でも PING に PONG", takeOutput(server, 3),
                  ":ircserv.local PONG ircserv.local :tok123\r\n");

        dispatchLine(server, 3, "PING");
        ASSERT_EQ("Auth: PING token なしは 409", takeOutput(server, 3),
                  ":ircserv.local 409 * :No origin specified\r\n");

        dispatchLine(server, 3, "PING :");
        ASSERT_EQ("Auth: PING 空 token も 409", takeOutput(server, 3),
                  ":ircserv.local 409 * :No origin specified\r\n");

        dispatchLine(server, 3, "PONG tok123");
        ASSERT_EQ("Auth: PONG は無視", takeOutput(server, 3), "");
    }

    /* ── Welcome 再送なし ─────────────────── */

    {
        Server server(6667, "secret");

        registerAlice(server, 3);
        dispatchLine(server, 3, "NICK alice2");
        takeOutput(server, 3);
        dispatchLine(server, 3, "PING x");
        ASSERT_EQ("Auth: 登録後に Welcome は再送されない",
                  takeOutput(server, 3),
                  ":ircserv.local PONG ircserv.local :x\r\n");
    }
}
