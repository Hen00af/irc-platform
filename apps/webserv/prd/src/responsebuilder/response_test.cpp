#include "HttpResponse.hpp"
#include "responsebuilder.hpp"
#include "responseSerializer.hpp"

#include <iostream>
#include <string>
#include <cstdlib>

static void assertEqual(const std::string& actual, const std::string& expected,
                        const std::string& test_name)
{
    if (actual == expected)
    {
        std::cout << "[OK] " << test_name << std::endl;
        return;
    }

    std::cout << "[KO] " << test_name << std::endl;
    std::cout << "----- expected -----" << std::endl;
    std::cout << expected << std::endl;
    std::cout << "----- actual -----" << std::endl;
    std::cout << actual << std::endl;

    std::exit(1);
}

static void testSerializeSimpleResponse()
{
    HttpResponse res;

    res.status_code = 200;
    res.reason_phrase = "OK";
    res.headers["Content-Type"] = "text/html";
    res.headers["Content-Length"] = "14";
    res.body = "<h1>Hello</h1>";

    std::string actual = serializeResponse(res);

    std::string expected;
    expected += "HTTP/1.1 200 OK\r\n";
    expected += "Content-Length: 14\r\n";
    expected += "Content-Type: text/html\r\n";
    expected += "\r\n";
    expected += "<h1>Hello</h1>";

    assertEqual(actual, expected, "serialize simple 200 response");
}

static void testSerializeEmptyBody()
{
    HttpResponse res;

    res.status_code = 404;
    res.reason_phrase = "Not Found";
    res.headers["Content-Type"] = "text/html";
    res.headers["Content-Length"] = "0";
    res.body = "";

    std::string actual = serializeResponse(res);

    std::string expected;
    expected += "HTTP/1.1 404 Not Found\r\n";
    expected += "Content-Length: 0\r\n";
    expected += "Content-Type: text/html\r\n";
    expected += "\r\n";

    assertEqual(actual, expected, "serialize empty body response");
}

static void testBuildResponse()
{
    HttpRequest req;
    ResponseContext ctx;

    req.method = "GET";
    req.target = "/";
    req.body = "";

    ctx.config = NULL;
    ctx.file_path = "";

    HttpResponse res = buildResponse(req, ctx);

    if (res.status_code != 200)
    {
        std::cout << "[KO] buildResponse status_code" << std::endl;
        std::exit(1);
    }

    if (res.reason_phrase != "OK")
    {
        std::cout << "[KO] buildResponse reason_phrase" << std::endl;
        std::exit(1);
    }

    if (res.body != "<h1>Hello from GET</h1>")
    {
        std::cout << "[KO] buildResponse body" << std::endl;
        std::exit(1);
    }

    if (res.headers["Content-Type"] != "text/html")
    {
        std::cout << "[KO] buildResponse Content-Type" << std::endl;
        std::exit(1);
    }

    if (res.headers["Content-Length"] != "23")
    {
        std::cout << "[KO] buildResponse Content-Length" << std::endl;
        std::exit(1);
    }

    std::cout << "[OK] buildResponse basic response" << std::endl;
}

static void testBuildAndSerialize()
{
    HttpRequest req;
    ResponseContext ctx;

    req.method = "GET";
    req.target = "/";
    req.body = "";

    ctx.config = NULL;
    ctx.file_path = "";

    HttpResponse res = buildResponse(req, ctx);
    std::string actual = serializeResponse(res);

    std::string expected;
    expected += "HTTP/1.1 200 OK\r\n";
    expected += "Content-Length: 23\r\n";
    expected += "Content-Type: text/html\r\n";
    expected += "\r\n";
    expected += "<h1>Hello from GET</h1>";

    assertEqual(actual, expected, "buildResponse + serializeResponse");
}

int main()
{
    testSerializeSimpleResponse();
    testSerializeEmptyBody();
    testBuildResponse();
    testBuildAndSerialize();

    std::cout << "All tests passed." << std::endl;
    return 0;
}
