#include "DeleteHandler.hpp"
#include "Dispatcher.hpp"
#include "ResponseFactory.hpp"
#include "Router.hpp"
#include "StaticHandler.hpp"
#include "UploadHandler.hpp"

Response Dispatcher::dispatch(const Request &request, const ServerConfig &config) {
    const RouteResult route = Router::resolve(request, config);
    if (route.status == ROUTE_METHOD_NOT_ALLOWED) {
        Response response = ResponseFactory::error(405, config);
        std::string allow;
        for (size_t i = 0; i < route.methods->size(); ++i)
            allow += (i ? ", " : "") + (*route.methods)[i];
        response.headers["Allow"] = allow;
        return response;
    }
    if (route.status == ROUTE_REDIRECT) {
        Response response(route.location->redirectStatus);
        response.headers["Location"] = route.redirect;
        return response;
    }
    if (request.method == "DELETE")
        return DeleteHandler::handle(route, config);
    if (request.method == "POST")
        return UploadHandler::handle(request, route, config);
    return StaticHandler::handle(request, route, config);
}
