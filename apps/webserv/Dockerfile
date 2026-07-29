FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY Makefile ./
COPY include ./include
COPY src ./src
RUN make -j"$(nproc)"

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends curl python3 php-cgi \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --create-home webserv

WORKDIR /app
COPY --from=build /src/webserv /app/webserv
COPY config /app/config
COPY www /app/www
RUN chown -R webserv:webserv /app/www/uploads

USER webserv
EXPOSE 8080
HEALTHCHECK --interval=10s --timeout=3s --start-period=3s --retries=3 \
  CMD ["curl", "--fail", "--silent", "--max-time", "2", "http://127.0.0.1:8080/health/"]
ENTRYPOINT ["/app/webserv"]
CMD ["config/default.conf"]
