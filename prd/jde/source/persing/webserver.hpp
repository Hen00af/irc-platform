#ifndef WEBSERV_HPP
# define WEBSERV_HPP

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <cctype>
#include <string>
#include <sstream>
#include <cstdlib>

#include <unistd.h>
#include <istream>

using std::cout;
using std::endl;
using std::cerr;

class location;
class Servers;
class Conf;

void parsing(int argc, char **argv, Conf &data);
void parse_basic(int argc, char **argv);


class Conf {
    public:
        Conf();
        ~Conf();

        std::vector<std::string> get_file() const {return (this->_file);};

        private:
            std::vector<Servers*>   _servers;
            std::vector<std::string>    _file;
            std::vector<int>        _file_pos;
            std::vector<std::string>    _directives;
};

#endif