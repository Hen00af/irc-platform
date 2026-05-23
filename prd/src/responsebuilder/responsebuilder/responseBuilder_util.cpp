#include <fcntl.h>
#include <unistd.h>
#include <string>

static std::string httpDate(time_t t) {
    char    buf[128];
    struct tm tm;

    gmtime_r(&t, &tm);
    strftime(buf, sizeof(buf), "%a %d %b %Y %H:%M:%S GMT", &tm);
    return std::string(buf);
}

static std::string httpDate() {
    return httpDate(time(NULL));
}


bool readFile(const std::string& path, std::string& body)
{
    int     fd;
    char    buffer[4096];
    ssize_t bytes_read;

    fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return false;

    body.clear();

    while (true)
    {
        bytes_read = read(fd, buffer, sizeof(buffer));
        if (bytes_read < 0)
        {
            close(fd);
            return false;
        }
        if (bytes_read == 0)
            break;

        body.append(buffer, bytes_read);
    }

    close(fd);
    return true;
}