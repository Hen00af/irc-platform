#include "utils.hpp"

void printFileInfo(const char *filestream) {
    struct stat fileStat;

    if(stat(filename, &fileStat) == 0) {
        std::cout << "Nom"
    }
}