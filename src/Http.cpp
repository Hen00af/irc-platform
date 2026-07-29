#include "Webserv.hpp"

Response::Response(int code) : status(code) {}

std::string reasonPhrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default: return "Error";
    }
}

std::string Response::serialize() const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << " " << reasonPhrase(status) << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
        out << it->first << ": " << it->second << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n\r\n";
    out << body;
    return out.str();
}

static bool decodePath(const std::string &input, std::string &output) {
    output.clear();
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            std::istringstream hex(input.substr(i + 1, 2));
            unsigned int value;
            hex >> std::hex >> value;
            if (hex.fail() || value == 0)
                return false;
            output += static_cast<char>(value);
            i += 2;
        } else {
            output += input[i];
        }
    }
    return output.find("..") == std::string::npos;
}

static bool parseChunked(const std::string &input, std::string &body, bool &complete) {
    size_t pos = 0;
    body.clear();
    complete = false;
    while (true) {
        size_t end = input.find("\r\n", pos);
        if (end == std::string::npos)
            return true;
        std::string sizeText = input.substr(pos, end - pos);
        const size_t extension = sizeText.find(';');
        if (extension != std::string::npos)
            sizeText.erase(extension);
        sizeText = trim(sizeText);
        if (sizeText.empty())
            return false;
        std::istringstream sizeStream(sizeText);
        size_t chunkSize = 0;
        sizeStream >> std::hex >> chunkSize;
        char extra;
        if (sizeStream.fail() || sizeStream >> extra)
            return false;
        pos = end + 2;
        if (chunkSize == 0) {
            if (input.compare(pos, 2, "\r\n") == 0) {
                complete = true;
                return true;
            }
            const size_t trailersEnd = input.find("\r\n\r\n", pos);
            if (trailersEnd == std::string::npos)
                return true;
            size_t trailer = pos;
            while (trailer < trailersEnd) {
                const size_t trailerEnd = input.find("\r\n", trailer);
                if (trailerEnd == std::string::npos ||
                    input.find(':', trailer) >= trailerEnd)
                    return false;
                trailer = trailerEnd + 2;
            }
            complete = true;
            return true;
        }
        if (input.size() < pos + chunkSize + 2)
            return true;
        body.append(input, pos, chunkSize);
        pos += chunkSize;
        if (input.compare(pos, 2, "\r\n") != 0)
            return false;
        pos += 2;
    }
}

static bool tokenCharacter(char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9'))
        return true;
    const std::string symbols = "!#$%&'*+-.^_`|~";
    return symbols.find(c) != std::string::npos;
}

static bool validToken(const std::string &value) {
    if (value.empty())
        return false;
    for (size_t i = 0; i < value.size(); ++i)
        if (!tokenCharacter(value[i]))
            return false;
    return true;
}

ParseResult parseRequest(const std::string &raw, size_t maxBody, Request &request) {
    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return raw.size() > 65536 ? REQUEST_BAD : REQUEST_INCOMPLETE;
    std::istringstream headers(raw.substr(0, headerEnd));
    std::string line;
    if (!std::getline(headers, line))
        return REQUEST_BAD;
    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
    std::istringstream first(line);
    std::string extra;
    if (!(first >> request.method >> request.target >> request.version) || first >> extra)
        return REQUEST_BAD;
    if (!validToken(request.method))
        return REQUEST_BAD;
    if (request.version != "HTTP/1.1")
        return REQUEST_VERSION_NOT_SUPPORTED;
    if (request.target.empty() || request.target[0] != '/')
        return REQUEST_BAD;
    while (std::getline(headers, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        size_t colon = line.find(':');
        if (colon == std::string::npos)
            return REQUEST_BAD;
        std::string key = lower(trim(line.substr(0, colon)));
        if (!validToken(key) || request.headers.count(key))
            return REQUEST_BAD;
        request.headers[key] = trim(line.substr(colon + 1));
    }
    if (!request.headers.count("host"))
        return REQUEST_BAD;
    size_t query = request.target.find('?');
    std::string encoded = request.target.substr(0, query);
    if (!decodePath(encoded, request.path))
        return REQUEST_BAD;
    if (query != std::string::npos)
        request.query = request.target.substr(query + 1);
    const std::string payload = raw.substr(headerEnd + 4);
    const bool hasTransferEncoding = request.headers.count("transfer-encoding");
    const bool hasContentLength = request.headers.count("content-length");
    if (hasTransferEncoding && hasContentLength)
        return REQUEST_BAD;
    if (hasTransferEncoding &&
        lower(trim(request.headers["transfer-encoding"])) != "chunked")
        return REQUEST_BAD;
    if (hasTransferEncoding) {
        bool complete = false;
        if (!parseChunked(payload, request.body, complete))
            return REQUEST_BAD;
        if (request.body.size() > maxBody)
            return REQUEST_TOO_LARGE;
        return complete ? REQUEST_OK : REQUEST_INCOMPLETE;
    }
    size_t length = 0;
    if (hasContentLength) {
        const std::string &text = request.headers["content-length"];
        if (text.empty() || text[0] == '-' || text[0] == '+')
            return REQUEST_BAD;
        errno = 0;
        char *end = NULL;
        unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
        if (errno == ERANGE || *end != '\0' ||
            parsed > std::numeric_limits<size_t>::max())
            return REQUEST_BAD;
        length = static_cast<size_t>(parsed);
    } else if (request.method == "POST") {
        return REQUEST_BAD;
    }
    if (length > maxBody)
        return REQUEST_TOO_LARGE;
    if (payload.size() < length)
        return REQUEST_INCOMPLETE;
    request.body = payload.substr(0, length);
    return REQUEST_OK;
}
