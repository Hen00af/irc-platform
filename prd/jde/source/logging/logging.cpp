#include "logging.hpp"
#include "../persing/webserver.hpp"

void print_all_data(const Conf &conf) {
    const std::vector<Servers*> &servers = conf.get_Servers();
    std::cout << endl;
    for (size_t i = 0; i < servers.size(); ++i) {
        cout << "--- server " << i << ":" << endl;

        cout << "   name = " << servers[i]->getName() << endl;
        cout << "   listen = " << servers[i]->getListen() << endl;
        cout << "   root = " << servers[i]->getRoot() << endl;
        cout << "   index = " << servers[i]->getIndex() << endl;
        cout << "   body = " << servers[i]->getBody() << endl;
        cout << "   listing = " << servers[i]->getListing() << endl;
        
        cout << "   method = ";
        for (size_t len = 0; len < servers[i]->getMethod().size(); ++len){
            cout << "   " ;
            cout << servers[i]->getMethod()[len] << " ";
        }
        cout << endl;

        cout << "   error pages:" << endl;
        std::map<std::string, std::string> copy = servers[i]->getError();
        for (std::map<std::string, std::string>::iterator it = copy.begin(); it != copy.end(); ++it) {
            cout << "   " ;
            cout << "error " << it->first << " = " << it->second << endl;
        }

        for (size_t x = 0; x < servers[i]->getLocation().size(); ++x) {
            cout << "   - location " << x << ":" << endl;
            cout << "       dir = " << servers[i]->getLocation()[x]->getDir() << endl;
            cout << "       root = " << servers[i]->getLocation()[x]->getRoot() << endl;
            cout << "       index = " << servers[i]->getLocation()[x]->getIndex() << endl;
            cout << "       listing = " << servers[i]->getLocation()[x]->getListing() << endl;
            cout << "       redir = " << servers[i]->getLocation()[x]->getRedir() << endl;
            cout << "       methods = ";
            for (size_t len = 0; len < servers[i]->getLocation()[x]->getMethod().size(); ++len)
                cout << servers[i]->getLocation()[x]->getMethod()[len] << " ";
            cout << endl;
        }
    }
}
