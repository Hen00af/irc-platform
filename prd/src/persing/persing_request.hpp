#ifndef REQUEST_HPP
#define REQUEST_HPP
#include <cstddef>
#include <exception>
#include <string>

struct Range {
    size_t start;
    size_t end;
};

class RequestParser {
public:
    RequestParser();
    ~RequestParser();

    bool parseRequest(const std::string& raw_request);
    bool is_allowed_method();
    bool equalMethod();
    std::string getMethod() const;
    std::string getTarget() const;
    std::string getVersion() const;
private:
    Range _request_line;
    Range _method;
    Range _target;
    Range _version;
    Range _headers;
    Range _body;
    std::string _raw_request;

    bool parseRequestLine(const std::string& raw_request);
    bool parseHeaders(const std::string& header_part);
    bool parseBody(const std::string& body_part);
};

/*
** Exceptions
*/

class BadRequest : public std::exception {public:const char* what() const throw(){
        return "400 Bad Request";
    }};
class VersionNotSupported : public std::exception {public:const char* what() const throw(){
        return "505 HTTP Version Not Supported";
    }};

# endif // PERSING_REQUEST_HPP