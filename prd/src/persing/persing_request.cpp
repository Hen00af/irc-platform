#include "persing_request.hpp"

RequestParser::Request() {

}

RequestParser::~Request() {

}




bool parse_request(const std::string& raw_request) {
    std::string line;
    ssize_t method_start_path;
    ssize_t method_end_path;
    ssize_t header_start_path:
    ssize_t header_end_path;
    ssize_t body_path;
    ssize_t bpdy_path;
    while(!getline(std::cin, line)) {
        std::
    }
}

int main() {
    std::ifstream ifs = requst;


}