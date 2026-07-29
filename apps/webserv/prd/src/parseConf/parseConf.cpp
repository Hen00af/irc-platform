#include "parseConf.hpp"
#include <algorithm>

namespace {

std::vector<std::string> split_line(const std::string &line) {
    std::vector<std::string> tokens;
    std::istringstream iss(trim(line));
    std::string token;

    while (iss >> token)
        tokens.push_back(token);
    return tokens;
}

bool check_location(const LocationConfig &location) {
    return !location.dir.empty() &&
           !location.root.empty() &&
           !location.index.empty() &&
           !location.methods.empty() &&
           !location.listing.empty();
}

bool check_error_pages(const ServerConfig &server) {
    for (std::map<std::string, std::string>::const_iterator it = server.error_pages.begin();
         it != server.error_pages.end(); ++it) {
        if (!my_atoi(it->first) || it->second.empty())
            return false;
    }
    return true;
}

bool check_methods(const ServerConfig &server) {
    for (size_t i = 0; i < server.methods.size(); ++i) {
        if (!is_valid_method(server.methods[i]))
            return false;
    }
    for (size_t i = 0; i < server.locations.size(); ++i) {
        for (size_t j = 0; j < server.locations[i].methods.size(); ++j) {
            if (!is_valid_method(server.locations[i].methods[j]))
                return false;
        }
    }
    return true;
}

bool check_root(const ServerConfig &server) {
    if (server.root.empty())
        return false;
    for (size_t i = 0; i < server.locations.size(); ++i) {
        if (server.locations[i].root.empty())
            return false;
    }
    return true;
}

bool check_index(const ServerConfig &server) {
    if (server.index.empty())
        return false;
    for (size_t i = 0; i < server.locations.size(); ++i) {
        if (server.locations[i].index.empty())
            return false;
    }
    return true;
}

bool check_listing(const ServerConfig &server) {
    if (!is_valid_listing_value(server.listing))
        return false;
    for (size_t i = 0; i < server.locations.size(); ++i) {
        if (!is_valid_listing_value(server.locations[i].listing))
            return false;
    }
    return true;
}

bool check_client_size(const ServerConfig &server) {
    return !server.body.empty() && my_atoi(server.body);
}

}  // namespace

void stock_server(const std::string &line, ServerConfig &server) {
    std::vector<std::string> tokens = split_line(line);
    if (tokens.empty())
        return;

    if (tokens[0] == "server_name" && tokens.size() >= 2)
        server.name = tokens[1];
    else if (tokens[0] == "listen" && tokens.size() >= 2)
        server.listen = tokens[1];
    else if (tokens[0] == "root" && tokens.size() >= 2)
        server.root = tokens[1];
    else if (tokens[0] == "index" && tokens.size() >= 2)
        server.index = tokens[1];
    else if (tokens[0] == "client_max_body_size" && tokens.size() >= 2)
        server.body = tokens[1];
    else if (tokens[0] == "dir_listing" && tokens.size() >= 2)
        server.listing = tokens[1];
    else if (tokens[0] == "allowed_methods" && tokens.size() >= 2)
        server.methods.push_back(tokens[1]);
    else if (tokens[0] == "error_page" && tokens.size() >= 3)
        server.error_pages[tokens[1]] = tokens[2];
}

void stock_location(const std::string &line, LocationConfig &location) {
    std::vector<std::string> tokens = split_line(line);
    if (tokens.empty())
        return;

    if (tokens[0] == "location" && tokens.size() >= 2)
        location.dir = tokens[1];
    else if (tokens[0] == "root" && tokens.size() >= 2)
        location.root = tokens[1];
    else if (tokens[0] == "index" && tokens.size() >= 2)
        location.index = tokens[1];
    else if (tokens[0] == "allowed_methods" && tokens.size() >= 2)
        location.methods.push_back(tokens[1]);
    else if (tokens[0] == "dir_listing" && tokens.size() >= 2)
        location.listing = tokens[1];
    else if (tokens[0] == "redir" && tokens.size() >= 2)
        location.redir = tokens[1];
}

const std::vector<ServerConfig> &Conf::get_servers() const {
    return _servers;
}

Conf::Conf() {
    _directives.push_back("server");
    _directives.push_back("listen");
    _directives.push_back("server_name");
    _directives.push_back("allowed_methods");
    _directives.push_back("root");
    _directives.push_back("error_page");
    _directives.push_back("index");
    _directives.push_back("client_max_body_size");
    _directives.push_back("location");
    _directives.push_back("dir_listing");
    _directives.push_back("redir");
}

Conf::~Conf() {}

void Conf::check_data() {
    for (size_t i = 0; i < _servers.size(); ++i) {
        if (_servers[i].name.empty() || _servers[i].listen.empty() || _servers[i].root.empty() ||
            _servers[i].index.empty() || _servers[i].methods.empty() || _servers[i].body.empty() ||
            _servers[i].listing.empty())
            throw DirMissing();

        for (size_t j = 0; j < _servers[i].locations.size(); ++j) {
            if (!check_location(_servers[i].locations[j]))
                throw DirMissing();
        }

        if (_servers[i].listen.size() > 5 || !my_atoi(_servers[i].listen) || !my_atoi(_servers[i].body))
            throw NotINT();
        if (!check_error_pages(_servers[i]))
            throw ErrorPage();
        if (!check_methods(_servers[i]))
            throw MethWrong();
        if (!check_root(_servers[i]))
            throw RootErr();
        if (!check_index(_servers[i]))
            throw IndexLoc();
        if (!check_listing(_servers[i]))
            throw ListingErr();
        if (!check_client_size(_servers[i]))
            throw SizeErr();
    }
}

void Conf::init_file_pos() {
    size_t len = _file.size();
    size_t pos = 0;
    std::string word;

    for (size_t i = 0; i < len; ++i) {
        word = ft_first_word(_file[i]);
        if (i == 0 && word != "server")
            throw DirMissing();
        if (word == "server")
            pos = 0;
        else if (word == "location")
            pos = 1;
        _file_pos.push_back(static_cast<int>(pos));
    }
}

void Conf::check_directive() {
    for (size_t i = 0; i < _file.size(); ++i)
        is_directive(_file[i], static_cast<int>(i));
}

void Conf::stock_data() {
    int nb_server = -1;
    int nb_locations = -1;

    for (size_t i = 0; i < _file.size(); ++i) {
        if (_file_pos[i] == 0) {
            if (ft_first_word(_file[i]) == "server") {
                add_server();
                ++nb_server;
                nb_locations = -1;
            } else {
                stock_server(_file[i], _servers[nb_server]);
            }
        } else {
            if (ft_first_word(_file[i]) == "location") {
                _servers[nb_server].locations.push_back(LocationConfig());
                ++nb_locations;
            }
            stock_location(_file[i], _servers[nb_server].locations[nb_locations]);
        }
    }
}

void Conf::is_directive(std::string line, int pos) {
    (void)pos;
    std::vector<std::string> tokens = split_words(line);
    if (tokens.empty())
        return;

    std::string first_word = tokens[0];
    for (size_t i = 0; i < _directives.size(); ++i) {
        if (first_word == _directives[i])
            return;
    }
    throw DirWrong();
}

void Conf::add_server() {
    _servers.push_back(ServerConfig());
}

void validate_arguments(int argc, char **argv) {
    if (argc != 2 || argv == NULL || argv[1] == NULL)
        throw ArgvErr();
}

bool is_valid_method(const std::string &method) {
    return method == "GET" || method == "POST" || method == "DELETE";
}

bool is_valid_listing_value(const std::string &value) {
    return value == "on" || value == "off";
}

void parseArgs(int argc, char **argv, Conf &conf)
{
    validate_arguments(argc, argv);

    conf.read_file(argv[1]);
    conf.check_directive();
    conf.init_file_pos();
    conf.stock_data();
    conf.check_data();
}
