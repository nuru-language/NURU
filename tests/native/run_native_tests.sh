#!/usr/bin/env bash
# Native execution tests for the NURU compiler.
# Each case is a directory containing:
#   program.nuru  - source
#   expected.txt   - expected stdout
#   (optional) exit_code - expected exit code (default 0)
set -u

NURUC="${NURUC:-./nuruc}"
TESTS_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$TESTS_DIR/../.." && pwd)"
cd "$ROOT"

pass=0
fail=0

run_case() {
    local dir="$1"
    local name
    name="$(basename "$dir")"
    local src="$dir/program.nuru"
    local expected="$dir/expected.txt"
    local want_code=0

    if [[ -f "$dir/exit_code" ]]; then
        want_code="$(cat "$dir/exit_code")"
    fi

    local out
    out="$(mktemp)"
    local err
    err="$(mktemp)"

    if ! "$NURUC" -o "$out.bin" "$src" >"$err" 2>&1; then
        echo "FAIL  $name  (compile failed)"
        sed 's/^/      /' "$err"
        fail=$((fail+1))
        rm -f "$out" "$err" "$out.bin"
        return
    fi

    if [[ -f "$dir/input.txt" ]]; then
        "$out.bin" < "$dir/input.txt" >"$out" 2>&1
    else
        "$out.bin" >"$out" 2>&1
    fi
    local got_code=$?

    if [[ "$got_code" -ne "$want_code" ]]; then
        echo "FAIL  $name  (exit $got_code, wanted $want_code)"
        fail=$((fail+1))
    elif ! diff -u "$expected" "$out" >"$err" 2>&1; then
        echo "FAIL  $name  (output mismatch)"
        sed 's/^/      /' "$err"
        fail=$((fail+1))
    else
        echo "PASS  $name"
        pass=$((pass+1))
    fi

    rm -f "$out" "$err" "$out.bin"
}

for case_dir in "$TESTS_DIR"/cases/*/; do
    [[ -d "$case_dir" ]] || continue
    run_case "$case_dir"
done

echo
echo "native tests: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
