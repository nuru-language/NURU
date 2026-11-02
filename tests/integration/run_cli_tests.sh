#!/usr/bin/env bash
# Integration tests for the `nuru` command-line tool.
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

pass=0
fail=0

# version
v="$(./nuru version)"
if [[ "$v" == "nuru 0.1.0" ]]; then
    echo "PASS  cli_version"; pass=$((pass+1))
else
    echo "FAIL  cli_version (got '$v')"; fail=$((fail+1))
fi

# check
if ./nuru check examples/hello/hello.nuru >/dev/null 2>&1; then
    echo "PASS  cli_check"; pass=$((pass+1))
else
    echo "FAIL  cli_check"; fail=$((fail+1))
fi

# build + run
t="$(mktemp)"; rm -f "$t"
if ./nuru build -o "$t" examples/hello/hello.nuru >/dev/null 2>&1 && \
   [[ "$("$t")" == "Hello, Nuru!" ]]; then
    echo "PASS  cli_build_run"; pass=$((pass+1))
else
    echo "FAIL  cli_build_run"; fail=$((fail+1))
fi
rm -f "$t" "$t.tmp" "$t.o"

# run
out="$(./nuru run examples/hello/hello.nuru 2>/dev/null | tail -1)"
if [[ "$out" == "Hello, Nuru!" ]]; then
    echo "PASS  cli_run"; pass=$((pass+1))
else
    echo "FAIL  cli_run (got '$out')"; fail=$((fail+1))
fi

# new + directory build/run
d="$(mktemp -d)"; rm -rf "$d"
if ./nuru new "$d" >/dev/null 2>&1 &&
   [[ -f "$d/src/main.nuru" ]] && [[ -f "$d/nuru.toml" ]] &&
   ./nuru build -o "$d/app" "$d" >/dev/null 2>&1 &&
   [[ "$("$d/app")" == "Hello, Nuru!" ]] &&
   [[ "$(./nuru run "$d" 2>/dev/null | tail -1)" == "Hello, Nuru!" ]]; then
    echo "PASS  cli_new_dir"; pass=$((pass+1))
else
    echo "FAIL  cli_new_dir"; fail=$((fail+1))
fi
rm -rf "$d"

echo
echo "cli tests: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
