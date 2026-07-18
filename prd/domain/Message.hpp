#pragma once

#include <string>
#include <vector>

/* ============================================================
 * Message
 *
 * IRC メッセージ 1 行を構造化したもの。
 *
 *   PRIVMSG #general :hello world
 *     prefix  = ""
 *     command = "PRIVMSG"
 *     params  = ["#general", "hello world"]
 *
 * Trailing parameter も params の末尾へ格納する。Parser 利用側が通常の
 * Parameter と Trailing を区別する必要がないためである。
 *
 * command は Parser が ASCII 大文字へ正規化する。
 * prefix は Client が送ってきた値であり信頼できないため、Handler では
 * 使用しない。送信元は常に socket FD から特定する。
 * ============================================================ */
struct Message
{
    std::string              prefix;
    std::string              command;
    std::vector<std::string> params;
};
