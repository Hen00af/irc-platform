#include <iostream>
#include <string>

struct range
{
    size_t start;
    size_t end;
};

HttpResponse handle_request(const HttpRequest& req, const Conf& cond)  //
{
}

int main()
{
    std::string _raw_request;
    _raw_request = "GET / HTTP/1.1 \r\n\r\n";

    range method;

    method.start = _raw_request.find("GET");
    method.end = method.start + 3;

    std::string tmp = _raw_request.substr(method.start, method.end - method.start);

    std::cout << tmp << std::endl;

    return 0;
}