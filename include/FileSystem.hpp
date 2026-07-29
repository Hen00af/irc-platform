#ifndef FILE_SYSTEM_HPP
#define FILE_SYSTEM_HPP

#include "Webserv.hpp"

enum FileResult {
    FILE_OK,
    FILE_NOT_FOUND,
    FILE_FORBIDDEN,
    FILE_CONFLICT,
    FILE_TOO_LARGE,
    FILE_ERROR
};

class FileSystem {
  public:
    static FileResult resolveExisting(const std::string &root,
                                      const std::string &path,
                                      std::string &resolved);
    static FileResult readRegular(const std::string &path, std::string &body);
    static FileResult createExclusive(const std::string &directory,
                                      const std::string &name,
                                      const std::string &body);
    static FileResult removeRegular(const std::string &root,
                                    const std::string &path);

  private:
    FileSystem();
};

#endif
