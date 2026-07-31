#!/bin/bash
set -uo pipefail

log() {
  echo "entrypoint: $*" >&2
}

if [[ -z "${IRC_PASSWORD:-}" ]]; then
  log "IRC_PASSWORD is not set; refusing to start"
  log "set it in the platform environment (Render: render.yaml generates it)"
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

log "config edge_proxy=${EDGE_PROXY:-off} port=${PORT:-unset} irc_port=${IRC_PORT:-6667} ws_port=${WS_PORT:-3001}"

start irc /app/irc ./ircserv "${IRC_PORT:-6667}"
start gateway /app/gateway node src/server.js
start webserv /app/webserv ./webserv config/default.conf

# Single-port platforms (Render) publish one port only; the edge proxy fans it
# out to webserv and the gateway.
if [[ "${EDGE_PROXY:-off}" == "on" ]]; then
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
