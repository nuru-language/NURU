#!/usr/bin/env bash
# Compile-time diagnostic tests: programs that must be rejected with a clear
# error and a nonzero exit status, never silently miscompiled.
set -u

NURUC="${NURUC:-./nuruc}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

pass=0
fail=0

check_rejects() {
    local name="$1"
    local src="$2"
    local needle="$3"
    local srcfile="$(mktemp)"
    local log="$(mktemp)"
    printf '%s\n' "$src" > "$srcfile"

    if "$NURUC" -o "$srcfile.bin" "$srcfile" >"$log" 2>&1; then
        echo "FAIL  $name  (compile unexpectedly succeeded)"
        fail=$((fail+1))
    elif grep -q "$needle" "$log"; then
        echo "PASS  $name"
        pass=$((pass+1))
    else
        echo "FAIL  $name  (no expected diagnostic; log below)"
        sed 's/^/      /' "$log"
        fail=$((fail+1))
    fi
    rm -f "$srcfile" "$log" "$srcfile.bin" "$srcfile.tmp" "$srcfile.o"
}

# Floating-point literals must be rejected, not silently truncated to ints.
check_rejects "float_literal_rejected" \
    'create main {
    make x be 1.5
    show x
}
' \
    "floating-point"

echo
echo "negative tests: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
