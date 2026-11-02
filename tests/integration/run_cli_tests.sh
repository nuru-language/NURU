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

# new + directory build/run + manifest-driven build
d="$(mktemp -d)"; rm -rf "$d"
if ./nuru new "$d" >/dev/null 2>&1 &&
   [[ -f "$d/src/main.nuru" ]] && [[ -f "$d/nuru.toml" ]] &&
   ./nuru build -o "$d/app" "$d" >/dev/null 2>&1 &&
   [[ "$("$d/app")" == "Hello, Nuru!" ]] &&
   [[ "$(./nuru run "$d" 2>/dev/null | tail -1)" == "Hello, Nuru!" ]] &&
   sed -i 's/name = ".*"/name = "myapp"/' "$d/nuru.toml" &&
   ./nuru build "$d" >/dev/null 2>&1 &&
   [[ -x "$d/build/myapp" ]] &&
   [[ "$("$d/build/myapp")" == "Hello, Nuru!" ]]; then
    echo "PASS  cli_new_dir"; pass=$((pass+1))
else
    echo "FAIL  cli_new_dir"; fail=$((fail+1))
fi

# manifest module_path + local module
mkdir -p "$d/lib"
printf 'create greet using name {\n give concat with "Hello, ", name\n}\n' > "$d/lib/greet.nuru"
printf 'module_path = "lib"\n' >> "$d/nuru.toml"
printf 'bring greet\n\ncreate main {\n show greet with "Nuru"\n}\n' > "$d/src/main.nuru"
if [[ "$(./nuru run "$d" 2>/dev/null | tail -1)" == "Hello, Nuru" ]]; then
    echo "PASS  cli_module_path"; pass=$((pass+1))
else
    echo "FAIL  cli_module_path"; fail=$((fail+1))
fi

# lock (write + in-sync check)
if ./nuru lock "$d" >/dev/null 2>&1 &&
   [[ -s "$d/nuru.lock" ]] &&
   ./nuru lock --check "$d" >/dev/null 2>&1; then
    echo "PASS  cli_lock"; pass=$((pass+1))
else
    echo "FAIL  cli_lock"; fail=$((fail+1))
fi

# lock drift detection (mutate module -> check must fail)
printf 'create greet using n {\n give "changed"\n}\n' > "$d/lib/greet.nuru"
if ./nuru lock --check "$d" >/dev/null 2>&1; then
    echo "FAIL  cli_lock_drift"; fail=$((fail+1))
else
    echo "PASS  cli_lock_drift"; pass=$((pass+1))
fi

# --locked build (in sync -> ok; drift -> fail)
printf 'create greet using n {\n give concat with "hi ", n\n}\n' > "$d/lib/greet.nuru"
./nuru lock "$d" >/dev/null 2>&1
if ./nuru build --locked -o "$d/app" "$d" >/dev/null 2>&1 &&
   [[ "$("$d/app")" == "hi Nuru" ]]; then
    echo "PASS  cli_locked_build"; pass=$((pass+1))
else
    echo "FAIL  cli_locked_build"; fail=$((fail+1))
fi
printf 'create greet using n {\n give "changed"\n}\n' > "$d/lib/greet.nuru"
if ./nuru build --locked -o "$d/app" "$d" >/dev/null 2>&1; then
    echo "FAIL  cli_locked_build_drift"; fail=$((fail+1))
else
    echo "PASS  cli_locked_build_drift"; pass=$((pass+1))
fi

# test subcommand
printf 'create main {\n show 42\n}\n' > "$d/tests/ok.nuru"
printf '42\n' > "$d/tests/ok.expected"
if ./nuru test "$d" >/dev/null 2>&1 &&
   [[ "$(./nuru test "$d" 2>/dev/null | tail -1)" == "test: 1 passed, 0 failed" ]]; then
    echo "PASS  cli_test"; pass=$((pass+1))
else
    echo "FAIL  cli_test"; fail=$((fail+1))
fi
rm -rf "$d"

# doctor
if ./nuru doctor >/dev/null 2>&1; then
    echo "PASS  cli_doctor"; pass=$((pass+1))
else
    echo "FAIL  cli_doctor"; fail=$((fail+1))
fi

echo
echo "cli tests: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
