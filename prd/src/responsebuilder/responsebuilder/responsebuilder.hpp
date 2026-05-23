#ifndef RESPONSE_BUILDER_HPP
# define RESPONSE_BUILDER_HPP

# include "HttpResponse.hpp"
# include "../http/request.hpp"
# include "../persing/persing_conf.hpp"
# include <string>

struct ResponseContext
{
    const Conf* config;
    std::string file_path;
};

HttpResponse buildResponse(const HttpRequest& req, const ResponseContext& ctx);

HttpResponse handleGet(const HttpRequest& req, const ResponseContext& ctx);
HttpResponse handlePost(const HttpRequest& req, const ResponseContext& ctx);
HttpResponse handleDelete(const HttpRequest& req, const ResponseContext& ctx);

HttpResponse buildErrorResponse(int status_code);

#endif
