#pragma once

#include <cstddef>
#include <string>
#include <vector>

/* ============================================================
 * IrcUtil
 *
 * IRC の文字列規則 (case mapping・名前の妥当性・行長・整形) を扱う、
 * 状態を持たないユーティリティ群。設計書 02 §10 の一覧に対応する。
 *
 * std::string しか受け取らないため、Client / Channel / Server の
 * いずれにも依存しない。
 *
 * 表示用の文字列は変更せず、Map / Set の検索 Key だけを正規化する
 * という方針のため、正規化関数は必ず新しい文字列を返す。
 *
 * 受信バッファからの行切り出しは責務が異なるため BufferUtil にある。
 * ============================================================ */
namespace IrcUtil
{
    /* IRC 行は CRLF を含めて 512 byte。本文に使えるのは 510 byte */
    const std::size_t IRC_MAX_LINE    = 512;
    const std::size_t IRC_MAX_CONTENT = 510;

    /* Nickname は RFC 2812 に合わせて 9 文字、Channel 名は 50 文字を上限とする */
    const std::size_t NICKNAME_MAX_LENGTH     = 9;
    const std::size_t CHANNEL_NAME_MAX_LENGTH = 50;

    /* ── 大文字小文字と正規化 ──────────────── */

    /* ASCII の a-z だけを A-Z へ変換する (locale 非依存)。
       Parser が Command 名を正規化するのに使う */
    std::string toUpperAscii(const std::string &value);

    /* RFC 2812 の IRC case mapping。
       A-Z→a-z, [→{, ]→}, \→|, ^→~
       Nickname の重複比較に使う検索 Key を作る */
    std::string ircCaseFold(const std::string &value);

    /* Channel 名の検索 Key を作る。表示名は Channel が別途保持する */
    std::string normalizeChannelName(const std::string &value);

    /* ── 検証 ─────────────────────────────── */

    /* 1〜9 文字。先頭は ASCII 英字か特殊文字 []\`_^{|} のいずれか。
       2 文字目以降は先頭許可文字に加えて ASCII 数字と '-' を許可する。
       空白 , : CR LF NUL はいずれの位置でも許可しない */
    bool isValidNickname(const std::string &value);

    /* '#' で始まる 2〜50 文字。空白 , : BELL CR LF NUL を含まない */
    bool isValidChannelName(const std::string &value);

    /* ASCII 数字のみ。0 と符号つきと overflow を拒否する。
     *
     * 成功時だけ result へ書き込む。失敗時は result を変更しない。
     * 先頭の 0 は許可し、値として解釈する ("007" → 7)。
     * 設計書 05 §13 の「0 を許可しない」は値に対する制約であり、
     * "000" は値 0 として拒否される。
     *
     * 同 §13 の「上限値は 100000 以下」は MODE +l 固有の制約のため
     * ここでは課さない。MODE Handler が本関数の後段で確認すること。 */
    bool parsePositiveSize(const std::string &value, std::size_t &result);

    /* CRLF を足しても 512 byte に収まるか (設計書 06 §13) */
    bool fitsIrcLine(const std::string &withoutCrlf);

    /* Prefix / Command / target として 1 つの token になり得るか
     * (設計書 06 §17.1)。
     *
     * 次のいずれかなら false:
     *   - 空文字
     *   - 空白 / CR / LF / NUL を含む (行が壊れる、別 Parameter になる)
     *   - 先頭が colon (Trailing の目印と誤解される)
     *
     * token 途中の colon は IPv6 hostname で正当なため許可する。
     *
     * Reply が Prefix / Command / target へ適用するほか、Server は
     * 起動時に serverName の検査へ使う。serverName が不正だと全 Client
     * への全 Reply が空文字になるため。 */
    bool isSafeToken(const std::string &value);

    /* ── 文字列操作 ───────────────────────── */

    /* CR / LF / NUL を除去する。他の文字は保持する。
     *
     * IRC は CRLF でメッセージを区切るため、Client 由来の文字列を無検査で
     * 中継すると、受信側に「サーバが発した別の行」として解釈させられる
     * (設計書 06 §21 の CRLF injection)。Reply が送信行を組み立てる際に
     * 必ず通すこと。
     *
     * このサーバでは findLine() が LF を、Parser::parse() が NUL を
     * それぞれ弾くため、実際に Parameter へ到達し得るのは単独 CR だけ。
     * ただし将来の経路変更に備え、3 文字すべてを除去する。 */
    std::string sanitizeMessageText(const std::string &value);

    /* comma 区切りの分割。
       JOIN の Channel と Key を位置で対応させるため空要素も残す
       ("#a,,#b" → ["#a", "", "#b"])。入力が空文字なら空の vector を返す。
       padding はしないため、Key 一覧が Channel 一覧より短くなり得る */
    std::vector<std::string> splitCommaList(const std::string &value);
}
