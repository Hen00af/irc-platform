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
        case 409: return "Conflict";
        case 408: return "Request Timeout";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        case 501: return "Not Implemented";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default: return "Error";
    }
}

// Header names are compared case-insensitively: CGI scripts choose their own
// capitalisation, and a script that sets a policy must not end up with two.
static bool hasHeader(const std::map<std::string, std::string> &headers,
                      const std::string &name) {
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
        if (lower(it->first) == name)
            return true;
    return false;
}

// Applied to every response the server sends. The CSP is the second line of
// defence for anything served out of an upload directory: even if a file that
// the browser would execute reaches disk, this origin runs no script it did
// not ship itself — script-src is the directive doing that work.
//
// Two directives are deliberately looser than the rest. style-src allows
// inline because www/index.html carries a <style> block. connect-src allows
// any ws/wss target because the chat client reaches the gateway on a separate
// port everywhere except Render — localhost:3001 under Compose, :8443 on
// Fly.io, and whatever ?gateway= names when debugging. Pinning it to 'self'
// breaks all three; tighten it once the gateway is only ever same-origin.
static const char *const SECURITY_HEADERS[][2] = {
    {"x-content-type-options", "nosniff"},
    {"x-frame-options", "DENY"},
    {"referrer-policy", "no-referrer"},
    {"content-security-policy",
     "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
     "img-src 'self' data:; connect-src 'self' ws: wss:; object-src 'none'; "
     "base-uri 'none'; frame-ancestors 'none'"}};

std::string Response::serialize() const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << " " << reasonPhrase(status) << "\r\n";
    static const size_t securityHeaderCount =
        sizeof(SECURITY_HEADERS) / sizeof(SECURITY_HEADERS[0]);
    for (size_t i = 0; i < securityHeaderCount; ++i)
        if (!hasHeader(headers, SECURITY_HEADERS[i][0]))
            out << SECURITY_HEADERS[i][0] << ": " << SECURITY_HEADERS[i][1]
                << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
        out << it->first << ": " << it->second << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n\r\n";
    out << body;
    return out.str();
}

static bool decodePath(const std::string &input, std::string &output) {
    std::string decoded;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%') {
            if (i + 2 >= input.size())
                return false;
            std::istringstream hex(input.substr(i + 1, 2));
            unsigned int value = 0;
            char extra;
            hex >> std::hex >> value;
            if (hex.fail() || hex >> extra || value == 0 || value < 0x20 ||
                value == 0x7f)
                return false;
            decoded += static_cast<char>(value);
            i += 2;
        } else {
            const unsigned char value = static_cast<unsigned char>(input[i]);
            if (value < 0x20 || value == 0x7f || input[i] == '\\')
                return false;
            decoded += input[i];
        }
    }
    output = "/";
    size_t position = 1;
    while (position <= decoded.size()) {
        const size_t slash = decoded.find('/', position);
        const size_t end = slash == std::string::npos ? decoded.size() : slash;
        const std::string segment = decoded.substr(position, end - position);
        if (segment == "..")
            return false;
        if (!segment.empty() && segment != ".") {
            if (output.size() > 1)
                output += "/";
            output += segment;
        }
        if (slash == std::string::npos)
            break;
        position = slash + 1;
    }
    if (decoded.size() > 1 && decoded[decoded.size() - 1] == '/' &&
        output[output.size() - 1] != '/')
        output += "/";
    return true;
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
