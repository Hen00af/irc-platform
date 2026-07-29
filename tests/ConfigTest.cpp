#include "Webserv.hpp"

#include <iostream>

static int g_failures = 0;

static void expect(bool condition, const std::string &message) {
    if (condition)
        return;
    std::cerr << "FAIL: " << message << std::endl;
    ++g_failures;
}

static void writeConfig(const std::string &path, const std::string &content) {
    std::ofstream file(path.c_str());
    file << content;
}

static bool parseFails(const std::string &path, const std::string &content) {
    writeConfig(path, content);
    try {
        Config::parse(path);
    } catch (const std::exception &) {
        return true;
    }
    return false;
}

int main() {
    const std::string path =
        "/tmp/webserv-config-test-" + toString(static_cast<size_t>(getpid())) + ".conf";

    writeConfig(
        path,
        "# directives may appear after a location\n"
        "server {\n"
        "  listen 127.0.0.1:8081;\n"
        "  location /cgi {\n"
        "    allow_methods GET POST;\n"
        "    cgi_extension .py;\n"
        "    cgi_path /usr/bin/python3;\n"
        "    cgi_timeout 12;\n"
        "    return 302 /moved;\n"
        "  }\n"
        "  root \"site root\";\n"
        "  index home.html;\n"
        "  autoindex on;\n"
        "  allow_methods GET DELETE;\n"
        "  error_page 404 errors/404.html;\n"
        "  error_page 500 errors/500.html;\n"
        "}\n"
        "server { listen 127.0.0.1:8082; }\n");

    try {
        const std::vector<ServerConfig> servers = Config::parse(path);
        expect(servers.size() == 2, "two server blocks should parse");
        expect(servers[0].host == "127.0.0.1", "listen host should parse");
        expect(servers[0].port == 8081, "listen port should parse");
        expect(servers[0].errorPages.size() == 2,
               "multiple error_page directives should parse");
        expect(servers[0].locations.size() == 1, "location should parse");
        const Location &location = servers[0].locations[0];
        expect(location.root == "site root",
               "location should inherit later server root");
        expect(location.index == "home.html",
               "location should inherit later server index");
        expect(location.autoindex, "location should inherit later autoindex");
        expect(location.methods.size() == 2 && location.methods[1] == "POST",
               "explicit location methods should override inheritance");
        expect(location.redirectStatus == 302 && location.redirect == "/moved",
               "redirect status and target should parse");
        expect(location.cgiExtension == ".py" &&
                   location.cgiPath == "/usr/bin/python3",
               "CGI configuration should parse as a pair");
        expect(location.cgiTimeout == 12, "CGI timeout should parse");
    } catch (const std::exception &error) {
        std::cerr << "FAIL: valid configuration threw: " << error.what() << std::endl;
        ++g_failures;
    }

    expect(parseFails(path, "server { listen 8080 root www; }"),
           "missing semicolon should fail");
    expect(parseFails(path, "server { listen 8080; root a; root b; }"),
           "duplicate directive should fail");
    expect(parseFails(path, "server { listen 8080; client_max_body_size -1; }"),
           "negative number should fail");
    expect(parseFails(path,
                      "server { listen 8080; "
                      "client_max_body_size 16777217; }"),
           "body limit above security cap should fail");
    expect(parseFails(path, "server { listen localhost:8080; }"),
           "non-IPv4 listen host should fail");
    expect(parseFails(path,
                      "server { listen 8080; }\nserver { listen 0.0.0.0:8080; }"),
           "duplicate listen address should fail");
    expect(parseFails(path,
                      "server { listen 8080; location /cgi { cgi_extension .py; } }"),
           "incomplete CGI configuration should fail");
    expect(parseFails(path,
                      "server { listen 8080; location /x { return 200 /ok; } }"),
           "non-redirect return status should fail");
    expect(parseFails(path,
                      "server { listen 8080; location /cgi { "
                      "cgi_timeout 0; } }"),
           "zero CGI timeout should fail");
    expect(parseFails(path, "server { root www; }"),
           "missing listen directive should fail");

    unlink(path.c_str());
    if (g_failures != 0)
        return 1;
    std::cout << "Config tests passed" << std::endl;
    return 0;
}
