#!/usr/bin/env bash
# End-to-end self-hosting demonstration:
#  1. Build the Nuru compiler (nuruc).
#  2. Run the bootstrap code generator (a Nuru program compiled by nuruc) to
#     produce x86-64 assembly from a small Nuru subset.
#  3. Assemble + link that assembly into an executable.
#  4. Run it and check the output.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

NURUC="${NURUC:-./nuruc}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "== stage: compile the bootstrap codegen with nuruc =="
"$NURUC" -o "$TMP/codegen" bootstrap/codegen.nuru >"$TMP/cg.log" 2>&1 || { cat "$TMP/cg.log"; echo "codegen compile failed"; exit 1; }

echo "== stage: run codegen to emit assembly for a sample program =="
"$TMP/codegen" > "$TMP/prog.s"
echo "== stage: assemble and link =="
as -o "$TMP/prog.o" "$TMP/prog.s"
ld -o "$TMP/prog" "$TMP/prog.o"

echo "== stage: run the generated program =="
"$TMP/prog" > "$TMP/out.txt"

cat > "$TMP/want.txt" <<'EOF'
20
30
4
99
ok
EOF

if diff -u "$TMP/want.txt" "$TMP/out.txt"; then
    echo "PASS  codegen_e2e"
else
    echo "FAIL  codegen_e2e (output mismatch)"
    exit 1
fi
