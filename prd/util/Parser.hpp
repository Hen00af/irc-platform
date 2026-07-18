#pragma once

#include <cstddef>
#include <string>

#include "../domain/Message.hpp"

/* ============================================================
 * Parser
 *
 * 1 行の文字列を Message へ変換する責務だけを持つ。
 *
 * Parser は以下を行わない (設計書 02 §8):
 *   - Client / Channel / Server の状態変更
 *   - Client 検索、登録状態確認
 *   - Command ごとの Parameter 数の検証
 *   - Numeric Reply の生成
 *
 * 状態を持たないため、インスタンス化しない。
 * ============================================================ */
class Parser
{
public:
    /* IRC の Parameter 数上限 (設計書 02 §7)。
       超過した行は先頭 15 個を使わず Message 全体を破棄する */
    static const std::size_t MAX_PARAMS = 15;

    /* 行を Message へ変換する。
     *
     * 入力は BufferUtil::findLine() が CRLF を取り除いた 1 行を想定する。
     * 行長制限は findLine() が受信時に判定するため、ここでは検査しない。
     *
     * 戻り値 true : Command を含む Message を生成し out へ格納した
     * 戻り値 false: 次のいずれか。out は変更しない
     *                 - 空行、空白のみの行
     *                 - Command がない (":prefix" だけなど)
     *                 - NUL を含む行
     *                 - Parameter が 16 個以上
     *
     * ':' で始まる Parameter は Trailing として行末までを 1 つの
     * Parameter とし、通常の Parameter と同じく params の末尾へ入れる。
     * Trailing 内の空白と colon はそのまま保持する。空の Trailing も
     * 1 つの Parameter として扱う ("PRIVMSG #a :" → params ["#a", ""])。
     *
     * Command は ASCII 大文字へ正規化する。
     *
     * Parameter に紛れ込んだ CR は除去せずそのまま格納する。除去は送信時に
     * Reply が IrcUtil::sanitizeMessageText() で行う (設計書 06 §17)。
     * 例: "PRIVMSG #a :hi\rPRIVMSG #b :evil" は
     *     params ["#a", "hi\rPRIVMSG #b :evil"] として解析され、
     *     中継時に Reply が CR を落として 1 行に保つ。
     *
     * この分担のため、設計書 06 §21「不正入力に CRLF injection が含まれても
     * 追加 Command として送信されない」は、Handler が必ず Reply を経由して
     * 行を組み立てて初めて成立する。Handler が生の文字列連結で行を作ると
     * 破れる。 */
    static bool parse(const std::string &line, Message &out);

private:
    Parser();
};
