# NIR v0.2 — Design Proposal

Status: proposal (Phase 6 Stage C). This describes a control-flow-graph NIR
that can replace the current stack-machine IR (`NIR.md`). It is a design and a
migration plan, not yet an implementation.

## 1. Goals

The current stack-machine NIR is simple but makes the native backend, alias
analysis, and the optimizer harder to reason about. v0.2 targets:

- Explicit basic blocks and branch edges (already partially present).
- Virtual registers instead of an implicit evaluation stack.
- Clear function boundaries and parameter passing.
- Local and global storage.
- First-class function / closure support.
- First-class list and map operations.
- Runtime checks (bounds, division, overflow, map keys).
- Debug metadata (source line/column).

## 2. Core concepts

- **NIRFunction v0.2**: name, parameter list (vregs), entry block, exit block,
  local/global slots, and a module/path identity.
- **NIRBasicBlock v0.2**: an ordered list of instructions ending in a
  terminator (`jmp`, `brcc`, `return`, `halt`, `unreachable`).
- **Virtual registers** (`%v0..%vN`): infinite set; SSA is an option but not
  required initially. Every value-producing instruction targets a vreg.
- **CFG edges**: explicit successor lists on each block, derived from
  terminators; enables dominance, liveness, and dead-code elimination.

## 3. Instruction set sketch

Typed operands are vregs or constants. Selected operations:

```
load_i %r, const
load_str %r, const
mov %r, %s              ; register copy (with refcount semantics)
store_var %slot, %r
load_var %r, %slot
add %r, %a, %b          ; also sub/mul/div/mod, with runtime checks
cmp %r, %a, %b, kind
neg %r, %a
not %r, %a
call %r, %callee, args...
return %r?
jmp B
brcc B, %cond           ; conditional branch
br %cond, Bt, Bf
len %r, %a
get_elt %r, %coll, %idx  ; list/map dispatch
set_elt %coll, %idx, %val
mk_list %r, elems...
mk_map %r, pairs...
field_get %r, %obj, %key
err_save / err_set / err_restore
debug line, col
```

Refcount/ownership and the error model (err_jmp / err_value) carry over as
described in `NIR.md`.

## 4. Lowering from the stack machine

A mechanical pass converts the stack IR to vregs:

- Maintain a stack of theregs representing the evaluation stack.
- `LOAD_CONST`/`LOAD_VAR`/call results allocate a fresh vreg and push it.
- Binary ops pop two vregs, compute into a new vreg, push it.
- Block structure maps 1:1; jump/conditional-jump targets become CFG edges.

This pass is a small, testable component and can be validated by running the
existing native test suite (which is the migration oracle).

## 5. Benefits unlocked

- The optimizer becomes pass-oriented (constant propagation, common-subexpr
  elimination, dead-code elimination, branch folding) instead of peephole-only.
- The backend no longer tracks an implicit stack; register allocation can be
  simple (linear scan) initially.
- Closures and first-class functions become expressible (a function value =
  a code pointer + captured environment frame, both referenced/refcounted).

## 6. Migration plan

1. Add a `niru_irgen_v2.c` producing v0.2 (opt-in via driver flag).
2. Add a `nuru_codegen_v2.c` x86-64 backend for v0.2.
3. Run the full native suite against the v0.2 backend; keep the stack backend
   until parity (identical stdout/exit) is demonstrated.
4. Port the optimizer passes; re-run optimizer unit tests + equivalence compare.
5. Once parity holds, remove the stack IR and backend.

Guidance: do **not** maintain two untested IRs indefinitely; the stack IR is
the fallback only until v0.2 reaches parity.

## 7. Non-goals for v0.2

- SSA construction/phi nodes (defer; vregs + liveness suffice).
- Aggressive global optimization (defer to a later pass phase).
- Platform-independent backend (still x86-64 Linux; PE later).
