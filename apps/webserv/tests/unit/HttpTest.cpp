#include "Webserv.hpp"

#include <iostream>

static int g_failures = 0;

static void expect(bool condition, const std::string &message) {
    if (condition)
        return;
    std::cerr << "FAIL: " << message << std::endl;
    ++g_failures;
}

int main() {
    Request request;
    ParseResult result = parseRequest("GET / HTTP/1.1\r\nHost: test\r\n", 1024, request);
    expect(result == REQUEST_INCOMPLETE, "partial headers should be incomplete");

    result = parseRequest("GET /hello?q=42 HTTP/1.1\r\nHost: test\r\n\r\n",
                          1024, request);
    expect(result == REQUEST_OK, "valid GET should parse");
    expect(request.method == "GET", "method should be parsed");
    expect(request.path == "/hello", "path should exclude query");
    expect(request.query == "q=42", "query should be parsed");

    request = Request();
    result = parseRequest("GET / HTTP/1.1\r\n\r\n", 1024, request);
    expect(result == REQUEST_BAD, "HTTP/1.1 request without Host should be bad");

    request = Request();
    result = parseRequest("GET / HTTP/2.0\r\nHost: test\r\n\r\n", 1024, request);
    expect(result == REQUEST_VERSION_NOT_SUPPORTED,
           "unsupported HTTP version should be distinguished");

    request = Request();
    result = parseRequest("POST /upload HTTP/1.1\r\nHost: test\r\n\r\n",
                          1024, request);
    expect(result == REQUEST_BAD, "POST without body length should be bad");

    request = Request();
    result = parseRequest(
        "POST /upload HTTP/1.1\r\nHost: test\r\nContent-Length: 5\r\n\r\nhello",
        4, request);
    expect(result == REQUEST_TOO_LARGE, "body over configured limit should be rejected");

    request = Request();
    result = parseRequest(
        "POST /upload HTTP/1.1\r\nHost: test\r\n"
        "Transfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n",
        1024, request);
    expect(result == REQUEST_OK, "complete chunked body should parse");
    expect(request.body == "hello", "chunked body should be decoded");

    request = Request();
    result = parseRequest(
        "POST /upload HTTP/1.1\r\nHost: test\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5;name=value\r\nhello\r\n0\r\nX-Trailer: yes\r\n\r\n",
        1024, request);
    expect(result == REQUEST_OK, "chunk extensions and trailers should parse");
    expect(request.body == "hello", "extended chunked body should be decoded");

    request = Request();
    result = parseRequest(
        "POST /upload HTTP/1.1\r\nHost: test\r\n"
        "Transfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n"
        "0\r\n\r\n",
        1024, request);
    expect(result == REQUEST_BAD,
           "Transfer-Encoding with Content-Length should be rejected");

    request = Request();
    result = parseRequest(
        "POST /upload HTTP/1.1\r\nHost: test\r\n"
        "Transfer-Encoding: gzip\r\n\r\n",
        1024, request);
    expect(result == REQUEST_BAD, "unsupported Transfer-Encoding should be rejected");

    request = Request();
    result = parseRequest(
        "GET / HTTP/1.1\r\nHost: test\r\nBad Header: value\r\n\r\n",
        1024, request);
    expect(result == REQUEST_BAD, "invalid header name should be rejected");

    request = Request();
    result = parseRequest(
        "POST / HTTP/1.1\r\nHost: test\r\n"
        "Content-Length: 999999999999999999999999999999\r\n\r\n",
        1024, request);
    expect(result == REQUEST_BAD, "overflowing Content-Length should be rejected");

    request = Request();
    result = parseRequest(
        "GET /%0d%0aInjected HTTP/1.1\r\nHost: test\r\n\r\n",
        1024, request);
    expect(result == REQUEST_BAD,
           "percent-encoded control characters should be rejected");

    request = Request();
    result = parseRequest("GET /bad%2 HTTP/1.1\r\nHost: test\r\n\r\n",
                          1024, request);
    expect(result == REQUEST_BAD, "incomplete percent encoding should be rejected");

    request = Request();
    result = parseRequest(
        "GET /safe/./file..txt HTTP/1.1\r\nHost: test\r\n\r\n",
        1024, request);
    expect(result == REQUEST_OK, "safe dot-like filename should be accepted");
    expect(request.path == "/safe/file..txt", "dot segment should be normalized");

    request = Request();
    result = parseRequest(
        "GET /safe/%2e%2e/secret HTTP/1.1\r\nHost: test\r\n\r\n",
        1024, request);
    expect(result == REQUEST_BAD, "encoded parent traversal should be rejected");

    Response response(200);
    response.headers["Content-Type"] = "text/plain";
    response.body = "hello";
    const std::string serialized = response.serialize();
    expect(serialized.find("HTTP/1.1 200 OK\r\n") == 0,
           "response should start with status line");
    expect(serialized.find("Content-Length: 5\r\n") != std::string::npos,
           "response should include Content-Length");
    expect(serialized.find("\r\n\r\nhello") != std::string::npos,
           "response should separate headers and body");
    expect(serialized.find("x-content-type-options: nosniff\r\n") !=
               std::string::npos,
           "response should refuse content sniffing");
    expect(serialized.find("frame-ancestors 'none'") != std::string::npos,
           "response should ship a content security policy");
    expect(serialized.find("x-frame-options: DENY\r\n") != std::string::npos,
           "response should refuse framing");

    // A CGI script that sets its own policy keeps it, and does not end up with
    // the default alongside it.
    Response scripted(200);
    scripted.headers["Content-Security-Policy"] = "default-src 'none'";
    const std::string scriptedOut = scripted.serialize();
    expect(scriptedOut.find("default-src 'none'") != std::string::npos,
           "a policy set by the response should survive");
    expect(scriptedOut.find("frame-ancestors 'none'") == std::string::npos,
           "the default policy should not be added twice");

    if (g_failures != 0)
        return 1;
    std::cout << "HTTP tests passed" << std::endl;
    return 0;
}
