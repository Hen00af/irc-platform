FROM debian:bookworm

RUN apt-get update && apt-get install -y \
    build-essential \
    make \
    valgrind \
    curl \
    netcat-openbsd

WORKDIR /app

COPY . .

RUN make