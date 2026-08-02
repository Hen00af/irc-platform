#!/bin/bash
set -uo pipefail

log() {
  echo "entrypoint: $*" >&2
}

# Single-port platforms (Render, Heroku) publish one port and announce it as
# PORT; there the edge proxy is required. Set EDGE_PROXY explicitly to override.
edge_proxy="${EDGE_PROXY:-}"
if [[ -z "$edge_proxy" ]]; then
  if [[ -n "${PORT:-}" ]]; then edge_proxy=on; else edge_proxy=off; fi
fi

log "config edge_proxy=${edge_proxy} port=${PORT:-unset} irc_port=${IRC_PORT:-6667} ws_port=${WS_PORT:-3001} webserv_config=${WEBSERV_CONFIG:-config/deploy.conf}"

if [[ -z "${IRC_PASSWORD:-}" ]]; then
  log "IRC_PASSWORD is not set; refusing to start"
  log "set it in the platform environment (Render: Environment tab, or let"
  log "render.yaml generate it by deploying as a Blueprint)"
  exit 1
fi

pids=()
names=()

shutdown() {
  trap - TERM INT EXIT
  if ((${#pids[@]})); then
    kill -TERM "${pids[@]}" 2>/dev/null || true
    wait "${pids[@]}" 2>/dev/null || true
  fi
}

trap shutdown TERM INT EXIT

start() {
  local name="$1" dir="$2"
  shift 2
  (
    cd "$dir" || exit 1
    exec "$@"
  ) &
  local pid=$!
  pids+=("$pid")
  names+=("$name")
  log "started ${name} (pid ${pid})"
}

start irc /app/irc ./ircserv "${IRC_PORT:-6667}"
start gateway /app/gateway node src/server.js
# deploy.conf is the default inside the image, not default.conf: this script
# only ever runs in a container, and a container is reachable from the
# internet. default.conf allows POST and DELETE on /upload and /files, which
# is fine on a laptop and is not fine on a public origin. Relying on the
# platform to pass WEBSERV_CONFIG meant one unset variable served the
# permissive config, which is exactly what production was doing.
start webserv /app/webserv ./webserv "${WEBSERV_CONFIG:-config/deploy.conf}"

if [[ "$edge_proxy" == "on" ]]; then
  start edge /app node deploy/edge.js
fi

# `wait -n` must not run under `set -e`: a non-zero child status would abort the
# script here and the diagnostic below would never be printed.
dead_pid=""
status=0
wait -n -p dead_pid "${pids[@]}" || status=$?

dead_name="unknown"
for i in "${!pids[@]}"; do
  if [[ -n "$dead_pid" && "${pids[$i]}" == "$dead_pid" ]]; then
    dead_name="${names[$i]}"
    break
  fi
  # Fallback when `wait -p` is unavailable: find the process that is gone.
  if [[ -z "$dead_pid" ]] && ! kill -0 "${pids[$i]}" 2>/dev/null; then
    dead_name="${names[$i]}"
    break
  fi
done

log "${dead_name} exited with status ${status}; stopping the deployment"
exit 1
