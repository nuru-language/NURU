# Nuru Windows x86-64 Backend

Status: groundwork. The PE32+ container-format builder is implemented and
structurally validated (`compiler/object/nuru_pe.{c,h}` + `tests/nir/test_pe.c`).
Full Windows code generation and execution require a Windows CI runner and are
not yet complete.

## Scope

- **Target**: Windows x86-64, PE32+ executables.
- **Calling convention**: Microsoft x64 (`rcx`, `rdx`, `r8`, `r9`, stack args,
  `rax` return, 16-byte stack alignment at call sites, 32-byte shadow space).
- **Subsystem**: console (`IMAGE_SUBSYSTEM_WINDOWS_CUI`).
- **Entry point**: `mainCRTStartup`-style stub that calls Nuru `main` and
  passes the exit code to `ExitProcess`.
- **Runtime interface**: instead of raw `syscall`, use kernel32 APIs
  (`WriteFile`, `ReadFile`, `GetStdHandle`) or the `ntdll` syscall numbers; the
  emitted runtime (`runtime/platform/linux/emit_runtime.c`) needs a Windows
  counterpart emitting the same helpers against the Win32 ABI.

## What exists

- `compiler/object/nuru_pe.{c,h}`: builds a minimal valid PE32+ image from a
  single `.text` section (DOS stub, PE signature, COFF header, PE32+ optional
  header, section table, section bytes). Structurally validated on any host.
- `tests/nir/test_pe.c`: asserts the emitted image conforms to the expected
  layout (MZ stub, `e_lfanew`, AMD64 machine, one section, PE32+ magic,
  console subsystem, `.text` code section with embedded bytes).

## Remaining (requires a Windows runner)

- x86-64 instruction emission under the Microsoft x64 convention (our codegen
  currently emits AT&T assembly for `as`/`ld`; Windows needs either MASM-style
  emission or direct machine-code generation).
- COFF object/PE linking (or direct image writing from the code generator).
- Windows runtime interface and process startup.
- Execution, integration, and regression testing on real Windows CI.
