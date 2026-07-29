#include "Dispatcher.hpp"

#include <iostream>

static int g_failures = 0;

static void expect(bool condition, const std::string &message) {
    if (condition)
        return;
    std::cerr << "FAIL: " << message << std::endl;
    ++g_failures;
}

static void writeFile(const std::string &path, const std::string &body) {
    std::ofstream file(path.c_str(), std::ios::out | std::ios::binary);
    file << body;
}

static Request makeRequest(const std::string &method, const std::string &path) {
    Request request;
    request.method = method;
    request.path = path;
    return request;
}

static void cleanup(const std::string &root) {
    unlink((root + "/uploads/new.txt").c_str());
    unlink((root + "/hello.txt").c_str());
    unlink((root + "/404.html").c_str());
    rmdir((root + "/uploads").c_str());
    rmdir(root.c_str());
}

int main() {
    const std::string root =
        "/tmp/webserv-dispatcher-test-" + toString(static_cast<size_t>(getpid()));
    cleanup(root);
    expect(mkdir(root.c_str(), 0700) == 0, "fixture root should be created");
    expect(mkdir((root + "/uploads").c_str(), 0700) == 0,
           "fixture upload directory should be created");
    writeFile(root + "/hello.txt", "hello dispatcher\n");
    writeFile(root + "/404.html", "custom not found\n");

    ServerConfig config;
    config.root = root;
    config.index = "";
    config.methods.clear();
    config.methods.push_back("GET");
    config.errorPages[404] = root + "/404.html";

    Location upload;
    upload.path = "/upload";
    upload.root = root + "/uploads";
    upload.uploadDir = root + "/uploads";
    upload.autoindex = true;
    upload.methods.clear();
    upload.methods.push_back("GET");
    upload.methods.push_back("POST");
    upload.methods.push_back("DELETE");
    config.locations.push_back(upload);

    Location redirect;
    redirect.path = "/old";
    redirect.redirect = "/new";
    redirect.methods.clear();
    redirect.methods.push_back("GET");
    config.locations.push_back(redirect);

    Response response = Dispatcher::dispatch(makeRequest("GET", "/hello.txt"), config);
    expect(response.status == 200, "GET file should return 200");
    expect(response.body == "hello dispatcher\n", "GET should preserve file body");
    expect(response.headers["Content-Type"] == "text/plain; charset=utf-8",
           "GET should determine MIME type");

    response = Dispatcher::dispatch(makeRequest("GET", "/missing"), config);
    expect(response.status == 404, "missing file should return 404");
    expect(response.body == "custom not found\n", "404 should use configured error page");

    response = Dispatcher::dispatch(makeRequest("GET", "/upload"), config);
    expect(response.status == 301, "directory without trailing slash should redirect");
    expect(response.headers["Location"] == "/upload/",
           "directory redirect should append trailing slash");

    response = Dispatcher::dispatch(makeRequest("GET", "/upload/"), config);
    expect(response.status == 200, "autoindex directory should return 200");
    expect(response.body.find("Index of /upload/") != std::string::npos,
           "autoindex should contain request path");

    Request post = makeRequest("POST", "/upload");
    post.headers["x-filename"] = "new.txt";
    post.body = "uploaded body\n";
    response = Dispatcher::dispatch(post, config);
    expect(response.status == 201, "valid upload should return 201");
    expect(response.headers["Location"] == "/upload/new.txt",
           "upload should return resource location");

    response = Dispatcher::dispatch(makeRequest("GET", "/upload/new.txt"), config);
    expect(response.status == 200, "uploaded file should be readable");
    expect(response.body == "uploaded body\n", "uploaded body should be preserved");

    response = Dispatcher::dispatch(makeRequest("DELETE", "/upload/new.txt"), config);
    expect(response.status == 204, "DELETE file should return 204");
    response = Dispatcher::dispatch(makeRequest("GET", "/upload/new.txt"), config);
    expect(response.status == 404, "deleted file should no longer exist");

    post.headers["x-filename"] = "../escape.txt";
    response = Dispatcher::dispatch(post, config);
    expect(response.status == 400, "unsafe upload filename should return 400");

    response = Dispatcher::dispatch(makeRequest("PUT", "/hello.txt"), config);
    expect(response.status == 405, "disallowed method should return 405");
    expect(response.headers["Allow"] == "GET", "405 should include Allow header");

    response = Dispatcher::dispatch(makeRequest("GET", "/old"), config);
    expect(response.status == 301, "configured redirect should return 301");
    expect(response.headers["Location"] == "/new",
           "redirect should include Location header");

    cleanup(root);
    if (g_failures != 0)
        return 1;
    std::cout << "Dispatcher tests passed" << std::endl;
    return 0;
}
