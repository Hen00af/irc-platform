#include "TestRunner.hpp"

#include "prd/domain/Client.hpp"
#include "prd/util/IrcUtil.hpp"
#include "prd/util/Numerics.hpp"
#include "prd/util/Reply.hpp"

namespace
{
    const std::string SERVER = "ircserv.local";

    Client makeClient(int fd, const std::string &nickname)
    {
        Client client(fd, "127.0.0.1");

        client.setNickname(nickname);
        client.setUser("u", "Real Name");
        return client;
    }
}

void runReplyTests()
{
    TestRunner::beginSuite("Reply");

    /* ── clientPrefix ─────────────────────── */

    {
        Client client = makeClient(3, "alice");

        /* 先頭 colon は付けない。command() へそのまま渡せる形式
           (設計書 06 §5) */
        ASSERT_EQ("Reply: clientPrefix", Reply::clientPrefix(client),
                  "alice!u@127.0.0.1");
    }

    {
        /* 未設定項目の fallback (設計書 06 §3) */
        Client bare(4, "127.0.0.1");

        ASSERT_EQ("Reply: 全未設定の clientPrefix", Reply::clientPrefix(bare),
                  "*!unknown@127.0.0.1");

        Client noHost(5, "");
        ASSERT_EQ("Reply: Hostname 未設定の fallback",
                  Reply::clientPrefix(noHost), "*!unknown@0.0.0.0");

        Client nickOnly(6, "127.0.0.1");
        nickOnly.setNickname("bob");
        ASSERT_EQ("Reply: Username 未設定の fallback",
                  Reply::clientPrefix(nickOnly), "bob!unknown@127.0.0.1");

        Client userOnly(7, "127.0.0.1");
        userOnly.setUser("u", "R");
        ASSERT_EQ("Reply: Nickname 未設定の fallback",
                  Reply::clientPrefix(userOnly), "*!u@127.0.0.1");
    }

    /* ── serverPrefix ─────────────────────── */

    ASSERT_EQ("Reply: serverPrefix も colon なし", Reply::serverPrefix(SERVER),
              "ircserv.local");

    /* ── numeric ──────────────────────────── */

    {
        Client client = makeClient(3, "alice");

        ASSERT_EQ("Reply: numeric の基本形",
                  Reply::numeric(SERVER, client, Numeric::ERR_NICKNAMEINUSE,
                                 "bob :Nickname is already in use"),
                  ":ircserv.local 433 alice bob :Nickname is already in use");

        /* code は必ず 3 桁へ 0 詰めする (設計書 06 §4) */
        ASSERT_EQ("Reply: 1 桁 code を 3 桁へ 0 詰め",
                  Reply::numeric(SERVER, client, Numeric::RPL_WELCOME,
                                 ":Welcome"),
                  ":ircserv.local 001 alice :Welcome");
        ASSERT_EQ("Reply: 2 桁 code を 3 桁へ 0 詰め",
                  Reply::numeric(SERVER, client, 42, ":x"),
                  ":ircserv.local 042 alice :x");
        ASSERT_EQ("Reply: 3 桁 code はそのまま",
                  Reply::numeric(SERVER, client, Numeric::ERR_CHANOPRIVSNEEDED,
                                 "#a :You're not channel operator"),
                  ":ircserv.local 482 alice #a :You're not channel operator");
        ASSERT_EQ("Reply: code 0", Reply::numeric(SERVER, client, 0, ":x"),
                  ":ircserv.local 000 alice :x");
        ASSERT_EQ("Reply: code 999", Reply::numeric(SERVER, client, 999, ":x"),
                  ":ircserv.local 999 alice :x");

        /* Parameter が空なら末尾に空白を付けない */
        ASSERT_EQ("Reply: Parameter が空の numeric",
                  Reply::numeric(SERVER, client, 421, ""),
                  ":ircserv.local 421 alice");
    }

    {
        /* 未登録 Client への target は "*" (設計書 06 §4) */
        Client bare(4, "127.0.0.1");

        ASSERT_EQ("Reply: 未登録 Client への numeric は target が *",
                  Reply::numeric(SERVER, bare, Numeric::ERR_NOTREGISTERED,
                                 ":You have not registered"),
                  ":ircserv.local 451 * :You have not registered");
    }

    {
        /* 範囲外の code は内部 Error として空文字 (設計書 06 §17)。
           呼び出し側は空文字を queue しないこと */
        Client client = makeClient(3, "alice");

        ASSERT_EQ("Reply: 負の code は空文字",
                  Reply::numeric(SERVER, client, -1, ":x"), "");
        ASSERT_EQ("Reply: 1000 以上の code は空文字",
                  Reply::numeric(SERVER, client, 1000, ":x"), "");
        ASSERT_EQ("Reply: 空の serverName は空文字",
                  Reply::numeric("", client, 401, ":x"), "");
        ASSERT_EQ("Reply: 空白を含む serverName は空文字",
                  Reply::numeric("irc serv", client, 401, ":x"), "");
    }

    /* ── command ──────────────────────────── */

    {
        Client client = makeClient(3, "alice");
        std::string prefix = Reply::clientPrefix(client);

        ASSERT_EQ("Reply: command が prefix へ colon を付ける",
                  Reply::command(prefix, "JOIN", ":#general"),
                  ":alice!u@127.0.0.1 JOIN :#general");
        ASSERT_EQ("Reply: PRIVMSG の組み立て",
                  Reply::command(prefix, "PRIVMSG", "#general :hello"),
                  ":alice!u@127.0.0.1 PRIVMSG #general :hello");
        ASSERT_EQ("Reply: MODE の組み立て",
                  Reply::command(prefix, "MODE", "#general +o bob"),
                  ":alice!u@127.0.0.1 MODE #general +o bob");

        /* prefix が空なら prefix 部を出力しない */
        ASSERT_EQ("Reply: prefix なしの command",
                  Reply::command("", "PING", ":token"), "PING :token");

        /* Parameter が空なら末尾に空白を付けない */
        ASSERT_EQ("Reply: Parameter が空の command",
                  Reply::command(prefix, "QUIT", ""),
                  ":alice!u@127.0.0.1 QUIT");
        ASSERT_EQ("Reply: prefix も Parameter も空",
                  Reply::command("", "QUIT", ""), "QUIT");

        /* Server Prefix での ERROR (設計書 06 §16) */
        ASSERT_EQ("Reply: Server Prefix の ERROR",
                  Reply::command(Reply::serverPrefix(SERVER), "ERROR",
                                 ":Input line too long"),
                  ":ircserv.local ERROR :Input line too long");
    }

    {
        /* 内部 Error として空文字 */
        ASSERT_EQ("Reply: 空の command は空文字", Reply::command("p", "", "x"),
                  "");
        ASSERT_EQ("Reply: 空白を含む command は空文字",
                  Reply::command("p", "JO IN", "x"), "");
        ASSERT_EQ("Reply: 空白を含む prefix は空文字",
                  Reply::command("a b", "JOIN", "x"), "");
        ASSERT_EQ("Reply: CR を含む prefix は空文字",
                  Reply::command("a\rb", "JOIN", "x"), "");
        ASSERT_EQ("Reply: LF を含む command は空文字",
                  Reply::command("p", "JO\nIN", "x"), "");
        ASSERT_EQ("Reply: NUL を含む prefix は空文字",
                  Reply::command(std::string("a\0b", 3), "JOIN", "x"), "");
    }

    /* ── CRLF injection の防御 (設計書 06 §21) ── */

    {
        /* IRC は CRLF で行を区切るため、Client 由来の本文をそのまま中継
           すると、受信側に「別の行」として解釈させられてしまう。
           Reply が Parameter を sanitize して防ぐ */
        Client      client = makeClient(3, "alice");
        std::string prefix = Reply::clientPrefix(client);

        /* このサーバで実際に Parameter へ到達し得るのは単独 CR だけ。
           findLine() が LF を、Parser::parse() が NUL を先に弾くため */
        ASSERT_EQ("Reply: 単独 CR を除去して 1 行に保つ",
                  Reply::command(prefix, "PRIVMSG",
                                 "#a :hi\rKICK #a bob :bye"),
                  ":alice!u@127.0.0.1 PRIVMSG #a :hiKICK #a bob :bye");

        /* 将来の経路変更に備え CRLF と LF も除去する */
        ASSERT_EQ("Reply: CRLF を除去して 1 行に保つ",
                  Reply::command(prefix, "PRIVMSG",
                                 "#a :hi\r\nKICK #a bob :bye"),
                  ":alice!u@127.0.0.1 PRIVMSG #a :hiKICK #a bob :bye");
        ASSERT_EQ("Reply: 単独 LF を除去して 1 行に保つ",
                  Reply::command(prefix, "PRIVMSG", "#a :hi\nQUIT"),
                  ":alice!u@127.0.0.1 PRIVMSG #a :hiQUIT");

        /* Numeric Reply の Parameter も同様に防ぐ (Topic や Nickname 経由) */
        ASSERT_EQ("Reply: numeric の Parameter も sanitize する",
                  Reply::numeric(SERVER, client, Numeric::RPL_TOPIC,
                                 "#a :topic\r\n:evil PRIVMSG #b :spoofed"),
                  ":ircserv.local 332 alice #a :topic:evil PRIVMSG #b :spoofed");

        /* 生成された行に CR / LF / NUL が 1 つも残らないこと */
        std::string injected =
            Reply::command(prefix, "PRIVMSG", "#a :x\r\ny\rz\nw");
        ASSERT_TRUE("Reply: 生成行に CR が残らない",
                    injected.find('\r') == std::string::npos);
        ASSERT_TRUE("Reply: 生成行に LF が残らない",
                    injected.find('\n') == std::string::npos);
        ASSERT_TRUE("Reply: 生成行に NUL が残らない",
                    injected.find('\0') == std::string::npos);

        std::string injectedNumeric = Reply::numeric(
            SERVER, client, 332, std::string("#a :x\r\ny\0z", 10));
        ASSERT_TRUE("Reply: numeric の生成行に CR が残らない",
                    injectedNumeric.find('\r') == std::string::npos);
        ASSERT_TRUE("Reply: numeric の生成行に LF が残らない",
                    injectedNumeric.find('\n') == std::string::npos);
        ASSERT_TRUE("Reply: numeric の生成行に NUL が残らない",
                    injectedNumeric.find('\0') == std::string::npos);
    }

    {
        /* Nickname 経由の injection。Nickname は NICK Handler が
           isValidNickname() で弾く前提だが、Reply も防御する。
           空白を含む Nickname で試すと空白だけで gate に掛かり、CR / LF の
           検査が効いているか判別できない。1 種類ずつ単独で試す */
        Client crNick(9, "127.0.0.1");
        crNick.setNickname("a\rb");
        crNick.setUser("u", "R");
        ASSERT_EQ("Reply: CR のみを含む Nickname は command で空文字",
                  Reply::command(Reply::clientPrefix(crNick), "PRIVMSG",
                                 "#a :hi"),
                  "");
        ASSERT_EQ("Reply: CR のみを含む Nickname は numeric の target で空文字",
                  Reply::numeric(SERVER, crNick, 401, ":x"), "");

        Client lfNick(10, "127.0.0.1");
        lfNick.setNickname("a\nb");
        lfNick.setUser("u", "R");
        ASSERT_EQ("Reply: LF のみを含む Nickname は command で空文字",
                  Reply::command(Reply::clientPrefix(lfNick), "PRIVMSG",
                                 "#a :hi"),
                  "");
        ASSERT_EQ("Reply: LF のみを含む Nickname は numeric の target で空文字",
                  Reply::numeric(SERVER, lfNick, 401, ":x"), "");

        Client nulNick(11, "127.0.0.1");
        nulNick.setNickname(std::string("a\0b", 3));
        nulNick.setUser("u", "R");
        ASSERT_EQ("Reply: NUL のみを含む Nickname は numeric の target で空文字",
                  Reply::numeric(SERVER, nulNick, 401, ":x"), "");

        Client spaceNick(12, "127.0.0.1");
        spaceNick.setNickname("a b");
        spaceNick.setUser("u", "R");
        ASSERT_EQ("Reply: 空白のみを含む Nickname は numeric の target で空文字",
                  Reply::numeric(SERVER, spaceNick, 401, ":x"), "");
    }

    {
        /* USER 由来の injection。Client::setUser() は検証しないため、
           Reply が唯一の防波堤になる経路 */
        Client evilUser(13, "127.0.0.1");

        evilUser.setNickname("alice");
        evilUser.setUser("a\rb", "R");
        ASSERT_EQ("Reply: CR を含む Username の prefix は command で空文字",
                  Reply::command(Reply::clientPrefix(evilUser), "PRIVMSG",
                                 "#a :hi"),
                  "");

        Client lfUser(14, "127.0.0.1");
        lfUser.setNickname("alice");
        lfUser.setUser("a\nb", "R");
        ASSERT_EQ("Reply: LF を含む Username の prefix は command で空文字",
                  Reply::command(Reply::clientPrefix(lfUser), "PRIVMSG",
                                 "#a :hi"),
                  "");

        Client spaceUser(15, "127.0.0.1");
        spaceUser.setNickname("alice");
        spaceUser.setUser("a b", "R");
        ASSERT_EQ("Reply: 空白を含む Username の prefix は command で空文字",
                  Reply::command(Reply::clientPrefix(spaceUser), "PRIVMSG",
                                 "#a :hi"),
                  "");

        /* Realname は prefix に入らないので影響しない */
        Client evilReal(16, "127.0.0.1");
        evilReal.setNickname("alice");
        evilReal.setUser("u", "a\r\nb");
        ASSERT_EQ("Reply: Realname は prefix に入らない",
                  Reply::clientPrefix(evilReal), "alice!u@127.0.0.1");
    }

    {
        /* serverName / command 側の単独文字 */
        Client client = makeClient(3, "alice");

        ASSERT_EQ("Reply: CR を含む serverName は空文字",
                  Reply::numeric("irc\rserv", client, 401, ":x"), "");
        ASSERT_EQ("Reply: LF を含む serverName は空文字",
                  Reply::numeric("irc\nserv", client, 401, ":x"), "");
        ASSERT_EQ("Reply: NUL を含む serverName は空文字",
                  Reply::numeric(std::string("irc\0serv", 8), client, 401,
                                 ":x"),
                  "");
        ASSERT_EQ("Reply: CR を含む command は空文字",
                  Reply::command("p", "JO\rIN", "x"), "");
        ASSERT_EQ("Reply: NUL を含む command は空文字",
                  Reply::command("p", std::string("JO\0IN", 5), "x"), "");
    }

    {
        /* 先頭 colon は Trailing の目印なので token の先頭には置けない。
           許すと target や command が Trailing として解釈され行が壊れる */
        Client colonNick(17, "127.0.0.1");

        colonNick.setNickname(":x");
        colonNick.setUser("u", "R");
        ASSERT_EQ("Reply: 先頭 colon の Nickname は numeric の target で空文字",
                  Reply::numeric(SERVER, colonNick, 401,
                                 ":No such nick/channel"),
                  "");
        ASSERT_EQ("Reply: 先頭 colon の prefix は空文字",
                  Reply::command(":x!u@h", "PRIVMSG", "#a :hi"), "");
        ASSERT_EQ("Reply: 先頭 colon の command は空文字",
                  Reply::command("p", ":JOIN", "#a"), "");
        ASSERT_EQ("Reply: 先頭 colon の serverName は空文字",
                  Reply::numeric(":evil", makeClient(3, "alice"), 401, ":x"),
                  "");

        /* token 途中の colon は IPv6 hostname で正当なため許可する */
        Client ipv6(18, "::1");
        ipv6.setNickname("alice");
        ipv6.setUser("u", "R");
        ASSERT_EQ("Reply: IPv6 hostname の prefix", Reply::clientPrefix(ipv6),
                  "alice!u@::1");
        ASSERT_EQ("Reply: 途中に colon を含む prefix は許可する",
                  Reply::command(Reply::clientPrefix(ipv6), "PRIVMSG",
                                 "#a :hi"),
                  ":alice!u@::1 PRIVMSG #a :hi");
    }

    {
        /* sanitize の結果が空になる Parameter で、末尾に空白を残さないこと */
        Client      client = makeClient(3, "alice");
        std::string prefix = Reply::clientPrefix(client);

        ASSERT_EQ("Reply: CR だけの Parameter は末尾空白を残さない",
                  Reply::command(prefix, "QUIT", "\r"),
                  ":alice!u@127.0.0.1 QUIT");
        ASSERT_EQ("Reply: CRLF だけの Parameter は末尾空白を残さない",
                  Reply::command(prefix, "QUIT", "\r\n"),
                  ":alice!u@127.0.0.1 QUIT");
        ASSERT_EQ("Reply: numeric でも末尾空白を残さない",
                  Reply::numeric(SERVER, client, 421, "\r\n"),
                  ":ircserv.local 421 alice");

        /* 末尾が CR の Parameter。"...\r\r\n" で受信すると findLine は
           末尾 1 つの CR しか外さないため、この形は実際に到達し得る */
        ASSERT_EQ("Reply: 末尾が CR の Parameter",
                  Reply::command(prefix, "PRIVMSG", "#a :hi\r"),
                  ":alice!u@127.0.0.1 PRIVMSG #a :hi");
    }

    /* ── CRLF は付けない (設計書 02 §9, 06 §2) ── */

    {
        Client      client = makeClient(3, "alice");
        std::string line =
            Reply::numeric(SERVER, client, Numeric::RPL_WELCOME, ":Welcome");

        ASSERT_TRUE("Reply: numeric は CRLF を含まない",
                    line.find('\r') == std::string::npos
                        && line.find('\n') == std::string::npos);
        ASSERT_TRUE("Reply: command は CRLF を含まない",
                    Reply::command(Reply::clientPrefix(client), "QUIT", ":bye")
                            .find('\r')
                        == std::string::npos);
    }

    /* ── 行長 (設計書 06 §13) ─────────────── */

    {
        /* Reply は切り詰めない。長さ確認は呼び出し側が
           IrcUtil::fitsIrcLine() で行う */
        Client      client = makeClient(3, "alice");
        std::string huge   = Reply::numeric(SERVER, client, 332,
                                            "#a :" + std::string(600, 'x'));

        ASSERT_TRUE("Reply: 上限超過でも切り詰めない", huge.size() > 510);
        ASSERT_FALSE("Reply: 上限超過は fitsIrcLine で検出できる",
                     IrcUtil::fitsIrcLine(huge));

        std::string normal =
            Reply::numeric(SERVER, client, 332, "#a :short topic");
        ASSERT_TRUE("Reply: 通常の行は fitsIrcLine を満たす",
                    IrcUtil::fitsIrcLine(normal));
    }

    /* ── Welcome シーケンス (設計書 06 §6) ── */

    {
        Client client = makeClient(3, "alice");

        ASSERT_EQ("Reply: 001 RPL_WELCOME",
                  Reply::numeric(SERVER, client, Numeric::RPL_WELCOME,
                                 ":Welcome to the Internet Relay Network "
                                 "alice!u@127.0.0.1"),
                  ":ircserv.local 001 alice :Welcome to the Internet Relay "
                  "Network alice!u@127.0.0.1");
        ASSERT_EQ("Reply: 422 ERR_NOMOTD",
                  Reply::numeric(SERVER, client, Numeric::ERR_NOMOTD,
                                 ":MOTD File is missing"),
                  ":ircserv.local 422 alice :MOTD File is missing");
    }

    /* ── Numeric 定数 (設計書 06 §8) ──────── */

    ASSERT_EQ("Numeric: RPL_WELCOME", Numeric::RPL_WELCOME, 1);
    ASSERT_EQ("Numeric: RPL_YOURHOST", Numeric::RPL_YOURHOST, 2);
    ASSERT_EQ("Numeric: RPL_CREATED", Numeric::RPL_CREATED, 3);
    ASSERT_EQ("Numeric: RPL_MYINFO", Numeric::RPL_MYINFO, 4);
    ASSERT_EQ("Numeric: RPL_CHANNELMODEIS", Numeric::RPL_CHANNELMODEIS, 324);
    ASSERT_EQ("Numeric: RPL_NOTOPIC", Numeric::RPL_NOTOPIC, 331);
    ASSERT_EQ("Numeric: RPL_TOPIC", Numeric::RPL_TOPIC, 332);
    ASSERT_EQ("Numeric: RPL_INVITING", Numeric::RPL_INVITING, 341);
    ASSERT_EQ("Numeric: RPL_NAMREPLY", Numeric::RPL_NAMREPLY, 353);
    ASSERT_EQ("Numeric: RPL_ENDOFNAMES", Numeric::RPL_ENDOFNAMES, 366);
    ASSERT_EQ("Numeric: ERR_NOSUCHNICK", Numeric::ERR_NOSUCHNICK, 401);
    ASSERT_EQ("Numeric: ERR_NOSUCHCHANNEL", Numeric::ERR_NOSUCHCHANNEL, 403);
    ASSERT_EQ("Numeric: ERR_CANNOTSENDTOCHAN", Numeric::ERR_CANNOTSENDTOCHAN,
              404);
    ASSERT_EQ("Numeric: ERR_NOORIGIN", Numeric::ERR_NOORIGIN, 409);
    ASSERT_EQ("Numeric: ERR_NORECIPIENT", Numeric::ERR_NORECIPIENT, 411);
    ASSERT_EQ("Numeric: ERR_NOTEXTTOSEND", Numeric::ERR_NOTEXTTOSEND, 412);
    ASSERT_EQ("Numeric: ERR_UNKNOWNCOMMAND", Numeric::ERR_UNKNOWNCOMMAND, 421);
    ASSERT_EQ("Numeric: ERR_NOMOTD", Numeric::ERR_NOMOTD, 422);
    ASSERT_EQ("Numeric: ERR_NONICKNAMEGIVEN", Numeric::ERR_NONICKNAMEGIVEN,
              431);
    ASSERT_EQ("Numeric: ERR_ERRONEUSNICKNAME", Numeric::ERR_ERRONEUSNICKNAME,
              432);
    ASSERT_EQ("Numeric: ERR_NICKNAMEINUSE", Numeric::ERR_NICKNAMEINUSE, 433);
    ASSERT_EQ("Numeric: ERR_USERNOTINCHANNEL", Numeric::ERR_USERNOTINCHANNEL,
              441);
    ASSERT_EQ("Numeric: ERR_NOTONCHANNEL", Numeric::ERR_NOTONCHANNEL, 442);
    ASSERT_EQ("Numeric: ERR_USERONCHANNEL", Numeric::ERR_USERONCHANNEL, 443);
    ASSERT_EQ("Numeric: ERR_NOTREGISTERED", Numeric::ERR_NOTREGISTERED, 451);
    ASSERT_EQ("Numeric: ERR_NEEDMOREPARAMS", Numeric::ERR_NEEDMOREPARAMS, 461);
    ASSERT_EQ("Numeric: ERR_ALREADYREGISTRED", Numeric::ERR_ALREADYREGISTRED,
              462);
    ASSERT_EQ("Numeric: ERR_PASSWDMISMATCH", Numeric::ERR_PASSWDMISMATCH, 464);
    ASSERT_EQ("Numeric: ERR_CHANNELISFULL", Numeric::ERR_CHANNELISFULL, 471);
    ASSERT_EQ("Numeric: ERR_UNKNOWNMODE", Numeric::ERR_UNKNOWNMODE, 472);
    ASSERT_EQ("Numeric: ERR_INVITEONLYCHAN", Numeric::ERR_INVITEONLYCHAN, 473);
    ASSERT_EQ("Numeric: ERR_BADCHANNELKEY", Numeric::ERR_BADCHANNELKEY, 475);
    ASSERT_EQ("Numeric: ERR_CHANOPRIVSNEEDED", Numeric::ERR_CHANOPRIVSNEEDED,
              482);
}
