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

    /* ── k: +k secret 通知・324 反映・JOIN の key 一致要求 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +k secret");
        ASSERT_EQ("MODE: +k secret の通知", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c +k secret\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: +k 後の照会は +tk secret (itkl 順)",
                  takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +tk secret\r\n");

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #c");
        ASSERT_EQ("MODE: key 不一致の JOIN は 475", takeOutput(server, 4),
                  ":ircserv.local 475 bob #c :Cannot join channel (+k)\r\n");

        dispatchLine(server, 4, "JOIN #c secret");
        ASSERT_EQ("MODE: 正しい key の JOIN は成功", takeOutput(server, 4),
                  ":bob!u@10.0.0.1 JOIN :#c\r\n"
                  ":ircserv.local 331 bob #c :No topic is set\r\n"
                  ":ircserv.local 353 bob = #c :@alice bob\r\n"
                  ":ircserv.local 366 bob #c :End of NAMES list\r\n");
    }

    /* ── k: 既存 key の置換可 ─────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +k secret");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +k newkey");
        ASSERT_EQ("MODE: 既存 key の置換も実変更扱い",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c +k newkey\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: 置換後の照会は新しい key",
                  takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +tk newkey\r\n");
    }

    /* ── k: -k は引数なしで解除 ───────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +k secret");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c -k");
        ASSERT_EQ("MODE: -k の通知", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c -k\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: -k 後の照会は +t だけ", takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +t\r\n");
    }

    /* ── k: 不正な key (24 文字) は 461、状態不変 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +k abcdefghijklmnopqrstuvwx");
        ASSERT_EQ("MODE: 24 文字の key は 461", takeOutput(server, 3),
                  ":ircserv.local 461 alice MODE :Not enough parameters"
                  "\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: 不正 key 後も状態は +t のまま",
                  takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +t\r\n");
    }

    /* ── k: key 未設定で -k は通知なし ─────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c -k");
        ASSERT_EQ("MODE: key 未設定の -k は通知なし",
                  takeOutput(server, 3), "");
    }

    /* ── o: +o bob で bob が Operator になり KICK 可 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #c");
        takeOutput(server, 3);
        takeOutput(server, 4);

        registerUser(server, 5, "charlie", "10.0.0.2");
        dispatchLine(server, 5, "JOIN #c");
        takeOutput(server, 3);
        takeOutput(server, 4);
        takeOutput(server, 5);

        dispatchLine(server, 3, "MODE #c +o bob");
        ASSERT_EQ("MODE: +o bob の通知", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c +o bob\r\n");
        takeOutput(server, 4);
        takeOutput(server, 5);

        dispatchLine(server, 4, "KICK #c charlie");
        ASSERT_EQ("MODE: +o で Operator になった bob は KICK 可",
                  takeOutput(server, 4),
                  ":bob!u@10.0.0.1 KICK #c charlie :bob\r\n");
    }

    /* ── o: -o bob で解除 ─────────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #c");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "MODE #c +o bob");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "MODE #c -o bob");
        ASSERT_EQ("MODE: -o bob の通知", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c -o bob\r\n");
        takeOutput(server, 4);

        dispatchLine(server, 4, "MODE #c +i");
        ASSERT_EQ("MODE: -o 後の bob は非 Operator に戻る",
                  takeOutput(server, 4),
                  ":ircserv.local 482 bob #c :You're not channel operator"
                  "\r\n");
    }

    /* ── o: 対象 Nick 不在 → 401 ───────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +o nosuch");
        ASSERT_EQ("MODE: 対象不在の +o は 401", takeOutput(server, 3),
                  ":ircserv.local 401 alice nosuch :No such nick/channel"
                  "\r\n");
    }

    /* ── o: 対象が非 Member → 441 ──────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");

        dispatchLine(server, 3, "MODE #c +o bob");
        ASSERT_EQ("MODE: 対象非 Member の +o は 441", takeOutput(server, 3),
                  ":ircserv.local 441 alice bob #c :They aren't on that "
                  "channel\r\n");
    }

    /* ── o: 引数不足は 461 ────────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +o");
        ASSERT_EQ("MODE: +o 引数不足は 461", takeOutput(server, 3),
                  ":ircserv.local 461 alice MODE :Not enough parameters"
                  "\r\n");
    }

    /* ── o: 既に Operator への +o は通知なし ──── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +o alice");
        ASSERT_EQ("MODE: 既に Operator への +o は通知なし",
                  takeOutput(server, 3), "");
    }

    /* ── o: 最後の Operator を -o しても許可 ──── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c -o alice");
        ASSERT_EQ("MODE: 最後の Operator への -o も成功",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c -o alice\r\n");
    }

    /* ── l: +l 3 設定・324 反映・満員 JOIN 471 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #c");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "MODE #c +l 3");
        ASSERT_EQ("MODE: +l 3 の通知", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c +l 3\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: +l 後の照会は +tl 3", takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +tl 3\r\n");

        registerUser(server, 5, "charlie", "10.0.0.2");
        dispatchLine(server, 5, "JOIN #c");
        takeOutput(server, 3);
        takeOutput(server, 4);
        takeOutput(server, 5);

        registerUser(server, 6, "dave", "10.0.0.3");
        dispatchLine(server, 6, "JOIN #c");
        ASSERT_EQ("MODE: 満員 Channel への JOIN は 471", takeOutput(server, 6),
                  ":ircserv.local 471 dave #c :Cannot join channel (+l)"
                  "\r\n");
    }

    /* ── l: 不正 limit (0/abc/100001) は黙って無視 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +l 0");
        ASSERT_EQ("MODE: +l 0 は無視され通知なし", takeOutput(server, 3), "");

        dispatchLine(server, 3, "MODE #c +l abc");
        ASSERT_EQ("MODE: +l abc は無視され通知なし",
                  takeOutput(server, 3), "");

        dispatchLine(server, 3, "MODE #c +l 100001");
        ASSERT_EQ("MODE: +l 100001 は無視され通知なし",
                  takeOutput(server, 3), "");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: 不正 limit はどれも状態を変えない",
                  takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +t\r\n");
    }

    /* ── l: -l で解除 ─────────────────────────── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +l 3");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c -l");
        ASSERT_EQ("MODE: -l の通知", takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c -l\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: -l 後の照会は +t だけ", takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +t\r\n");
    }

    /* ── l: 現 Member 数未満の limit も設定可 ──── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #c");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "MODE #c +l 1");
        ASSERT_EQ("MODE: 現 Member 数 (2) 未満の limit (1) も設定可",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c +l 1\r\n");

        registerUser(server, 5, "charlie", "10.0.0.2");
        dispatchLine(server, 5, "JOIN #c");
        ASSERT_EQ("MODE: 既存 Member 超過の limit は新規 JOIN を拒否",
                  takeOutput(server, 5),
                  ":ircserv.local 471 charlie #c :Cannot join channel (+l)"
                  "\r\n");
    }

    /* ── 複数: +kol secret bob 10 の引数消費順・単一通知 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        registerUser(server, 4, "bob", "10.0.0.1");
        dispatchLine(server, 4, "JOIN #c");
        takeOutput(server, 3);
        takeOutput(server, 4);

        dispatchLine(server, 3, "MODE #c +kol secret bob 10");
        ASSERT_EQ("MODE: +kol secret bob 10 は左から順に引数消費・単一通知",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c +kol secret bob 10\r\n");

        dispatchLine(server, 3, "MODE #c");
        ASSERT_EQ("MODE: +kol 後の照会は +tkl secret 10",
                  takeOutput(server, 3),
                  ":ircserv.local 324 alice #c +tkl secret 10\r\n");
    }

    /* ── 複数: +it-k (先に +k してから) の符号切替通知 ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        /* t を初期値 true から false へ落としておく。i は初期値 false
           のままなので、後続の +it-k で i/t とも実変更になる */
        dispatchLine(server, 3, "MODE #c -t");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +k secret");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +it-k");
        ASSERT_EQ("MODE: +it-k は符号切替を1回ずつ出力",
                  takeOutput(server, 3),
                  ":alice!u@127.0.0.1 MODE #c +it-k\r\n");
    }

    /* ── 複数: +io nosuch は +i 成功・o は 401・通知は +i だけ ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +io nosuch");
        ASSERT_EQ("MODE: +io nosuch は o が 401",
                  takeOutput(server, 3),
                  ":ircserv.local 401 alice nosuch :No such nick/channel"
                  "\r\n"
                  ":alice!u@127.0.0.1 MODE #c +i\r\n");
    }

    /* ── 複数: +im は +i 成功・m は 472・通知は +i だけ ── */

    {
        Server server(6667, "secret");

        registerUser(server, 3, "alice", "127.0.0.1");
        dispatchLine(server, 3, "JOIN #c");
        takeOutput(server, 3);

        dispatchLine(server, 3, "MODE #c +im");
        ASSERT_EQ("MODE: +im は m が 472",
                  takeOutput(server, 3),
                  ":ircserv.local 472 alice m :is unknown mode char to me "
                  "for #c\r\n"
                  ":alice!u@127.0.0.1 MODE #c +i\r\n");
    }
}
