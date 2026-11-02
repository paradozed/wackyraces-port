# Ghidra decompilation audit and Linux x64 port notes

## Audit scope

The machine output is in `decompilation/wpc/`. Its index has 5,434 records,
all marked `decompiled` by Ghidra 12.1.2 DEV. This means the decompiler returned
a C-like representation for each function Ghidra had identified; it does not
mean each result is semantically correct or compilable. 2,742 function bodies
are shorter than 16 bytes and 1,935 are shorter than 8 bytes, so a large share
of the count is small wrappers and thunks. There are 101 bodies at least 1,000
bytes long.

## Cross-checks against disassembly

- `0x004ada60` agrees with `wpc-main-lifecycle.txt`: it requests the
  `WackyMap` mapping, checks `ERROR_ALREADY_EXISTS`, calls startup, pumps
  messages, calls the update path while the active flag is set, waits on the
  idle path, and cleans up after `WM_QUIT`. It returns a nonzero result on
  startup failure. This is a strong control-flow cross-check, though exact
  parameter types and global declarations remain inferred.
- `0x004ad1f0` copies the executable path into a local buffer, sets the current
  directory, calls initialization routines, chooses menu/general archive
  names using global state, and invokes virtual methods. The initialization
  routines and virtual targets still need separate behavior checks; their
  Ghidra names are not recovered original names.
- `0x004ad540` reads `GetTickCount`, normalizes a timing value, invokes two
  application virtual slots, waits until at least `0x21` ms have elapsed, and
  invokes additional update helpers. This is consistent with a timed frame
  path, but does not establish the complete frame or render order.
- `0x004133f0` opens AVI files with AVIFile, obtains optional `vids` and `auds`
  streams, reads stream information, opens frame retrieval and DrawDib state,
  and initializes audio buffering. `0x00413760` fetches a video frame and
  calls `DrawDibDraw`. These results agree with the import table and observed
  intro playback. They establish the original playback path but not a
  portable implementation of the original window or audio synchronization.

The focused listing files and function index are navigational aids. When a
pseudocode result determines a porting decision, verify its instructions and
callers in the original image. For example, import-address functions can be
represented as calls back to the same import name; that is a thunk, not a
recursive implementation. Ghidra also emits `undefined4`, `unaff_EBP`, and
absolute `DAT_...` globals where type recovery is weak.

## x64 port constraints and current state

`wpc.exe` is PE32 i386. Its pseudocode includes x86 `thiscall`/`fastcall`
conventions, 32-bit pointers, packed object offsets, and stack/register
artifacts. Those object layouts cannot be copied into a 64-bit process by
changing pointer widths; doing so would alter offsets and invalidate embedded
pointer assumptions. The C port must preserve behavior with explicit native
types and bounded parsers, then implement subsystems against Linux interfaces.

The native C port under `c-port/` builds as an ELF x86-64 Linux binary. It
checks a per-user instance lock, sets the game directory, plays the intro AVI
through `ffplay`, and opens an SDL2 window displaying the supplied screenshot.
Its WGM inspector validates top-level and nested resource framing, CLAS names,
menu BRTR properties, and joins main-menu text keys to `STRINGS.DAT`. Menu
rendering, actual page transitions, game logic, DirectDraw, DirectInput, and
DirectSound behavior remain unported. See `c-port/TODO.md` for the active
implementation checklist.
