#!/bin/sh
# usage:
#   ./lint.sh          # check only (clang-format dry-run + cpplint)
#   ./lint.sh fix      # auto-format with clang-format

set -e

cd "$(dirname "$0")"

FILES=$(find src mock -type f \( -name "*.cpp" -o -name "*.hpp" \) 2>/dev/null)

if [ "$1" = "fix" ]; then
    echo ">>> clang-format (rewriting in place)"
    echo "$FILES" | xargs clang-format -i
    exit 0
fi

echo ">>> clang-format --dry-run (show files that would change)"
for f in $FILES; do
    if ! clang-format --dry-run -Werror "$f" >/dev/null 2>&1; then
        echo "  needs format: $f"
    fi
done

echo ""
echo ">>> cpplint"
echo "$FILES" | xargs cpplint 2>&1 || true
