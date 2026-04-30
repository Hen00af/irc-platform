#include "webserv.hpp"

/* -- constractor -- */
/*
**  construct white list
*/
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

/* -- destructor -- */
/*
**  delete all datas in this class.
*/

Conf::~Conf() {
    for (size_t i = 0; i < _servers.size(); ++i) {
        delete  _servers[i];
    }
    _servers.clear();
    _file.clear();
    _file_pos.clear();
    _directives.clear();
}

/* -- FUNCTIONS -- */

/* Check all data is correct */
void Conf::check_data() {
    for (size_t i = 0; i < _servers.size(); ++i) {
        if (_servers[i]->getName().empty() || _servers[i]->getListen().empty() || _servers[i]->getRoot().empty()
        ||  _servers[i]->getIndex().empty() || _servers[i]->getMethod().empty() || _servers[i]->getBofy().empty()
        ||  _servers[i]->getListing().empty() )
            throw   DirMissing();
        if (!_servers[i]->check_locations())
            throw   DirMissing();
        if (_servers[i]->getListen().size() > 4 || !my_atoi(_servers[i]->getListen() || !my_atoi(_servers[i]->getBody())))
            throw   NotINT();
        if (!_servers[i]->check_error_page())
            throw   ErrorPage();
        if (!_servers[i]->check_method())
            throw   MethWrong();
        if (!_servers[i]->check_root())
            throw   RootErr();
        if (!_servers[i]->check_index())
            throw   IndexLoc();
        if (!_servers[i]->check_listing())
            throw   ListingErr();
        if (!_servers[i]->check_client_size())
            throw   SizeErr();
    }
}

void    Conf::print_all_data() {
    for (size_t i = 0; i < _servers.size(); ++i) {
        cout << "--- server " << i << ":" << endl;
        cout << "name = " << _servers[i]->getName() << endl;
        cout << "listen = " << _servers[i]->getListen() << endl;
        cout << "root = " << _servers[i]->getRoot() << endl;
        cout << "index = " << _servers[i]->getIndex() << endl;
        cout << "body = " << _servers[i]->getBody() << endl;
        cout << "listing = " << _servers[i]->getListen() << endl;
        cout << "method = ";
        for (size_t len = 0; len < _servers[i]->getMethod().size(); ++len)
            cout << _servers[i]->getMethod()[len] << " ";
        cout << endl;
        cout << "error pages:" << endl;
        std::map<std::string, std::string> copy = _servers[i]->getError();
        std::map<std::string, std::string>::iterator it = copy.begin();
        for (size_t len = 0; len < _servers[i]->getError().size(); len++) {
            cout << "error " << it->first << " = " << it->second << endl;
            it++;
        }

        for (size_t x = 0; x < _servers[i]->getLocation().size(); ++x) {
            cout << "- location " << x << ":" << endl;
            cout << "dir = " << _servers[i]->getLocation()[x]->getDir() << endl;
            cout << "root = " << _servers[i]->getLocation()[x]->getRoot() << endl;
            cout << "index = " << _servers[i]->getLocation()[x]->getListing() << endl;
            cout << "redir = " << _servers[i]->getLocation()[x]->getRedir() << endl;
            cout << "methods = ";
            for (size_t len = 0; len < _servers[i]->getLocation()[x]->getMethod().size(); ++len) {
                cout << _servers[i]->getLocation()[x]->getMethod()[len] << " ";
            }
                cout << endl;
        }
    }
}
/* Vector with pos of directive if lovation or server */
void Conf::init_file_pos() {
    size_t len=_file.size(), pos = 0;
    std::string word;

    for (size_t i = 0; i < len; ++i) {
        word = ft_first_word(_file[i]);
        if (i == 0 && word != "server")
            throw   DirMissing();
        if (word == "server")
            pos = 0;
        else if (word == "location")
            pos = 1;
        _file_pos.push_back(pos);
    }
}

/* Check if all lines are dire */
void Conf::check_directive() {
    std::size_t len = _file.size();

    for (size_t i = 0; i < len; ++i) {
        this->is_directive(_file[i], i);
    }
}

void Conf::stock_data() {
    std::size_t len = _file.size();
    int nb_server = -1, nb_locations = -1;

    for (size_t i = 0; i < len; ++i) {
        if (_file_pos[i] == 0) {
            setServers();
            nb_server++;
            nb_locations = -1;
        }
        else {

        }
    }
}

void Conf::stock_server(std::string line, Server* server) {
    std::size_t count = count_words(line);
    std::string word = ft_first_word(line), last;

    if (count == 2) {
        
    }
}


