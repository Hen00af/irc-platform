#include "responsebuilder.hpp"
#include <sstream>

/*
    status code
    reason phrase
    headers
    body
    を決める
*/

static std::string toString(size_t n)
{
    std::stringstream ss;
    ss << n;
    return ss.str();
}

static void setBasicHeaders(HttpResponse& res)
{
    res.headers["Content-Length"] = toString(res.body.size());
}

HttpResponse buildResponse(const Conf& config)
{
    HttpResponse res;

    (void)config;

    res.status_code = 200;
    res.reason_phrase = "OK";
    res.headers["Content-Type"] = "text/html";
    res.body = "<h1>Hello</h1>";

    setBasicHeaders(res);

    return res;
}