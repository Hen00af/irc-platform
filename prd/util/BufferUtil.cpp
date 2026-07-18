#include "BufferUtil.hpp"

#include "IrcUtil.hpp"

bool BufferUtil::hasCompleteLine(const std::string &buffer)
{
    return buffer.find('\n') != std::string::npos;
}

BufferUtil::LineStatus BufferUtil::findLine(const std::string &buffer,
                                            std::string       &out,
                                            std::size_t       &consumed)
{
    consumed = 0;

    std::string::size_type pos = buffer.find('\n');

    if (pos == std::string::npos)
    {
        /* LF がまだ無い。512 byte 以上溜まっていれば、この先 LF が来ても
           本文が 510 byte を超えるため、正しい IRC 行にはなり得ない。
           511 byte までは「本文 510 byte + CR」で LF 待ちの可能性があり、
           正当な最大長行を誤って切断しないため、まだ超過と判定しない。 */
        if (buffer.size() >= IrcUtil::IRC_MAX_LINE)
        {
            consumed = buffer.size();
            return LINE_TOO_LONG;
        }
        return LINE_INCOMPLETE;
    }

    consumed = pos + 1; /* LF まで取り除く */

    std::string raw = buffer.substr(0, pos); /* LF は含めない */

    if (!raw.empty() && raw[raw.size() - 1] == '\r')
        raw.erase(raw.size() - 1);

    /* 本文が 510 byte 以下であることを確認する。LF だけで終端された行も
       仮想的な CRLF を含めて 512 byte 以内と判断する */
    if (raw.size() > IrcUtil::IRC_MAX_CONTENT)
        return LINE_TOO_LONG;

    out = raw;
    return LINE_EXTRACTED;
}
