#ifndef WEBSERV_HPP
# define WEBSERV_HPP

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

using std::cout;
using std::endl;
using std::cerr;

class Conf;


/* conf_utils */
bool my_atoi(const std::string &str);
std::string trim(const std::string &input);
bool is_valid_method(const std::string &method);
bool is_valid_listing_value(const std::string &value);

/*
** Raw config structures
*/
struct RawLocation {
    std::string path;
    std::map<std::string, std::vector<std::string> > directives;
};

struct RawServer {
    std::map<std::string, std::vector<std::string> > directives;
    std::vector<RawLocation> locations;
};

class Location {
public:
    Location();

    void setDir(const std::string &dir);
    void setRoot(const std::string &root);
    void setIndex(const std::string &index);
    void setListing(const std::string &listing);
    void setRedir(const std::string &redir);
    void addMethod(const std::string &method);

    const std::string &getDir() const;
    const std::string &getRoot() const;
    const std::string &getIndex() const;
    const std::string &getListing() const;
    const std::string &getRedir() const;
    const std::vector<std::string> &getMethod() const;

private:
    std::string _dir;
    std::string _root;
    std::string _index;
    std::string _listing;
    std::string _redir;
    std::vector<std::string> _method;
};

/*
*** server class
*/
class Servers {
public:
    Servers();
    ~Servers();

    void setName(const std::string &name);
    void setListen(const std::string &listen);
    void setRoot(const std::string &root);
    void setIndex(const std::string &index);
    void setBody(const std::string &body);
    void setListing(const std::string &listing);
    void addMethod(const std::string &method);
    void addErrorPage(const std::string &code, const std::string &path);
    void setLocation();
    void stock_location(const std::string &line, int index);

    const std::string &getName() const;
    const std::string &getListen() const;
    const std::string &getRoot() const;
    const std::string &getIndex() const;
    const std::string &getBody() const;
    const std::string &getListing() const;
    const std::vector<std::string> &getMethod() const;
    std::map<std::string, std::string> getError() const;
    const std::vector<Location*> &getLocation() const;

    bool check_locations() const;
    bool check_error_page() const;
    bool check_method() const;
    bool check_root() const;
    bool check_index() const;
    bool check_listing() const;
    bool check_client_size() const;

private:
    std::string _name;
    std::string _listen;
    std::string _root;
    std::string _index;
    std::string _body;
    std::string _listing;
    std::vector<std::string> _method;
    std::map<std::string, std::string> _error;
    std::vector<Location*> _locations;
};

void stock_server(const std::string &line, Servers *server);

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
    void setServers();
    void check_data();
    void print_raw_data();

    const std::vector<Servers*> &get_Servers() const;

    std::string ft_first_word(std::string line);
    std::vector<std::string> split_words(std::string line);

private:
    std::vector<std::string> _file;
    std::vector<std::string> _directives;
    std::vector<int> _file_pos;
    std::vector<Servers*> _servers;
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

#endif
