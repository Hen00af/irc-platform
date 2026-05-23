#ifndef RESPONSE_SERIALIZER_HPP
# define RESPONSE_SERIALIZER_HPP

# include "HttpResponse.hpp"
# include <string>

std::string serializeResponse(const HttpResponse& res);

#endif