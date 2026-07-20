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

    /* ── PRIVMSG: 411 (target なし) ──────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "PRIVMSG");
        ASSERT_EQ("PRIVMSG: target なしは 411", takeOutput(server, 3),
                  ":ircserv.local 411 alice :No recipient given "
                  "(PRIVMSG)\r\n");
    }

    /* ── PRIVMSG: 412 (text なし) ────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "PRIVMSG bob");
        ASSERT_EQ("PRIVMSG: text なしは 412", takeOutput(server, 3),
                  ":ircserv.local 412 alice :No text to send\r\n");
    }

    /* ── PRIVMSG: 412 (text が空) ────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "PRIVMSG bob :");
        ASSERT_EQ("PRIVMSG: text が空文字は 412", takeOutput(server, 3),
                  ":ircserv.local 412 alice :No text to send\r\n");
    }

    /* ── PRIVMSG: 401 (Nickname 未存在) ──────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "PRIVMSG nobody :hi");
        ASSERT_EQ("PRIVMSG: 存在しない Nickname は 401", takeOutput(server, 3),
                  ":ircserv.local 401 alice nobody :No such nick/channel"
                  "\r\n");
    }

    /* ── PRIVMSG: 403 (Channel 未存在) ───────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "PRIVMSG #nope :hi");
        ASSERT_EQ("PRIVMSG: 存在しない Channel は 403", takeOutput(server, 3),
                  ":ircserv.local 403 alice #nope :No such channel\r\n");
    }

    /* ── PRIVMSG: 404 (Channel 非 Member) ────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #general");
        takeOutput(server, 4);

        dispatchLine(server, 3, "PRIVMSG #general :hi");
        ASSERT_EQ("PRIVMSG: 非 Member から Channel 宛は 404",
                  takeOutput(server, 3),
                  ":ircserv.local 404 alice #general :Cannot send to "
                  "channel\r\n");
    }

    /* ── PRIVMSG: Channel 宛は送信者以外の全 Member へ配送 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "JOIN #general");
        dispatchLine(server, 4, "JOIN #general");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "PRIVMSG #general :hello");
        ASSERT_EQ("PRIVMSG: 送信者自身には Channel message が届かない",
                  takeOutput(server, 3), "");
        ASSERT_EQ("PRIVMSG: 送信者以外の Member へ配送", takeOutput(server, 4),
                  ":alice!u@127.0.0.1 PRIVMSG #general :hello\r\n");
    }

    /* ── PRIVMSG: Direct は対象のみへ配送 ────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        registerUser(server, 5, "carol", "10.0.0.2");

        dispatchLine(server, 3, "PRIVMSG bob :hi");
        ASSERT_EQ("PRIVMSG: Direct 送信者には応答なし", takeOutput(server, 3),
                  "");
        ASSERT_EQ("PRIVMSG: Direct は対象のみへ配送", takeOutput(server, 4),
                  ":alice!u@127.0.0.1 PRIVMSG bob :hi\r\n");
        ASSERT_EQ("PRIVMSG: Direct は無関係な Client には届かない",
                  takeOutput(server, 5), "");
    }

    /* ── PRIVMSG: 自分宛でも 1 回 queue される ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "PRIVMSG alice :myself");
        ASSERT_EQ("PRIVMSG: 自分宛は 1 回だけ届く", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 PRIVMSG alice :myself\r\n");
    }

    /* ── PRIVMSG: comma 区切り複数 target で 1 つ失敗しても他へ配送 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");

        dispatchLine(server, 3, "PRIVMSG bob,nobody :hi");
        ASSERT_EQ("PRIVMSG: comma 区切りで存在しない target は 401 のみ返す",
                  takeOutput(server, 3),
                  ":ircserv.local 401 alice nobody :No such nick/channel"
                  "\r\n");
        ASSERT_EQ("PRIVMSG: comma 区切りで存在する target には配送される",
                  takeOutput(server, 4),
                  ":alice!u@127.0.0.1 PRIVMSG bob :hi\r\n");
    }

    /* ── KICK: 461 ────────────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "KICK #g");
        ASSERT_EQ("KICK: Parameter 不足は 461", takeOutput(server, 3),
                  ":ircserv.local 461 alice KICK :Not enough parameters\r\n");
    }

    /* ── KICK: 403 (存在しない Channel) ─────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "KICK #nope bob");
        ASSERT_EQ("KICK: 存在しない Channel は 403", takeOutput(server, 3),
                  ":ircserv.local 403 alice #nope :No such channel\r\n");
    }

    /* ── KICK: 442 (実行者非 Member、対象 Nickname も不在) ──
       非 Member チェックが対象 Nickname 探索より先に行われることを
       確認する (401 ではなく 442) */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 4);

        dispatchLine(server, 3, "KICK #g nobody");
        ASSERT_EQ("KICK: 非 Member は 442 (対象不在より優先)",
                  takeOutput(server, 3),
                  ":ircserv.local 442 alice #g :You're not on that "
                  "channel\r\n");
    }

    /* ── KICK: 482 (実行者が非 Operator、対象 Nickname も不在) ──
       非 Operator チェックが対象 Nickname 探索より先に行われることを
       確認する (401 ではなく 482) */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 4, "KICK #g nobody");
        ASSERT_EQ("KICK: 非 Operator は 482 (対象不在より優先)",
                  takeOutput(server, 4),
                  ":ircserv.local 482 bob #g :You're not channel "
                  "operator\r\n");
    }

    /* ── KICK: 401 (対象 Nickname 不在) ─────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        takeOutput(server, 3);

        dispatchLine(server, 3, "KICK #g nobody");
        ASSERT_EQ("KICK: 対象 Nickname 不在は 401", takeOutput(server, 3),
                  ":ircserv.local 401 alice nobody :No such nick/channel"
                  "\r\n");
    }

    /* ── KICK: 441 (対象が Channel 非 Member) ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");

        dispatchLine(server, 3, "KICK #g bob");
        ASSERT_EQ("KICK: 対象が非 Member は 441", takeOutput(server, 3),
                  ":ircserv.local 441 alice bob #g :They aren't on that "
                  "channel\r\n");
    }

    /* ── KICK: 通知が削除前の全 Member (対象含む) へ届き、
       対象は Channel から消える ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "KICK #g bob :spam");
        ASSERT_EQ("KICK: 実行者へも KICK 通知が届く", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 KICK #g bob :spam\r\n");
        ASSERT_EQ("KICK: 対象自身にも削除前の KICK 通知が届く",
                  takeOutput(server, 4),
                  ":alice!u@127.0.0.1 KICK #g bob :spam\r\n");

        dispatchLine(server, 4, "PART #g");
        ASSERT_EQ("KICK: 対象は Channel から削除済み (再 PART は 442)",
                  takeOutput(server, 4),
                  ":ircserv.local 442 bob #g :You're not on that "
                  "channel\r\n");
    }

    /* ── KICK: reason 省略時は実行者 Nickname ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "KICK #g bob");
        ASSERT_EQ("KICK: reason 省略時は実行者 Nickname",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 KICK #g bob :alice\r\n");
    }

    /* ── INVITE: 461 ──────────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "INVITE bob");
        ASSERT_EQ("INVITE: Parameter 不足は 461", takeOutput(server, 3),
                  ":ircserv.local 461 alice INVITE :Not enough parameters"
                  "\r\n");
    }

    /* ── INVITE: 401 (対象 Nickname 不在、Channel も不在) ──
       対象 Nickname 探索が Channel 探索より先に行われることを確認する
       (403 ではなく 401) */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "INVITE nobody #nope");
        ASSERT_EQ("INVITE: 対象不在は 401 (Channel 不在より優先)",
                  takeOutput(server, 3),
                  ":ircserv.local 401 alice nobody :No such nick/channel"
                  "\r\n");
    }

    /* ── INVITE: 403 (Channel 不在) ──────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "INVITE bob #nope");
        ASSERT_EQ("INVITE: 存在しない Channel は 403", takeOutput(server, 3),
                  ":ircserv.local 403 alice #nope :No such channel\r\n");
    }

    /* ── INVITE: 442 (実行者非 Member、対象は既に Member) ──
       非 Member チェックが「既に Member」チェックより先に行われることを
       確認する (443 ではなく 442) */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 4);

        dispatchLine(server, 3, "INVITE bob #g");
        ASSERT_EQ("INVITE: 非 Member は 442 (既に Member より優先)",
                  takeOutput(server, 3),
                  ":ircserv.local 442 alice #g :You're not on that "
                  "channel\r\n");
    }

    /* ── INVITE: 482 (実行者が非 Operator、対象は既に Member) ──
       非 Operator チェックが「既に Member」チェックより先に行われることを
       確認する (443 ではなく 482) */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 4, "INVITE alice #g");
        ASSERT_EQ("INVITE: 非 Operator は 482 (既に Member より優先)",
                  takeOutput(server, 4),
                  ":ircserv.local 482 bob #g :You're not channel "
                  "operator\r\n");
    }

    /* ── INVITE: 443 (対象が既に Member) ────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "INVITE bob #g");
        ASSERT_EQ("INVITE: 対象が既に Member は 443", takeOutput(server, 3),
                  ":ircserv.local 443 alice bob #g :is already on channel"
                  "\r\n");
    }

    /* ── INVITE: 成功で 341 + 対象への通知、再 INVITE も再送 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");

        dispatchLine(server, 3, "INVITE bob #g");
        ASSERT_EQ("INVITE: 実行者へ 341", takeOutput(server, 3),
                  ":ircserv.local 341 alice #g bob\r\n");
        ASSERT_EQ("INVITE: 対象へ INVITE 通知", takeOutput(server, 4),
                  ":alice!u@127.0.0.1 INVITE bob :#g\r\n");

        dispatchLine(server, 3, "INVITE bob #g");
        ASSERT_EQ("INVITE: 再 INVITE でも 341 が再送される",
                  takeOutput(server, 3),
                  ":ircserv.local 341 alice #g bob\r\n");
        ASSERT_EQ("INVITE: 再 INVITE でも通知が再送される",
                  takeOutput(server, 4),
                  ":alice!u@127.0.0.1 INVITE bob :#g\r\n");
    }

    /* ── TOPIC: 461 ───────────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "TOPIC");
        ASSERT_EQ("TOPIC: Parameter 不足は 461", takeOutput(server, 3),
                  ":ircserv.local 461 alice TOPIC :Not enough parameters"
                  "\r\n");
    }

    /* ── TOPIC: 403 (存在しない Channel) ─────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "TOPIC #nope");
        ASSERT_EQ("TOPIC: 存在しない Channel は 403", takeOutput(server, 3),
                  ":ircserv.local 403 alice #nope :No such channel\r\n");
    }

    /* ── TOPIC: 442 (非 Member) ─────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 4);

        dispatchLine(server, 3, "TOPIC #g");
        ASSERT_EQ("TOPIC: 非 Member は 442", takeOutput(server, 3),
                  ":ircserv.local 442 alice #g :You're not on that "
                  "channel\r\n");
    }

    /* ── TOPIC: 照会 331 → 変更 → 照会 332 ────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        takeOutput(server, 3);

        dispatchLine(server, 3, "TOPIC #g");
        ASSERT_EQ("TOPIC: 未設定の照会は 331", takeOutput(server, 3),
                  ":ircserv.local 331 alice #g :No topic is set\r\n");

        dispatchLine(server, 3, "TOPIC #g :hello world");
        ASSERT_EQ("TOPIC: 変更通知 (自分)", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 TOPIC #g :hello world\r\n");

        dispatchLine(server, 3, "TOPIC #g");
        ASSERT_EQ("TOPIC: 設定済みの照会は 332", takeOutput(server, 3),
                  ":ircserv.local 332 alice #g :hello world\r\n");
    }

    /* ── TOPIC: +t (既定で有効) は非 Operator が 482、Operator は変更可 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 4, "TOPIC #g :bob's topic");
        ASSERT_EQ("TOPIC: +t 既定有効、非 Operator は 482",
                  takeOutput(server, 4),
                  ":ircserv.local 482 bob #g :You're not channel "
                  "operator\r\n");

        dispatchLine(server, 3, "TOPIC #g :alice's topic");
        ASSERT_EQ("TOPIC: +t 有効でも Operator は変更可 (実行者)",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 TOPIC #g :alice's topic\r\n");
        ASSERT_EQ("TOPIC: 変更通知は全 Member (残り Member) へ届く",
                  takeOutput(server, 4),
                  ":alice!u@127.0.0.1 TOPIC #g :alice's topic\r\n");
    }

    /* ── TOPIC: +t を無効化すれば非 Operator でも変更可 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #g");
        takeOutput(server, 3);
        takeOutput(server, 4);

        Channel *channel = server.findChannel("#g");

        ASSERT_TRUE("TOPIC: +t 無効化前提の Channel が見つかる",
                    channel != NULL);
        channel->setTopicRestricted(false);

        dispatchLine(server, 4, "TOPIC #g :bob's topic");
        ASSERT_EQ("TOPIC: +t 無効なら非 Operator でも変更可 (実行者)",
                  takeOutput(server, 4),
                  ":bob!u@10.0.0.1 TOPIC #g :bob's topic\r\n");
        ASSERT_EQ("TOPIC: +t 無効時も変更通知は全 Member へ届く",
                  takeOutput(server, 3),
                  ":bob!u@10.0.0.1 TOPIC #g :bob's topic\r\n");
    }

    /* ── TOPIC: 空 topic は削除され、照会が 331 に戻る ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #g");
        takeOutput(server, 3);

        dispatchLine(server, 3, "TOPIC #g :hello");
        takeOutput(server, 3);

        dispatchLine(server, 3, "TOPIC #g :");
        ASSERT_EQ("TOPIC: 空文字への変更通知", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 TOPIC #g :\r\n");

        dispatchLine(server, 3, "TOPIC #g");
        ASSERT_EQ("TOPIC: 空 topic 削除後の照会は 331", takeOutput(server, 3),
                  ":ircserv.local 331 alice #g :No topic is set\r\n");
    }
}
