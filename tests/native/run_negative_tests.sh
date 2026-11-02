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

# A `bring` of a nonexistent module must be a clear compile-time error.
check_rejects "bring_module_missing" \
    'bring does_not_exist

create main {
    show 1
}
' \
    "module 'does_not_exist' not found"

# A top-level executable statement must be rejected with a diagnostic, not crash.
check_rejects "toplevel_statement_rejected" \
    'show 1
' \
    "top-level statements are not allowed"

# A selective import that is followed by use of a non-imported symbol must fail.
check_rejects "bring_selective_missing_symbol" \
    'bring square from math

create main {
    show clamp with 1, 0, 5
}
' \
    "Call to undeclared function"

# A missing module in a selective import must be a clear error.
check_rejects "bring_selective_missing_module" \
    'bring foo from nosuchmodule

create main {
    show 1
}
' \
    "module 'nosuchmodule' not found"

# A duplicate top-level function definition must be a clear error.
check_rejects "duplicate_function_rejected" \
    'create f using x {
    give x
}

create f using y {
    give y
}

create main {
    show f with 1
}
' \
    "function 'f' is already defined"

# An unwritable output path must be a graceful, nonzero error, not a segfault.
srcfile="$(mktemp)"
log="$(mktemp)"
printf 'create main {\n show 1\n}\n' > "$srcfile"
if "$NURUC" -o /nonexistent_dir/out "$srcfile" >"$log" 2>&1; then
    echo "FAIL  unwritable_output  (compile unexpectedly succeeded)"
    fail=$((fail+1))
elif grep -q "cannot create assembly file" "$log"; then
    echo "PASS  unwritable_output"
    pass=$((pass+1))
else
    echo "FAIL  unwritable_output  (no diagnostic; log below)"
    sed 's/^/      /' "$log"
    fail=$((fail+1))
fi
rm -f "$srcfile" "$log"

echo
echo "negative tests: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
