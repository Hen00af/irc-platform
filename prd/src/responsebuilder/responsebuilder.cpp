#include "responsebuilder.hpp"
#include <sstream>

static std::map<int, std::string> createReasonPhraseMap()
{
    std::map<int, std::string> m;

    m[400] = "Bad Request";
    m[401] = "Unauthorized";
    m[403] = "Forbidden";
    m[404] = "Not Found";
    m[405] = "Method Not Allowed";
    m[406] = "Not Acceptable";
    m[407] = "Proxy Authentication Required";
    m[408] = "Request Timeout";
    m[409] = "Conflict";
    m[410] = "Gone";
    m[411] = "Length Required";
    m[412] = "Precondition Failed";
    m[413] = "Payload Too Large";
    m[414] = "URI Too Long";
    m[415] = "Unsupported Media Type";
    m[500] = "Internal Server Error";
    m[501] = "Not Implemented";
    m[502] = "Bad Gateway";
    m[503] = "Service Unavailable";
    m[504] = "Gateway Timeout";
    m[505] = "HTTP Version Not Supported";

    return m;
}

static const std::string& getReasonPhrase(int status_code)
{
    static const std::map<int, std::string> reason_map = createReasonPhraseMap();
    static const std::string unknown = "Unknown";

    std::map<int, std::string>::const_iterator it = reason_map.find(status_code);
    if (it == reason_map.end())
        return unknown;
    return it->second;
}

static std::string toString(size_t n)
{
    std::stringstream ss;
    ss << n;
    return ss.str();
}

static std::string toString(int n)
{
    std::stringstream ss;
    ss << n;
    return ss.str();
}

static void setBasicHeaders(HttpResponse& res)
{
    res.headers["Content-Length"] = toString(res.body.size());
}

HttpResponse buildErrorResponse(int status_code)
{
    HttpResponse res;

    res.status_code = status_code;
    res.reason_phrase = getReasonPhrase(status_code);

    res.headers["Content-Type"] = "text/html";
    res.body = "<h1>" + toString(status_code) + " " + res.reason_phrase + "</h1>";
    setBasicHeaders(res);

    return res;
}

HttpResponse handleGet(const HttpRequest& req, const ResponseContext& ctx)
{
    HttpResponse res;

    (void)req;
    (void)ctx;

    res.status_code = 200;
    res.reason_phrase = "OK";
    res.headers["Content-Type"] = "text/html";
    res.body = "<h1>Hello from GET</h1>";
    setBasicHeaders(res);

    return res;
}

HttpResponse handlePost(const HttpRequest& req, const ResponseContext& ctx)
{
    (void)req;
    (void)ctx;
    return buildErrorResponse(501);
}

HttpResponse handleDelete(const HttpRequest& req, const ResponseContext& ctx)
{
    (void)req;
    (void)ctx;
    return buildErrorResponse(501);
}

HttpResponse buildResponse(const HttpRequest& req, const ResponseContext& ctx)
{
    if (req.method == "GET")
        return handleGet(req, ctx);

    if (req.method == "POST")
        return handlePost(req, ctx);

    if (req.method == "DELETE")
        return handleDelete(req, ctx);

    return buildErrorResponse(405);
}
