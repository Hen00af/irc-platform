FROM nginx:latest

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        curl \
        vim \
        netcat-openbsd \
        python3 \
        fcgiwrap \
        spawn-fcgi \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY ./nginx.conf /etc/nginx/conf.d/default.conf

# CGI 用 FastCGI ブリッジ (fcgiwrap) を nginx 起動前に立ち上げる
COPY ./40-spawn-fcgiwrap.sh /docker-entrypoint.d/40-spawn-fcgiwrap.sh
RUN chmod +x /docker-entrypoint.d/40-spawn-fcgiwrap.sh