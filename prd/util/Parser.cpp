#include "Parser.hpp"

#include "IrcUtil.hpp"

const std::size_t Parser::MAX_PARAMS;

namespace
{
    void skipSpaces(const std::string &line, std::size_t &pos)
    {
        while (pos < line.size() && line[pos] == ' ')
            ++pos;
    }

    /* pos から次の space 直前までを取り出し、pos を進める */
    std::string takeToken(const std::string &line, std::size_t &pos)
    {
        std::size_t start = pos;

        while (pos < line.size() && line[pos] != ' ')
            ++pos;
        return line.substr(start, pos - start);
    }
}

bool Parser::parse(const std::string &line, Message &out)
{
    /* NUL を含む行は不正入力として破棄する (設計書 06 §12.1) */
    if (line.find('\0') != std::string::npos)
        return false;

    Message     parsed;
    std::size_t pos = 0;

    skipSpaces(line, pos);

    /* Prefix (あれば)。Client 由来の Prefix は信頼しないが、構文上は
       受理して読み飛ばす必要がある */
    if (pos < line.size() && line[pos] == ':')
    {
        ++pos;
        parsed.prefix = takeToken(line, pos);
        if (parsed.prefix.empty())
            return false;
        skipSpaces(line, pos);
    }

    parsed.command = IrcUtil::toUpperAscii(takeToken(line, pos));
    if (parsed.command.empty())
        return false;

    while (true)
    {
        skipSpaces(line, pos);
        if (pos >= line.size())
            break;

        if (parsed.params.size() >= MAX_PARAMS)
            return false;

        /* Trailing は行末までが 1 つの Parameter。空白と colon を
           そのまま保持し、空文字も 1 つの Parameter として扱う */
        if (line[pos] == ':')
        {
            ++pos;
            parsed.params.push_back(line.substr(pos));
            break;
        }
        parsed.params.push_back(takeToken(line, pos));
    }

    out = parsed;
    return true;
}
