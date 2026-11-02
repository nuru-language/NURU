# Nuru Runtime — Tracing Mark-and-Sweep GC

Status: design + host-side prototype. The collector algorithm is implemented and
tested in `tests/nir/test_gc.c`; integration into the emitted assembly runtime
is the remaining work.

## Why

The runtime uses reference counting (`runtime/README.md`). It is sound for the
value-semantics programs expressible today, but cyclic structures (if they ever
become expressible) and leaked references are not reclaimed. Mark-and-sweep
adds full-graph collection.

## Algorithm

- **Objects** form a directed graph; each object holds typed references to
  children (strings/leaves have none; lists reference their elements; maps
  reference keys and values; errors reference message/category/line).
- **Roots** are the live program references: the evaluation stack entries and
  the global `_err_value`. Conservative root discovery scans the machine stack
  (between the recorded stack bottom and `rsp` at collection time) for words
  that look like tagged object pointers (tag 4–7, payload within the heap
  range). False positives only keep extra objects alive (safe).
- **Mark**: start from roots; recursively mark reachable objects. A separate
  mark bit (not the refcount) is used so reference counting and collection do
  not interfere.
- **Sweep**: visit every allocated object; reclaim those not marked; clear the
  mark bits for the next cycle.

## Allocation & triggering

- Allocation remains bump-based over a contiguous arena; each block header
  stores its size so objects can be enumerated during sweep.
- When bump space is exhausted, `_alloc` invokes `gc_collect()` first; if that
  frees space, allocation retries, otherwise it fails (returns 0) as today.
- `gc_collect()` is also exposed as a callable helper for testing/`nuru
  build --gc` diagnostics.

## Cycle handling

Because marking reaches objects from roots regardless of incoming reference
counts, unreachable cycles are reclaimed (tested:
`gc_cycle_reclaimed`), while reachable cycles are preserved
(`gc_reachable_cycle_preserved`).

## Integration notes

- Add the object walker (kind dispatch) and stack-root scan to the emitted
  runtime (`runtime/platform/linux/emit_runtime.c` → `_gc_collect`).
- Keep `_incref`/`_decref` as-is; collection is supplementary and does not
  change ownership semantics.
- The host prototype (`tests/nir/test_gc.c`) validates the algorithm; the
  assembly version must pass the same behavioral tests plus a memory stress
  case.
