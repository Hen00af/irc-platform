#include "CgiHandler.hpp"

#include <iostream>

static int g_failures = 0;

static void expect(bool condition, const std::string &message) {
    if (condition)
        return;
    std::cerr << "FAIL: " << message << std::endl;
    ++g_failures;
}

int main() {
    ServerConfig config;
    Location location;
    location.cgiExtension = ".py";
    location.cgiPath = "/usr/bin/python3";
    location.cgiHandlers[".py"] = "/usr/bin/python3";
    location.cgiHandlers[".php"] = "/usr/bin/php-cgi";
    RouteResult route;
    route.location = &location;
    route.diskPath = "www/cgi-bin/echo.py";

    expect(CgiHandler::matches(route), "configured CGI extension should match");
    route.diskPath = "www/cgi-bin/echo.txt";
    expect(!CgiHandler::matches(route), "different extension should not match");
    route.diskPath = "www/cgi-bin/hello.php";
    expect(CgiHandler::matches(route), "second CGI extension should match");

    Response response = CgiHandler::makeResponse(
        "Status: 201 Created\r\nContent-Type: text/plain\r\n"
        "X-Test: yes\r\nContent-Length: 999\r\n\r\ncreated\n",
        config);
    expect(response.status == 201, "CGI Status header should set response status");
    expect(response.headers["Content-Type"] == "text/plain",
           "CGI Content-Type should be preserved");
    expect(response.headers["X-Test"] == "yes",
           "CGI extension header should be preserved");
    expect(!response.headers.count("Content-Length"),
           "CGI Content-Length should be regenerated");
    expect(response.body == "created\n", "CGI body should be preserved");

    response = CgiHandler::makeResponse("invalid output", config);
    expect(response.status == 500, "malformed CGI output should return 500");

    if (g_failures != 0)
        return 1;
    std::cout << "CGI handler tests passed" << std::endl;
    return 0;
}
