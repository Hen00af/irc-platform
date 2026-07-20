#include <cstddef>

#include "TestRunner.hpp"

#include "prd/domain/Channel.hpp"
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

    /* fd を addClient + PASS/USER/NICK で登録済みにし、Welcome を
       読み捨てる (auth テストの registerAlice と同形) */
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

void runChannelCmdTests()
{
    TestRunner::beginSuite("ChannelCmd");

    /* ── JOIN: 461 ────────────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN");
        ASSERT_EQ("JOIN: Parameter なしは 461", takeOutput(server, 3),
                  ":ircserv.local 461 alice JOIN :Not enough parameters\r\n");
    }

    /* ── JOIN: 403 (不正な Channel 名) ───────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN notachannel");
        ASSERT_EQ("JOIN: 不正な Channel 名は 403", takeOutput(server, 3),
                  ":ircserv.local 403 alice notachannel :No such channel\r\n");
    }

    /* ── JOIN: 新規作成で JOIN+331+353+366 の 4 行バースト ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #general");
        ASSERT_EQ("JOIN: 新規 Channel 作成時の 4 行バースト",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 JOIN :#general\r\n"
                  ":ircserv.local 331 alice #general :No topic is set\r\n"
                  ":ircserv.local 353 alice = #general :@alice\r\n"
                  ":ircserv.local 366 alice #general :End of NAMES list\r\n");
    }

    /* ── JOIN: 2 人目参加で両者へ通知・Names に 2 人 (FD 昇順) ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "JOIN #general");
        takeOutput(server, 3);

        dispatchLine(server, 4, "JOIN #general");
        ASSERT_EQ("JOIN: 2 人目 (bob) 参加時の 4 行バースト",
                  takeOutput(server, 4),
                  ":bob!u@10.0.0.1 JOIN :#general\r\n"
                  ":ircserv.local 331 bob #general :No topic is set\r\n"
                  ":ircserv.local 353 bob = #general :@alice bob\r\n"
                  ":ircserv.local 366 bob #general :End of NAMES list\r\n");
        ASSERT_EQ("JOIN: 既存 Member (alice) には JOIN 通知だけ届く",
                  takeOutput(server, 3),
                  ":bob!u@10.0.0.1 JOIN :#general\r\n");
    }

    /* ── JOIN: 参加済み Channel への再 JOIN は無応答 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #general");
        takeOutput(server, 3);

        dispatchLine(server, 3, "JOIN #general");
        ASSERT_EQ("JOIN: 参加済みへの再 JOIN は無応答", takeOutput(server, 3),
                  "");
    }

    /* ── JOIN: 複数 Channel を comma 区切りで同時参加 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #foo,#bar");
        ASSERT_EQ("JOIN: comma 区切りで 2 Channel とも参加",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 JOIN :#foo\r\n"
                  ":ircserv.local 331 alice #foo :No topic is set\r\n"
                  ":ircserv.local 353 alice = #foo :@alice\r\n"
                  ":ircserv.local 366 alice #foo :End of NAMES list\r\n"
                  ":alice!u@127.0.0.1 JOIN :#bar\r\n"
                  ":ircserv.local 331 alice #bar :No topic is set\r\n"
                  ":ircserv.local 353 alice = #bar :@alice\r\n"
                  ":ircserv.local 366 alice #bar :End of NAMES list\r\n");
    }

    /* ── JOIN: 1 つの失敗が他 Channel の参加を止めない ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #ok,badname");
        ASSERT_EQ("JOIN: #ok は参加成功し badname だけ 403",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 JOIN :#ok\r\n"
                  ":ircserv.local 331 alice #ok :No topic is set\r\n"
                  ":ircserv.local 353 alice = #ok :@alice\r\n"
                  ":ircserv.local 366 alice #ok :End of NAMES list\r\n"
                  ":ircserv.local 403 alice badname :No such channel\r\n");
    }

    /* ── JOIN: +i (invite-only) ─────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "JOIN #priv");
        takeOutput(server, 3);

        Channel *channel = server.findChannel("#priv");

        ASSERT_TRUE("JOIN: +i 設定前提の Channel が見つかる", channel != NULL);
        channel->setInviteOnly(true);

        dispatchLine(server, 4, "JOIN #priv");
        ASSERT_EQ("JOIN: 未招待の +i 参加は 473", takeOutput(server, 4),
                  ":ircserv.local 473 bob #priv :Cannot join channel (+i)\r\n");

        /* Operator の INVITE 実装前のため Channel 公開 API で直接招待する
           (spec の決定事項) */
        channel->addInvite(4);
        dispatchLine(server, 4, "JOIN #priv");
        ASSERT_EQ("JOIN: 招待後は +i でも参加できる", takeOutput(server, 4),
                  ":bob!u@10.0.0.1 JOIN :#priv\r\n"
                  ":ircserv.local 331 bob #priv :No topic is set\r\n"
                  ":ircserv.local 353 bob = #priv :@alice bob\r\n"
                  ":ircserv.local 366 bob #priv :End of NAMES list\r\n");
        takeOutput(server, 3);

        /* Invite は参加で消費されるため、再度 PART → JOIN すると 473 に戻る */
        dispatchLine(server, 4, "PART #priv");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 4, "JOIN #priv");
        ASSERT_EQ("JOIN: Invite 消費後の再 JOIN はまた 473",
                  takeOutput(server, 4),
                  ":ircserv.local 473 bob #priv :Cannot join channel (+i)\r\n");
    }

    /* ── JOIN: +k (channel key) ─────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "JOIN #keyed");
        takeOutput(server, 3);

        Channel *channel = server.findChannel("#keyed");

        ASSERT_TRUE("JOIN: +k 設定前提の Channel が見つかる", channel != NULL);
        channel->setChannelKey("s3cr3t");

        dispatchLine(server, 4, "JOIN #keyed wrong");
        ASSERT_EQ("JOIN: Key 不一致は 475", takeOutput(server, 4),
                  ":ircserv.local 475 bob #keyed :Cannot join channel (+k)\r\n");

        dispatchLine(server, 4, "JOIN #keyed s3cr3t");
        ASSERT_EQ("JOIN: Key 一致で参加成功", takeOutput(server, 4),
                  ":bob!u@10.0.0.1 JOIN :#keyed\r\n"
                  ":ircserv.local 331 bob #keyed :No topic is set\r\n"
                  ":ircserv.local 353 bob = #keyed :@alice bob\r\n"
                  ":ircserv.local 366 bob #keyed :End of NAMES list\r\n");
    }

    /* ── JOIN: +l (user limit) ──────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "JOIN #full");
        takeOutput(server, 3);

        Channel *channel = server.findChannel("#full");

        ASSERT_TRUE("JOIN: +l 設定前提の Channel が見つかる", channel != NULL);
        channel->setUserLimit(1);

        dispatchLine(server, 4, "JOIN #full");
        ASSERT_EQ("JOIN: Limit 到達は 471", takeOutput(server, 4),
                  ":ircserv.local 471 bob #full :Cannot join channel (+l)\r\n");
    }

    /* ── JOIN 0: 全 Channel から退出 (PART 通知) ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "JOIN #g1,#g2");
        dispatchLine(server, 4, "JOIN #g1,#g2");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "JOIN 0");
        ASSERT_EQ("JOIN 0: 自身にも削除前の PART 通知が届く",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 PART #g1 :alice\r\n"
                  ":alice!u@127.0.0.1 PART #g2 :alice\r\n");
        ASSERT_EQ("JOIN 0: 残る Member (bob) にも PART 通知", takeOutput(server, 4),
                  ":alice!u@127.0.0.1 PART #g1 :alice\r\n"
                  ":alice!u@127.0.0.1 PART #g2 :alice\r\n");
    }

    /* ── PART: 461 ────────────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "PART");
        ASSERT_EQ("PART: Parameter なしは 461", takeOutput(server, 3),
                  ":ircserv.local 461 alice PART :Not enough parameters\r\n");
    }

    /* ── PART: 403 (存在しない Channel) ─────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "PART #nope");
        ASSERT_EQ("PART: 存在しない Channel は 403", takeOutput(server, 3),
                  ":ircserv.local 403 alice #nope :No such channel\r\n");
    }

    /* ── PART: 442 (非 Member) ──────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #other");
        takeOutput(server, 4);

        dispatchLine(server, 3, "PART #other");
        ASSERT_EQ("PART: 非 Member は 442", takeOutput(server, 3),
                  ":ircserv.local 442 alice #other :You're not on that channel\r\n");
    }

    /* ── PART: reason 付き通知が全 Member へ (自分含む) ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "PART #g :bye all");
        ASSERT_EQ("PART: reason 付き通知 (自分)", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 PART #g :bye all\r\n");
        ASSERT_EQ("PART: reason 付き通知 (残り Member)", takeOutput(server, 4),
                  ":alice!u@127.0.0.1 PART #g :bye all\r\n");
    }

    /* ── PART: reason 省略時は Nickname ─────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        takeOutput(server, 3);

        dispatchLine(server, 3, "PART #g");
        ASSERT_EQ("PART: reason 省略時は Nickname", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 PART #g :alice\r\n");
    }

    /* ── PART: 複数 Channel の 1 つの失敗が他を止めない ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g1");
        takeOutput(server, 3);

        dispatchLine(server, 3, "PART #g1,#nope");
        ASSERT_EQ("PART: #g1 は退出成功し #nope だけ 403",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 PART #g1 :alice\r\n"
                  ":ircserv.local 403 alice #nope :No such channel\r\n");
    }

    /* ── PART: 空 Channel は削除される (再 JOIN で新規扱い) ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #solo");
        takeOutput(server, 3);

        dispatchLine(server, 3, "PART #solo");
        takeOutput(server, 3);
        ASSERT_TRUE("PART: 最後の Member 退出で Channel が削除される",
                    server.findChannel("#solo") == NULL);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #solo");
        ASSERT_EQ("PART: 削除後の再 JOIN は新規 Channel 扱いで自分が Operator",
                  takeOutput(server, 4),
                  ":bob!u@10.0.0.1 JOIN :#solo\r\n"
                  ":ircserv.local 331 bob #solo :No topic is set\r\n"
                  ":ircserv.local 353 bob = #solo :@bob\r\n"
                  ":ircserv.local 366 bob #solo :End of NAMES list\r\n");
    }
}
