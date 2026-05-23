#include "responsebuilder.hpp"
#include <sstream>
#include <time.h>
#include <sys/stat.h>

#include <map>
#include <string>

static std::map<int, std::string> createReasonPhraseMap()
{
    std::map<int, std::string> m;

    // 1xx Informational responses
    m[100] = "Continue";
    m[101] = "Switching Protocols";
    m[102] = "Processing";
    m[103] = "Early Hints";

    // 2xx Successful responses
    m[200] = "OK";
    m[201] = "Created";
    m[202] = "Accepted";
    m[203] = "Non-Authoritative Information";
    m[204] = "No Content";
    m[205] = "Reset Content";
    m[206] = "Partial Content";
    m[207] = "Multi-Status";
    m[208] = "Already Reported";
    m[226] = "IM Used";

    // 3xx Redirection messages
    m[300] = "Multiple Choices";
    m[301] = "Moved Permanently";
    m[302] = "Found";
    m[303] = "See Other";
    m[304] = "Not Modified";
    m[305] = "Use Proxy";
    m[306] = "unused";
    m[307] = "Temporary Redirect";
    m[308] = "Permanent Redirect";

    // 4xx Client error responses
    m[400] = "Bad Request";
    m[401] = "Unauthorized";
    m[402] = "Payment Required";
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
    m[413] = "Content Too Large";
    m[414] = "URI Too Long";
    m[415] = "Unsupported Media Type";
    m[416] = "Range Not Satisfiable";
    m[417] = "Expectation Failed";
    m[418] = "I'm a teapot";
    m[421] = "Misdirected Request";
    m[422] = "Unprocessable Content";
    m[423] = "Locked";
    m[424] = "Failed Dependency";
    m[425] = "Too Early";
    m[426] = "Upgrade Required";
    m[428] = "Precondition Required";
    m[429] = "Too Many Requests";
    m[431] = "Request Header Fields Too Large";
    m[451] = "Unavailable For Legal Reasons";

    // 5xx Server error responses
    m[500] = "Internal Server Error";
    m[501] = "Not Implemented";
    m[502] = "Bad Gateway";
    m[503] = "Service Unavailable";
    m[504] = "Gateway Timeout";
    m[505] = "HTTP Version Not Supported";
    m[506] = "Variant Also Negotiates";
    m[507] = "Insufficient Storage";
    m[508] = "Loop Detected";
    m[510] = "Not Extended";
    m[511] = "Network Authentication Required";

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

static bool endsWith(const std::string& ref, const std::string& obj)
{
    if (ref.length() >= obj.length())
    {
        return (0 == ref.compare(ref.length() - obj.length(), obj.length(), obj));
    }
    else
        return false;
}

static std::string getContentType(const std::string& path)
{
    if (endsWith(path, ".html"))
        return "text/html";
    if (endsWith(path, ".css"))
        return "text/css";
    if (endsWith(path, ".js"))
        return "application/javascript";
    if (endsWith(path, ".json"))
        return "application/json";
    if (endsWith(path, ".png"))
        return "image/png";
    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg"))
        return "image/jpeg";
    if (endsWith(path, ".txt"))
        return "text/plain";
    return "application/octet-stream";
}

static std::string httpDate()
{
    char buf[128];
    struct tm tm;
    time_t now = time(NULL);

    gmtime_r(&now, &tm);
    strftime(buf, sizeof(buf), "%a %d %b %Y %H:%M:%S GMT", &tm);
    return std::string(buf);
}

static void setBasicHeaders(HttpResponse& res)
{
    res.headers["Content-Length"] = toString(res.body.size());
    res.headers["Date"] = httpDate();
    res.headers["Server"] = "webserv/0.1";

    if (res.headers.find("Content-Type") == res.headers.end())
        res.headers["Content-Type"] = getContentType(".html");
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
    (void)req;
    (void)ctx;

    HttpResponse res;
    res.status_code = 200;
    res.reason_phrase = getReasonPhrase(200);
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
