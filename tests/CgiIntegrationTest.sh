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

curl -fsS -X POST -H 'Content-Type: text/plain' --data-binary 'hello cgi' \
    http://127.0.0.1:8080/cgi-bin/echo.py >/tmp/webserv-cgi-post
test "$(sed -n '1p' /tmp/webserv-cgi-post)" = "method=POST"
test "$(sed -n '3p' /tmp/webserv-cgi-post)" = "body=hello cgi"

curl -fsS 'http://127.0.0.1:8080/cgi-bin/echo.py?sleep=2' \
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
    'http://127.0.0.1:8080/cgi-bin/echo.py?mode=error')" = "500"
test "$(curl -sS -o /tmp/webserv-cgi-malformed -w '%{http_code}' \
    'http://127.0.0.1:8080/cgi-bin/echo.py?mode=malformed')" = "500"
test "$(curl --max-time 6 -sS -o /tmp/webserv-cgi-timeout -w '%{http_code}' \
    'http://127.0.0.1:8080/cgi-bin/echo.py?sleep=5')" = "504"

echo "CGI integration tests passed"
