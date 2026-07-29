#include "DeleteHandler.hpp"
#include "FileSystem.hpp"
#include "ResponseFactory.hpp"

Response DeleteHandler::handle(const RouteResult &route,
                               const ServerConfig &config) {
    const std::string &root =
        route.location ? route.location->root : config.root;
    const FileResult result =
        FileSystem::removeRegular(root, route.diskPath);
    if (result == FILE_NOT_FOUND)
        return ResponseFactory::error(404, config);
    if (result != FILE_OK)
        return ResponseFactory::error(403, config);
    return Response(204);
}
