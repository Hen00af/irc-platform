#include "CgiHandler.hpp"
#include "ResponseFactory.hpp"

extern char **environ;

CgiProcess::CgiProcess() : pid(-1), inputFd(-1), outputFd(-1) {}

static std::string directoryName(const std::string &path) {
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

static std::string baseName(const std::string &path) {
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static std::string headerEnvironmentName(const std::string &header) {
    std::string name = "HTTP_";
    for (size_t i = 0; i < header.size(); ++i) {
        char c = header[i];
        if (c == '-')
            c = '_';
        else if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        name += c;
    }
    return name;
}

static void addEnvironment(std::vector<std::string> &environment,
                           const std::string &name, const std::string &value) {
    environment.push_back(name + "=" + value);
}

static std::string absolutePath(const std::string &path) {
    if (!path.empty() && path[0] == '/')
        return path;
    char current[4096];
    if (!getcwd(current, sizeof(current)))
        return path;
    return std::string(current) + "/" + path;
}

static std::string serverName(const Request &request,
                              const ServerConfig &config) {
    std::map<std::string, std::string>::const_iterator host =
        request.headers.find("host");
    if (host == request.headers.end())
        return config.host;
    const size_t colon = host->second.rfind(':');
    return colon == std::string::npos ? host->second
                                     : host->second.substr(0, colon);
}

static void setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("cannot make CGI pipe non-blocking");
}

static void setResourceLimit(int resource, rlim_t value) {
    struct rlimit limit;
    limit.rlim_cur = value;
    limit.rlim_max = value;
    setrlimit(resource, &limit);
}

static std::string selectInterpreter(const RouteResult &route) {
    if (!route.location)
        return "";
    std::string selected;
    size_t selectedLength = 0;
    for (std::map<std::string, std::string>::const_iterator it =
             route.location->cgiHandlers.begin();
         it != route.location->cgiHandlers.end(); ++it) {
        const std::string &extension = it->first;
        if (extension.size() > selectedLength &&
            route.diskPath.size() >= extension.size() &&
            route.diskPath.compare(route.diskPath.size() - extension.size(),
                                   extension.size(), extension) == 0) {
            selected = it->second;
            selectedLength = extension.size();
        }
    }
    return selected;
}

bool CgiHandler::matches(const RouteResult &route) {
    return !selectInterpreter(route).empty();
}

bool CgiHandler::start(const Request &request, const RouteResult &route,
                       const ServerConfig &config, CgiProcess &process) {
    struct stat info;
    if (stat(route.diskPath.c_str(), &info) != 0 || !S_ISREG(info.st_mode))
        return false;

    int inputPipe[2];
    int outputPipe[2];
    if (pipe(inputPipe) != 0)
        return false;
    if (pipe(outputPipe) != 0) {
        close(inputPipe[0]);
        close(inputPipe[1]);
        return false;
    }

    std::vector<std::string> environment;
    addEnvironment(environment, "GATEWAY_INTERFACE", "CGI/1.1");
    addEnvironment(environment, "SERVER_PROTOCOL", request.version);
    addEnvironment(environment, "SERVER_SOFTWARE", "webserv/1.0");
    addEnvironment(environment, "REQUEST_METHOD", request.method);
    addEnvironment(environment, "REQUEST_URI", request.target);
    addEnvironment(environment, "QUERY_STRING", request.query);
    addEnvironment(environment, "CONTENT_LENGTH", toString(request.body.size()));
    addEnvironment(environment, "SCRIPT_NAME", request.path);
    addEnvironment(environment, "SCRIPT_FILENAME", absolutePath(route.diskPath));
    addEnvironment(environment, "PATH_INFO", request.path);
    addEnvironment(environment, "PATH_TRANSLATED", absolutePath(route.diskPath));
    addEnvironment(environment, "DOCUMENT_ROOT",
                   absolutePath(route.location->root));
    addEnvironment(environment, "SERVER_NAME", serverName(request, config));
    addEnvironment(environment, "SERVER_PORT", toString(config.port));
    addEnvironment(environment, "REMOTE_ADDR", request.remoteAddress);
    addEnvironment(environment, "REMOTE_PORT", toString(request.remotePort));
    addEnvironment(environment, "REDIRECT_STATUS", "200");
    std::map<std::string, std::string>::const_iterator contentType =
        request.headers.find("content-type");
    if (contentType != request.headers.end())
        addEnvironment(environment, "CONTENT_TYPE", contentType->second);
    for (std::map<std::string, std::string>::const_iterator it =
             request.headers.begin();
         it != request.headers.end(); ++it) {
        if (it->first != "content-length" && it->first != "content-type")
            addEnvironment(environment, headerEnvironmentName(it->first), it->second);
    }
    std::vector<char *> envPointers;
    for (size_t i = 0; i < environment.size(); ++i)
        envPointers.push_back(const_cast<char *>(environment[i].c_str()));
    envPointers.push_back(NULL);

    const std::string script = baseName(route.diskPath);
    const std::string directory = directoryName(route.diskPath);
    const std::string executable = selectInterpreter(route);
    if (executable.empty()) {
        close(inputPipe[0]);
        close(inputPipe[1]);
        close(outputPipe[0]);
        close(outputPipe[1]);
        return false;
    }
    process.pid = fork();
    if (process.pid == 0) {
        dup2(inputPipe[0], STDIN_FILENO);
        dup2(outputPipe[1], STDOUT_FILENO);
        close(inputPipe[0]);
        close(inputPipe[1]);
        close(outputPipe[0]);
        close(outputPipe[1]);
        const long descriptorLimit = sysconf(_SC_OPEN_MAX);
        for (int fd = 3; fd < descriptorLimit; ++fd)
            close(fd);
        setResourceLimit(RLIMIT_CPU,
                         static_cast<rlim_t>(route.location->cgiTimeout + 1));
        setResourceLimit(RLIMIT_AS, static_cast<rlim_t>(256) * 1024 * 1024);
        setResourceLimit(RLIMIT_FSIZE, static_cast<rlim_t>(17) * 1024 * 1024);
#ifdef RLIMIT_NPROC
        setResourceLimit(RLIMIT_NPROC, 16);
#endif
        if (chdir(directory.c_str()) != 0)
            _exit(126);
        char *arguments[3];
        arguments[0] = const_cast<char *>(executable.c_str());
        arguments[1] = const_cast<char *>(script.c_str());
        arguments[2] = NULL;
        execve(executable.c_str(), arguments, &envPointers[0]);
        _exit(127);
    }
    close(inputPipe[0]);
    close(outputPipe[1]);
    if (process.pid < 0) {
        close(inputPipe[1]);
        close(outputPipe[0]);
        return false;
    }
    try {
        setNonBlocking(inputPipe[1]);
        setNonBlocking(outputPipe[0]);
    } catch (...) {
        close(inputPipe[1]);
        close(outputPipe[0]);
        kill(process.pid, SIGKILL);
        waitpid(process.pid, NULL, 0);
        return false;
    }
    process.inputFd = inputPipe[1];
    process.outputFd = outputPipe[0];
    return true;
}

Response CgiHandler::makeResponse(const std::string &output,
                                  const ServerConfig &config) {
    size_t separator = output.find("\r\n\r\n");
    size_t separatorSize = 4;
    if (separator == std::string::npos) {
        separator = output.find("\n\n");
        separatorSize = 2;
    }
    if (separator == std::string::npos)
        return ResponseFactory::error(500, config);

    Response response(200);
    bool hasContentType = false;
    std::istringstream headers(output.substr(0, separator));
    std::string line;
    while (std::getline(headers, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        const size_t colon = line.find(':');
        if (colon == std::string::npos)
            return ResponseFactory::error(500, config);
        const std::string name = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if (lower(name) == "status") {
            std::istringstream status(value);
            int code;
            if (!(status >> code) || code < 100 || code > 599)
                return ResponseFactory::error(500, config);
            response.status = code;
        } else if (lower(name) != "content-length" &&
                   lower(name) != "connection") {
            response.headers[name] = value;
            if (lower(name) == "content-type")
                hasContentType = true;
        }
    }
    response.body = output.substr(separator + separatorSize);
    if (!hasContentType)
        response.headers["Content-Type"] = "text/plain; charset=utf-8";
    return response;
}
