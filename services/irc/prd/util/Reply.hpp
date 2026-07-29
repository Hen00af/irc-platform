#pragma once

#include <string>

class Client;

/* ============================================================
 * Reply
 *
 * Server から送信する IRC 行の本文を組み立てる。返信形式を Handler から
 * 分離するため、Handler 内で Numeric 文字列を直接連結しないこと
 * (設計書 06 §10)。
 *
 * 状態を持たないため、インスタンス化しない。
 *
 * CRLF は付けない。付与は Server::queueToClient() へ一元化する
 * (設計書 02 §9, 06 §2)。
 *
 * 安全性の保証 (設計書 06 §17):
 *   - parameters の CR / LF / NUL を IrcUtil::sanitizeMessageText() で除去し、
 *     Client 由来の文字列が追加の行として中継されるのを防ぐ (06 §21)
 *   - prefix / command / target に空白・CR・LF・NUL があれば内部 Error と
 *     みなし空文字を返す
 *   - code が 0〜999 の範囲外なら内部 Error とみなし空文字を返す
 *   - 呼び出し側は空文字の Reply を queue しないこと
 *
 * 行長 (CRLF 込み 512 byte) は切り詰めない。Numeric Reply は
 * Parameter を切り詰めない方針のため、必要なら呼び出し側が
 * IrcUtil::fitsIrcLine() で確認する (設計書 06 §13)。
 * ============================================================ */
class Reply
{
public:
    /* "<nickname>!<username>@<hostname>" を返す。先頭 colon は付けない。
       command() へそのまま渡せる形式である (設計書 06 §5)。
       未設定項目は nickname→"*", username→"unknown", hostname→"0.0.0.0"
       へ落とす (設計書 06 §3)。通常、登録済み Client では使われない */
    static std::string clientPrefix(const Client &client);

    /* Server Prefix。先頭 colon は付けない */
    static std::string serverPrefix(const std::string &serverName);

    /* ":<serverName> <3 桁 code> <target> <parameters>"
     *
     * target は Nickname。未設定なら "*" (設計書 06 §4)。
     * parameters は組み立て済みの Parameter 列を渡す。
     *   Reply::numeric(server, client, Numeric::ERR_NICKNAMEINUSE,
     *                  nick + " :Nickname is already in use");
     * parameters が空なら末尾に空白を付けない。 */
    static std::string numeric(const std::string &serverName,
                               const Client      &target,
                               int                code,
                               const std::string &parameters);

    /* "[:<prefix> ]<command> [parameters]"
     *
     * prefix は先頭 colon なしで渡し、colon はここで付ける
     * (設計書 06 §5)。prefix が空なら prefix 部を出力しない。
     *   Reply::command(Reply::clientPrefix(c), "JOIN", ":#general");
     *   → ":alice!u@h JOIN :#general" */
    static std::string command(const std::string &prefix,
                               const std::string &command,
                               const std::string &parameters);

private:
    Reply();
};
