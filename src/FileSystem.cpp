#include "FileSystem.hpp"

static bool pathInside(const std::string &root, const std::string &path) {
    if (root == "/")
        return !path.empty() && path[0] == '/';
    return path == root ||
           (path.size() > root.size() &&
            path.compare(0, root.size(), root) == 0 &&
            path[root.size()] == '/');
}

static FileResult canonicalPath(const std::string &path,
                                std::string &canonical) {
    char buffer[PATH_MAX];
    errno = 0;
    if (!realpath(path.c_str(), buffer))
        return errno == ENOENT || errno == ENOTDIR ? FILE_NOT_FOUND
                                                   : FILE_FORBIDDEN;
    canonical = buffer;
    return FILE_OK;
}

FileResult FileSystem::resolveExisting(const std::string &root,
                                       const std::string &path,
                                       std::string &resolved) {
    std::string canonicalRoot;
    FileResult result = canonicalPath(root, canonicalRoot);
    if (result != FILE_OK)
        return FILE_ERROR;

    struct stat linkInfo;
    if (lstat(path.c_str(), &linkInfo) != 0)
        return errno == ENOENT || errno == ENOTDIR ? FILE_NOT_FOUND
                                                   : FILE_FORBIDDEN;
    if (S_ISLNK(linkInfo.st_mode))
        return FILE_FORBIDDEN;

    result = canonicalPath(path, resolved);
    if (result != FILE_OK)
        return result;
    if (!pathInside(canonicalRoot, resolved))
        return FILE_FORBIDDEN;
    return FILE_OK;
}

FileResult FileSystem::readRegular(const std::string &path,
                                   std::string &body) {
    const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
        return errno == ENOENT ? FILE_NOT_FOUND : FILE_FORBIDDEN;
    struct stat info;
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(fd);
        return FILE_FORBIDDEN;
    }
    if (info.st_size < 0 ||
        static_cast<unsigned long>(info.st_size) > 16UL * 1024 * 1024) {
        close(fd);
        return FILE_TOO_LARGE;
    }
    body.clear();
    char buffer[16384];
    while (true) {
        const ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count > 0)
            body.append(buffer, static_cast<size_t>(count));
        else if (count == 0)
            break;
        else if (errno != EINTR) {
            close(fd);
            return FILE_ERROR;
        }
    }
    close(fd);
    return FILE_OK;
}

FileResult FileSystem::createExclusive(const std::string &directory,
                                       const std::string &name,
                                       const std::string &body) {
    std::string canonicalDirectory;
    FileResult result = canonicalPath(directory, canonicalDirectory);
    if (result != FILE_OK)
        return FILE_ERROR;
    const int directoryFd =
        open(canonicalDirectory.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (directoryFd < 0)
        return FILE_FORBIDDEN;
    const int fd = openat(directoryFd, name.c_str(),
                          O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) {
        const int savedError = errno;
        close(directoryFd);
        return savedError == EEXIST ? FILE_CONFLICT : FILE_FORBIDDEN;
    }
    size_t written = 0;
    while (written < body.size()) {
        const ssize_t count =
            write(fd, body.data() + written, body.size() - written);
        if (count > 0)
            written += static_cast<size_t>(count);
        else if (count < 0 && errno == EINTR)
            continue;
        else {
            close(fd);
            unlinkat(directoryFd, name.c_str(), 0);
            close(directoryFd);
            return FILE_ERROR;
        }
    }
    close(fd);
    close(directoryFd);
    return FILE_OK;
}

FileResult FileSystem::removeRegular(const std::string &root,
                                     const std::string &path) {
    std::string resolved;
    FileResult result = resolveExisting(root, path, resolved);
    if (result != FILE_OK)
        return result;
    const int fd = open(resolved.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
        return FILE_FORBIDDEN;
    struct stat info;
    const bool regular = fstat(fd, &info) == 0 && S_ISREG(info.st_mode);
    close(fd);
    if (!regular)
        return FILE_FORBIDDEN;
    if (unlink(path.c_str()) != 0)
        return FILE_FORBIDDEN;
    return FILE_OK;
}
