#include "responseSerializer.hpp"
#include <sstream>

static std::string crlf()
{
    return "\r\n";
}

static std::string toString(int n)
{
    std::stringstream ss;
    ss << n;
    return ss.str();
}

static std::string serializeStatusLine(const HttpResponse& res)
{
    std::string line;

    line.append("HTTP/1.1 ");
    line.append(toString(res.status_code));
    line.append(" ");
    line.append(res.reason_phrase);
    line.append(crlf());

    return line;
}

#include <iostream>
static std::string serializeHeaders(const HttpResponse& res)
{
    std::string headers;
    std::map<std::string, std::string>::const_iterator it;

    for (it = res.headers.begin(); it != res.headers.end(); ++it)
    {
        headers.append(it->first);
        headers.append(": ");
        headers.append(it->second);
        headers.append(crlf());
        std::cout << "first:" << it->first << " second:" << it->second << std::endl;
    }
    return headers;
}

std::string createResponse(const HttpResponse& res)
{
    std::string response;

    response.append(serializeStatusLine(res));
    response.append(serializeHeaders(res));
    response.append(crlf());
    response.append(res.body);

    return response;
}
