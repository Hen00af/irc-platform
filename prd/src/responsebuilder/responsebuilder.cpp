#include "responseBulder.hpp"

/*
    int status_code;
    std::string reason_phrase;

    std::map<std::string, std::string> headers;

    std::string body;
*/

/*
    <Setting Value
*/

bool setStatusCode() {

}

bool setReasonPhrase() {

}

bool setHeaders() {

}

bool setBody() {

}

/*
    Setting Value>
*/

/*
    <Setting status line
    HTTP/1.1 200 OK
*/

std::string serializeStatusLine(const HttpResponse& res) {
    std::string line;

    line.append("HTTP/1.1 ");
    line.append(toString(res.status_code));
    line.append(" ");
    line.append(res.reason_phrase);
    line.append(CRLF());

    return line;
}

/*
    Setting status line>
*/

/*
    <Setting request
*/
std::string serializeHeaders(const HttpResponse& res) {
    std::string headers;

    std::map<std::string, std::string>::const_iterator it;

    for (it = res.headers.begin(); it != res.headers.end(); ++it)
    {
        headers.append(it->first);
        headers.append(": ");
        headers.append(it->second);
        headers.append(CRLF());
    }

    return headers;
}
/*
    Setting request>
*/

/*
    <Setting body
*/
std::string serializeBody(const HttpResponse& res) {
    return res.body;
}
/*
    Setting body>
*/

std::string serialize(const HttpResponse& res) {
    std::string response;

    response.append(serializeStatusLine(res));

    response.append(serializeHeaders(res));

    response.append(CRLF());

    response.append(serializeBody(res));

    return response;
}
/*
    <utils
*/
std::string numberToString(size_t n)
{
    std::stringstream ss;
    ss << n;
    return ss.str();
}

std::string CRLF() {
    return "\r\n";
}

/*
    utils>
*/
HttpResponse buildResponse() {
    HttpResponse res;

    res.status_code = 200;
    res.reason_phrase = "OK";

    res.body = "<h1>Hello</h1>";

    res.headers["Content-Type"] = "text/html";
    res.headers["Content-Length"] = toString(res.body.size());

    return res;
}