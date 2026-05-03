#include "conf.hpp"

std::vector<std::string> Conf::split_words(std::string line) {
    std::vector<std::string> tokens;
    std::istringstream iss(trim(line));
    std::string token;
    while (iss >> token)
        tokens.push_back(token);
    return tokens;
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