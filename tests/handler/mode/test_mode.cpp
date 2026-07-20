#include <cstddef>

#include "TestRunner.hpp"

#include "prd/domain/Channel.hpp"
#include "prd/domain/Message.hpp"
#include "prd/interface/Server.hpp"
#include "prd/util/Parser.hpp"

namespace
{
    /* 生の IRC 行を Parser 経由で dispatch する。実運用と同じ経路
       (channel_cmd テストの dispatchLine と同形) */
    void dispatchLine(Server &server, int fd, const std::string &line)
    {
        Message message;

        if (Parser::parse(line, message))
            server.dispatchCommand(fd, message);
    }

    /* 送信バッファの内容を取り出して空にする
       (channel_cmd テストの takeOutput と同形) */
    std::string takeOutput(Server &server, int fd)
    {
        Client *client = server.findClientByFd(fd);

        if (client == NULL)
            return "";

        std::string output = client->getSendBuffer();

        client->eraseSendPrefix(output.size());
        return output;
    }

    /* fd を addClient + PASS/USER/NICK で登録済みにし、Welcome を
       読み捨てる (channel_cmd テストの registerUser と同形) */
    void registerUser(Server &server, int fd, const std::string &nick,
                      const std::string &host)
    {
        server.addClient(fd, host);
        dispatchLine(server, fd, "PASS secret");
        dispatchLine(server, fd, "NICK " + nick);
        dispatchLine(server, fd, "USER u 0 * :Real Name");
        takeOutput(server, fd);
    }
}

void runModeTests()
{
    TestRunner::beginSuite("Mode");

    /* ── 分岐: params なし → 461 ─────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE");
        ASSERT_EQ("MODE: Parameter なしは 461", takeOutput(server, 3),
                  ":ircserv.local 461 alice MODE :Not enough parameters\r\n");
    }

    /* ── 分岐: target が # 以外 → 421 ────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "MODE bob");
        ASSERT_EQ("MODE: target が # 以外は 421", takeOutput(server, 3),
                  ":ircserv.local 421 alice bob :Unknown command\r\n");
    }

    /* ── 分岐: Channel なし → 403 ─────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "MODE #nosuch");
        ASSERT_EQ("MODE: 存在しない Channel は 403", takeOutput(server, 3),
                  ":ircserv.local 403 alice #nosuch :No such channel\r\n");
    }

    /* ── 照会: 新規 Channel は +t (初期値) ────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: 新規 Channel の照会は +t だけ", takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +t\r\n");
    }

    /* ── 照会: +i+k+l 設定後は +itkl <key> <limit> ──
       (k/l の MODE 変更自体は Task 2 実装のため、Channel API を直接
       呼んで状態を作り、324 の組み立てだけを検証する) */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        Channel *channel = server.findChannel("#c");
        channel->setInviteOnly(true);
        channel->setChannelKey("secret");
        channel->setUserLimit(10);

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: +i+k+l 設定後の照会は itkl 順",
                  takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +itkl secret 10\r\n");
    }

    /* ── 権限: 非 Member が変更 → 442 (状態不変) ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "MODE #c +i");
        ASSERT_EQ("MODE: 非 Member の変更は 442", takeOutput(server, 4),
                  ":ircserv.local 442 bob #c :You're not on that channel\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: 442 後も状態は不変 (+t のまま)",
                  takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +t\r\n");
    }

    /* ── 権限: Member 非 Op が変更 → 482 (状態不変) ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #c");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 4, "MODE #c +i");
        ASSERT_EQ("MODE: Member 非 Op の変更は 482", takeOutput(server, 4),
                  ":ircserv.local 482 bob #c :You're not channel operator\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: 482 後も状態は不変 (+t のまま)",
                  takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +t\r\n");
    }

    /* ── i: +i 通知・照会反映 ─────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +i");
        ASSERT_EQ("MODE: +i は全 Member (自分含む) へ通知",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c +i\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: +i 後の照会は +it (t は初期値)",
                  takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +it\r\n");
    }

    /* ── i: JOIN への反映 (未招待 473 → 招待後成功) ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +i");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #c");
        ASSERT_EQ("MODE: +i 中は未招待 JOIN が 473", takeOutput(server, 4),
                  ":ircserv.local 473 bob #c :Cannot join channel (+i)\r\n");

        dispatchLine(server, 3, "INVITE bob #c");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 4, "JOIN #c");
        ASSERT_EQ("MODE: 招待後は +i 中でも JOIN 成功",
                  takeOutput(server, 4),
                  ":bob!u@10.0.0.1 JOIN :#c\r\n"
                  ":ircserv.local 331 bob #c :No topic is set\r\n"
                  ":ircserv.local 353 bob = #c :@alice bob\r\n"
                  ":ircserv.local 366 bob #c :End of NAMES list\r\n");
    }

    /* ── i: -i で解除 ─────────────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +i");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c -i");
        ASSERT_EQ("MODE: -i の通知", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c -i\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: -i 後の照会は +t だけ", takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +t\r\n");
    }

    /* ── i: 既に +i の Channel へ +i は通知なし ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +i");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +i");
        ASSERT_EQ("MODE: 実変更のない +i は通知なし",
                  takeOutput(server, 3), "");
    }

    /* ── t: +t (初期値のまま) は実変更なし・通知なし ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +t");
        ASSERT_EQ("MODE: 初期値のままの +t は通知なし",
                  takeOutput(server, 3), "");
    }

    /* ── t: -t で解除・TOPIC が非 Op でも可に ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c -t");
        ASSERT_EQ("MODE: -t の通知", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c -t\r\n");

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #c");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 4, "TOPIC #c :new topic");
        ASSERT_EQ("MODE: -t 後は非 Op の bob も TOPIC 変更可",
                  takeOutput(server, 4),
                  ":bob!u@10.0.0.1 TOPIC #c :new topic\r\n");
    }

    /* ── t: 再 +t ─────────────────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c -t");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +t");
        ASSERT_EQ("MODE: 再 +t の通知", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c +t\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: 再 +t 後の照会は +t", takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +t\r\n");
    }

    /* ── 集約: 実変更 0 件 (既存状態と同一) は通知なし ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        /* t は初期値 true のまま、i は初期値 false のまま。
           どちらも実変更が無いので通知は送らない */
        dispatchLine(server, 3, "MODE #c +t-i");
        ASSERT_EQ("MODE: 全 Mode が実変更なしなら通知しない",
                  takeOutput(server, 3), "");
    }

    /* ── 集約: 符号切替 (i/t だけでも符号切替を検証できる) ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        /* i: false→true (実変更), t: true→false (実変更)。
           符号が + → - へ変わるので通知は "+i-t" になる */
        dispatchLine(server, 3, "MODE #c +i-t");
        ASSERT_EQ("MODE: 符号切替は変わった時だけ符号を出す",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c +i-t\r\n");
    }

    /* ── 符号なし `it` は各文字へ 472、通知なし ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c it");
        ASSERT_EQ("MODE: 符号なしは各文字へ 472・通知なし",
                  takeOutput(server, 3),
                  ":ircserv.local 472 alice i :is unknown mode char to me "
                  "for #c\r\n"
                  ":ircserv.local 472 alice t :is unknown mode char to me "
                  "for #c\r\n");
    }
}
