#include "TestRunner.hpp"

#include "prd/util/BufferUtil.hpp"
#include "prd/util/IrcUtil.hpp"

namespace
{
    std::string statusName(BufferUtil::LineStatus status)
    {
        switch (status)
        {
        case BufferUtil::LINE_INCOMPLETE:
            return "LINE_INCOMPLETE";
        case BufferUtil::LINE_EXTRACTED:
            return "LINE_EXTRACTED";
        case BufferUtil::LINE_TOO_LONG:
            return "LINE_TOO_LONG";
        }
        return "UNKNOWN";
    }

    /* findLine を呼び、状態名を返す。out / consumed も受け取る */
    std::string findLineStatus(const std::string &buffer,
                               std::string       &out,
                               std::size_t       &consumed)
    {
        return statusName(BufferUtil::findLine(buffer, out, consumed));
    }
}

void runBufferUtilTests()
{
    TestRunner::beginSuite("BufferUtil");

    /* ── hasCompleteLine ──────────────────── */

    ASSERT_FALSE("hasCompleteLine: 空バッファ", BufferUtil::hasCompleteLine(""));
    ASSERT_FALSE("hasCompleteLine: LF なし",
                 BufferUtil::hasCompleteLine("PASS secret"));
    ASSERT_FALSE("hasCompleteLine: CR のみ",
                 BufferUtil::hasCompleteLine("PASS secret\r"));
    ASSERT_TRUE("hasCompleteLine: LF あり",
                BufferUtil::hasCompleteLine("PASS secret\n"));
    ASSERT_TRUE("hasCompleteLine: CRLF あり",
                BufferUtil::hasCompleteLine("PASS secret\r\n"));
    ASSERT_TRUE("hasCompleteLine: 空行", BufferUtil::hasCompleteLine("\n"));
    ASSERT_TRUE("hasCompleteLine: 途中に LF、末尾は未完成",
                BufferUtil::hasCompleteLine("A\r\nB"));

    /* ── findLine ─────────────────────────── */

    {
        std::string line;
        std::size_t consumed;

        /* 未完成 */
        ASSERT_EQ("findLine: 空バッファ",
                  findLineStatus("", line, consumed), "LINE_INCOMPLETE");
        ASSERT_EQ("findLine: 空バッファは consumed = 0",
                  consumed, static_cast<std::size_t>(0));
        ASSERT_EQ("findLine: LF なしは未完成",
                  findLineStatus("PASS secret", line, consumed),
                  "LINE_INCOMPLETE");
        ASSERT_EQ("findLine: CR だけでは未完成",
                  findLineStatus("PASS secret\r", line, consumed),
                  "LINE_INCOMPLETE");

        /* CRLF 終端 */
        ASSERT_EQ("findLine: CRLF 終端",
                  findLineStatus("PASS secret\r\n", line, consumed),
                  "LINE_EXTRACTED");
        ASSERT_EQ("findLine: CRLF 終端の本文", line, "PASS secret");
        ASSERT_EQ("findLine: CRLF 終端の consumed",
                  consumed, static_cast<std::size_t>(13));

        /* LF のみの終端も受理する */
        ASSERT_EQ("findLine: LF のみ終端",
                  findLineStatus("PASS secret\n", line, consumed),
                  "LINE_EXTRACTED");
        ASSERT_EQ("findLine: LF のみ終端の本文", line, "PASS secret");
        ASSERT_EQ("findLine: LF のみ終端の consumed",
                  consumed, static_cast<std::size_t>(12));

        /* 空行 */
        ASSERT_EQ("findLine: 空行 (LF のみ)",
                  findLineStatus("\n", line, consumed), "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 空行の本文は空文字", line, "");
        ASSERT_EQ("findLine: 空行の consumed",
                  consumed, static_cast<std::size_t>(1));
        ASSERT_EQ("findLine: 空行 (CRLF)",
                  findLineStatus("\r\n", line, consumed), "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 空行 (CRLF) の本文は空文字", line, "");
        ASSERT_EQ("findLine: 空行 (CRLF) の consumed",
                  consumed, static_cast<std::size_t>(2));

        /* CR の扱い: LF 直前の CR だけを取り除く */
        ASSERT_EQ("findLine: 途中の CR は保持",
                  findLineStatus("A\rB\n", line, consumed), "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 途中の CR は保持 (本文)", line, "A\rB");
        ASSERT_EQ("findLine: 連続 CR は末尾 1 つだけ除去",
                  findLineStatus("A\r\r\n", line, consumed), "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 連続 CR は末尾 1 つだけ除去 (本文)", line, "A\r");

        /* 先頭が LF */
        ASSERT_EQ("findLine: 先頭が LF なら空行を返す",
                  findLineStatus("\nPASS x\r\n", line, consumed),
                  "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 先頭が LF の本文は空文字", line, "");
        ASSERT_EQ("findLine: 先頭が LF の consumed",
                  consumed, static_cast<std::size_t>(1));

        /* NUL は行として取り出し、破棄は Parser が行う */
        ASSERT_EQ("findLine: NUL を含む行も取り出す",
                  findLineStatus(std::string("A\0B\r\n", 5), line, consumed),
                  "LINE_EXTRACTED");
        ASSERT_EQ("findLine: NUL を含む行の本文長",
                  line.size(), static_cast<std::size_t>(3));
    }

    /* 1 回の受信に複数行が含まれる場合: 繰り返し取り出せる */
    {
        std::string buffer = "PASS secret\r\nNICK alice\r\nPAR";
        std::string line;
        std::size_t consumed;

        ASSERT_EQ("findLine: 複数行 1 行目",
                  findLineStatus(buffer, line, consumed), "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 複数行 1 行目の本文", line, "PASS secret");
        buffer.erase(0, consumed);

        ASSERT_EQ("findLine: 複数行 2 行目",
                  findLineStatus(buffer, line, consumed), "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 複数行 2 行目の本文", line, "NICK alice");
        buffer.erase(0, consumed);

        ASSERT_EQ("findLine: 複数行の残りは未完成",
                  findLineStatus(buffer, line, consumed), "LINE_INCOMPLETE");
        ASSERT_EQ("findLine: 未完成分がバッファに残る", buffer, "PAR");
    }

    /* 1 行が複数回の受信に分割される場合 (課題文の nc テスト) */
    {
        std::string buffer;
        std::string line;
        std::size_t consumed;

        buffer += "com";
        ASSERT_EQ("findLine: 分割受信 'com' は未完成",
                  findLineStatus(buffer, line, consumed), "LINE_INCOMPLETE");
        buffer += "man";
        ASSERT_EQ("findLine: 分割受信 'comman' は未完成",
                  findLineStatus(buffer, line, consumed), "LINE_INCOMPLETE");
        buffer += "d\n";
        ASSERT_EQ("findLine: 分割受信 'command\\n' で完成",
                  findLineStatus(buffer, line, consumed), "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 分割受信の復元結果", line, "command");
    }

    /* CRLF がパケット境界で分断される場合 */
    {
        std::string buffer = "PASS x\r";
        std::string line;
        std::size_t consumed;

        ASSERT_EQ("findLine: CR で終わるバッファは未完成",
                  findLineStatus(buffer, line, consumed), "LINE_INCOMPLETE");
        buffer += "\n";
        ASSERT_EQ("findLine: LF 到着で完成",
                  findLineStatus(buffer, line, consumed), "LINE_EXTRACTED");
        ASSERT_EQ("findLine: CRLF 分断でも本文は正しい", line, "PASS x");
    }

    /* 行長制限: 本文 510 byte + CRLF = 512 byte が最大。
       consumed は呼び出しごとに確認する。まとめて 1 回だけ確認すると
       直前の呼び出しの値を見ているのか区別できない */
    {
        std::string line;
        std::size_t consumed;
        std::string content510(510, 'a');
        std::string content511(511, 'a');

        ASSERT_EQ("findLine: 本文 510 + CRLF (512 byte) は受理",
                  findLineStatus(content510 + "\r\n", line, consumed),
                  "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 本文 510 + CRLF の本文長",
                  line.size(), static_cast<std::size_t>(510));
        ASSERT_EQ("findLine: 本文 510 + CRLF の consumed",
                  consumed, static_cast<std::size_t>(512));

        ASSERT_EQ("findLine: 本文 510 + LF のみ も受理",
                  findLineStatus(content510 + "\n", line, consumed),
                  "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 本文 510 + LF のみ の consumed",
                  consumed, static_cast<std::size_t>(511));

        ASSERT_EQ("findLine: 本文 511 + CRLF は超過",
                  findLineStatus(content511 + "\r\n", line, consumed),
                  "LINE_TOO_LONG");
        ASSERT_EQ("findLine: 本文 511 + CRLF 超過時の consumed",
                  consumed, static_cast<std::size_t>(513));

        /* LF のみで届いても仮想 CRLF を含めて判定する (設計書 06 §13) */
        ASSERT_EQ("findLine: 本文 511 + LF のみ も超過",
                  findLineStatus(content511 + "\n", line, consumed),
                  "LINE_TOO_LONG");
        ASSERT_EQ("findLine: 本文 511 + LF のみ 超過時の consumed",
                  consumed, static_cast<std::size_t>(512));
    }

    /* 失敗時の out / consumed の契約 (IrcUtil.hpp の宣言どおりか)。
       out = raw の代入が長さ検査より前へ移動するような改変が入ると、
       Server が 511 byte の超過行を処理してしまう。ここで固定する */
    {
        std::string out;
        std::size_t consumed;

        out      = "SENTINEL";
        consumed = 999;
        ASSERT_EQ("findLine: LF なし 512 は超過",
                  statusName(BufferUtil::findLine(std::string(512, 'a'), out,
                                               consumed)),
                  "LINE_TOO_LONG");
        ASSERT_EQ("findLine: 超過時の consumed は破棄すべきバイト数",
                  consumed, static_cast<std::size_t>(512));
        ASSERT_EQ("findLine: 超過時は out を変更しない", out, "SENTINEL");

        out      = "SENTINEL";
        consumed = 999;
        ASSERT_EQ("findLine: 本文 511 + CRLF は超過 (out 契約)",
                  statusName(BufferUtil::findLine(std::string(511, 'a') + "\r\n",
                                               out, consumed)),
                  "LINE_TOO_LONG");
        ASSERT_EQ("findLine: 完成行が超過でも out を変更しない", out,
                  "SENTINEL");

        out      = "SENTINEL";
        consumed = 999;
        ASSERT_EQ("findLine: LF なしは未完成 (out 契約)",
                  statusName(BufferUtil::findLine("PASS secret", out, consumed)),
                  "LINE_INCOMPLETE");
        ASSERT_EQ("findLine: 未完成時の consumed は 0", consumed,
                  static_cast<std::size_t>(0));
        ASSERT_EQ("findLine: 未完成時は out を変更しない", out, "SENTINEL");

        out      = "SENTINEL";
        consumed = 999;
        ASSERT_EQ("findLine: LF なし 1000 byte も超過",
                  statusName(BufferUtil::findLine(std::string(1000, 'a'), out,
                                               consumed)),
                  "LINE_TOO_LONG");
        ASSERT_EQ("findLine: LF なし超過の consumed はバッファ全体",
                  consumed, static_cast<std::size_t>(1000));
    }

    /* LF なしバッファの超過判定。511 byte までは
       「本文 510 + CR」で LF 待ちの可能性があるため切断しない */
    {
        std::string line;
        std::size_t consumed;

        ASSERT_EQ("findLine: LF なし 510 byte は未完成",
                  findLineStatus(std::string(510, 'a'), line, consumed),
                  "LINE_INCOMPLETE");
        ASSERT_EQ("findLine: LF なし 511 byte は未完成 (510+CR の可能性)",
                  findLineStatus(std::string(511, 'a'), line, consumed),
                  "LINE_INCOMPLETE");
        ASSERT_EQ("findLine: LF なし 512 byte は超過",
                  findLineStatus(std::string(512, 'a'), line, consumed),
                  "LINE_TOO_LONG");

        /* 本文 510 + CR で LF 待ち (511 byte) の状態から LF が届けば受理する。
           この分割受信を誤って切断しないことが 511 で切らない理由 */
        std::string buffer = std::string(510, 'a') + "\r";
        ASSERT_EQ("findLine: 本文 510 + CR は LF 待ち",
                  findLineStatus(buffer, line, consumed), "LINE_INCOMPLETE");
        buffer += "\n";
        ASSERT_EQ("findLine: 本文 510 + CR + LF は受理",
                  findLineStatus(buffer, line, consumed), "LINE_EXTRACTED");
        ASSERT_EQ("findLine: 分割された最大長行の本文長",
                  line.size(), static_cast<std::size_t>(510));
    }
}
