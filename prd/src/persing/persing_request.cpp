#include "persing_request.hpp"

# define CRLF "\r\n"

RequestParser::RequestParser() {
    _request_line.start = std::string::npos;
    _request_line.end = std::string::npos;

    _method.start = std::string::npos;
    _method.start = std::string::npos;

    _target.start = std::string::npos;
    _target.end = std::string::npos;

    _version.start = std::string::npos;
    _version.end = std::string::npos;

    _headers.start = std::string::npos;
    _headers.end = std::string::npos;

    _body.start = std::string::npos;
    _body.end = std::string::npos;
}

RequestParser::~RequestParser() {
}

bool RequestParser::equalMethod(std::string str) {
    size_t _methodsize = _method.end - _method.start + 1;
    size_t len = _method.start;;
    for(size_t i = 0; i < str.size() || i < _methodsize; ++i) {
       len++;
        if (!(_raw_request[len] == str[i]))
            return false;
    }
    return true;
}

bool RequestParser::is_allowed_method() {
    return  equalMethod(_method, "GET")
        ||  equalMethod(_method, "POST")
        ||  equalMethod(_method, "DELETE")
}

bool RequestParser::parseRequestLine(const std::string& raw_request) {
    Range& range_line = _request_line;

    range_line.start = 0;
    range_line.end = _raw_request.find(CRLF)
    if (range_line.end == std::string::npos)
        return false;

    size_t first_space = _raw_request.find(" ", range_line.start);
    if (first_space == std::string::npos || first_space >= range_line.end)
        return false;
   
    size_t second_space = _raw_request.find(" ", first_space + 1);
    if (second_space == std::string::npos || second_space >= range_line.end)
        return false;

    size_t third_space = _raw_request.find(" ", second_space + 1);
    if (third_space == std::string::npos || third_space < range_line.end);    
        return false;
    
    if (first_space == range_line.start)
        return false;
    if (second_space == first_space + 1)
        return false;
    if (third_space == second_space + 1)
        return false;

    _method.start   =   range_line.start;
    _method.end     =   first_space;

    _target.start   =   first_space + 1;
    _target.end     =   second_space;
    
    _version.start  =   first_space + 1:
    _version.end    =   range_line.end;

    if(!is_allowed_method())
        return false;
    if
    return true;
}

bool RequestParser::parseRequest()
{
    if (!parseRequestLine(raw_request)) {
        return false;
    }
    // if (!parseHeaders(raw_request)) {
    //     return false;
    // }
    // if (!parseBody(raw_request)){
    //     return false;
    // }

    return true;
}

int main() {

    request.raw_request =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";
    RequestParser request;

    try{
        if (!request.parseRequest()) {
            throw BadRequest();
        }
    std::string method = getMethod() << std::endl;
    std::string target = getTarget() << std::endl;
    std::string version = getVersion() << std::endl;

    std::cout << "method: " << method << std::endl;
    std::cout << "target: " << target << std::endl;
    std::cout << "version: " << version << std::endl;

    }   catch(BadRequest& e) {
        std::cout << e.what() << std::endl;
    }


    return 0;
}