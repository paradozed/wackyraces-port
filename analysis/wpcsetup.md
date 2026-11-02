# `wpcsetup.exe` static inventory

Evidence collected from the installed file on 2026-10-21. No game files were
modified.

## PE identity

- SHA-256: `c98baa9cb748896176a3c687ed495d80ad3be91d38368c656e927cd3d4c2a2cf`
- Size: 126,976 bytes; filesystem mtime: 2000-11-09 12:19:50 +0100.
- PE32, Intel i386, Windows GUI; image base `0x00400000`; entry point RVA
  `0x365a` (VA `0x0040365a`); four sections: `.text`, `.rdata`, `.data`,
  `.rsrc`.
- COFF timestamp: 2000-11-09 22:19:49 UTC. Linker version fields are 6.0;
  this is consistent with, but does not by itself prove, a Visual C++ 6-era
  toolchain.
- Resource directory contains eight numeric top-level types, including dialog,
  string, bitmap, menu, and icon/cursor style resources. Resource language
  entries observed by `objdump -x` are `0x0409` (English - United States).
  No PE version-information resource was listed.

## Imports and strings

Imported DLLs: `DDRAW.dll`, `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`,
`WINSPOOL.DRV`, `ADVAPI32.dll`, and `COMCTL32.dll`. Notable imported APIs
include DirectDraw create/enumerate, registry read/write, Win32 dialog/window
and GDI functions, printer configuration, and resource loading. This points to
a Windows GUI configuration/setup program with DirectDraw-related probing;
imports alone do not establish the complete UI flow.

Notable embedded strings:

- `DirectX 7 Not Installed. Please Install.` at VA `0x004190c0`, referenced
  from function `0x004016c0`; the failure path shows an `ERROR` message box.
- `Software\\InfoGrames\\Wacky Races\\1.00.000\\` at VA `0x004190fc`,
  referenced by function `0x00401700`, which calls registry open/create APIs.
  The spelling `InfoGrames` is present in the executable.
- `english` at VA `0x0041a3b8`.
- `LANG` at VA `0x00419450`; setup code around `0x00401e60` reads this DWORD
  from the product registry key to choose setup UI labels. It is a separate
  value from the binary `Prefs` and `MenuPrefs` blobs.

Radare2 6.2.2 `aaa` recognized entry point `0x0040365a`, `main` at
`0x0040b167`, and hundreds of functions. The two functions above and their
references were disassembled. There is no Ghidra headless executable on PATH
in this environment, so this target was inspected with GNU `objdump` and
radare2 rather than added to the existing Ghidra project. No function names
beyond tool-generated labels are recovered.

## Mounted source-disc identification

The user-mounted source disc is available at `/run/media/nicolas/Wacky/`.
Its `Setup.ini` identifies the installer product as `Wacky Races` and gives
this language configuration:

```ini
[Languages]
Default=0x0009
count=4
key0=0x0009
key1=0x040c
key2=0x0416
key3=0x000a
```

The 191-byte `Setup.ini` is dated 2000-11-16 and has SHA-256
`0a11b81b89927636247c72691df41b8fa679b20f427cf10babd85d160d83a121`.

Interpreting these as Windows LANGIDs gives English-neutral as the installer
default, with French-France, Portuguese-Brazil, and Spanish-neutral also
offered. This is installer metadata; it does not prove how the game stores its
own `LANG` value. The installed Wine prefix has `LANG=3`, whose French meaning
is inferred from the selected string table's contents. The game's seven
`STRINGS.DAT` table indexes therefore cannot yet be mapped one-to-one to the
four installer IDs.

The disc's four-byte `WID.ID` contains ASCII `WCKY`. Disc `wpc.exe` is 1,486,848
bytes and has SHA-256
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`, exactly
matching the installed executable and the hash in `analysis/manifest.tsv`.
Disc `Setup.ini` is dated 2000-11-16; its `readme.txt` carries a 2000
Infogrames copyright. These identify the mounted disc contents and their
timestamps, but do not establish a retail-region name, revision number, or
whether this is the only published edition. The setup program's registry path
component `1.00.000` remains a version-like string, not proof of a release
version.
