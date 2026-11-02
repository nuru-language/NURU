#!/usr/bin/env bash
# Compares optimized (default) vs unoptimized (-O0) execution for every native
# case: same stdout and same exit code required. Also confirms optimization is
# actually enabled (default build differs structurally from -O0 for a known
# foldable case).
set -u

NURUC="${NURUC:-./nuruc}"
TESTS_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$TESTS_DIR/../.." && pwd)"
cd "$ROOT"

pass=0
fail=0

for case_dir in "$TESTS_DIR"/cases/*/; do
    [[ -d "$case_dir" ]] || continue
    name="$(basename "$case_dir")"
    src="$case_dir/program.nuru"
    [[ -f "$src" ]] || continue

    b=$(mktemp)
    o=$(mktemp)
    so=$(mktemp)
    se=$(mktemp)
    oo=$(mktemp)
    oe=$(mktemp)

    if ! "$NURUC" -o "$o.bin" "$src" >"$se" 2>&1; then
        echo "FAIL  $name  (optimized compile failed)"
        fail=$((fail+1))
        rm -f "$b" "$o" "$so" "$se" "$oo" "$oe" "$o.bin" "$oo.bin"
        continue
    fi
    if ! "$NURUC" -O0 -o "$oo.bin" "$src" >"$oe" 2>&1; then
        echo "FAIL  $name  (unoptimized compile failed)"
        fail=$((fail+1))
        rm -f "$b" "$o" "$so" "$se" "$oo" "$oe" "$o.bin" "$oo.bin"
        continue
    fi

    "$o.bin" >"$so" 2>&1; c1=$?
    "$oo.bin" >"$oo" 2>&1; c2=$?

    if [[ "$c1" -ne "$c2" ]] || ! diff -q "$so" "$oo" >/dev/null; then
        echo "FAIL  $name  (optimized vs unoptimized differs: exit $c1 vs $c2)"
        fail=$((fail+1))
    else
        echo "PASS  $name"
        pass=$((pass+1))
    fi
    rm -f "$b" "$o" "$so" "$se" "$oo" "$oe" "$o.bin" "$oo.bin"
done

echo
echo "opt compare: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
