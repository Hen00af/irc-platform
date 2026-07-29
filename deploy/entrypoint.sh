#!/bin/bash
set -euo pipefail

: "${IRC_PASSWORD:?IRC_PASSWORD must be set}"

pids=()

shutdown() {
  trap - TERM INT
  if ((${#pids[@]})); then
    kill -TERM "${pids[@]}" 2>/dev/null || true
    wait "${pids[@]}" 2>/dev/null || true
  fi
}

trap shutdown TERM INT EXIT

(
  cd /app/irc
  exec ./ircserv "${IRC_PORT:-6667}"
) &
pids+=("$!")

(
  cd /app/gateway
  exec node src/server.js
) &
pids+=("$!")

(
  cd /app/webserv
  exec ./webserv config/default.conf
) &
pids+=("$!")

wait -n "${pids[@]}"
echo "entrypoint: a service exited; stopping the deployment" >&2
exit 1
