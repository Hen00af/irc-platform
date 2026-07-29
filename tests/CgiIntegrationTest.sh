#!/bin/sh

set -eu

./webserv config/default.conf >/tmp/webserv-cgi-integration.log 2>&1 &
server_pid=$!
slow_pid=

cleanup() {
    if [ -n "$slow_pid" ]; then
        kill "$slow_pid" 2>/dev/null || true
    fi
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

attempt=0
while [ "$attempt" -lt 5 ]; do
    if curl -fsS http://127.0.0.1:8080/health/ >/tmp/webserv-cgi-ready; then
        break
    fi
    attempt=$((attempt + 1))
    sleep 1
done

curl -fsS 'http://127.0.0.1:8080/cgi-bin/echo.py?name=webserv' \
    >/tmp/webserv-cgi-get
test "$(sed -n '1p' /tmp/webserv-cgi-get)" = "method=GET"
test "$(sed -n '2p' /tmp/webserv-cgi-get)" = "query=name=webserv"
test "$(sed -n '4p' /tmp/webserv-cgi-get)" = "remote_addr=127.0.0.1"
test "$(sed -n '5p' /tmp/webserv-cgi-get)" = \
    "request_uri=/cgi-bin/echo.py?name=webserv"

curl -fsS http://127.0.0.1:8080/cgi-bin/hello.py \
    >/tmp/webserv-cgi-hello
test "$(sed -n '1p' /tmp/webserv-cgi-hello)" = "Hello from webserv CGI"

curl -fsS 'http://127.0.0.1:8080/cgi-bin/form.py?name=alice' \
    >/tmp/webserv-cgi-form-get
test "$(sed -n '1p' /tmp/webserv-cgi-form-get)" = "method=GET"
test "$(sed -n '2p' /tmp/webserv-cgi-form-get)" = "name=alice"

curl -fsS -X POST --data-binary 'form body' \
    http://127.0.0.1:8080/cgi-bin/form.py >/tmp/webserv-cgi-form-post
test "$(sed -n '1p' /tmp/webserv-cgi-form-post)" = "method=POST"
test "$(sed -n '3p' /tmp/webserv-cgi-form-post)" = "body=form body"

curl -fsS -X POST -H 'Content-Type: text/plain' --data-binary 'hello cgi' \
    http://127.0.0.1:8080/cgi-bin/echo.py >/tmp/webserv-cgi-post
test "$(sed -n '1p' /tmp/webserv-cgi-post)" = "method=POST"
test "$(sed -n '3p' /tmp/webserv-cgi-post)" = "body=hello cgi"

test "$(curl -sS -o /tmp/webserv-cgi-status -w '%{http_code}' \
    'http://127.0.0.1:8080/cgi-bin/status.py?code=201')" = "201"
test "$(sed -n '1p' /tmp/webserv-cgi-status)" = "status=201"

test "$(curl -sS -D /tmp/webserv-cgi-redirect-headers \
    -o /tmp/webserv-cgi-redirect -w '%{http_code}' \
    http://127.0.0.1:8080/cgi-bin/redirect.py)" = "302"
tr -d '\r' </tmp/webserv-cgi-redirect-headers \
    >/tmp/webserv-cgi-redirect-headers-clean
grep -q '^Location: /$' /tmp/webserv-cgi-redirect-headers-clean

curl -fsS -D /tmp/webserv-cgi-cookie-headers \
    -o /tmp/webserv-cgi-cookie http://127.0.0.1:8080/cgi-bin/cookie.py
tr -d '\r' </tmp/webserv-cgi-cookie-headers \
    >/tmp/webserv-cgi-cookie-headers-clean
grep -q '^Set-Cookie: webserv=cgi;' /tmp/webserv-cgi-cookie-headers-clean

curl -fsS -c /tmp/webserv-cgi-session-cookies \
    http://127.0.0.1:8080/cgi-bin/session.py >/tmp/webserv-cgi-session-new
test "$(sed -n '2p' /tmp/webserv-cgi-session-new)" = "existing=no"
curl -fsS -b /tmp/webserv-cgi-session-cookies \
    http://127.0.0.1:8080/cgi-bin/session.py >/tmp/webserv-cgi-session-existing
test "$(sed -n '2p' /tmp/webserv-cgi-session-existing)" = "existing=yes"
test "$(sed -n '1p' /tmp/webserv-cgi-session-new)" = \
    "$(sed -n '1p' /tmp/webserv-cgi-session-existing)"

if [ -x /usr/bin/php-cgi ]; then
    curl -fsS 'http://127.0.0.1:8080/cgi-bin/hello.php?runtime=php' \
        >/tmp/webserv-cgi-php
    test "$(sed -n '1p' /tmp/webserv-cgi-php)" = "Hello from PHP-CGI"
    test "$(sed -n '3p' /tmp/webserv-cgi-php)" = "query=runtime=php"
fi

curl -fsS 'http://127.0.0.1:8080/cgi-bin/slow.py?seconds=2' \
    >/tmp/webserv-cgi-slow &
slow_pid=$!
sleep 1
test "$(curl --max-time 1 -sS -o /tmp/webserv-cgi-health \
    -w '%{http_code}' http://127.0.0.1:8080/health/)" = "200"
wait "$slow_pid"
slow_pid=

test "$(curl -sS -o /tmp/webserv-cgi-missing -w '%{http_code}' \
    http://127.0.0.1:8080/cgi-bin/missing.py)" = "404"
test "$(curl -sS -o /tmp/webserv-cgi-error -w '%{http_code}' \
    http://127.0.0.1:8080/cgi-bin/error.py)" = "500"
test "$(curl -sS -o /tmp/webserv-cgi-malformed -w '%{http_code}' \
    'http://127.0.0.1:8080/cgi-bin/echo.py?mode=malformed')" = "500"
test "$(curl --max-time 6 -sS -o /tmp/webserv-cgi-timeout -w '%{http_code}' \
    'http://127.0.0.1:8080/cgi-bin/slow.py?seconds=5')" = "504"

echo "CGI integration tests passed"
