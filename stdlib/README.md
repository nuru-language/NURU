# Nuru Standard Library

The standard library is a set of Nuru source modules. A program pulls a module
in with a `bring` directive:

```nuru
bring math
bring core
```

## How `bring` works

- `bring <module>` is expanded by the compiler driver before lexing: the line
  is replaced with the contents of the resolved module file.
- Module search order: `stdlib/<module>.nuru`, then `<module>.nuru`
  (relative to the current directory).
- Each module file is included at most once per compilation (include-once), so
  importing the same module twice is safe.
- A missing module is a compile-time error.
- Modules may use `bring` themselves (recursive resolution).

## Modules

| Module | Contents |
|--------|----------|
| `core` | `triple(x)` |
| `math` | `square(x)`, `cube(x)`, `clamp(x, lo, hi)` |
