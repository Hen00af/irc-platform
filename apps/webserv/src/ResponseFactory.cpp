#include "ResponseFactory.hpp"

static bool readErrorPage(const std::string &path, std::string &body) {
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file)
        return false;
    std::ostringstream content;
    content << file.rdbuf();
    body = content.str();
    return true;
}

Response ResponseFactory::error(int status, const ServerConfig &config) {
    Response response(status);
    std::map<int, std::string>::const_iterator page = config.errorPages.find(status);
    if (page != config.errorPages.end())
        readErrorPage(page->second, response.body);
    if (response.body.empty())
        response.body = "<!doctype html><html><body><h1>" + toString(status) + " " +
                        reasonPhrase(status) + "</h1></body></html>";
    response.headers["Content-Type"] = "text/html; charset=utf-8";
    return response;
}
