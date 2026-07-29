FROM node:22-bookworm-slim AS webserv-build

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src/webserv
COPY apps/webserv/Makefile ./
COPY apps/webserv/include ./include
COPY apps/webserv/src ./src
RUN make -j"$(nproc)"

FROM node:22-bookworm-slim AS irc-build

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src/irc
COPY services/irc/prd ./
RUN make -j"$(nproc)"

FROM node:22-bookworm-slim AS gateway-deps

WORKDIR /src/gateway
COPY services/gateway/package.json services/gateway/package-lock.json ./
RUN npm ci --omit=dev

FROM node:22-bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates curl python3 php-cgi tini \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --create-home relay

WORKDIR /app
COPY --from=webserv-build /src/webserv/webserv /app/webserv/webserv
COPY apps/webserv/config /app/webserv/config
COPY apps/webserv/www /app/webserv/www
COPY --from=irc-build /src/irc/ircserv /app/irc/ircserv
COPY --from=gateway-deps /src/gateway/node_modules /app/gateway/node_modules
COPY services/gateway/package.json /app/gateway/package.json
COPY services/gateway/src /app/gateway/src
COPY deploy/entrypoint.sh /app/entrypoint.sh

RUN mkdir -p /app/webserv/www/uploads \
    && chown -R relay:relay /app \
    && chmod +x /app/entrypoint.sh

USER relay
EXPOSE 8080 3001 6667

ENTRYPOINT ["/usr/bin/tini", "--", "/app/entrypoint.sh"]
