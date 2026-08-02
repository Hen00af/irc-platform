#include "StaticHandler.hpp"
#include "FileSystem.hpp"
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
        if (body.size() > 1024 * 1024) {
            closedir(directory);
            return false;
        }
    }
    closedir(directory);
    body += "</ul></body></html>";
    return true;
}

Response StaticHandler::handle(const Request &request, const RouteResult &route,
                               const ServerConfig &config) {
    const std::string &root =
        route.location ? route.location->root : config.root;
    std::string resolved;
    const FileResult resolvedResult =
        FileSystem::resolveExisting(root, route.diskPath, resolved);
    if (resolvedResult == FILE_NOT_FOUND)
        return ResponseFactory::error(404, config);
    if (resolvedResult != FILE_OK)
        return ResponseFactory::error(403, config);

    struct stat info;
    if (stat(resolved.c_str(), &info) != 0)
        return ResponseFactory::error(403, config);

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
        std::string resolvedIndex;
        const FileResult indexResult =
            index.empty() ? FILE_NOT_FOUND
                          : FileSystem::resolveExisting(root, filePath, resolvedIndex);
        if (indexResult != FILE_OK) {
            if (indexResult == FILE_FORBIDDEN)
                return ResponseFactory::error(403, config);
            const bool autoindex =
                route.location ? route.location->autoindex : config.autoindex;
            if (!autoindex)
                return ResponseFactory::error(403, config);
            Response response(200);
            if (!directoryListing(resolved, request.path, response.body))
                return ResponseFactory::error(403, config);
            response.headers["Content-Type"] = "text/html; charset=utf-8";
            return response;
        }
        resolved = resolvedIndex;
        if (stat(resolved.c_str(), &info) != 0)
            return ResponseFactory::error(403, config);
    }
    if (!S_ISREG(info.st_mode))
        return ResponseFactory::error(403, config);
    Response response(200);
    const FileResult readResult =
        FileSystem::readRegular(resolved, response.body);
    if (readResult == FILE_TOO_LARGE)
        return ResponseFactory::error(413, config);
    if (readResult != FILE_OK)
        return ResponseFactory::error(403, config);
    // Whatever is inside an upload directory was written by a client, so it is
    // handed back as an opaque download instead of by extension. Serving an
    // uploaded .html as text/html would run it as a page of this origin.
    if (route.location && !route.location->uploadDir.empty()) {
        response.headers["Content-Type"] = "application/octet-stream";
        response.headers["Content-Disposition"] = "attachment";
    } else {
        response.headers["Content-Type"] = mimeType(filePath);
    }
    return response;
}
