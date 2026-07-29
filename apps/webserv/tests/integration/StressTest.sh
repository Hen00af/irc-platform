#!/bin/sh

set -eu

./webserv config/default.conf >/tmp/webserv-stress.log 2>&1 &
server_pid=$!
request_pids=

cleanup() {
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

attempt=0
while [ "$attempt" -lt 5 ]; do
    if curl -fsS http://127.0.0.1:8080/health/ >/dev/null; then
        break
    fi
    attempt=$((attempt + 1))
    sleep 1
done

count=0
while [ "$count" -lt 44 ]; do
    curl -fsS http://127.0.0.1:8080/ >/dev/null &
    request_pids="$request_pids $!"
    count=$((count + 1))
done

count=0
while [ "$count" -lt 8 ]; do
    curl -fsS "http://127.0.0.1:8080/cgi-bin/echo.py?sleep=0.1&id=$count" \
        >/dev/null &
    request_pids="$request_pids $!"
    count=$((count + 1))
done

count=0
while [ "$count" -lt 8 ]; do
    curl -fsS -X POST --data-binary "body-$count" \
        http://127.0.0.1:8080/cgi-bin/echo.py >/dev/null &
    request_pids="$request_pids $!"
    count=$((count + 1))
done

failed=0
for request_pid in $request_pids; do
    if ! wait "$request_pid"; then
        failed=1
    fi
done
test "$failed" = "0"

test "$(curl -sS -o /tmp/webserv-stress-health -w '%{http_code}' \
    http://127.0.0.1:8080/health/)" = "200"
sleep 1
children=$(pgrep -P "$server_pid" || true)
test -z "$children"

echo "Stress tests passed: 60 concurrent requests, no remaining CGI children"
