#!/usr/bin/env bash
# Builds and runs the feed-parser tests on the host (no PS4 toolchain needed).
set -euo pipefail
cd "$(dirname "$0")"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
${CC:-gcc} -Wall -Wextra -O1 -g -fsanitize=address,undefined -I../src test_feed.c ../src/feed.c ../src/json.c -o "$OUT/test_feed"
check() { # file expected_count
  local got; got="$("$OUT/test_feed" "feeds/$1" | head -1 | sed -E 's/^== [^:]+: (-?[0-9]+).*/\1/')"
  if [ "$got" != "$2" ]; then echo "FAIL $1: expected $2 entries, got $got"; exit 1; fi
  echo "ok   $1 -> $got"
}
check a_array.json 2
check b_wrapped.json 3
check c_fpkgi.json 2
check d_keyed.json 3
check e_bad.json -1
