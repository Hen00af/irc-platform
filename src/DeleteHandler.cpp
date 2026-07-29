#include "DeleteHandler.hpp"
#include "ResponseFactory.hpp"

Response DeleteHandler::handle(const RouteResult &route,
                               const ServerConfig &config) {
    struct stat info;
    if (stat(route.diskPath.c_str(), &info) != 0)
        return ResponseFactory::error(404, config);
    if (!S_ISREG(info.st_mode) || unlink(route.diskPath.c_str()) != 0)
        return ResponseFactory::error(403, config);
    return Response(204);
}
