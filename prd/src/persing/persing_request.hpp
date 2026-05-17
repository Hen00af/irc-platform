#ifndef REQUEST_HPP
#define REQUEST_HPP
#include <cstddef>
#include <exception>
#include <map>
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

    std::string slice(const Range& range) const;
    std::string getMethod() const;
    std::string getTarget() const;
    std::string getVersion() const;
    std::string getHeader(const std::string& key) const;

private:
    std::string _raw_request;

    Range _request_line;
    Range _method;
    Range _target;
    Range _version;
    Range _headers_range;
    Range _body;

    std::map<std::string, Range> _headers;

    bool parseRequestLine();
    bool parseHeaders();
    bool parseBody();
    bool equalsRange(const Range& range, const std::string& expected) const;
    bool isAllowedMethod() const;
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

#endif // REQUEST_HPP
