#ifndef PERSING_REQUEST_HPP
#define PERSING_REQUEST_HPP

struct HttpRequest {
    std::string method;          // "GET"
    std::string request_target;  // "/"
    std::string http_version;    // "HTTP/1.1"

    std::map<std::string, std::string> headers;
    std::string body;

};

class RequestParser {
public:
    RequestParser();

    bool parse(const std::string& raw_request);
    const HttpRequest& getRequest() const;

private:
    HttpRequest _request;

    bool parseRequestLine(const std::string& line);
    bool parseHeaders(const std::string& header_part);
    void parseBody(const std::string& body_part);
};

# endif // PERSING_REQUEST_HPP