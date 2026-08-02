#include "UploadHandler.hpp"
#include "FileSystem.hpp"
#include "ResponseFactory.hpp"

// An allow-list, not a deny-list. The name arrives in the X-Filename header,
// so the client picks it: anything that is not a plain [A-Za-z0-9._-] name is
// refused, and a leading dot is refused too, which rules out dotfiles as well
// as "." and "..".
static bool validFileName(const std::string &name) {
    if (name.empty() || name.size() > 128 || name[0] == '.')
        return false;
    for (size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9') || c == '.' || c == '-' ||
                             c == '_';
        if (!allowed)
            return false;
    }
    return true;
}

Response UploadHandler::handle(const Request &request, const RouteResult &route,
                               const ServerConfig &config) {
    if (!route.location || route.location->uploadDir.empty())
        return ResponseFactory::error(403, config);

    const std::string name =
        request.headers.count("x-filename")
            ? request.headers.find("x-filename")->second
            : "upload-" + toString(static_cast<size_t>(std::time(NULL))) + ".bin";
    if (!validFileName(name))
        return ResponseFactory::error(400, config);

    const FileResult result = FileSystem::createExclusive(
        route.location->uploadDir, name, request.body);
    if (result == FILE_CONFLICT)
        return ResponseFactory::error(409, config);
    if (result == FILE_FORBIDDEN)
        return ResponseFactory::error(403, config);
    if (result != FILE_OK)
        return ResponseFactory::error(500, config);

    Response response(201);
    response.headers["Location"] = request.path + "/" + name;
    response.headers["Content-Type"] = "text/plain; charset=utf-8";
    response.body = "created\n";
    return response;
}
