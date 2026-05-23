#ifndef RESPONSE_SERIALIZER_HPP
#define RESPONSE_SERIALIZER_HPP

#include "../HttpResponse.hpp"
#include <string>
// #include <map>

std::string createResponse(const HttpResponse& res);

#endif