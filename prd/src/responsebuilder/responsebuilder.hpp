#ifndef RESPONSEBUILDER_CPP
#define RESPONSEBUILDER_CPP

struct HttpResponse {
    int status_code;
    std::string reason_phrase;

    std::map<std::string, std::string> headers;

    std::string body;
};

#endif // RESPONSEBUILDER_CPP