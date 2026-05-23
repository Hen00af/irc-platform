#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

# include <string>
# include <map>

struct HttpResponse
{
    int status_code;
    std::string reason_phrase;
    std::map<std::string, std::string> headers;
    std::string body;
};

#endif