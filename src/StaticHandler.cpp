#include "StaticHandler.hpp"
#include "ResponseFactory.hpp"

static std::string mimeType(const std::string &path) {
    size_t dot = path.rfind('.');
    const std::string ext = dot == std::string::npos ? "" : lower(path.substr(dot));
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

static bool readFile(const std::string &path, std::string &body) {
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file)
        return false;
    std::ostringstream content;
    content << file.rdbuf();
    body = content.str();
    return true;
}

static std::string escapeHtml(const std::string &text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '&') out += "&amp;";
        else if (text[i] == '<') out += "&lt;";
        else if (text[i] == '>') out += "&gt;";
        else if (text[i] == '"') out += "&quot;";
        else out += text[i];
    }
    return out;
}

static bool directoryListing(const std::string &disk, const std::string &uri,
                             std::string &body) {
    DIR *directory = opendir(disk.c_str());
    if (!directory)
        return false;
    body = "<!doctype html><html><body><h1>Index of " + escapeHtml(uri) +
           "</h1><ul>";
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        std::string name(entry->d_name);
        if (name == ".")
            continue;
        body += "<li><a href=\"" + escapeHtml(name) + "\">" +
                escapeHtml(name) + "</a></li>";
    }
    closedir(directory);
    body += "</ul></body></html>";
    return true;
}

Response StaticHandler::handle(const Request &request, const RouteResult &route,
                               const ServerConfig &config) {
    struct stat info;
    if (stat(route.diskPath.c_str(), &info) != 0)
        return ResponseFactory::error(404, config);

    std::string filePath = route.diskPath;
    if (S_ISDIR(info.st_mode)) {
        if (request.path.empty() || request.path[request.path.size() - 1] != '/') {
            Response response(301);
            response.headers["Location"] =
                request.path + "/" +
                (request.query.empty() ? "" : "?" + request.query);
            return response;
        }
        const std::string index = route.location ? route.location->index : config.index;
        if (!index.empty() && filePath[filePath.size() - 1] != '/')
            filePath += "/";
        filePath += index;
        if (index.empty() || stat(filePath.c_str(), &info) != 0) {
            const bool autoindex =
                route.location ? route.location->autoindex : config.autoindex;
            if (!autoindex)
                return ResponseFactory::error(403, config);
            Response response(200);
            if (!directoryListing(route.diskPath, request.path, response.body))
                return ResponseFactory::error(403, config);
            response.headers["Content-Type"] = "text/html; charset=utf-8";
            return response;
        }
    }
    if (!S_ISREG(info.st_mode))
        return ResponseFactory::error(403, config);
    Response response(200);
    if (!readFile(filePath, response.body))
        return ResponseFactory::error(403, config);
    response.headers["Content-Type"] = mimeType(filePath);
    return response;
}
