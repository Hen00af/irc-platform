#include "Router.hpp"

RouteResult::RouteResult()
    : status(ROUTE_READY), location(NULL), methods(NULL) {}

static bool methodAllowed(const std::vector<std::string> &methods,
                          const std::string &method) {
    for (size_t i = 0; i < methods.size(); ++i)
        if (methods[i] == method)
            return true;
    return false;
}

static const Location *matchLocation(const ServerConfig &config,
                                     const std::string &path) {
    const Location *best = NULL;
    for (size_t i = 0; i < config.locations.size(); ++i) {
        const std::string &prefix = config.locations[i].path;
        const bool boundary =
            prefix == "/" || (!prefix.empty() && prefix[prefix.size() - 1] == '/') ||
            path.size() == prefix.size() ||
            (path.size() > prefix.size() && path[prefix.size()] == '/');
        if (path.compare(0, prefix.size(), prefix) == 0 && boundary &&
            (!best || prefix.size() > best->path.size()))
            best = &config.locations[i];
    }
    return best;
}

RouteResult Router::resolve(const Request &request, const ServerConfig &config) {
    RouteResult result;
    result.location = matchLocation(config, request.path);
    result.methods = result.location ? &result.location->methods : &config.methods;

    if (!methodAllowed(*result.methods, request.method)) {
        result.status = ROUTE_METHOD_NOT_ALLOWED;
        return result;
    }
    if (result.location && !result.location->redirect.empty()) {
        result.status = ROUTE_REDIRECT;
        result.redirect = result.location->redirect;
        return result;
    }

    const std::string &root = result.location ? result.location->root : config.root;
    std::string relative = request.path;
    if (result.location &&
        relative.compare(0, result.location->path.size(), result.location->path) == 0)
        relative.erase(0, result.location->path.size());
    if (relative.empty() || relative[0] != '/')
        relative = "/" + relative;
    result.diskPath = root + relative;
    return result;
}
