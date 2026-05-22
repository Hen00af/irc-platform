#include "responsebuilder.hpp"
#include <sstream>

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
    if (status_code == 404)
        res.reason_phrase = "Not Found";
    else if (status_code == 405)
        res.reason_phrase = "Method Not Allowed";
    else if (status_code == 406)
        res.reason_phrase = "Not Acceptable";
    else if (status_code == 407)
        res.reason_phrase == "Proxy Authentication Required";
    else if (status_code == 408)
        res.reason_phrase == "request Timeout";
    else if (status_code == 409)
        res.reason_phrase == "Conflict";
    else if (status_code == 410)
        res.reason_phrase == "Gone";
    else if (status_code == 411)
        res.reason_phrase == "Length Required";
    else if (status_code == 412)
        res.reason_phase == "Precondition Failed"
    else if (status_code == 501)
        res.reason_phrase = "Not Implemented";
    else
        res.reason_phrase = "Internal Server Error";

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
