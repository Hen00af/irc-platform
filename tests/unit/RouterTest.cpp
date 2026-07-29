#include "Router.hpp"

#include <iostream>

static int g_failures = 0;

static void expect(bool condition, const std::string &message) {
    if (condition)
        return;
    std::cerr << "FAIL: " << message << std::endl;
    ++g_failures;
}

static Location makeLocation(const std::string &path, const std::string &root) {
    Location location;
    location.path = path;
    location.root = root;
    location.methods.clear();
    location.methods.push_back("GET");
    return location;
}

int main() {
    ServerConfig config;
    config.root = "www";
    config.methods.clear();
    config.methods.push_back("GET");
    config.locations.push_back(makeLocation("/assets", "public"));
    config.locations.push_back(makeLocation("/assets/images", "images"));

    Request request;
    request.method = "GET";
    request.path = "/index.html";
    RouteResult route = Router::resolve(request, config);
    expect(route.status == ROUTE_READY, "server route should be ready");
    expect(route.location == NULL, "server route should have no location");
    expect(route.diskPath == "www/index.html", "server root should prefix request path");

    request.path = "/assets/images/logo.png";
    route = Router::resolve(request, config);
    expect(route.location == &config.locations[1], "longest location should win");
    expect(route.diskPath == "images/logo.png", "location prefix should be replaced");

    request.path = "/assets-old/file.txt";
    route = Router::resolve(request, config);
    expect(route.location == NULL, "location prefix must end at a path boundary");
    expect(route.diskPath == "www/assets-old/file.txt",
           "boundary mismatch should use server root");

    request.path = "/assets/images/logo.png";
    request.method = "DELETE";
    route = Router::resolve(request, config);
    expect(route.status == ROUTE_METHOD_NOT_ALLOWED, "disallowed method should return 405 route");
    expect(route.methods == &config.locations[1].methods, "route should expose allowed methods");

    config.locations[1].methods.push_back("DELETE");
    config.locations[1].redirect = "/moved";
    route = Router::resolve(request, config);
    expect(route.status == ROUTE_REDIRECT, "redirect should be selected after method validation");
    expect(route.redirect == "/moved", "redirect target should be preserved");

    if (g_failures != 0)
        return 1;
    std::cout << "Router tests passed" << std::endl;
    return 0;
}
