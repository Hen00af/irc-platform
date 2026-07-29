#include "UploadHandler.hpp"
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

    std::ofstream output((route.location->uploadDir + "/" + name).c_str(),
                         std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output)
        return ResponseFactory::error(500, config);
    output.write(request.body.data(), request.body.size());
    if (!output)
        return ResponseFactory::error(500, config);

    Response response(201);
    response.headers["Location"] = request.path + "/" + name;
    response.headers["Content-Type"] = "text/plain; charset=utf-8";
    response.body = "created\n";
    return response;
}
