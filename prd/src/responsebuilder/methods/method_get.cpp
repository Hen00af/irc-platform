#include "method.hpp"

bool readFile(const std::string& path, std::string& body)
{
    std::ifstream file(path.c_str());
    std::string   line;

    if (!file.is_open())
        return false;

    body.clear();

    while (std::getline(file, line))
    {
        body += line;
        body += "\n";
    }

    return true;
}

HttpResponse handleGetFile(const std::string path)
{
    HttpResponse res;
    std::string body;

    if(!readFile(path, body)) 
        return buildErrorResponse(403);

    res.status_code = 200;
    res.reason_phrase = getReasonPhrase(200);
    res.body = body;

    setBasicHeaders(res);

    return res;
}

HttpResponse handleGet(const HttpRequest& req, const ResponseContext& ctx) {
    HttpResponse res;
    res = handleGetFile(ctx.file_path);

    return res;
}
