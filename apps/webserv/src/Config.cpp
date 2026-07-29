#include "Webserv.hpp"

Location::Location()
    : redirectStatus(301), cgiTimeout(30), autoindex(false) {}

ServerConfig::ServerConfig()
    : host("0.0.0.0"), port(8080), root("www"), index("index.html"),
      maxBody(1048576), autoindex(false) {
    methods.push_back("GET");
}

std::string trim(const std::string &value) {
    const std::string whitespace = " \t\r\n";
    const size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos)
        return "";
    return value.substr(first, value.find_last_not_of(whitespace) - first + 1);
}

std::string lower(const std::string &value) {
    std::string result(value);
    for (size_t i = 0; i < result.size(); ++i)
        if (result[i] >= 'A' && result[i] <= 'Z')
            result[i] += 'a' - 'A';
    return result;
}

std::string toString(size_t value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

struct ConfigToken {
    std::string value;
    size_t line;

    ConfigToken(const std::string &text, size_t lineNumber)
        : value(text), line(lineNumber) {}
};

static std::runtime_error configError(size_t line, const std::string &message) {
    return std::runtime_error("line " + toString(line) + ": " + message);
}

static std::vector<ConfigToken> tokenize(std::istream &input) {
    std::vector<ConfigToken> tokens;
    std::string current;
    size_t tokenLine = 1;
    size_t line = 1;
    char quote = '\0';
    bool comment = false;
    char c;
    while (input.get(c)) {
        if (comment) {
            if (c == '\n') {
                comment = false;
                ++line;
            }
            continue;
        }
        if (quote != '\0') {
            if (c == quote) {
                tokens.push_back(ConfigToken(current, tokenLine));
                current.clear();
                quote = '\0';
            } else {
                if (c == '\n')
                    throw configError(line, "unterminated quoted value");
                current += c;
            }
            continue;
        }
        if (c == '#') {
            if (!current.empty()) {
                tokens.push_back(ConfigToken(current, tokenLine));
                current.clear();
            }
            comment = true;
        } else if (c == '"' || c == '\'') {
            if (!current.empty())
                throw configError(line, "quote must start a value");
            quote = c;
            tokenLine = line;
        } else if (c == '{' || c == '}' || c == ';') {
            if (!current.empty()) {
                tokens.push_back(ConfigToken(current, tokenLine));
                current.clear();
            }
            tokens.push_back(ConfigToken(std::string(1, c), line));
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!current.empty()) {
                tokens.push_back(ConfigToken(current, tokenLine));
                current.clear();
            }
            if (c == '\n')
                ++line;
        } else {
            if (current.empty())
                tokenLine = line;
            current += c;
        }
    }
    if (quote != '\0')
        throw configError(line, "unterminated quoted value");
    if (!current.empty())
        tokens.push_back(ConfigToken(current, tokenLine));
    return tokens;
}

static size_t parseNumber(const ConfigToken &token) {
    if (token.value.empty() || token.value[0] == '-' || token.value[0] == '+')
        throw configError(token.line, "invalid number");
    errno = 0;
    char *end = NULL;
    const unsigned long value = std::strtoul(token.value.c_str(), &end, 10);
    if (errno == ERANGE || *end != '\0' ||
        value > std::numeric_limits<size_t>::max())
        throw configError(token.line, "invalid number");
    return static_cast<size_t>(value);
}

static bool parseOnOff(const ConfigToken &token) {
    if (token.value == "on")
        return true;
    if (token.value == "off")
        return false;
    throw configError(token.line, "expected on or off");
}

struct LocationState {
    Location value;
    bool hasRoot;
    bool hasIndex;
    bool hasAutoindex;
    bool hasMethods;

    LocationState()
        : hasRoot(false), hasIndex(false), hasAutoindex(false),
          hasMethods(false) {}
};

class ConfigParser {
    const std::vector<ConfigToken> &_tokens;
    size_t _position;

    const ConfigToken &peek() const {
        if (_position >= _tokens.size())
            throw std::runtime_error("unexpected end of configuration");
        return _tokens[_position];
    }

    ConfigToken take() {
        const ConfigToken token = peek();
        ++_position;
        return token;
    }

    void expect(const std::string &value) {
        const ConfigToken token = take();
        if (token.value != value)
            throw configError(token.line, "expected '" + value + "'");
    }

    void markUnique(std::map<std::string, bool> &seen,
                    const ConfigToken &directive) {
        if (seen[directive.value])
            throw configError(directive.line,
                              "duplicate directive '" + directive.value + "'");
        seen[directive.value] = true;
    }

    std::vector<ConfigToken> arguments() {
        std::vector<ConfigToken> values;
        while (peek().value != ";") {
            if (peek().value == "{" || peek().value == "}")
                throw configError(peek().line, "expected ';'");
            values.push_back(take());
        }
        expect(";");
        return values;
    }

    void parseMethods(std::vector<std::string> &methods,
                      const std::vector<ConfigToken> &args, size_t line) {
        if (args.empty())
            throw configError(line, "methods are missing");
        methods.clear();
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i].value != "GET" && args[i].value != "POST" &&
                args[i].value != "DELETE")
                throw configError(args[i].line, "unsupported method");
            for (size_t j = 0; j < methods.size(); ++j)
                if (methods[j] == args[i].value)
                    throw configError(args[i].line, "duplicate method");
            methods.push_back(args[i].value);
        }
    }

    void parseListen(ServerConfig &server, const ConfigToken &arg) {
        const size_t colon = arg.value.rfind(':');
        const std::string host =
            colon == std::string::npos ? "0.0.0.0" : arg.value.substr(0, colon);
        const std::string port =
            colon == std::string::npos ? arg.value : arg.value.substr(colon + 1);
        struct in_addr address;
        if (host.empty() || inet_pton(AF_INET, host.c_str(), &address) != 1)
            throw configError(arg.line, "invalid IPv4 listen address");
        const ConfigToken portToken(port, arg.line);
        const size_t number = parseNumber(portToken);
        if (number == 0 || number > 65535)
            throw configError(arg.line, "invalid port");
        server.host = host;
        server.port = static_cast<unsigned short>(number);
    }

    LocationState parseLocation() {
        LocationState location;
        const ConfigToken path = take();
        if (path.value.empty() || path.value[0] != '/')
            throw configError(path.line, "location path must start with '/'");
        location.value.path = path.value;
        expect("{");
        std::map<std::string, bool> seen;
        while (peek().value != "}") {
            const ConfigToken directive = take();
            const std::vector<ConfigToken> args = arguments();
            std::string uniqueName = directive.value;
            if (uniqueName == "dir_listing")
                uniqueName = "autoindex";
            if (uniqueName == "allowed_methods")
                uniqueName = "allow_methods";
            if (uniqueName == "redir")
                uniqueName = "return";
            ConfigToken normalized(uniqueName, directive.line);
            if (uniqueName != "cgi_handler")
                markUnique(seen, normalized);

            if (uniqueName == "root" && args.size() == 1) {
                location.value.root = args[0].value;
                location.hasRoot = true;
            } else if (uniqueName == "index" && args.size() == 1) {
                location.value.index = args[0].value;
                location.hasIndex = true;
            } else if (uniqueName == "autoindex" && args.size() == 1) {
                location.value.autoindex = parseOnOff(args[0]);
                location.hasAutoindex = true;
            } else if (uniqueName == "allow_methods") {
                parseMethods(location.value.methods, args, directive.line);
                location.hasMethods = true;
            } else if (uniqueName == "return" &&
                       (args.size() == 1 || args.size() == 2)) {
                size_t target = 0;
                if (args.size() == 2) {
                    const size_t status = parseNumber(args[0]);
                    if (status < 300 || status > 399)
                        throw configError(args[0].line,
                                          "redirect status must be 300-399");
                    location.value.redirectStatus = static_cast<int>(status);
                    target = 1;
                }
                location.value.redirect = args[target].value;
            } else if (uniqueName == "upload_dir" && args.size() == 1) {
                location.value.uploadDir = args[0].value;
            } else if (uniqueName == "cgi_extension" && args.size() == 1) {
                if (args[0].value.empty() || args[0].value[0] != '.')
                    throw configError(args[0].line,
                                      "CGI extension must start with '.'");
                location.value.cgiExtension = args[0].value;
            } else if (uniqueName == "cgi_path" && args.size() == 1) {
                location.value.cgiPath = args[0].value;
            } else if (uniqueName == "cgi_handler" && args.size() == 2) {
                if (args[0].value.empty() || args[0].value[0] != '.')
                    throw configError(args[0].line,
                                      "CGI extension must start with '.'");
                if (location.value.cgiHandlers.count(args[0].value))
                    throw configError(args[0].line,
                                      "duplicate CGI extension");
                location.value.cgiHandlers[args[0].value] = args[1].value;
            } else if (uniqueName == "cgi_timeout" && args.size() == 1) {
                const size_t timeout = parseNumber(args[0]);
                if (timeout == 0 || timeout > 300)
                    throw configError(args[0].line,
                                      "CGI timeout must be 1-300 seconds");
                location.value.cgiTimeout = timeout;
            } else {
                throw configError(directive.line,
                                  "invalid location directive or arguments");
            }
        }
        expect("}");
        if (location.value.cgiExtension.empty() != location.value.cgiPath.empty())
            throw configError(path.line,
                              "cgi_extension and cgi_path must be configured together");
        if (!location.value.cgiExtension.empty()) {
            if (location.value.cgiHandlers.count(location.value.cgiExtension))
                throw configError(path.line, "duplicate CGI extension");
            location.value.cgiHandlers[location.value.cgiExtension] =
                location.value.cgiPath;
        }
        return location;
    }

    ServerConfig parseServer() {
        expect("{");
        ServerConfig server;
        std::map<std::string, bool> seen;
        std::vector<LocationState> locations;
        while (peek().value != "}") {
            const ConfigToken directive = take();
            if (directive.value == "location") {
                locations.push_back(parseLocation());
                continue;
            }
            const std::vector<ConfigToken> args = arguments();
            std::string uniqueName = directive.value;
            if (uniqueName == "dir_listing")
                uniqueName = "autoindex";
            if (uniqueName == "allowed_methods")
                uniqueName = "allow_methods";
            ConfigToken normalized(uniqueName, directive.line);
            if (uniqueName != "error_page")
                markUnique(seen, normalized);

            if (uniqueName == "listen" && args.size() == 1)
                parseListen(server, args[0]);
            else if (uniqueName == "root" && args.size() == 1)
                server.root = args[0].value;
            else if (uniqueName == "index" && args.size() == 1)
                server.index = args[0].value;
            else if (uniqueName == "client_max_body_size" && args.size() == 1)
            {
                server.maxBody = parseNumber(args[0]);
                if (server.maxBody > 16 * 1024 * 1024)
                    throw configError(args[0].line,
                                      "client body limit exceeds 16 MiB");
            }
            else if (uniqueName == "autoindex" && args.size() == 1)
                server.autoindex = parseOnOff(args[0]);
            else if (uniqueName == "allow_methods")
                parseMethods(server.methods, args, directive.line);
            else if (uniqueName == "error_page" && args.size() == 2) {
                const size_t status = parseNumber(args[0]);
                if (status < 300 || status > 599)
                    throw configError(args[0].line,
                                      "error status must be 300-599");
                if (server.errorPages.count(static_cast<int>(status)))
                    throw configError(args[0].line,
                                      "duplicate error_page status");
                server.errorPages[static_cast<int>(status)] = args[1].value;
            } else
                throw configError(directive.line,
                                  "invalid server directive or arguments");
        }
        expect("}");
        if (!seen["listen"])
            throw std::runtime_error("server block is missing listen directive");
        for (size_t i = 0; i < locations.size(); ++i) {
            if (!locations[i].hasRoot)
                locations[i].value.root = server.root;
            if (!locations[i].hasIndex)
                locations[i].value.index = server.index;
            if (!locations[i].hasAutoindex)
                locations[i].value.autoindex = server.autoindex;
            if (!locations[i].hasMethods)
                locations[i].value.methods = server.methods;
            server.locations.push_back(locations[i].value);
        }
        return server;
    }

  public:
    explicit ConfigParser(const std::vector<ConfigToken> &tokens)
        : _tokens(tokens), _position(0) {}

    std::vector<ServerConfig> parse() {
        std::vector<ServerConfig> servers;
        while (_position < _tokens.size()) {
            const ConfigToken keyword = take();
            if (keyword.value != "server")
                throw configError(keyword.line, "expected 'server'");
            servers.push_back(parseServer());
        }
        if (servers.empty())
            throw std::runtime_error("config contains no server");
        for (size_t i = 0; i < servers.size(); ++i)
            for (size_t j = i + 1; j < servers.size(); ++j)
                if (servers[i].host == servers[j].host &&
                    servers[i].port == servers[j].port)
                    throw std::runtime_error("duplicate listen address");
        return servers;
    }
};

std::vector<ServerConfig> Config::parse(const std::string &path) {
    std::ifstream file(path.c_str());
    if (!file)
        throw std::runtime_error("cannot open config: " + path);
    const std::vector<ConfigToken> tokens = tokenize(file);
    ConfigParser parser(tokens);
    return parser.parse();
}
