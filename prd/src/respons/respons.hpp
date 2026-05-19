struct HttpResponse {
    int status_code;
    std::string reason_phase;
    std::map<std::string, std::string> headers;
    std::string body;
};

