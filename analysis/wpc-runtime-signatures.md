# `wpc.exe` compiler/runtime signatures

Static evidence only; addresses are radare2 6.2.2 analysis addresses for the
installed `wpc.exe`.

## Observed signatures

- The PE COFF linker-version bytes are `6.0`; PE entry point is
  `0x004f18a0`. The executable imports Windows/system/game APIs but does not
  import `MSVCRT.dll` or another C runtime DLL. Its CRT support is therefore
  statically present in the image or otherwise supplied internally.
- The image contains `Microsoft Visual C++ Debug Library` at VA `0x005190c8`,
  `Microsoft Visual C++ Runtime Library` at `0x005196ec`,
  `__MSVCRT_HEAP_SELECT` at `0x005193d8`, the CRT report string
  `dbgrpt.c`, and `R6002 - floating point not loaded`. Radare2 cross-references
  the first string from `0x004f38c0`, the runtime library string from
  `0x004fc960`, and the heap-select symbol from `0x004fa1a0`.
- The entry routine at `0x004f18a0` has CRT-like startup operations: `GetVersion`,
  command-line/startup-info access, runtime initialization calls, then transfer
  through `0x00402185` to the application function labeled `main` at
  `0x004ada60`.

These facts strongly support Microsoft Visual C++ 6-era linkage with static
runtime components, including debug-report support. Linker version 6.0 alone
cannot prove the exact compiler release or build flags. Runtime helper
functions can be identified, but no reliable whole-program boundary cleanly
separates all CRT/library code from game logic yet; the main executable has no
useful symbols and radare2's function boundaries are approximate.

## Scope of manual classification

Known runtime candidates for focused exclusion from game behavior analysis:

- `0x004f18a0`: CRT-like process entry/startup.
- `0x004f38c0`: debug report/assert path (`dbgrpt.c` and Visual C++ Debug
  Library string references).
- `0x004fc960`: Visual C++ Runtime Library reporting path.
- `0x004fa1a0`: `__MSVCRT_HEAP_SELECT` handling.

This list is not an exhaustive CRT function inventory. Game subsystem anchors
are listed separately in `wpc-static-map.md`.
