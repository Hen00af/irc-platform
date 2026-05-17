#include "routing.hpp"

#include <cstddef>

const ServerConf* pick_server(
    const std::vector<ServerConf>& servers,
    const std::string& host)
{
    if (servers.empty()) return 0;
    for (std::size_t i = 0; i < servers.size(); ++i) {
        if (servers[i].name == host) return &servers[i];
    }
    return &servers[0];
}

const LocationConf* pick_location(
    const ServerConf& server,
    const std::string& target)
{
    const LocationConf* best = 0;
    std::size_t best_len = 0;
    for (std::size_t i = 0; i < server.locations.size(); ++i) {
        const std::string& dir = server.locations[i].dir;
        if (dir.empty()) continue;
        if (target.compare(0, dir.size(), dir) != 0) continue;
        if (dir.size() < target.size()
            && dir[dir.size() - 1] != '/'
            && target[dir.size()] != '/')
            continue;
        if (dir.size() > best_len) {
            best = &server.locations[i];
            best_len = dir.size();
        }
    }
    return best;
}

static bool method_allowed(const LocationConf& loc, const std::string& method)
{
    if (loc.methods.empty()) return method == "GET";
    for (std::size_t i = 0; i < loc.methods.size(); ++i) {
        if (loc.methods[i] == method) return true;
    }
    return false;
}

static std::string join_path(const std::string& root, const std::string& tail)
{
    if (root.empty()) return tail;
    bool root_slash = root[root.size() - 1] == '/';
    bool tail_slash = !tail.empty() && tail[0] == '/';
    if (root_slash && tail_slash) return root + tail.substr(1);
    if (!root_slash && !tail_slash) return root + "/" + tail;
    return root + tail;
}

RouteResult route(
    const std::vector<ServerConf>& servers,
    const std::string& method,
    const std::string& target,
    const std::string& host)
{
    RouteResult r;
    r.status = ROUTE_NOT_FOUND;
    r.server = 0;
    r.location = 0;

    r.server = pick_server(servers, host);
    if (!r.server) return r;

    r.location = pick_location(*r.server, target);
    if (!r.location) return r;

    if (!method_allowed(*r.location, method)) {
        r.status = ROUTE_METHOD_NOT_ALLOWED;
        return r;
    }

    std::string tail = target.substr(r.location->dir.size());
    const std::string& root = r.location->root.empty()
        ? r.server->root
        : r.location->root;
    r.fs_path = join_path(root, tail);
    r.status = ROUTE_OK;
    return r;
}

const char* route_status_str(RouteStatus s)
{
    switch (s) {
    case ROUTE_OK:                  return "ROUTE_OK";
    case ROUTE_NOT_FOUND:           return "ROUTE_NOT_FOUND";
    case ROUTE_METHOD_NOT_ALLOWED:  return "ROUTE_METHOD_NOT_ALLOWED";
    }
    return "ROUTE_UNKNOWN";
}
