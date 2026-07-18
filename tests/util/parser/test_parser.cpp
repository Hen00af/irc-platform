#include "TestRunner.hpp"

#include "prd/domain/Message.hpp"
#include "prd/util/Parser.hpp"

#include <sstream>

namespace
{
    /* アサーション失敗時も実行は継続するため、size を確認したあとの
       素の params[i] は範囲外アクセス (未定義動作) になり得る。
       範囲外なら目印を返して、UB ではなく読めるテスト失敗にする */
    std::string param(const Message &message, std::size_t index)
    {
        if (index >= message.params.size())
            return "<OUT_OF_RANGE>";
        return message.params[index];
    }
}

void runParserTests()
{
    TestRunner::beginSuite("Parser");

    /* ── 基本形 ───────────────────────────── */

    {
        Message message;

        ASSERT_TRUE("Parser: 単純な Command", Parser::parse("PASS secret",
                                                            message));
        ASSERT_EQ("Parser: prefix は空", message.prefix, "");
        ASSERT_EQ("Parser: command", message.command, "PASS");
        ASSERT_EQ("Parser: params 数", message.params.size(),
                  static_cast<std::size_t>(1));
        ASSERT_EQ("Parser: params[0]", param(message, 0), "secret");
    }

    {
        /* 空白区切りの複数 Parameter */
        Message message;

        ASSERT_TRUE("Parser: 複数 Parameter",
                    Parser::parse("MODE #general +o bob", message));
        ASSERT_EQ("Parser: 複数 Parameter の command", message.command, "MODE");
        ASSERT_EQ("Parser: 複数 Parameter の数", message.params.size(),
                  static_cast<std::size_t>(3));
        ASSERT_EQ("Parser: 複数 Parameter[0]", param(message, 0), "#general");
        ASSERT_EQ("Parser: 複数 Parameter[1]", param(message, 1), "+o");
        ASSERT_EQ("Parser: 複数 Parameter[2]", param(message, 2), "bob");
    }

    /* ── 引数なしコマンド ─────────────────── */

    {
        Message message;

        ASSERT_TRUE("Parser: 引数なしコマンド", Parser::parse("LIST", message));
        ASSERT_EQ("Parser: 引数なしの command", message.command, "LIST");
        ASSERT_EQ("Parser: 引数なしの params は空", message.params.size(),
                  static_cast<std::size_t>(0));
    }

    {
        /* 末尾に空白があっても引数なしとして扱う */
        Message message;

        ASSERT_TRUE("Parser: 末尾空白つき引数なしコマンド",
                    Parser::parse("LIST   ", message));
        ASSERT_EQ("Parser: 末尾空白つきの command", message.command, "LIST");
        ASSERT_EQ("Parser: 末尾空白は Parameter にしない",
                  message.params.size(), static_cast<std::size_t>(0));
    }

    /* ── Command の大文字・小文字 ─────────── */

    {
        Message message;

        Parser::parse("privmsg #a :hi", message);
        ASSERT_EQ("Parser: 小文字 Command を大文字へ", message.command,
                  "PRIVMSG");

        Parser::parse("PrIvMsG #a :hi", message);
        ASSERT_EQ("Parser: 大小混在 Command を大文字へ", message.command,
                  "PRIVMSG");

        Parser::parse("JOIN #a", message);
        ASSERT_EQ("Parser: 既に大文字の Command", message.command, "JOIN");

        /* Parameter は正規化しない */
        Parser::parse("JOIN #General", message);
        ASSERT_EQ("Parser: Parameter は大文字化しない", param(message, 0),
                  "#General");

        /* 数値 Command も受理する (検証は Dispatcher の責務) */
        Parser::parse("001 alice", message);
        ASSERT_EQ("Parser: 数値 Command", message.command, "001");
    }

    /* ── Prefix ───────────────────────────── */

    {
        Message message;

        ASSERT_TRUE("Parser: Prefix つき",
                    Parser::parse(":alice!user@host PRIVMSG #a :hi", message));
        ASSERT_EQ("Parser: prefix", message.prefix, "alice!user@host");
        ASSERT_EQ("Parser: Prefix つきの command", message.command, "PRIVMSG");
        ASSERT_EQ("Parser: Prefix つきの params 数", message.params.size(),
                  static_cast<std::size_t>(2));
        ASSERT_EQ("Parser: Prefix つきの params[0]", param(message, 0), "#a");
        ASSERT_EQ("Parser: Prefix つきの params[1]", param(message, 1), "hi");
    }

    {
        Message message;

        ASSERT_TRUE("Parser: Prefix + 引数なし Command",
                    Parser::parse(":server PING", message));
        ASSERT_EQ("Parser: Prefix のみの prefix", message.prefix, "server");
        ASSERT_EQ("Parser: Prefix のみの command", message.command, "PING");
        ASSERT_EQ("Parser: Prefix のみの params", message.params.size(),
                  static_cast<std::size_t>(0));
    }

    {
        /* Prefix は大文字化しない */
        Message message;

        Parser::parse(":Alice!u@h nick bob", message);
        ASSERT_EQ("Parser: prefix は正規化しない", message.prefix,
                  "Alice!u@h");
        ASSERT_EQ("Parser: prefix つきでも command は大文字化",
                  message.command, "NICK");
    }

    /* ── Trailing parameter ───────────────── */

    {
        Message message;

        ASSERT_TRUE("Parser: Trailing に空白を含む",
                    Parser::parse("PRIVMSG #general :hello world", message));
        ASSERT_EQ("Parser: Trailing の params 数", message.params.size(),
                  static_cast<std::size_t>(2));
        ASSERT_EQ("Parser: Trailing 前の Parameter", param(message, 0),
                  "#general");
        ASSERT_EQ("Parser: Trailing は空白を保持", param(message, 1),
                  "hello world");
    }

    {
        Message message;

        /* Trailing 内の colon は区切りとして扱わない */
        Parser::parse("PRIVMSG #a :a:b:c", message);
        ASSERT_EQ("Parser: Trailing 内の colon を保持", param(message, 1),
                  "a:b:c");

        /* Trailing の先頭 colon 直後の空白も保持する */
        Parser::parse("PRIVMSG #a :  hi", message);
        ASSERT_EQ("Parser: Trailing 先頭の空白を保持", param(message, 1),
                  "  hi");

        /* Trailing 末尾の空白も保持する */
        Parser::parse("PRIVMSG #a :hi  ", message);
        ASSERT_EQ("Parser: Trailing 末尾の空白を保持", param(message, 1),
                  "hi  ");

        /* 空の Trailing も 1 つの Parameter として扱う */
        ASSERT_TRUE("Parser: 空の Trailing", Parser::parse("PRIVMSG #a :",
                                                           message));
        ASSERT_EQ("Parser: 空 Trailing の params 数", message.params.size(),
                  static_cast<std::size_t>(2));
        ASSERT_EQ("Parser: 空 Trailing は空文字の Parameter",
                  param(message, 1), "");

        /* Trailing だけの Command */
        ASSERT_TRUE("Parser: Trailing だけ", Parser::parse("QUIT :bye bye",
                                                           message));
        ASSERT_EQ("Parser: Trailing だけの params 数", message.params.size(),
                  static_cast<std::size_t>(1));
        ASSERT_EQ("Parser: Trailing だけの内容", param(message, 0), "bye bye");

        /* Trailing 内の連続空白を保持する */
        Parser::parse("PRIVMSG #a :a  b", message);
        ASSERT_EQ("Parser: Trailing 内の連続空白を保持", param(message, 1),
                  "a  b");
    }

    {
        /* Trailing 以降に別の colon があっても分割しない */
        Message message;

        Parser::parse("PRIVMSG #a :hi :there", message);
        ASSERT_EQ("Parser: Trailing 以降を分割しない params 数",
                  message.params.size(), static_cast<std::size_t>(2));
        ASSERT_EQ("Parser: Trailing 以降を分割しない内容", param(message, 1),
                  "hi :there");
    }

    /* ── 空白区切りの寛容な扱い ───────────── */

    {
        Message message;

        /* 連続空白は 1 つの区切りとして扱う */
        ASSERT_TRUE("Parser: Parameter 間の連続空白",
                    Parser::parse("PRIVMSG   #a   :hi", message));
        ASSERT_EQ("Parser: 連続空白でも params 数は同じ",
                  message.params.size(), static_cast<std::size_t>(2));
        ASSERT_EQ("Parser: 連続空白でも params[0]", param(message, 0), "#a");
        ASSERT_EQ("Parser: 連続空白でも params[1]", param(message, 1), "hi");

        /* 先頭の空白を無視する */
        ASSERT_TRUE("Parser: 先頭に空白", Parser::parse("  PASS x", message));
        ASSERT_EQ("Parser: 先頭空白つきの command", message.command, "PASS");
        ASSERT_EQ("Parser: 先頭空白つきの params[0]", param(message, 0), "x");

        /* Prefix と Command の間の連続空白 */
        ASSERT_TRUE("Parser: Prefix 後の連続空白",
                    Parser::parse(":alice   PRIVMSG #a :hi", message));
        ASSERT_EQ("Parser: Prefix 後の連続空白の prefix", message.prefix,
                  "alice");
        ASSERT_EQ("Parser: Prefix 後の連続空白の command", message.command,
                  "PRIVMSG");
    }

    /* ── 空行・不正入力 ───────────────────── */

    {
        Message message;

        ASSERT_FALSE("Parser: 空行", Parser::parse("", message));
        ASSERT_FALSE("Parser: 空白だけの行", Parser::parse("   ", message));
        ASSERT_FALSE("Parser: colon だけ", Parser::parse(":", message));
        ASSERT_FALSE("Parser: Prefix のみで Command なし",
                     Parser::parse(":alice", message));
        ASSERT_FALSE("Parser: Prefix のみ + 末尾空白",
                     Parser::parse(":alice   ", message));
        ASSERT_FALSE("Parser: colon + 空白",
                     Parser::parse(": PRIVMSG", message));
    }

    {
        /* NUL を含む行は破棄する (設計書 06 §12.1) */
        Message message;

        ASSERT_FALSE("Parser: NUL を含む行",
                     Parser::parse(std::string("PASS\0secret", 11), message));
        ASSERT_FALSE("Parser: 先頭が NUL の行",
                     Parser::parse(std::string("\0PASS", 5), message));
        ASSERT_FALSE("Parser: 末尾が NUL の行",
                     Parser::parse(std::string("PASS x\0", 7), message));
    }

    {
        /* 失敗時は out を変更しない */
        Message message;

        Parser::parse("PRIVMSG #a :hi", message);
        ASSERT_EQ("Parser: 成功時の command", message.command, "PRIVMSG");

        ASSERT_FALSE("Parser: 続く空行の解析は失敗",
                     Parser::parse("", message));
        ASSERT_EQ("Parser: 失敗しても out は変更されない", message.command,
                  "PRIVMSG");
        ASSERT_EQ("Parser: 失敗しても params は変更されない",
                  message.params.size(), static_cast<std::size_t>(2));
    }

    {
        /* 成功時は前回の内容を残さない */
        Message message;

        Parser::parse("PRIVMSG #a :hi", message);
        ASSERT_TRUE("Parser: 引数なし Command で上書き",
                    Parser::parse("LIST", message));
        ASSERT_EQ("Parser: 上書き後の params は空", message.params.size(),
                  static_cast<std::size_t>(0));

        Parser::parse(":alice PING x", message);
        ASSERT_EQ("Parser: prefix を設定", message.prefix, "alice");
        Parser::parse("PING x", message);
        ASSERT_EQ("Parser: prefix なしの行で prefix が消える", message.prefix,
                  "");
    }

    /* ── Parameter 数上限 (15) ────────────── */

    {
        Message     message;
        std::string line = "CMD";

        for (int i = 1; i <= 15; ++i)
        {
            std::ostringstream oss;

            oss << " p" << i;
            line += oss.str();
        }

        ASSERT_TRUE("Parser: Parameter 15 個は受理",
                    Parser::parse(line, message));
        ASSERT_EQ("Parser: Parameter 15 個の数", message.params.size(),
                  static_cast<std::size_t>(15));
        ASSERT_EQ("Parser: Parameter 15 個目", param(message, 14), "p15");

        /* 16 個目があれば先頭 15 個を使わず Message 全体を破棄する */
        ASSERT_FALSE("Parser: Parameter 16 個は破棄",
                     Parser::parse(line + " p16", message));

        /* 15 個の Middle + Trailing = 16 個も破棄 */
        ASSERT_FALSE("Parser: Middle 15 + Trailing は破棄",
                     Parser::parse(line + " :trailing", message));
    }

    {
        /* Middle 14 + Trailing = 15 個は受理 */
        Message     message;
        std::string line = "CMD";

        for (int i = 1; i <= 14; ++i)
        {
            std::ostringstream oss;

            oss << " p" << i;
            line += oss.str();
        }
        line += " :trailing text";

        ASSERT_TRUE("Parser: Middle 14 + Trailing は受理",
                    Parser::parse(line, message));
        ASSERT_EQ("Parser: Middle 14 + Trailing の数", message.params.size(),
                  static_cast<std::size_t>(15));
        ASSERT_EQ("Parser: Middle 14 + Trailing の末尾", param(message, 14),
                  "trailing text");
    }

    /* ── CR の扱い ────────────────────────── */

    {
        /* findLine が CRLF を取り除いた後の行を受け取る前提。
           Parameter に紛れ込んだ CR は除去せず保持し、送信時に
           Reply が sanitize する (設計書 06 §17) */
        Message message;

        ASSERT_TRUE("Parser: Trailing 内の CR を含む行も解析する",
                    Parser::parse("PRIVMSG #a :hi\rthere", message));
        ASSERT_EQ("Parser: Trailing 内の CR を保持", param(message, 1),
                  "hi\rthere");
    }

    /* ── 実コマンドの例 ───────────────────── */

    {
        /* parse() は失敗時に out を変更しないため、戻り値を無視すると
           前回の Message を検査してしまう。必ず戻り値を確認する */
        Message message;

        ASSERT_TRUE("Parser: USER を解析できる",
                    Parser::parse("USER alice 0 * :Alice Example", message));
        ASSERT_EQ("Parser: USER の command", message.command, "USER");
        ASSERT_EQ("Parser: USER の params 数", message.params.size(),
                  static_cast<std::size_t>(4));
        ASSERT_EQ("Parser: USER の username", param(message, 0), "alice");
        ASSERT_EQ("Parser: USER の mode", param(message, 1), "0");
        ASSERT_EQ("Parser: USER の unused", param(message, 2), "*");
        ASSERT_EQ("Parser: USER の realname", param(message, 3),
                  "Alice Example");

        ASSERT_TRUE("Parser: JOIN を解析できる",
                    Parser::parse("JOIN #a,#b key1,key2", message));
        ASSERT_EQ("Parser: JOIN の params 数", message.params.size(),
                  static_cast<std::size_t>(2));
        ASSERT_EQ("Parser: JOIN の Channel 一覧 (分割は Handler の責務)",
                  param(message, 0), "#a,#b");
        ASSERT_EQ("Parser: JOIN の Key 一覧", param(message, 1), "key1,key2");

        ASSERT_TRUE("Parser: KICK を解析できる",
                    Parser::parse("KICK #general bob :spamming", message));
        ASSERT_EQ("Parser: KICK の params 数", message.params.size(),
                  static_cast<std::size_t>(3));
        ASSERT_EQ("Parser: KICK の reason", param(message, 2), "spamming");

        ASSERT_TRUE("Parser: MODE を解析できる",
                    Parser::parse("MODE #general +kol secret bob 10",
                                  message));
        ASSERT_EQ("Parser: MODE の params 数", message.params.size(),
                  static_cast<std::size_t>(5));
        ASSERT_EQ("Parser: MODE の Mode 文字列", param(message, 1), "+kol");

        ASSERT_TRUE("Parser: CAP を解析できる",
                    Parser::parse("CAP LS 302", message));
        ASSERT_EQ("Parser: CAP の command", message.command, "CAP");
        ASSERT_EQ("Parser: CAP の subcommand", param(message, 0), "LS");

        ASSERT_TRUE("Parser: QUIT を解析できる", Parser::parse("QUIT", message));
        ASSERT_EQ("Parser: 引数なし QUIT", message.command, "QUIT");
        ASSERT_EQ("Parser: 引数なし QUIT の params", message.params.size(),
                  static_cast<std::size_t>(0));

        ASSERT_TRUE("Parser: PING を解析できる",
                    Parser::parse("PING :token123", message));
        ASSERT_EQ("Parser: PING の token", param(message, 0), "token123");
    }

    /* ── 病的な入力 ───────────────────────── */

    {
        Message message;

        /* Trailing の目印になる colon は最初の 1 つだけ。
           2 つ目以降は本文の一部として保持する */
        ASSERT_TRUE("Parser: Trailing が colon で始まる",
                    Parser::parse("PRIVMSG #a ::hi", message));
        ASSERT_EQ("Parser: Trailing の 2 つ目の colon は本文",
                  param(message, 1), ":hi");

        /* Prefix が colon だけ → Command なしで破棄 */
        ASSERT_FALSE("Parser: colon 2 つだけ", Parser::parse("::", message));
        ASSERT_FALSE("Parser: colon 3 つだけ", Parser::parse(":::", message));

        /* takeToken は space でしか止まらないので prefix に colon を含める */
        ASSERT_TRUE("Parser: prefix に colon を含む",
                    Parser::parse(":a:b PING", message));
        ASSERT_EQ("Parser: prefix 内の colon を保持", message.prefix, "a:b");

        /* 先頭空白のあとに prefix */
        ASSERT_TRUE("Parser: 先頭空白のあとに prefix",
                    Parser::parse("  :alice PING", message));
        ASSERT_EQ("Parser: 先頭空白つき prefix", message.prefix, "alice");
        ASSERT_EQ("Parser: 先頭空白つき prefix の command", message.command,
                  "PING");

        /* CR は Trailing だけでなく通常 Parameter でも保持する */
        ASSERT_TRUE("Parser: 通常 Parameter 内の CR を含む行",
                    Parser::parse("PRIVMSG #a\rb :hi", message));
        ASSERT_EQ("Parser: 通常 Parameter 内の CR を保持", param(message, 0),
                  "#a\rb");

        /* RFC 2812 の区切りは SPACE (0x20) だけ。TAB は区切らないため
           Command の一部になり、Dispatcher からは未知 Command に見える */
        ASSERT_TRUE("Parser: TAB は区切りではない",
                    Parser::parse("PASS\tsecret", message));
        ASSERT_EQ("Parser: TAB は Command の一部になる", message.command,
                  "PASS\tSECRET");
        ASSERT_EQ("Parser: TAB 区切りでは Parameter が生じない",
                  message.params.size(), static_cast<std::size_t>(0));

        /* 行長制限は findLine の責務。Parser は長さで拒否しない */
        ASSERT_TRUE("Parser: 長い Trailing も解析する",
                    Parser::parse("PRIVMSG #a :" + std::string(500, 'x'),
                                  message));
        ASSERT_EQ("Parser: 長い Trailing の長さを保つ",
                  param(message, 1).size(), static_cast<std::size_t>(500));

        /* NUL の検査は解析より前 */
        ASSERT_FALSE("Parser: Trailing 内の NUL も破棄",
                     Parser::parse(std::string("PRIVMSG #a :hi\0there", 20),
                                   message));
    }

    /* ── 定数 ─────────────────────────────── */

    ASSERT_EQ("Parser: MAX_PARAMS は 15", Parser::MAX_PARAMS,
              static_cast<std::size_t>(15));
}
