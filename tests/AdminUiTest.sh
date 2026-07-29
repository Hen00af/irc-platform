#!/bin/sh

set -eu

./webserv config/default.conf >/tmp/webserv-admin-ui.log 2>&1 &
server_pid=$!

cleanup() {
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

attempt=0
ready=false
while [ "$attempt" -lt 5 ]; do
    if curl -fsS http://127.0.0.1:8080/health/ >/tmp/webserv-admin-ready; then
        ready=true
        break
    fi
    attempt=$((attempt + 1))
    sleep 1
done
test "$ready" = "true"

test "$(curl -sS -o /tmp/webserv-admin-index -w '%{http_code}' \
    http://127.0.0.1:8080/admin/)" = "200"
grep -q '<title>Webserv Control Plane</title>' /tmp/webserv-admin-index

test "$(curl -sS -o /tmp/webserv-admin-css -w '%{http_code}' \
    http://127.0.0.1:8080/admin/styles.css)" = "200"
grep -q 'pulse-rail' /tmp/webserv-admin-css

test "$(curl -sS -o /tmp/webserv-admin-js -w '%{http_code}' \
    http://127.0.0.1:8080/admin/app.js)" = "200"
grep -q 'const MAX_REQUESTS = 100' /tmp/webserv-admin-js
grep -q 'const MAX_CONCURRENCY = 8' /tmp/webserv-admin-js
grep -q 'SAFE_TARGETS' /tmp/webserv-admin-js
grep -q 'LOCAL_HOSTS' /tmp/webserv-admin-js

test "$(curl -sS -o /tmp/webserv-admin-post -w '%{http_code}' \
    -X POST -d '' http://127.0.0.1:8080/admin/)" = "405"

echo "Admin UI integration tests passed"
