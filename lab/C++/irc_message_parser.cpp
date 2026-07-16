/* ============================================================
 * irc_message_parser.cpp
 *
 * IRC メッセージパーサーの実験コード (C++98)
 *
 * IRC メッセージのフォーマット (RFC 1459):
 *   [:prefix] COMMAND [params...] [:trailing]
 *
 * 例:
 *   :nick!user@host PRIVMSG #channel :Hello world
 *   PING :irc.example.com
 *   JOIN #test,#foo key1
 *
 * ビルド:
 *   c++ -Wall -Wextra -Werror -std=c++98 irc_message_parser.cpp -o parser
 *   ./parser
 * ============================================================ */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cassert>

/* ============================================================
 * IRC メッセージ構造体
 * ============================================================ */
struct IrcMessage
{
    std::string              prefix;   /* "nick!user@host" または "" */
    std::string              command;  /* "PRIVMSG", "JOIN", "001" など */
    std::vector<std::string> params;   /* コマンド引数 */
};

/* ============================================================
 * IRC メッセージをパースする
 *
 * picoshell のコマンドパーサーに似ているが、
 * 最後のパラメータだけ ':' で始まってスペースを含める点が違う。
 * ============================================================ */
IrcMessage parseIrcMessage(const std::string& raw)
{
    IrcMessage msg;
    std::string line = raw;

    /* \r\n を除去 */
    while (!line.empty() && (line[line.size() - 1] == '\r' ||
                              line[line.size() - 1] == '\n'))
        line.erase(line.size() - 1);

    if (line.empty())
        return msg;

    std::size_t pos = 0;

    /* prefix: ':' で始まる場合 */
    if (line[0] == ':')
    {
        std::size_t end = line.find(' ', 1);
        if (end == std::string::npos)
            return msg; /* 不正なメッセージ */
        msg.prefix = line.substr(1, end - 1);
        pos = end + 1;
    }

    /* 空白をスキップ */
    while (pos < line.size() && line[pos] == ' ')
        ++pos;

    /* command */
    {
        std::size_t end = line.find(' ', pos);
        if (end == std::string::npos)
        {
            msg.command = line.substr(pos);
            return msg;
        }
        msg.command = line.substr(pos, end - pos);
        pos = end + 1;
    }

    /* params */
    while (pos < line.size())
    {
        /* 空白をスキップ */
        while (pos < line.size() && line[pos] == ' ')
            ++pos;
        if (pos >= line.size())
            break;

        /* trailing パラメータ: ':' で始まる → 残り全部 */
        if (line[pos] == ':')
        {
            msg.params.push_back(line.substr(pos + 1));
            break;
        }

        /* 通常のパラメータ: 次の空白まで */
        std::size_t end = line.find(' ', pos);
        if (end == std::string::npos)
        {
            msg.params.push_back(line.substr(pos));
            break;
        }
        msg.params.push_back(line.substr(pos, end - pos));
        pos = end + 1;
    }

    return msg;
}

/* ============================================================
 * デバッグ出力
 * ============================================================ */
void printMessage(const IrcMessage& msg)
{
    std::cout << "prefix : [" << msg.prefix  << "]\n";
    std::cout << "command: [" << msg.command << "]\n";
    for (std::size_t i = 0; i < msg.params.size(); ++i)
        std::cout << "param[" << i << "]: [" << msg.params[i] << "]\n";
    std::cout << std::endl;
}

/* ============================================================
 * IRC 数値リプライを組み立てる
 *
 * フォーマット: ":server NNN target :message\r\n"
 * ============================================================ */
std::string makeNumericReply(const std::string& server, int num,
                             const std::string& target,
                             const std::string& msg)
{
    std::ostringstream oss;
    oss << ":" << server << " "
        << (num < 100 ? "0" : "")
        << (num < 10  ? "0" : "")
        << num << " " << target << " " << msg << "\r\n";
    return oss.str();
}

/* ============================================================
 * テスト
 * ============================================================ */
int main(void)
{
    std::cout << "=== IRC メッセージパーサー テスト ===\n\n";

    /* テスト 1: prefix あり */
    {
        IrcMessage m = parseIrcMessage(
            ":Alice!alice@127.0.0.1 PRIVMSG #test :Hello world!");
        std::cout << "Test 1: PRIVMSG with prefix\n";
        printMessage(m);
        assert(m.prefix  == "Alice!alice@127.0.0.1");
        assert(m.command == "PRIVMSG");
        assert(m.params.size() == 2);
        assert(m.params[0] == "#test");
        assert(m.params[1] == "Hello world!");
    }

    /* テスト 2: prefix なし */
    {
        IrcMessage m = parseIrcMessage("JOIN #channel1,#channel2 key1,key2");
        std::cout << "Test 2: JOIN without prefix\n";
        printMessage(m);
        assert(m.prefix  == "");
        assert(m.command == "JOIN");
        assert(m.params.size() == 2);
        assert(m.params[0] == "#channel1,#channel2");
        assert(m.params[1] == "key1,key2");
    }

    /* テスト 3: PING */
    {
        IrcMessage m = parseIrcMessage("PING :irc.example.com");
        std::cout << "Test 3: PING\n";
        printMessage(m);
        assert(m.command == "PING");
        assert(m.params.size() == 1);
        assert(m.params[0] == "irc.example.com");
    }

    /* テスト 4: 数値リプライの組み立て */
    {
        std::string reply = makeNumericReply("ircserv", 1, "Alice",
                                             ":Welcome to the IRC Network");
        std::cout << "Test 4: Numeric reply\n";
        std::cout << reply;
        assert(reply == ":ircserv 001 Alice :Welcome to the IRC Network\r\n");
    }

    /* テスト 5: \r\n の除去 */
    {
        IrcMessage m = parseIrcMessage("NICK Bob\r\n");
        std::cout << "\nTest 5: CRLF stripping\n";
        printMessage(m);
        assert(m.command == "NICK");
        assert(m.params.size() == 1);
        assert(m.params[0] == "Bob");
    }

    std::cout << "All tests passed!\n";
    return 0;
}
