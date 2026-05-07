#ifndef REQUEST_HPP
#define REQUEST_HPP
#include <cstddef>
#include <map>
#include <string>

typedef struct HttpRequest {
    std::string method;          // "GET"
    std::string request_target;  // "/"
    std::string http_version;    // "HTTP/1.1"

    std::map<std::string, std::string> headers;
    std::string body;
} HttpRequest;

typedef struct HttpRequestRange {
    size_t request_line_start;
    size_t request_line_end;

    size_t method_start;
    size_t method_end;

    size_t target_start;
    size_t target_end;

    size_t version_start;
    size_t version_end;

    size_t headers_start;
    size_t headers_end;

    size_t body_start;
    size_t body_end;
} HttpRequestRange;

class Request {
public:
    Request();
    ~Request();

    bool parse(const std::string& raw_request);
    const HttpRequest& getRequest() const;

private:
    HttpRequest _request;

    bool parseRequestLine(const std::string& line);
    bool parseHeaders(const std::string& header_part);
    void parseBody(const std::string& body_part);
};

# endif // PERSING_REQUEST_HPP