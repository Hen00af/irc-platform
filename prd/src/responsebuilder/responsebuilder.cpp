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

}

/*
    Setting status line>
*/

/*
    <Setting request
*/
std::string serializeHeaders(const HttpResponse& res) {
}
/*
    Setting request>
*/

/*
    <Setting body
*/
std::string serializeBody(const HttpResponse& res) {
}
/*
    Setting body>
*/

std::string CRLF() {
    return "\r\n";
}


std::string serialize(const HttpResponse& res) {
    std::string response;

    response.append(serializeStatusLine(res));
    response.append(serializeHeaders(res));
    response.append(CRLF());
    response.append(serializeBody(res));

    return response;
}

std::string CRLF() {
    return "\r\n";
}

HttpResponse buildResponse() {
    HttpResponse res;

    res.status_code = 200;
    res.reason_phrase = "OK";

    res.body = "<h1>Hello</h1>";

    res.headers["Content-Type"] = "text/html";
    res.headers["Content-Length"] = toString(res.body.size());

    return res;
}