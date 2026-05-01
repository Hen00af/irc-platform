#ifndef Conf_HPP
# define Conf_HPP

#include "../server/server.hpp"
# include <iostream>
# include <fstream>
# include <vector>
# include <map>
# include <set>
# include <cctype>
# include <string>
# include <sstream>
# include <cstdlib>
# include <exception>
# include <unistd.h>

// using std::cout;
// using std::endl;
// using std::cerr;

class Conf;

void parsing(int argc, char **argv, Conf &conf);
void validate_arguments(int argc, char **argv);

/* conf_utils */
bool my_atoi(const std::string &str);
std::string trim(const std::string &input);
bool is_valid_method(const std::string &method);
bool is_valid_listing_value(const std::string &value);


struct LocationConfig;
/*
** Parsed config structures
*/
struct ServerConfig {
    std::string name;
    std::string listen;
    std::string root;
    std::string index;
    std::string body;
    std::string listing;
    std::vector<std::string> methods;
    std::map<std::string, std::string> error_pages;
    std::vector<LocationConfig> locations;
};
/*
** Raw config structures
*/
struct LocationConfig {
    std::string dir;
    std::string root;
    std::string index;
    std::string listing;
    std::string redir;
    std::vector<std::string> methods;
};

void stock_server(const std::string &line, ServerConfig &server);
void stock_location(const std::string &line, LocationConfig &location);

/*
** config class
*/
class Conf {
public:
    Conf();
    ~Conf();
    
    void read_file(std::string name);
    void check_directive();
    void is_directive(std::string line, int pos);
    void stock_data();
    void init_file_pos();
    void add_server();
    void check_data();

    const std::vector<ServerConfig> &get_servers() const;

    std::string ft_first_word(std::string line);
    std::vector<std::string> split_words(std::string line);

private:
    std::vector<std::string> _file;
    std::vector<std::string> _directives;
    std::vector<int> _file_pos;
    std::vector<ServerConfig> _servers;
};
/*
** Exceptions
*/

# define EXCEPTION public std::exception
# define WHAT const char * what () const throw()

class ArgvErr     : EXCEPTION { WHAT { return "Usage : ./Webserv <config_file>"; } };
class MissingArgv : EXCEPTION { WHAT { return "Error: Missing argument after a directive"; } };
class TooMuchArgv : EXCEPTION { WHAT { return "Error: Too much arguments after a directive"; } };
class DirWrong    : EXCEPTION { WHAT { return "Error: Directive is wrong"; } };
class DirMissing  : EXCEPTION { WHAT { return "Error: Missing a directive"; } };
class NotINT      : EXCEPTION { WHAT { return "Error: Argument needs to be a number"; } };
class MethWrong   : EXCEPTION { WHAT { return "Error: Method is wrong"; } };
class ErrorPage   : EXCEPTION { WHAT { return "Error: Error page is wrong"; } };
class DirTwice    : EXCEPTION { WHAT { return "Error: Two times the same directive"; } };
class RequestErr  : EXCEPTION { WHAT { return "Error: Request method wrong"; } };
class RootErr     : EXCEPTION { WHAT { return "Error: In root path"; } };
class IndexLoc    : EXCEPTION { WHAT { return "Error: Missing index in location"; } };
class ListingErr  : EXCEPTION { WHAT { return "Error: Dir_listing must be on or off"; } };
class SizeErr     : EXCEPTION { WHAT { return "Error: Client size"; } };

/*
*** Exceptions fot test
*/

class Boot_serverErr   : EXCEPTION { WHAT { return "Error: in Boot_server error"; } };

#endif
