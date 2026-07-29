#include "parseConf.hpp"

std::vector<std::string> Conf::split_words(std::string line) {
    std::vector<std::string> tokens;
    std::istringstream iss(trim(line));
    std::string token;
    while (iss >> token)
        tokens.push_back(token);
    return tokens;
}

std::string trim(const std::string &input) {
    const std::string spaces = " \t\r\n";
    const std::string::size_type start = input.find_first_not_of(spaces);
    if (start == std::string::npos)
        return "";
    const std::string::size_type end = input.find_last_not_of(spaces);
    return input.substr(start, end - start + 1);
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

std::string Conf::ft_first_word(std::string line) {
    std::vector<std::string> tokens = split_words(line);
    if (tokens.empty())
        return "";
    return tokens[0];
}

void Conf::read_file(std::string name) {
    std::ifstream file(name.c_str());
    if (!file.is_open())
        throw ArgvErr();

    std::string output;
    while (std::getline(file, output)) {
        bool is_empty = true;
        for (size_t i = 0; i < output.length(); ++i) {
            if (!isspace(static_cast<unsigned char>(output[i]))) {
                is_empty = false;
                break;
            }
        }
        if (!is_empty)
            _file.push_back(output);
    }
    if (_file.empty())
        throw DirMissing();
}
