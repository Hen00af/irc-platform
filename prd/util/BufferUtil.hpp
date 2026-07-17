#pragma once

#include <cstddef>
#include <string>

/* ============================================================
 * BufferUtil
 *
 * 受信バッファから IRC 行を切り出す、状態を持たないユーティリティ。
 * 設計書 02 §5.6 が言う「Buffer Utility」にあたる。
 *
 *   「Clientは行解析を行わない。CRLFの検索と完成行の抽出はServerの
 *     受信処理またはBuffer Utilityが担当する」
 *
 * Client はバッファを保持するだけ、Server は I/O だけを行い、
 * 行の切り出しはここへ集約する。
 *
 * std::string しか受け取らないため Client / Channel / Server には
 * 依存しない。IRC の行長上限は protocol の規則なので IrcUtil が持つ。
 * ============================================================ */
namespace BufferUtil
{
    enum LineStatus
    {
        LINE_INCOMPLETE, /* 完成行がまだない。次の recv() を待つ */
        LINE_EXTRACTED,  /* 1 行取り出した */
        LINE_TOO_LONG    /* 行長超過。呼び出し側は Client を切断する */
    };

    /* 受信バッファに完成行 (LF 終端の行) が含まれるか */
    bool hasCompleteLine(const std::string &buffer);

    /* buffer の先頭から IRC 行を 1 行取り出す。buffer は変更しない。
     *
     * LF を行終端として扱い、直前に CR があれば併せて取り除く。
     * consumed には buffer の先頭から取り除くべきバイト数 (LF を含む) を返す。
     * 呼び出し側は Client::eraseReceivePrefix(consumed) で取り除き、
     * 繰り返し呼ぶことで 1 度の recv() に含まれる複数行をすべて処理する。
     *
     * 戻り値ごとの out / consumed:
     *   LINE_INCOMPLETE : out 未変更、consumed = 0
     *   LINE_EXTRACTED  : out = 行本文 (空行なら空文字)、consumed = LF まで
     *   LINE_TOO_LONG   : out 未変更、consumed = 破棄すべきバイト数
     *
     * 空行は LINE_EXTRACTED かつ out が空文字として返る。空行の破棄は
     * Parser::parse() が false を返すことで行われる。
     *
     * NUL は除去せずそのまま out へ入れる。破棄は Parser::parse() が行う。 */
    LineStatus findLine(const std::string &buffer,
                        std::string       &out,
                        std::size_t       &consumed);
}
