#!/usr/bin/env bash
# Regression tests for safe external-tool invocation (no shell injection).
#
# The compiler invokes the assembler and linker via fork/execvp, passing
# paths as separate argv entries. This test verifies:
#   1. Output paths containing shell metacharacters are handled literally
#      (no command substitution, word splitting, or globbing).
#   2. A missing external tool is reported as a clean nonzero failure with a
#      diagnostic.
set -u

NURUC="${NURUC:-./nuruc}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

cat > "$TMP/prog.nuru" <<'EOF'
create main {
    show 7
}
EOF

# --- 1. safe filenames with shell metacharacters ---
# Deliberately contains characters the shell would otherwise interpret:
# $(...) command substitution, quotes, semicolon, pipe, glob, spaces.
name="$TMP/out $(echo pwned) ' ; & | *.bin"
if ! "$NURUC" -o "$name" "$TMP/prog.nuru" >"$TMP/compile.log" 2>&1; then
    echo "FAIL  safe_io_filenames  (compile failed)"
    sed 's/^/      /' "$TMP/compile.log"
    fail=$((fail+1))
else
    if [[ -f "$name" ]]; then
        got="$("$name" 2>&1)"
        if [[ "$got" == "7" ]]; then
            echo "PASS  safe_io_filenames"
            pass=$((pass+1))
        else
            echo "FAIL  safe_io_filenames  (output '$got', wanted '7')"
            fail=$((fail+1))
        fi
    else
        echo "FAIL  safe_io_filenames  (output file not created at literal path)"
        fail=$((fail+1))
    fi
fi

# --- 2. missing tool (as) -> clean nonzero failure ---
if PATH="/nonexistent" "$NURUC" -o "$TMP/normal.bin" "$TMP/prog.nuru" \
        >"$TMP/missing.log" 2>&1; then
    echo "FAIL  safe_io_missing_tool  (compile unexpectedly succeeded)"
    fail=$((fail+1))
else
    if grep -q "Assembly failed" "$TMP/missing.log"; then
        echo "PASS  safe_io_missing_tool"
        pass=$((pass+1))
    else
        echo "FAIL  safe_io_missing_tool  (no diagnostic; log below)"
        sed 's/^/      /' "$TMP/missing.log"
        fail=$((fail+1))
    fi
fi

echo
echo "safe io tests: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
