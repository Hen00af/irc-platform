#include "UploadHandler.hpp"
#include "FileSystem.hpp"
#include "ResponseFactory.hpp"

static bool validFileName(const std::string &name) {
    return !name.empty() && name.find('/') == std::string::npos &&
           name.find("..") == std::string::npos;
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
