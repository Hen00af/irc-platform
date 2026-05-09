#include "persing_request.hpp"

#include <cctype>
#include <iostream>

namespace {

const char* kCrlf = "\r\n";
const char* kHeaderTerminator = "\r\n\r\n";

Range makeInvalidRange() {
    Range range;
    range.start = std::string::npos;
    range.end = std::string::npos;
    return range;
}

std::string toLower(const std::string& value) {
    std::string lowered;

    lowered.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        lowered += static_cast<char>(std::tolower(
            static_cast<unsigned char>(value[i])));
    }
    return lowered;
}

} // namespace

RequestParser::RequestParser()
    : _raw_request(""),
      _request_line(makeInvalidRange()),
      _method(makeInvalidRange()),
      _target(makeInvalidRange()),
      _version(makeInvalidRange()),
      _headers_range(makeInvalidRange()),
      _body(makeInvalidRange()) {
}

RequestParser::~RequestParser() {
}

std::string RequestParser::slice(const Range& range) const {
    if (range.start == std::string::npos || range.end == std::string::npos) {
        return "";
    }
    if (range.start > range.end || range.end > _raw_request.size()) {
        return "";
    }
    return _raw_request.substr(range.start, range.end - range.start);
}

std::string RequestParser::getMethod() const {
    return slice(_method);
}

std::string RequestParser::getTarget() const {
    return slice(_target);
}

std::string RequestParser::getVersion() const {
    return slice(_version);
}

std::string RequestParser::getHeader(const std::string& key) const {
    std::map<std::string, Range>::const_iterator it = _headers.find(toLower(key));

    if (it == _headers.end()) {
        return "";
    }
    return slice(it->second);
}

bool RequestParser::equalsRange(const Range& range,
                                const std::string& expected) const {
    if (range.start == std::string::npos || range.end == std::string::npos) {
        return false;
    }
    if (range.start > range.end || range.end > _raw_request.size()) {
        return false;
    }
    if (range.end - range.start != expected.size()) {
        return false;
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (_raw_request[range.start + i] != expected[i]) {
            return false;
        }
    }
    return true;
}

bool RequestParser::isAllowedMethod() const {
    return equalsRange(_method, "GET")
        || equalsRange(_method, "POST")
        || equalsRange(_method, "DELETE");
}

bool RequestParser::parseRequestLine() {
    _request_line.start = 0;
    _request_line.end = _raw_request.find(kCrlf);
    if (_request_line.end == std::string::npos) {
        return false;
    }

    size_t first_space = _raw_request.find(' ', _request_line.start);
    if (first_space == std::string::npos || first_space >= _request_line.end) {
        return false;
    }

    size_t second_space = _raw_request.find(' ', first_space + 1);
    if (second_space == std::string::npos || second_space >= _request_line.end) {
        return false;
    }

    size_t third_space = _raw_request.find(' ', second_space + 1);
    if (third_space != std::string::npos && third_space < _request_line.end) {
        return false;
    }

    if (first_space == _request_line.start) {
        return false;
    }
    if (second_space == first_space + 1) {
        return false;
    }
    if (_request_line.end == second_space + 1) {
        return false;
    }

    _method.start = _request_line.start;
    _method.end = first_space;

    _target.start = first_space + 1;
    _target.end = second_space;

    _version.start = second_space + 1;
    _version.end = _request_line.end;

    if (!isAllowedMethod()) {
        return false;
    }
    if (!equalsRange(_version, "HTTP/1.1")) {
        throw VersionNotSupported();
    }
    return true;
}

bool RequestParser::parseHeaders() {
    size_t header_start = _request_line.end + 2;
    size_t header_end = _raw_request.find(kHeaderTerminator, header_start);

    if (header_end == std::string::npos) {
        return false;
    }

    _headers_range.start = header_start;
    _headers_range.end = header_end;

    size_t pos = header_start;
    while (pos < header_end) {
        size_t line_end = _raw_request.find(kCrlf, pos);
        if (line_end == std::string::npos || line_end > header_end) {
            return false;
        }

        size_t colon = _raw_request.find(':', pos);
        if (colon == std::string::npos || colon > line_end) {
            return false;
        }
        if (colon == pos) {
            return false;
        }

        std::string key = toLower(_raw_request.substr(pos, colon - pos));

        size_t value_start = colon + 1;
        while (value_start < line_end
               && (_raw_request[value_start] == ' '
                   || _raw_request[value_start] == '\t')) {
            ++value_start;
        }

        Range value;
        value.start = value_start;
        value.end = line_end;
        _headers[key] = value;

        pos = line_end + 2;
    }

    return _headers.find("host") != _headers.end();
}

bool RequestParser::parseBody() {
    _body.start = _headers_range.end + 4;
    _body.end = _raw_request.size();
    return true;
}

bool RequestParser::parseRequest(const std::string& raw_request) {
    _raw_request = raw_request;
    _request_line = makeInvalidRange();
    _method = makeInvalidRange();
    _target = makeInvalidRange();
    _version = makeInvalidRange();
    _headers_range = makeInvalidRange();
    _body = makeInvalidRange();
    _headers.clear();

    if (!parseRequestLine()) {
        return false;
    }
    if (!parseHeaders()) {
        return false;
    }
    return true;
}

int main() {
    RequestParser request;
    std::string raw_request =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    try {
        if (!request.parseRequest(raw_request)) {
            throw BadRequest();
        }

        std::cout << "method: " << request.getMethod() << std::endl;
        std::cout << "target: " << request.getTarget() << std::endl;
        std::cout << "version: " << request.getVersion() << std::endl;
        std::cout << "host: " << request.getHeader("host") << std::endl;
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    return 0;
}
