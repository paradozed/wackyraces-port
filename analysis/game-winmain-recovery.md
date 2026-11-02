# Application entry at 0x004ada60

The GNU i386 implementation in `src/game_winmain.cpp` reproduces all 403
instruction bytes at `0x004ada60..0x004adbf3` in the original executable with
SHA-256 `e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`.
The last instruction is `RET 16`; the following 13 NOP bytes are padding and
are excluded. Four stdcall stack arguments follow the WinMain ABI. The saved
decompilation's three-argument signature does not describe the final `RET 16`.
The source uses mnemonic assembly and symbolic relocations, with typed source
definitions for three strings totaling 41 bytes.

Static evidence is the bounded original-image read, GNU objdump disassembly,
the saved function at `analysis/decompilation/wpc/wpc-decompiled.c:14185`, the
reconstructed import table and jump bank. REA was unavailable in this session;
this is local static evidence plus native fixture execution, without a claim
that the game or Windows APIs were executed.

The function stores the first argument at `0x006f5fb8`, calls
`GlobalMemoryStatus`, enforces an unsigned 48 MiB minimum on global
`0x0214c3b8`, and calls the 38-byte CRT malloc wrapper at `0x004eeed0`.
Its return is retained at `0x006f5fc8`, including a null result. It copies the
zero-terminated text at `0x006f58d0` to `0x006f5fd4`, then copies the third
argument to the same destination. A shorter second string leaves observable
bytes from the first copy beyond its new terminator. The source buffer
capacities are not recovered; address aliases do not claim their extent.

The original import slots identify `CreateFileMappingA` with arguments
`(-1, 0, 2, 0, 32, "WackyMap")`. A non-null result followed by last error 183
calls `GetActiveWindow`, displays `"Wacky is running already."` with caption
`"ERROR"` through `MessageBoxA`, closes the mapping and returns 1. A null
mapping skips `GetLastError` and continues. This path retains the mapping
global and allocator-mode byte. It contains no foreground-window API.

The initialization bank slot `0x00404d04` jumps to `0x004ad1f0`. A return
other than exactly 1 calls shutdown slot `0x00405ba5` (target `0x004ad650`)
and returns 2. The message loop calls `PeekMessageA` with flag 1 when
`0x006f5bbc` is zero and flag 3 otherwise. A returned message other than
`WM_QUIT` is translated and dispatched. With no returned message, the code
reads active state again and either calls tick slot `0x00401087` (target
`0x004ad540`) or calls `WaitMessage`. The second active-state read can differ
from the value used to derive the peek flags. On `WM_QUIT`, shutdown runs;
the current mapping value is then read and closed/cleared if nonzero, and the
allocator-mode byte at `0x0214c398` is cleared before returning 0.

`tools/verify_game_winmain.py --standalone` verifies byte equality, all 28
absolute references against the reconstructed HIGHLOW relocation sites,
the direct-call inventory, the ten actual import identities/ABIs, three bank
destinations, string objects and ownership of the reused allocator-mode alias.
Native Linux compatibility-mode fixtures execute 8,192 original and 8,192
rebuilt calls at image bases `0x00400000` and `0x10400000`. The independent
model covers all three exits, unsigned size edges, null allocation/mapping,
failed handle closure, initial and callback-induced active-state changes,
different message scripts, idle tick/wait decisions, and shutdown replacing
or clearing the mapping before final cleanup. It compares the entire callback
context, captured 28-byte message, both bases' state/IAT/text mappings, and the
allocation pool, including untouched bytes and inactive-base poison.

Five fault controls are rejected: bypassing the minimum-size clamp, omitting
the first string copy, substituting error 184 for 183, omitting the active peek
flag, and failing to rebase the final allocator-mode store. The verifier's
normal mode additionally checks the linked partial PE and source/object input
hashes. APIs, malloc, application initialization, shutdown and tick are explicit
modeled boundaries; this proof does not recover their behavior or the complete
executable.

Integration uses `config/game-winmain-functions.json` with compiler profile
`default`, its three `defined_data_symbols` as shared data rows using
`src/game_winmain.cpp`, and its three `linked_symbols` as jump-bank aliases.
The allocator-mode alias is owned only by
`config/game-heap-delete-functions.json`; WinMain lists it under
`reused_data_symbols` and must not duplicate the unresolved declaration.
