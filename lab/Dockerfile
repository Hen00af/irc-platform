FROM nginx:latest

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        curl \
        vim \
        netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY ./nginx.conf /etc/nginx/conf.d/default.conf