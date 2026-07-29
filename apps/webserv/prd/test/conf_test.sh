#!/bin/bash
# Usage: ./conf_test.sh
# Builds parseConf and verifies that ValidTestConf/*.conf parse successfully
# while invalidTestConf/*.conf fail.

set -u
# パスを安定にするためのもの
cd "$(dirname "$0")"

SRC_DIR="../src/parseConf"
VALID_DIR="../conf/ValidTestConf"
INVALID_DIR="../conf/invalidTestConf"
BIN="./parseConf.o"

echo "== build =="
c++ -Wall -Wextra -Werror -std=c++98 \
    "$SRC_DIR/parseConf.cpp" \
    "$SRC_DIR/parseConfUtil.cpp" \
    "$SRC_DIR/main_test.cpp" \
    -o "$BIN" || { echo "build failed"; exit 1; }

pass=0
fail=0

run_case() {
    local conf="$1"
    local expect="$2"  # "ok" or "ng"
    "$BIN" "$conf" >/dev/null 2>&1
    local rc=$?
    if { [ "$expect" = "ok" ] && [ $rc -eq 0 ]; } || \
       { [ "$expect" = "ng" ] && [ $rc -ne 0 ]; }; then
        echo "  PASS [$expect] $conf"
        pass=$((pass + 1))
    else
        echo "  FAIL [$expect, rc=$rc] $conf"
        fail=$((fail + 1))
    fi
}

echo "== valid (expect exit 0) =="
for f in "$VALID_DIR"/*.conf; do
    [ -e "$f" ] || continue
    run_case "$f" ok
done

echo "== invalid (expect exit != 0) =="
for f in "$INVALID_DIR"/*.conf; do
    [ -e "$f" ] || continue
    run_case "$f" ng
done

echo "== result: $pass passed, $fail failed =="
rm -f "$BIN"
exit $fail
