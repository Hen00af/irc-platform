#include "webserver.hpp"
#include "../server/server.hpp" 
#include <algorithm>

/*
*** location
*/

Location::Location() {}

// setter
void Location::setDir(const std::string &dir) { _dir = dir; }
void Location::setRoot(const std::string &root) { _root = root; }
void Location::setIndex(const std::string &index) { _index = index; }
void Location::setListing(const std::string &listing) { _listing = listing; }
void Location::setRedir(const std::string &redir) { _redir = redir; }
void Location::addMethod(const std::string &method) { _method.push_back(method); }

// getter
const std::string &Location::getDir() const { return _dir; }
const std::string &Location::getRoot() const { return _root; }
const std::string &Location::getIndex() const { return _index; }
const std::string &Location::getListing() const { return _listing; }
const std::string &Location::getRedir() const { return _redir; }
const std::vector<std::string> &Location::getMethod() const { return _method; }


/*
**  Server class
*/

// 中間表現として機能。
Servers::Servers() {}

Servers::~Servers() {
    for (size_t i = 0; i < _locations.size(); ++i)
        delete _locations[i];
}

void Servers::setName(const std::string &name) { _name = name; }
void Servers::setListen(const std::string &listen) { _listen = listen; }
void Servers::setRoot(const std::string &root) { _root = root; }
void Servers::setIndex(const std::string &index) { _index = index; }
void Servers::setBody(const std::string &body) { _body = body; }
void Servers::setListing(const std::string &listing) { _listing = listing; }
void Servers::addMethod(const std::string &method) { _method.push_back(method); }
void Servers::addErrorPage(const std::string &code, const std::string &path) { _error[code] = path; }

void Servers::setLocation() {
    _locations.push_back(new Location());
}

void Servers::stock_location(const std::string &line, int index) {
    if (index < 0 || static_cast<size_t>(index) >= _locations.size())
        return;

    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token)
        tokens.push_back(token);
    if (tokens.empty())
        return;

    Location *location = _locations[index];
    if (tokens[0] == "location" && tokens.size() >= 2)
        location->setDir(tokens[1]);
    else if (tokens[0] == "root" && tokens.size() >= 2)
        location->setRoot(tokens[1]);
    else if (tokens[0] == "index" && tokens.size() >= 2)
        location->setIndex(tokens[1]);
    else if (tokens[0] == "allowed_methods" && tokens.size() >= 2)
        location->addMethod(tokens[1]);
    else if (tokens[0] == "dir_listing" && tokens.size() >= 2)
        location->setListing(tokens[1]);
    else if (tokens[0] == "redir" && tokens.size() >= 2)
        location->setRedir(tokens[1]);
}

const std::string &Servers::getName() const { return _name; }
const std::string &Servers::getListen() const { return _listen; }
const std::string &Servers::getRoot() const { return _root; }
const std::string &Servers::getIndex() const { return _index; }
const std::string &Servers::getBody() const { return _body; }
const std::string &Servers::getListing() const { return _listing; }
const std::vector<std::string> &Servers::getMethod() const { return _method; }
std::map<std::string, std::string> Servers::getError() const { return _error; }
const std::vector<Location*> &Servers::getLocation() const { return _locations; }

bool Servers::check_locations() const {
    for (size_t i = 0; i < _locations.size(); ++i) {
        if (_locations[i]->getDir().empty() || _locations[i]->getRoot().empty() ||
            _locations[i]->getIndex().empty() || _locations[i]->getMethod().empty() ||
            _locations[i]->getListing().empty())
            return false;
    }
    return true;
}

bool Servers::check_error_page() const {
    for (std::map<std::string, std::string>::const_iterator it = _error.begin(); it != _error.end(); ++it) {
        if (!my_atoi(it->first) || it->second.empty())
            return false;
    }
    return true;
}

bool Servers::check_method() const {
    for (size_t i = 0; i < _method.size(); ++i) {
        if (!is_valid_method(_method[i]))
            return false;
    }
    for (size_t i = 0; i < _locations.size(); ++i) {
        const std::vector<std::string> &methods = _locations[i]->getMethod();
        for (size_t j = 0; j < methods.size(); ++j) {
            if (!is_valid_method(methods[j]))
                return false;
        }
    }
    return true;
}

bool Servers::check_root() const {
    if (_root.empty())
        return false;
    for (size_t i = 0; i < _locations.size(); ++i) {
        if (_locations[i]->getRoot().empty())
            return false;
    }
    return true;
}

bool Servers::check_index() const {
    if (_index.empty())
        return false;
    for (size_t i = 0; i < _locations.size(); ++i) {
        if (_locations[i]->getIndex().empty())
            return false;
    }
    return true;
}

bool Servers::check_listing() const {
    if (!is_valid_listing_value(_listing))
        return false;
    for (size_t i = 0; i < _locations.size(); ++i) {
        if (!is_valid_listing_value(_locations[i]->getListing()))
            return false;
    }
    return true;
}

bool Servers::check_client_size() const {
    return !_body.empty() && my_atoi(_body);
}

void stock_server(const std::string &line, Servers *server) {
    if (server == NULL)
        return;

    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token)
        tokens.push_back(token);
    if (tokens.empty())
        return;

    if (tokens[0] == "server_name" && tokens.size() >= 2)
        server->setName(tokens[1]);
    else if (tokens[0] == "listen" && tokens.size() >= 2)
        server->setListen(tokens[1]);
    else if (tokens[0] == "root" && tokens.size() >= 2)
        server->setRoot(tokens[1]);
    else if (tokens[0] == "index" && tokens.size() >= 2)
        server->setIndex(tokens[1]);
    else if (tokens[0] == "client_max_body_size" && tokens.size() >= 2)
        server->setBody(tokens[1]);
    else if (tokens[0] == "dir_listing" && tokens.size() >= 2)
        server->setListing(tokens[1]);
    else if (tokens[0] == "allowed_methods" && tokens.size() >= 2)
        server->addMethod(tokens[1]);
    else if (tokens[0] == "error_page" && tokens.size() >= 3)
        server->addErrorPage(tokens[1], tokens[2]);
}

/*
**  Conf class
*/

// getter
const std::vector<Servers*> &Conf::get_Servers() const { return _servers;};

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

Conf::~Conf() {
    for (size_t i = 0; i < _servers.size(); ++i) {
        delete _servers[i];
    }
    _servers.clear();
    _file.clear();
    _file_pos.clear();
    _directives.clear();
}

void Conf::check_data() {
    for (size_t i = 0; i < _servers.size(); ++i) {
        if (_servers[i]->getName().empty() || _servers[i]->getListen().empty() || _servers[i]->getRoot().empty()
        ||  _servers[i]->getIndex().empty() || _servers[i]->getMethod().empty() || _servers[i]->getBody().empty()
        ||  _servers[i]->getListing().empty() )
            throw DirMissing();
        
        if (!_servers[i]->check_locations())
            throw DirMissing();
        
        if (_servers[i]->getListen().size() > 5 || !my_atoi(_servers[i]->getListen()) || !my_atoi(_servers[i]->getBody()))
            throw NotINT();
            
        if (!_servers[i]->check_error_page())
            throw ErrorPage();
        if (!_servers[i]->check_method())
            throw MethWrong();
        if (!_servers[i]->check_root())
            throw RootErr();
        if (!_servers[i]->check_index())
            throw IndexLoc();
        if (!_servers[i]->check_listing())
            throw ListingErr();
        if (!_servers[i]->check_client_size())
            throw SizeErr();
    }
}

void Conf::init_file_pos() {
    size_t len = _file.size(), pos = 0;
    std::string word;

    for (size_t i = 0; i < len; ++i) {
        word = ft_first_word(_file[i]);
        if (i == 0 && word != "server")
            throw DirMissing();
        if (word == "server")
            pos = 0;
        else if (word == "location")
            pos = 1;
        _file_pos.push_back(pos);
    }
}

void Conf::check_directive() {
    for (size_t i = 0; i < _file.size(); ++i) {
        this->is_directive(_file[i], i);
    }
}

void Conf::stock_data() {
    size_t len = _file.size();
    int nb_server = -1, nb_locations = -1;

    for (size_t i = 0; i < len; ++i) {
        if (_file_pos[i] == 0) {
            if (ft_first_word(_file[i]) == "server") {
                setServers();
                nb_server++;
                nb_locations = -1;
            } else {
                stock_server(_file[i], _servers[nb_server]);
            }
        } else {
            if (ft_first_word(_file[i]) == "location") {
                _servers[nb_server]->setLocation();
                nb_locations++;
            }
            _servers[nb_server]->stock_location(_file[i], nb_locations);
        }
    }
}

void Conf::is_directive(std::string line, int pos) {
    (void)pos;
    std::vector<std::string> tokens = split_words(line);
    if (tokens.empty()) return;

    std::string first_word = tokens[0];
    for (size_t i = 0; i < _directives.size(); ++i) {
        if (first_word == _directives[i])
            return;
    }
    throw DirWrong();
}

void Conf::read_file(std::string name) {
    std::ifstream file(name.c_str());
    if (!file.is_open()) throw ArgvErr();

    std::string output;
    while (std::getline(file, output)) {
        bool is_empty = true;
        for (size_t i = 0; i < output.length(); ++i) {
            if (!isspace(output[i])) {
                is_empty = false;
                break;
            }
        }
        if (!is_empty) _file.push_back(output);
    }
    if (_file.empty()) throw DirMissing();
}

void Conf::setServers() {
    _servers.push_back(new Servers());
}


std::string Conf::ft_first_word(std::string line) {
    std::vector<std::string> tokens = split_words(line);
    if (tokens.empty())
        return "";
    return tokens[0];
}

/*
*** utils for paese
*/

void validate_arguments(int argc, char **argv) {
    if (argc != 2 || argv == NULL || argv[1] == NULL)
        throw ArgvErr();
}

bool my_atoi(const std::string &str) {
    if (str.empty())
        return false;
    for (size_t i = 0; i < str.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(str[i])))
            return false;
    }
    return true;
}

std::string trim(const std::string &input) {
    const std::string spaces = " \t\r\n";
    const std::string::size_type start = input.find_first_not_of(spaces);
    if (start == std::string::npos)
        return "";
    const std::string::size_type end = input.find_last_not_of(spaces);
    return input.substr(start, end - start + 1);
}

bool is_valid_method(const std::string &method) {
    return method == "GET" || method == "POST" || method == "DELETE";
}

bool is_valid_listing_value(const std::string &value) {
    return value == "on" || value == "off";
}

std::vector<std::string> Conf::split_words(std::string line) {
    std::vector<std::string> tokens;
    std::istringstream iss(trim(line));
    std::string token;

    while (iss >> token)
        tokens.push_back(token);
    return tokens;
}
