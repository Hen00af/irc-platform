#ifndef RESPONSE_BUILDER_HPP
# define RESPONSE_BUILDER_HPP

# include "HttpResponse.hpp"
# include "../persing/persing_conf.hpp"

HttpResponse buildResponse(const Conf& config);

#endif