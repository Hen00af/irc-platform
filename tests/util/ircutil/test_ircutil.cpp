#include "TestRunner.hpp"

#include "prd/util/IrcUtil.hpp"

#include <limits>
#include <sstream>

namespace
{
    /* size_t の桁数は環境によって違うため、境界値はリテラルで書かず
       実際の max から組み立てる */
    std::string toDecimal(std::size_t value)
    {
        std::ostringstream oss;

        oss << value;
        return oss.str();
    }
}

void runIrcUtilTests()
{
    TestRunner::beginSuite("IrcUtil");

    /* ── 定数 ─────────────────────────────── */

    /* 各テストがリテラルを独立に持つと、定数を変えても気付けない */
    ASSERT_EQ("IrcUtil: IRC_MAX_LINE は 512 (CRLF 込み)",
              IrcUtil::IRC_MAX_LINE, static_cast<std::size_t>(512));
    ASSERT_EQ("IrcUtil: IRC_MAX_CONTENT は 510",
              IrcUtil::IRC_MAX_CONTENT, static_cast<std::size_t>(510));
    ASSERT_EQ("IrcUtil: NICKNAME_MAX_LENGTH は 9",
              IrcUtil::NICKNAME_MAX_LENGTH, static_cast<std::size_t>(9));
    ASSERT_EQ("IrcUtil: CHANNEL_NAME_MAX_LENGTH は 50",
              IrcUtil::CHANNEL_NAME_MAX_LENGTH, static_cast<std::size_t>(50));
    ASSERT_EQ("IrcUtil: 本文 + CRLF が行長上限に一致",
              IrcUtil::IRC_MAX_CONTENT + 2, IrcUtil::IRC_MAX_LINE);

    /* ── toUpperAscii ─────────────────────── */

    ASSERT_EQ("toUpperAscii: 小文字を大文字へ",
              IrcUtil::toUpperAscii("privmsg"), "PRIVMSG");
    ASSERT_EQ("toUpperAscii: 大小混在",
              IrcUtil::toUpperAscii("PrIvMsG"), "PRIVMSG");
    ASSERT_EQ("toUpperAscii: 既に大文字",
              IrcUtil::toUpperAscii("JOIN"), "JOIN");
    ASSERT_EQ("toUpperAscii: 空文字", IrcUtil::toUpperAscii(""), "");
    ASSERT_EQ("toUpperAscii: 数字と記号は不変",
              IrcUtil::toUpperAscii("123-_[]"), "123-_[]");
    /* case folding とは違い、[ ] \ ^ は変換しない */
    ASSERT_EQ("toUpperAscii: 特殊文字は不変",
              IrcUtil::toUpperAscii("{|}~"), "{|}~");
    ASSERT_EQ("toUpperAscii: 非ASCIIは不変",
              IrcUtil::toUpperAscii("\xC3\xA9"), "\xC3\xA9");

    /* ── ircCaseFold ──────────────────────── */

    ASSERT_EQ("ircCaseFold: A-Z を a-z へ",
              IrcUtil::ircCaseFold("ALICE"), "alice");
    ASSERT_EQ("ircCaseFold: 既に小文字",
              IrcUtil::ircCaseFold("alice"), "alice");
    ASSERT_EQ("ircCaseFold: 空文字", IrcUtil::ircCaseFold(""), "");
    /* RFC 2812 の IRC case mapping: [ ] \ ^ → { } | ~ */
    ASSERT_EQ("ircCaseFold: [ を { へ", IrcUtil::ircCaseFold("["), "{");
    ASSERT_EQ("ircCaseFold: ] を } へ", IrcUtil::ircCaseFold("]"), "}");
    ASSERT_EQ("ircCaseFold: バックスラッシュを | へ",
              IrcUtil::ircCaseFold("\\"), "|");
    ASSERT_EQ("ircCaseFold: ^ を ~ へ", IrcUtil::ircCaseFold("^"), "~");
    ASSERT_EQ("ircCaseFold: 特殊文字をまとめて",
              IrcUtil::ircCaseFold("[]\\^"), "{}|~");
    /* 変換先の文字は再変換しない (冪等性)。期待値は実装ではなく
       仕様から導いたリテラルで固定する */
    ASSERT_EQ("ircCaseFold: { } | ~ は不変",
              IrcUtil::ircCaseFold("{}|~"), "{}|~");
    ASSERT_EQ("ircCaseFold: Nickname の実例",
              IrcUtil::ircCaseFold("Alice[1]"), "alice{1}");
    ASSERT_EQ("ircCaseFold: 変換結果を再変換しても不変 (冪等)",
              IrcUtil::ircCaseFold("alice{1}"), "alice{1}");
    ASSERT_EQ("ircCaseFold: 数字と - は不変",
              IrcUtil::ircCaseFold("a1-b"), "a1-b");
    /* _ と ` は case mapping の対象外 */
    ASSERT_EQ("ircCaseFold: _ と ` は不変", IrcUtil::ircCaseFold("_`"), "_`");
    /* 大文字小文字だけが違う Nickname は同じ Key になる */
    ASSERT_EQ("ircCaseFold: 大小違いが同じ Key になる",
              IrcUtil::ircCaseFold("ALICE"), IrcUtil::ircCaseFold("alice"));
    ASSERT_EQ("ircCaseFold: [ と { が同じ Key になる",
              IrcUtil::ircCaseFold("a[b"), IrcUtil::ircCaseFold("a{b"));

    /* ── normalizeChannelName ─────────────── */

    ASSERT_EQ("normalizeChannelName: 大文字を小文字へ",
              IrcUtil::normalizeChannelName("#General"), "#general");
    ASSERT_EQ("normalizeChannelName: 特殊文字も case mapping",
              IrcUtil::normalizeChannelName("#Foo[Bar]"), "#foo{bar}");
    ASSERT_EQ("normalizeChannelName: # は保持",
              IrcUtil::normalizeChannelName("#a"), "#a");
    ASSERT_EQ("normalizeChannelName: 大小違いが同じ Key になる",
              IrcUtil::normalizeChannelName("#GENERAL"),
              IrcUtil::normalizeChannelName("#general"));

    /* ── isValidNickname ──────────────────── */

    ASSERT_TRUE("isValidNickname: 通常", IrcUtil::isValidNickname("alice"));
    ASSERT_TRUE("isValidNickname: 1 文字", IrcUtil::isValidNickname("a"));
    ASSERT_TRUE("isValidNickname: 9 文字 (上限)",
                IrcUtil::isValidNickname("abcdefghi"));
    ASSERT_FALSE("isValidNickname: 10 文字 (上限超過)",
                 IrcUtil::isValidNickname("abcdefghij"));
    ASSERT_FALSE("isValidNickname: 空文字", IrcUtil::isValidNickname(""));
    /* 先頭は英字か特殊文字 []\`_^{|} のみ */
    ASSERT_TRUE("isValidNickname: 先頭 [", IrcUtil::isValidNickname("[a"));
    ASSERT_TRUE("isValidNickname: 先頭 ]", IrcUtil::isValidNickname("]a"));
    ASSERT_TRUE("isValidNickname: 先頭 バックスラッシュ",
                IrcUtil::isValidNickname("\\a"));
    ASSERT_TRUE("isValidNickname: 先頭 `", IrcUtil::isValidNickname("`a"));
    ASSERT_TRUE("isValidNickname: 先頭 _", IrcUtil::isValidNickname("_a"));
    ASSERT_TRUE("isValidNickname: 先頭 ^", IrcUtil::isValidNickname("^a"));
    ASSERT_TRUE("isValidNickname: 先頭 {", IrcUtil::isValidNickname("{a"));
    ASSERT_TRUE("isValidNickname: 先頭 |", IrcUtil::isValidNickname("|a"));
    ASSERT_TRUE("isValidNickname: 先頭 }", IrcUtil::isValidNickname("}a"));
    ASSERT_FALSE("isValidNickname: 先頭が数字",
                 IrcUtil::isValidNickname("1alice"));
    ASSERT_FALSE("isValidNickname: 先頭が -", IrcUtil::isValidNickname("-a"));
    /* 2 文字目以降は数字と - も許可 */
    ASSERT_TRUE("isValidNickname: 途中に数字", IrcUtil::isValidNickname("a1"));
    ASSERT_TRUE("isValidNickname: 途中に -", IrcUtil::isValidNickname("a-b"));
    ASSERT_TRUE("isValidNickname: 途中に特殊文字",
                IrcUtil::isValidNickname("a[1]"));
    ASSERT_TRUE("isValidNickname: 英数記号の混在",
                IrcUtil::isValidNickname("a1-b_c"));
    /* 禁止文字 */
    ASSERT_FALSE("isValidNickname: 空白を含む",
                 IrcUtil::isValidNickname("a b"));
    ASSERT_FALSE("isValidNickname: comma を含む",
                 IrcUtil::isValidNickname("a,b"));
    ASSERT_FALSE("isValidNickname: colon を含む",
                 IrcUtil::isValidNickname("a:b"));
    ASSERT_FALSE("isValidNickname: CR を含む",
                 IrcUtil::isValidNickname("a\rb"));
    ASSERT_FALSE("isValidNickname: LF を含む",
                 IrcUtil::isValidNickname("a\nb"));
    ASSERT_FALSE("isValidNickname: NUL を含む",
                 IrcUtil::isValidNickname(std::string("a\0b", 3)));
    ASSERT_FALSE("isValidNickname: 先頭が空白",
                 IrcUtil::isValidNickname(" a"));
    ASSERT_FALSE("isValidNickname: # を含む", IrcUtil::isValidNickname("a#b"));
    ASSERT_FALSE("isValidNickname: @ を含む", IrcUtil::isValidNickname("a@b"));
    ASSERT_FALSE("isValidNickname: ! を含む", IrcUtil::isValidNickname("a!b"));
    ASSERT_FALSE("isValidNickname: . を含む", IrcUtil::isValidNickname("a.b"));
    ASSERT_FALSE("isValidNickname: 先頭 @", IrcUtil::isValidNickname("@a"));
    ASSERT_FALSE("isValidNickname: 先頭 #", IrcUtil::isValidNickname("#a"));

    /* ~ (0x7E) は RFC 2812 の special (%x5B-60 / %x7B-7D) に含まれない。
       一方 ircCaseFold は ^ を ~ へ変換するため、「妥当な Nickname を
       fold した結果」が「Nickname としては不正な文字列」になる。
       fold 済みの検索 Key を検証へ回すと誤判定するので、その非対称性を
       ここで固定しておく */
    ASSERT_FALSE("isValidNickname: 先頭 ~ は不可 (special ではない)",
                 IrcUtil::isValidNickname("~a"));
    ASSERT_FALSE("isValidNickname: 途中の ~ も不可",
                 IrcUtil::isValidNickname("a~b"));
    ASSERT_TRUE("isValidNickname: ^a は妥当", IrcUtil::isValidNickname("^a"));
    ASSERT_FALSE("isValidNickname: ^a の fold 結果 ~a は Nickname として不正",
                 IrcUtil::isValidNickname(IrcUtil::ircCaseFold("^a")));

    /* ── isValidChannelName ───────────────── */

    ASSERT_TRUE("isValidChannelName: 通常",
                IrcUtil::isValidChannelName("#general"));
    ASSERT_TRUE("isValidChannelName: 2 文字 (下限)",
                IrcUtil::isValidChannelName("#a"));
    ASSERT_FALSE("isValidChannelName: # だけ (1 文字)",
                 IrcUtil::isValidChannelName("#"));
    ASSERT_FALSE("isValidChannelName: 空文字",
                 IrcUtil::isValidChannelName(""));
    ASSERT_FALSE("isValidChannelName: # で始まらない",
                 IrcUtil::isValidChannelName("general"));
    ASSERT_FALSE("isValidChannelName: & で始まる (Mandatory 対象外)",
                 IrcUtil::isValidChannelName("&general"));
    ASSERT_TRUE("isValidChannelName: 50 文字 (上限)",
                IrcUtil::isValidChannelName("#" + std::string(49, 'a')));
    ASSERT_FALSE("isValidChannelName: 51 文字 (上限超過)",
                 IrcUtil::isValidChannelName("#" + std::string(50, 'a')));
    /* 禁止文字 */
    ASSERT_FALSE("isValidChannelName: 空白を含む",
                 IrcUtil::isValidChannelName("#a b"));
    ASSERT_FALSE("isValidChannelName: comma を含む",
                 IrcUtil::isValidChannelName("#a,b"));
    ASSERT_FALSE("isValidChannelName: colon を含む",
                 IrcUtil::isValidChannelName("#a:b"));
    ASSERT_FALSE("isValidChannelName: BELL を含む",
                 IrcUtil::isValidChannelName("#a\ab"));
    ASSERT_FALSE("isValidChannelName: CR を含む",
                 IrcUtil::isValidChannelName("#a\rb"));
    ASSERT_FALSE("isValidChannelName: LF を含む",
                 IrcUtil::isValidChannelName("#a\nb"));
    ASSERT_FALSE("isValidChannelName: NUL を含む",
                 IrcUtil::isValidChannelName(std::string("#a\0b", 4)));
    /* 上記以外の記号は許可する */
    ASSERT_TRUE("isValidChannelName: # を 2 つ",
                IrcUtil::isValidChannelName("##a"));
    ASSERT_TRUE("isValidChannelName: 大文字を含む (正規化は別責務)",
                IrcUtil::isValidChannelName("#General"));
    ASSERT_TRUE("isValidChannelName: 記号を含む",
                IrcUtil::isValidChannelName("#a-b_c.d"));

    /* ── parsePositiveSize ────────────────── */

    {
        std::size_t result = 0;

        ASSERT_TRUE("parsePositiveSize: 1", IrcUtil::parsePositiveSize("1",
                                                                       result));
        ASSERT_EQ("parsePositiveSize: 1 の値", result,
                  static_cast<std::size_t>(1));
        ASSERT_TRUE("parsePositiveSize: 10",
                    IrcUtil::parsePositiveSize("10", result));
        ASSERT_EQ("parsePositiveSize: 10 の値", result,
                  static_cast<std::size_t>(10));
        ASSERT_TRUE("parsePositiveSize: 100000",
                    IrcUtil::parsePositiveSize("100000", result));
        ASSERT_EQ("parsePositiveSize: 100000 の値", result,
                  static_cast<std::size_t>(100000));
        /* 先頭 0 は許可し、値として解釈する */
        ASSERT_TRUE("parsePositiveSize: 先頭 0 つき",
                    IrcUtil::parsePositiveSize("007", result));
        ASSERT_EQ("parsePositiveSize: 先頭 0 つきの値", result,
                  static_cast<std::size_t>(7));

        /* 0 は許可しない (設計書 05 §13) */
        ASSERT_FALSE("parsePositiveSize: 0 は不可",
                     IrcUtil::parsePositiveSize("0", result));
        ASSERT_FALSE("parsePositiveSize: 000 も不可",
                     IrcUtil::parsePositiveSize("000", result));
        /* 符号つきは許可しない */
        ASSERT_FALSE("parsePositiveSize: 先頭 + は不可",
                     IrcUtil::parsePositiveSize("+5", result));
        ASSERT_FALSE("parsePositiveSize: 先頭 - は不可",
                     IrcUtil::parsePositiveSize("-5", result));
        /* 数字以外を含むものは許可しない */
        ASSERT_FALSE("parsePositiveSize: 空文字",
                     IrcUtil::parsePositiveSize("", result));
        ASSERT_FALSE("parsePositiveSize: 英字混じり",
                     IrcUtil::parsePositiveSize("1a", result));
        ASSERT_FALSE("parsePositiveSize: 小数点",
                     IrcUtil::parsePositiveSize("1.5", result));
        ASSERT_FALSE("parsePositiveSize: 空白混じり",
                     IrcUtil::parsePositiveSize("1 0", result));
        ASSERT_FALSE("parsePositiveSize: 前後の空白",
                     IrcUtil::parsePositiveSize(" 1", result));

        /* 100000 の上限は MODE +l Handler の責務であり、この関数は
           課さない (設計書 02 §10 の署名は汎用)。それを固定する */
        ASSERT_TRUE("parsePositiveSize: 100001 も受理する (上限は Handler 責務)",
                    IrcUtil::parsePositiveSize("100001", result));
        ASSERT_EQ("parsePositiveSize: 100001 の値", result,
                  static_cast<std::size_t>(100001));

        /* NUL は数字ではない */
        ASSERT_FALSE("parsePositiveSize: 埋め込み NUL",
                     IrcUtil::parsePositiveSize(std::string("1\0" "2", 3),
                                                result));
    }

    /* overflow の境界。桁数から遠い値だけでは、素朴な
       (parsed > maximum / 10) のような誤ったガードでも通ってしまうため、
       size_t の max ちょうどで検証する */
    {
        const std::size_t maximum = std::numeric_limits<std::size_t>::max();
        std::size_t       result  = 0;

        ASSERT_TRUE("parsePositiveSize: size_t の max ちょうどは受理",
                    IrcUtil::parsePositiveSize(toDecimal(maximum), result));
        ASSERT_EQ("parsePositiveSize: max の値", result, maximum);

        ASSERT_TRUE("parsePositiveSize: max - 1 は受理",
                    IrcUtil::parsePositiveSize(toDecimal(maximum - 1),
                                               result));
        ASSERT_EQ("parsePositiveSize: max - 1 の値", result, maximum - 1);

        /* size_t の max は 32bit / 64bit とも末尾が 5。末尾を 6〜9 にすれば
           繰り上がりなしで max + 1 〜 max + 4 になる */
        ASSERT_TRUE("parsePositiveSize: max の末尾は 5 (テストの前提)",
                    toDecimal(maximum)[toDecimal(maximum).size() - 1] == '5');

        /* max + 1 〜 max + 4 をすべて試す。
           max + 1 だけでは overflow ガードの検証として不十分である。
           ガードが digit を無視した (parsed > maximum / 10) だった場合、
           max + 1 は桁上がりの結果がちょうど 0 になり、別の「0 を拒否」の
           検査に引っかかって偶然 false になる。一方 max + 2 以降は 1〜3 へ
           ラップするため、誤ったガードだと「成功」して通ってしまう */
        for (char last = '6'; last <= '9'; ++last)
        {
            std::string over = toDecimal(maximum);

            over[over.size() - 1] = last;
            ASSERT_FALSE("parsePositiveSize: max を超える値は拒否 (末尾 "
                             + std::string(1, last) + ")",
                         IrcUtil::parsePositiveSize(over, result));
        }

        /* max を 10 倍した桁数 */
        ASSERT_FALSE("parsePositiveSize: max の 10 倍は拒否",
                     IrcUtil::parsePositiveSize(toDecimal(maximum) + "0",
                                                result));
        ASSERT_FALSE("parsePositiveSize: 極端な桁数",
                     IrcUtil::parsePositiveSize(std::string(100, '9'),
                                                result));

        /* 先頭 0 は何桁あっても overflow しない */
        ASSERT_TRUE("parsePositiveSize: 先頭 0 が 100 個でも overflow しない",
                    IrcUtil::parsePositiveSize(std::string(100, '0') + "1",
                                               result));
        ASSERT_EQ("parsePositiveSize: 先頭 0 が 100 個の値", result,
                  static_cast<std::size_t>(1));
    }

    /* 失敗時は result を書き換えない。3 つある失敗経路すべてで確認する */
    {
        std::size_t       result  = 12345;
        const std::size_t maximum = std::numeric_limits<std::size_t>::max();

        ASSERT_FALSE("parsePositiveSize: 非数字で失敗",
                     IrcUtil::parsePositiveSize("abc", result));
        ASSERT_EQ("parsePositiveSize: 非数字の失敗で result は不変", result,
                  static_cast<std::size_t>(12345));

        ASSERT_FALSE("parsePositiveSize: 0 で失敗",
                     IrcUtil::parsePositiveSize("0", result));
        ASSERT_EQ("parsePositiveSize: 0 の失敗で result は不変", result,
                  static_cast<std::size_t>(12345));

        ASSERT_FALSE("parsePositiveSize: overflow で失敗",
                     IrcUtil::parsePositiveSize(toDecimal(maximum) + "0",
                                                result));
        ASSERT_EQ("parsePositiveSize: overflow の失敗で result は不変", result,
                  static_cast<std::size_t>(12345));

        ASSERT_FALSE("parsePositiveSize: 空文字で失敗",
                     IrcUtil::parsePositiveSize("", result));
        ASSERT_EQ("parsePositiveSize: 空文字の失敗で result は不変", result,
                  static_cast<std::size_t>(12345));
    }

    /* ── fitsIrcLine ──────────────────────── */

    /* リテラルではなく定数から組み立てる。定数を変えたときに
       追随できないテストにしないため */
    ASSERT_TRUE("fitsIrcLine: 空文字", IrcUtil::fitsIrcLine(""));
    ASSERT_TRUE("fitsIrcLine: 本文が上限ちょうど (CRLF 込みで 512)",
                IrcUtil::fitsIrcLine(
                    std::string(IrcUtil::IRC_MAX_CONTENT, 'a')));
    ASSERT_FALSE("fitsIrcLine: 本文が上限 + 1 は超過",
                 IrcUtil::fitsIrcLine(
                     std::string(IrcUtil::IRC_MAX_CONTENT + 1, 'a')));
    ASSERT_TRUE("fitsIrcLine: 本文が上限 - 1",
                IrcUtil::fitsIrcLine(
                    std::string(IrcUtil::IRC_MAX_CONTENT - 1, 'a')));

    /* ── isSafeToken ──────────────────────── */

    ASSERT_TRUE("isSafeToken: 通常の Nickname", IrcUtil::isSafeToken("alice"));
    ASSERT_TRUE("isSafeToken: Client Prefix",
                IrcUtil::isSafeToken("alice!u@127.0.0.1"));
    ASSERT_TRUE("isSafeToken: Server 名",
                IrcUtil::isSafeToken("ircserv.local"));
    ASSERT_TRUE("isSafeToken: Command", IrcUtil::isSafeToken("PRIVMSG"));
    /* token 途中の colon は IPv6 hostname で正当 */
    ASSERT_TRUE("isSafeToken: 途中の colon は許可 (IPv6)",
                IrcUtil::isSafeToken("alice!u@::1"));
    ASSERT_TRUE("isSafeToken: 途中の colon だけの token",
                IrcUtil::isSafeToken("a:b"));

    ASSERT_FALSE("isSafeToken: 空文字", IrcUtil::isSafeToken(""));
    /* 先頭 colon は Trailing の目印と誤解される */
    ASSERT_FALSE("isSafeToken: 先頭 colon", IrcUtil::isSafeToken(":x"));
    ASSERT_FALSE("isSafeToken: colon だけ", IrcUtil::isSafeToken(":"));
    ASSERT_FALSE("isSafeToken: 空白を含む", IrcUtil::isSafeToken("a b"));
    ASSERT_FALSE("isSafeToken: 先頭が空白", IrcUtil::isSafeToken(" a"));
    ASSERT_FALSE("isSafeToken: 末尾が空白", IrcUtil::isSafeToken("a "));
    ASSERT_FALSE("isSafeToken: CR を含む", IrcUtil::isSafeToken("a\rb"));
    ASSERT_FALSE("isSafeToken: LF を含む", IrcUtil::isSafeToken("a\nb"));
    ASSERT_FALSE("isSafeToken: NUL を含む",
                 IrcUtil::isSafeToken(std::string("a\0b", 3)));
    ASSERT_FALSE("isSafeToken: CRLF injection を狙った Nickname",
                 IrcUtil::isSafeToken("bad\r\nJOIN #x"));
    /* Server 起動時の serverName 検査に使う */
    ASSERT_FALSE("isSafeToken: 空白入りの serverName は不正",
                 IrcUtil::isSafeToken("irc serv"));

    /* ── sanitizeMessageText ──────────────── */

    ASSERT_EQ("sanitizeMessageText: 変換不要", IrcUtil::sanitizeMessageText(
                                                   "hello world"),
              "hello world");
    ASSERT_EQ("sanitizeMessageText: 空文字", IrcUtil::sanitizeMessageText(""),
              "");
    ASSERT_EQ("sanitizeMessageText: CR を除去",
              IrcUtil::sanitizeMessageText("a\rb"), "ab");
    ASSERT_EQ("sanitizeMessageText: LF を除去",
              IrcUtil::sanitizeMessageText("a\nb"), "ab");
    ASSERT_EQ("sanitizeMessageText: CRLF を除去",
              IrcUtil::sanitizeMessageText("a\r\nb"), "ab");
    ASSERT_EQ("sanitizeMessageText: NUL を除去",
              IrcUtil::sanitizeMessageText(std::string("a\0b", 3)), "ab");
    ASSERT_EQ("sanitizeMessageText: 連続する制御文字を除去",
              IrcUtil::sanitizeMessageText("a\r\n\r\nb"), "ab");
    ASSERT_EQ("sanitizeMessageText: 先頭と末尾の制御文字を除去",
              IrcUtil::sanitizeMessageText("\r\nabc\r\n"), "abc");
    /* 空白や colon など、他の文字は保持する */
    ASSERT_EQ("sanitizeMessageText: 空白と colon は保持",
              IrcUtil::sanitizeMessageText("#a :hi there"), "#a :hi there");
    ASSERT_EQ("sanitizeMessageText: TAB は保持",
              IrcUtil::sanitizeMessageText("a\tb"), "a\tb");
    ASSERT_EQ("sanitizeMessageText: 非 ASCII は保持",
              IrcUtil::sanitizeMessageText("\xC3\xA9"), "\xC3\xA9");
    /* 除去するのは CR / LF / NUL の 3 つだけ。「制御文字をすべて落とす」
       実装への退行を検出する。CTCP の \x01 や BELL \x07 は保持する
       (設計書 06 §17「それ以外の文字は保持する」)。
       BELL は isValidChannelName では拒否されるが、本文としては正当 */
    ASSERT_EQ("sanitizeMessageText: CTCP の \\x01 は保持",
              IrcUtil::sanitizeMessageText("\x01" "ACTION waves\x01"),
              "\x01" "ACTION waves\x01");
    ASSERT_EQ("sanitizeMessageText: BELL は保持",
              IrcUtil::sanitizeMessageText("a\ab"), "a\ab");
    ASSERT_EQ("sanitizeMessageText: ESC は保持 (色コード)",
              IrcUtil::sanitizeMessageText("\x03" "04red"), "\x03" "04red");
    /* 高位バイトと制御文字が混在しても、高位バイトだけ残る
       (char の符号有無に依存しないこと) */
    ASSERT_EQ("sanitizeMessageText: 高位バイトと制御文字の混在",
              IrcUtil::sanitizeMessageText(std::string("\x80\r\xFF", 3)),
              std::string("\x80\xFF", 2));
    /* すべて制御文字なら空文字になる */
    ASSERT_EQ("sanitizeMessageText: 制御文字だけなら空文字",
              IrcUtil::sanitizeMessageText(std::string("\r\n\0", 3)), "");

    /* CRLF injection の実例 (設計書 06 §21) */
    ASSERT_EQ("sanitizeMessageText: 注入された行を 1 行へ潰す",
              IrcUtil::sanitizeMessageText("hi\r\nKICK #a bob"),
              "hiKICK #a bob");
    {
        /* 結果に CR / LF / NUL が 1 つも残らない */
        std::string cleaned =
            IrcUtil::sanitizeMessageText(std::string("a\rb\nc\0d\r\ne", 10));

        ASSERT_EQ("sanitizeMessageText: 制御文字をすべて除去した結果", cleaned,
                  "abcde");
    }

    /* ── splitCommaList ───────────────────── */

    {
        std::vector<std::string> parts = IrcUtil::splitCommaList("#a,#b,#c");

        ASSERT_EQ("splitCommaList: 3 要素", parts.size(),
                  static_cast<std::size_t>(3));
        if (parts.size() == 3)
        {
            ASSERT_EQ("splitCommaList: 1 番目", parts[0], "#a");
            ASSERT_EQ("splitCommaList: 2 番目", parts[1], "#b");
            ASSERT_EQ("splitCommaList: 3 番目", parts[2], "#c");
        }
    }

    {
        std::vector<std::string> parts = IrcUtil::splitCommaList("#a");

        ASSERT_EQ("splitCommaList: comma なしは 1 要素", parts.size(),
                  static_cast<std::size_t>(1));
        if (parts.size() == 1)
            ASSERT_EQ("splitCommaList: comma なしの内容", parts[0], "#a");
    }

    ASSERT_EQ("splitCommaList: 空文字は空の vector",
              IrcUtil::splitCommaList("").size(), static_cast<std::size_t>(0));

    {
        /* JOIN の Channel と Key を位置で対応させるため空要素も残す。
           "#a,#b" と ",key2" で key2 が #b に対応する必要がある */
        std::vector<std::string> parts = IrcUtil::splitCommaList("#a,,#b");

        ASSERT_EQ("splitCommaList: 空要素も残す", parts.size(),
                  static_cast<std::size_t>(3));
        if (parts.size() == 3)
        {
            ASSERT_EQ("splitCommaList: 空要素の前", parts[0], "#a");
            ASSERT_EQ("splitCommaList: 空要素は空文字", parts[1], "");
            ASSERT_EQ("splitCommaList: 空要素の後", parts[2], "#b");
        }
    }

    {
        std::vector<std::string> parts = IrcUtil::splitCommaList("#a,");

        ASSERT_EQ("splitCommaList: 末尾 comma は空要素を生む", parts.size(),
                  static_cast<std::size_t>(2));
        if (parts.size() == 2)
            ASSERT_EQ("splitCommaList: 末尾の空要素", parts[1], "");
    }

    {
        std::vector<std::string> parts = IrcUtil::splitCommaList(",#a");

        ASSERT_EQ("splitCommaList: 先頭 comma は空要素を生む", parts.size(),
                  static_cast<std::size_t>(2));
        if (parts.size() == 2)
        {
            ASSERT_EQ("splitCommaList: 先頭の空要素", parts[0], "");
            ASSERT_EQ("splitCommaList: 先頭 comma の後", parts[1], "#a");
        }
    }

    {
        std::vector<std::string> parts = IrcUtil::splitCommaList(",");

        ASSERT_EQ("splitCommaList: comma だけなら空要素 2 つ", parts.size(),
                  static_cast<std::size_t>(2));
        if (parts.size() == 2)
        {
            ASSERT_EQ("splitCommaList: comma だけの 1 番目", parts[0], "");
            ASSERT_EQ("splitCommaList: comma だけの 2 番目", parts[1], "");
        }
    }

    {
        /* 要素数は常に comma 数 + 1。空文字だけが例外で 0 個を返す */
        std::vector<std::string> parts = IrcUtil::splitCommaList(",,");

        ASSERT_EQ("splitCommaList: comma 2 つなら空要素 3 つ", parts.size(),
                  static_cast<std::size_t>(3));
    }

    {
        /* 前後の空白は落とさない。落とすと不正な Channel 名を
           見逃してしまう */
        std::vector<std::string> parts = IrcUtil::splitCommaList("#a, #b");

        ASSERT_EQ("splitCommaList: 空白を trim しない要素数", parts.size(),
                  static_cast<std::size_t>(2));
        if (parts.size() == 2)
        {
            ASSERT_EQ("splitCommaList: trim しない 1 番目", parts[0], "#a");
            ASSERT_EQ("splitCommaList: 空白を trim しない", parts[1], " #b");
        }
    }

    {
        /* JOIN の Channel と Key の位置対応。
           空要素を落とす実装だと、"#a,#b" と ",k2" で k2 が #a に
           対応してしまい、#b が Key なし扱いで誤って 475 になる */
        std::vector<std::string> channels =
            IrcUtil::splitCommaList("#a,#b,#c");
        std::vector<std::string> keys = IrcUtil::splitCommaList("k1,,k3");

        ASSERT_EQ("splitCommaList: Channel 数と Key 数が一致", channels.size(),
                  keys.size());
        if (keys.size() == 3)
        {
            ASSERT_EQ("splitCommaList: #a の Key", keys[0], "k1");
            ASSERT_EQ("splitCommaList: #b は Key なし", keys[1], "");
            ASSERT_EQ("splitCommaList: #c の Key", keys[2], "k3");
        }
    }

    {
        /* Key 一覧が Channel 一覧より短い場合。padding しないので、
           JOIN Handler 側が i < keys.size() で境界確認する必要がある */
        std::vector<std::string> channels =
            IrcUtil::splitCommaList("#a,#b,#c");
        std::vector<std::string> keys = IrcUtil::splitCommaList("k1");

        ASSERT_EQ("splitCommaList: Channel は 3 個", channels.size(),
                  static_cast<std::size_t>(3));
        ASSERT_EQ("splitCommaList: Key は 1 個のまま padding しない",
                  keys.size(), static_cast<std::size_t>(1));
        ASSERT_TRUE("splitCommaList: Key 一覧が短くなり得る",
                    keys.size() < channels.size());
    }

    {
        /* Key Parameter が無い JOIN。空文字なら空の vector になり、
           どの Channel も Key なしとして扱える */
        std::vector<std::string> keys = IrcUtil::splitCommaList("");

        ASSERT_EQ("splitCommaList: Key Parameter なしは 0 個", keys.size(),
                  static_cast<std::size_t>(0));
    }

    /* ── ASCII 全域の網羅 ─────────────────── */

    /* 個別の spot check では「許可集合が閉じているか」を示せない。
       期待値は仕様から数えて導く */
    {
        /* RFC 2812: nickname = ( letter / special ) *8( letter / digit /
                                special / "-" )
           letter = A-Z a-z (52), special = []\`_^{|} (9) → 先頭は 61 種 */
        int acceptedFirst = 0;
        int acceptedRest  = 0;

        for (int ch = 0; ch < 128; ++ch)
        {
            std::string first(1, static_cast<char>(ch));
            std::string rest = "a" + std::string(1, static_cast<char>(ch));

            if (IrcUtil::isValidNickname(first))
                ++acceptedFirst;
            if (IrcUtil::isValidNickname(rest))
                ++acceptedRest;
        }
        ASSERT_EQ("isValidNickname: 先頭に許可される ASCII は 61 種 (52+9)",
                  acceptedFirst, 61);
        /* 2 文字目以降は letter 52 + digit 10 + special 9 + '-' 1 = 72 種 */
        ASSERT_EQ("isValidNickname: 2 文字目に許可される ASCII は 72 種",
                  acceptedRest, 72);
    }

    {
        /* RFC 2812 の case mapping で変化するのは
           A-Z (26) + [ ] \ ^ (4) = 30 種だけ */
        int changed = 0;

        for (int ch = 0; ch < 128; ++ch)
        {
            std::string one(1, static_cast<char>(ch));

            if (IrcUtil::ircCaseFold(one) != one)
                ++changed;
        }
        ASSERT_EQ("ircCaseFold: 変換される ASCII は 30 種 (26+4)", changed,
                  30);
    }

    {
        /* 非 ASCII (0x80-0xFF) は変換しない。char が signed でも
           unsigned でも同じ結果になることを示す */
        int foldChanged  = 0;
        int upperChanged = 0;

        for (int ch = 128; ch < 256; ++ch)
        {
            std::string one(1, static_cast<char>(ch));

            if (IrcUtil::ircCaseFold(one) != one)
                ++foldChanged;
            if (IrcUtil::toUpperAscii(one) != one)
                ++upperChanged;
        }
        ASSERT_EQ("ircCaseFold: 非 ASCII は変換しない", foldChanged, 0);
        ASSERT_EQ("toUpperAscii: 非 ASCII は変換しない", upperChanged, 0);
    }

    {
        /* toUpperAscii が変えるのは a-z の 26 種だけ */
        int changed = 0;

        for (int ch = 0; ch < 128; ++ch)
        {
            std::string one(1, static_cast<char>(ch));

            if (IrcUtil::toUpperAscii(one) != one)
                ++changed;
        }
        ASSERT_EQ("toUpperAscii: 変換される ASCII は 26 種", changed, 26);
    }
}
