
#include "persing_request.hpp"
#include <sstream>
#include <iostream>

Request::Request() {
}

Request::~Request() {
}

static bool is_allowed_method(const std::string& method) {
    return  method == "GET"
        ||  method == "POST"
        ||  method == "DELETE";
}
bool parse_requestline(const std::string& raw_request, HttpRequest& req) {
    size_t request_line_end = raw_request.find("\r\n");
    if (request_line_end == std::string::npos)
        return false;
    std::string request_line = raw_request.substr(0, request_line_end);
    std::istringstream request_stream(request_line);

    if (!(request_stream >> req.method >> req.request_target >> req.http_version))
        return false;
    if (req.http_version == "HTTP/1.1") 
        return false;
    if (is_allowed_method(req.method))
        return false;
    if (request_target == )
}

bool parse_request(const std::string& raw_request, HttpRequest& req)
{
    size_t header_end = raw_request.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return false;

    // std::string header_lines =
    //     raw_request.substr(request_line_end + 2,
    //                        header_end - (request_line_end + 2));



    if()

    std::string extra;
    if (request_stream >> extra)
        return false;

    std::istringstream header_stream(header_lines);
    std::string line;

    std::cout << "=== headers ===" << std::endl;

    while (std::getline(header_stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        std::cout << line << std::endl;
    }

    return true;
}

int main() {
    std::string raw_request =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    HttpRequest req;

    if (!parse_request(raw_request, req)) {
        std::cerr << "parse error" << std::endl;
        return 1;
    }

    std::cout << "method: " << req.method << std::endl;
    std::cout << "target: " << req.request_target << std::endl;
    std::cout << "version: " << req.http_version << std::endl;

    return 0;
}