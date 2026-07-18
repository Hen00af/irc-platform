#include "IrcUtil.hpp"

#include <limits>

namespace
{
    bool isAsciiLetter(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    bool isAsciiDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    /* RFC 2812 の special: [ ] \ ` _ ^ { | } */
    bool isNicknameSpecial(char c)
    {
        return c == '[' || c == ']' || c == '\\' || c == '`' || c == '_'
            || c == '^' || c == '{' || c == '|' || c == '}';
    }
}

/* ── 大文字小文字と正規化 ─────────────────── */

std::string IrcUtil::toUpperAscii(const std::string &value)
{
    std::string result(value);

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        if (result[i] >= 'a' && result[i] <= 'z')
            result[i] = static_cast<char>(result[i] - 'a' + 'A');
    }
    return result;
}

std::string IrcUtil::ircCaseFold(const std::string &value)
{
    std::string result(value);

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        char c = result[i];

        if (c >= 'A' && c <= 'Z')
            result[i] = static_cast<char>(c - 'A' + 'a');
        else if (c == '[')
            result[i] = '{';
        else if (c == ']')
            result[i] = '}';
        else if (c == '\\')
            result[i] = '|';
        else if (c == '^')
            result[i] = '~';
    }
    return result;
}

std::string IrcUtil::normalizeChannelName(const std::string &value)
{
    return ircCaseFold(value);
}

/* ── 検証 ─────────────────────────────────── */

bool IrcUtil::isValidNickname(const std::string &value)
{
    if (value.empty() || value.size() > NICKNAME_MAX_LENGTH)
        return false;

    if (!isAsciiLetter(value[0]) && !isNicknameSpecial(value[0]))
        return false;

    for (std::size_t i = 1; i < value.size(); ++i)
    {
        char c = value[i];

        if (!isAsciiLetter(c) && !isAsciiDigit(c) && !isNicknameSpecial(c)
            && c != '-')
            return false;
    }
    return true;
}

bool IrcUtil::isValidChannelName(const std::string &value)
{
    if (value.size() < 2 || value.size() > CHANNEL_NAME_MAX_LENGTH)
        return false;

    if (value[0] != '#')
        return false;

    for (std::size_t i = 1; i < value.size(); ++i)
    {
        char c = value[i];

        if (c == ' ' || c == ',' || c == ':' || c == '\a' || c == '\r'
            || c == '\n' || c == '\0')
            return false;
    }
    return true;
}

bool IrcUtil::parsePositiveSize(const std::string &value, std::size_t &result)
{
    if (value.empty())
        return false;

    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    std::size_t       parsed  = 0;

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        /* 数字以外を弾くので、先頭の '+' '-' や空白も自動的に拒否される */
        if (!isAsciiDigit(value[i]))
            return false;

        std::size_t digit = static_cast<std::size_t>(value[i] - '0');

        /* parsed * 10 + digit が size_t を超えるなら overflow */
        if (parsed > (maximum - digit) / 10)
            return false;

        parsed = parsed * 10 + digit;
    }

    if (parsed == 0)
        return false;

    result = parsed;
    return true;
}

bool IrcUtil::fitsIrcLine(const std::string &withoutCrlf)
{
    return withoutCrlf.size() <= IRC_MAX_CONTENT;
}

bool IrcUtil::isSafeToken(const std::string &value)
{
    if (value.empty())
        return false;

    /* 先頭の colon は Trailing の目印なので token の先頭には置けない。
       例えば Nickname ":x" を許すと
         ":ircserv.local 401 :x :No such nick/channel"
       となり、受信側は ":x :No such nick/channel" を 1 つの Trailing と
       解釈して target が壊れる。
       token 途中の colon は IPv6 hostname で正当なため許可する */
    if (value[0] == ':')
        return false;

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        char c = value[i];

        if (c == ' ' || c == '\r' || c == '\n' || c == '\0')
            return false;
    }
    return true;
}

/* ── 文字列操作 ───────────────────────────── */

std::string IrcUtil::sanitizeMessageText(const std::string &value)
{
    std::string result;

    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        char c = value[i];

        if (c != '\r' && c != '\n' && c != '\0')
            result += c;
    }
    return result;
}

std::vector<std::string> IrcUtil::splitCommaList(const std::string &value)
{
    std::vector<std::string> result;

    if (value.empty())
        return result;

    std::string::size_type start = 0;

    while (true)
    {
        std::string::size_type pos = value.find(',', start);

        if (pos == std::string::npos)
        {
            result.push_back(value.substr(start));
            break;
        }
        result.push_back(value.substr(start, pos - start));
        start = pos + 1;
    }
    return result;
}

