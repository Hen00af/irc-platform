

HttpResponse handleGetFile(const std::string path)
{
    HttpResponse res;
    std::string body;

    if(!readFile(path, body)) 
        return buildErrorResponse(403);

    res.status_code = 200;
    res.reason_phrase = getReasonPhrase(200);
    res.body = body;

    setBasicHeaders(res);

    return res;
}

HttpResponse handleGet(const HttpRequest& req, const ResponseContext& ctx) {
    HttpResponse res;
    res = handleGetFile(ctx.file_path);

    return res;
}
