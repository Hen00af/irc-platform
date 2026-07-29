*This project was created as part of the 42 curriculum by Hen00af and contributors.*

# Webserv

## Description

Webserv is a small HTTP/1.1 server written in C++98. It uses one `poll()` event
loop for non-blocking socket I/O and supports multiple listening ports, static
files, directory indexes, uploads, redirects, custom error pages, and the GET,
POST, and DELETE methods.

The repository root is the canonical, deployable implementation. Historical
experiments remain under `prd/`, `lab/`, and `sample/` and are excluded from the
container image.

## Instructions

Build and run locally:

```sh
make
./webserv config/default.conf
curl http://localhost:8080/health/
```

Run the unit and component tests:

```sh
make test
make integration-test
make stress-test
```

The test suite covers configuration parsing, HTTP parsing and serialization,
longest-prefix routing, method and redirect decisions, static files, directory
listings, custom error pages, uploads, and deletion. Handler tests create
isolated fixtures under `/tmp` and do not modify `www/`.
The integration test exercises CGI GET/POST and verifies that a static request
is still served while another CGI process is sleeping.

Run on Linux with Docker:

```sh
docker compose up --build -d
docker compose ps
curl http://localhost:8080/
docker compose logs -f webserv
```

Set a different host port with `WEBSERV_PORT=8081 docker compose up -d`.
Uploaded files are stored in the named `uploads` volume. Stop the service with
`docker compose down`; add `--volumes` only when uploaded data should also be
deleted.

The production container:

- builds on Debian Linux with `-Wall -Wextra -Werror -std=c++98`;
- runs as the unprivileged UID 10001;
- has a read-only root filesystem and no Linux capabilities;
- persists only `/app/www/uploads`;
- exposes `/health/` for container health checks;
- handles SIGTERM for graceful container shutdown.

## Configuration

The default configuration is [`config/default.conf`](config/default.conf).
The syntax uses nginx-style braces and semicolons:

```nginx
server {
    listen 0.0.0.0:8080;
    root www;
    allow_methods GET;

    location /upload {
        root www/uploads;
        allow_methods GET POST DELETE;
        upload_dir www/uploads;
    }
}
```

Server directives are `listen`, `root`, `index`, `client_max_body_size`,
`autoindex`, `allow_methods`, and `error_page`. Location directives are `root`,
`index`, `autoindex`, `allow_methods`, `return`, `upload_dir`,
`cgi_extension`, `cgi_path`, and `cgi_timeout`. A redirect may be written as
`return /target;` or `return 302 /target;`. CGI extension and executable path
must be configured together.

The parser rejects malformed blocks, missing semicolons, duplicate singleton
directives, invalid IPv4 listen addresses and ports, invalid numeric ranges,
duplicate methods, duplicate listen addresses, and incomplete CGI settings.

Raw upload example:

```sh
curl -i -X POST \
  -H 'X-Filename: hello.txt' \
  --data-binary 'hello from Docker' \
  http://localhost:8080/upload
```

CGI examples:

```sh
curl 'http://localhost:8080/cgi-bin/echo.py?name=webserv'
curl -X POST --data-binary 'hello CGI' \
  http://localhost:8080/cgi-bin/echo.py
```

## Deployment notes

This service needs a platform that accepts a long-running TCP/HTTP container and
mounts persistent storage for uploads. Configure the platform health check as
`GET /health/` on port `8080`. If the platform injects a `PORT` environment
variable, render a matching config before startup or map that platform port to
container port 8080. Place TLS termination and public HTTPS in front of Webserv
using the platform load balancer or a reverse proxy.

## Resources

- RFC 9110, HTTP Semantics
- RFC 9112, HTTP/1.1
- `poll(2)`, `socket(2)`, and `fcntl(2)` manual pages
- nginx documentation for location and static-file behavior

AI was used to review deployment risks, consolidate the build, draft the
container hardening, and propose tests. All generated changes should be reviewed
and understood by the project contributors before a 42 evaluation.
