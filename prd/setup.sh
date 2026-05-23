#!/bin/sh
# Initial setup for lint/format tooling.
#
# usage: ./setup.sh
#
# - Installs clang-format (via brew on macOS, apt on Linux)
# - Installs cpplint (via pip3)
# - Skips anything already installed
# - Verifies .clang-format / CPPLINT.cfg / lint.sh are present

set -e

cd "$(dirname "$0")"

ok()    { printf "  \033[32mok\033[0m   %s\n" "$1"; }
skip()  { printf "  \033[33mskip\033[0m %s (already installed)\n" "$1"; }
do_()   { printf "  \033[36m...\033[0m  %s\n" "$1"; }
fail()  { printf "  \033[31mfail\033[0m %s\n" "$1"; exit 1; }

echo ">>> checking tools"

if command -v clang-format >/dev/null 2>&1; then
    skip "clang-format"
else
    do_ "installing clang-format"
    if command -v brew >/dev/null 2>&1; then
        brew install clang-format
    elif command -v apt-get >/dev/null 2>&1; then
        sudo apt-get install -y clang-format
    else
        fail "no supported package manager (brew/apt-get) found"
    fi
    ok "clang-format installed"
fi

if command -v cpplint >/dev/null 2>&1; then
    skip "cpplint"
else
    do_ "installing cpplint"
    if command -v pip3 >/dev/null 2>&1; then
        pip3 install cpplint
    elif command -v pip >/dev/null 2>&1; then
        pip install cpplint
    else
        fail "no pip found (need python3)"
    fi
    ok "cpplint installed"
fi

echo ""
echo ">>> checking config files"

for f in .clang-format CPPLINT.cfg lint.sh; do
    if [ -f "$f" ]; then
        ok "$f"
    else
        fail "$f is missing — re-run after restoring it"
    fi
done

if [ ! -x lint.sh ]; then
    chmod +x lint.sh
    ok "made lint.sh executable"
fi

echo ""
echo ">>> done. try:"
echo "  ./lint.sh          # check"
echo "  ./lint.sh fix      # auto-format"
