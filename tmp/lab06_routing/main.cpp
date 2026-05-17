#include "routing.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_pass = 0;
int g_fail = 0;

void check(const char* label, bool ok)
{
    if (ok) {
        ++g_pass;
        std::cout << "  [OK]   " << label << "\n";
    } else {
        ++g_fail;
        std::cout << "  [FAIL] " << label << "\n";
    }
}

LocationConf make_loc(const std::string& dir,
                     const std::string& root,
                     const char* m1 = 0,
                     const char* m2 = 0)
{
    LocationConf l;
    l.dir = dir;
    l.root = root;
    if (m1) l.methods.push_back(m1);
    if (m2) l.methods.push_back(m2);
    return l;
}

std::vector<ServerConf> fixture()
{
    std::vector<ServerConf> v;

    ServerConf a;
    a.name = "example.com";
    a.root = "/var/www/example";
    a.locations.push_back(make_loc("/", "", "GET"));
    a.locations.push_back(make_loc("/api", "/srv/api", "GET", "POST"));
    a.locations.push_back(make_loc("/api/v2", "/srv/api/v2", "GET"));
    v.push_back(a);

    ServerConf b;
    b.name = "other.test";
    b.root = "/var/www/other";
    b.locations.push_back(make_loc("/", "", "GET"));
    v.push_back(b);

    return v;
}

void test_pick_server()
{
    std::cout << "== pick_server ==\n";
    std::vector<ServerConf> s = fixture();

    const ServerConf* hit = pick_server(s, "example.com");
    check("Host=example.com -> example.com", hit && hit->name == "example.com");

    const ServerConf* hit2 = pick_server(s, "other.test");
    check("Host=other.test -> other.test", hit2 && hit2->name == "other.test");

    const ServerConf* miss = pick_server(s, "no-such.host");
    check("unknown Host -> first server (default)",
          miss && miss->name == "example.com");

    std::vector<ServerConf> empty;
    check("empty server list -> null", pick_server(empty, "x") == 0);
}

void test_pick_location()
{
    std::cout << "== pick_location ==\n";
    std::vector<ServerConf> s = fixture();
    const ServerConf& srv = s[0];

    const LocationConf* l1 = pick_location(srv, "/index.html");
    check("/index.html -> /", l1 && l1->dir == "/");

    const LocationConf* l2 = pick_location(srv, "/api/users");
    check("/api/users -> /api", l2 && l2->dir == "/api");

    const LocationConf* l3 = pick_location(srv, "/api/v2/items");
    check("/api/v2/items -> /api/v2 (longest match)",
          l3 && l3->dir == "/api/v2");

    const LocationConf* l4 = pick_location(srv, "/apix");
    check("/apix does NOT match /api (segment boundary)",
          l4 && l4->dir == "/");
}

void test_route_ok()
{
    std::cout << "== route OK ==\n";
    std::vector<ServerConf> s = fixture();

    RouteResult r = route(s, "GET", "/api/v2/items", "example.com");
    check("status ROUTE_OK", r.status == ROUTE_OK);
    check("server = example.com",
          r.server && r.server->name == "example.com");
    check("location = /api/v2",
          r.location && r.location->dir == "/api/v2");
    check("fs_path = /srv/api/v2/items",
          r.fs_path == "/srv/api/v2/items");

    RouteResult r2 = route(s, "GET", "/", "example.com");
    check("'/' -> location '/' uses server.root",
          r2.status == ROUTE_OK && r2.fs_path == "/var/www/example/");
}

void test_route_errors()
{
    std::cout << "== route errors ==\n";
    std::vector<ServerConf> s = fixture();

    RouteResult r = route(s, "DELETE", "/api/users", "example.com");
    check("DELETE on /api -> 405 hint",
          r.status == ROUTE_METHOD_NOT_ALLOWED);

    std::vector<ServerConf> nodefault;
    ServerConf x;
    x.name = "only.test";
    nodefault.push_back(x);
    RouteResult r2 = route(nodefault, "GET", "/foo", "only.test");
    check("no matching location -> ROUTE_NOT_FOUND",
          r2.status == ROUTE_NOT_FOUND);
}

}  // namespace

int main()
{
    test_pick_server();
    test_pick_location();
    test_route_ok();
    test_route_errors();

    std::cout << "\nresult: " << g_pass << " passed, "
              << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
