# WPC save files and `NICO.WSG`

This note compares the user's saved file with the installed `wpc.exe` and its
available Ghidra pseudocode. The file was inspected read-only; no bytes were
changed.

## Artifact identity

| Property | Observation |
|---|---|
| Path | `save/NICO.WSG` |
| Size | 126 bytes (`0x7e`) |
| SHA-256 | `e685b8a880698062ad93a30e76f30b1354bfb088830e76c07a994d570bf1a570` |
| Executable compared | `wpc.exe`, SHA-256 `e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b` |

The user reports creating this save from the menu under the name `NICO`. Its
name and location match the executable's `save\\` path builder and `.WSG`
extension.

## `.WSG` serialization observed in `wpc.exe`

Ghidra pseudocode shows a symmetric, fixed-layout reader and writer:

- `FUN_004b7b40` builds `save\\<name>.WSG`, opens it, and writes five
  consecutive blocks with `FUN_004f1680` (the CRT `fwrite` implementation):
  20, 5, 1, 80, and 20 bytes. It then closes the file.
- `FUN_004b7cd0` opens the same `.WSG` path and reads those same five blocks
  into the corresponding global buffers with `FUN_004ee150` (`fread`).
- `FUN_004b78e0` selects `.WSG` or `.WSR` while enumerating a save directory;
  the enumerator strips the extension and keeps up to eight names in 32-byte
  entries. The two file types therefore have separate slot lists.
- `FUN_004b7ca0` and `FUN_004b7e30` are index-based wrappers around the WSG
  writer and reader routines.
- `FUN_004b7690` enumerates matching names under `save\\`, creates the
  directory if absent, and caps the displayed filename list at eight entries.
  This agrees with the eight `LIBRE` slots the user saw before saving, though
  the UI-to-helper connection is inferred from the call paths.

## Slot listing and save-name handling

The file selector handler (`FUN_00495df0` / `FUN_00495f30`) routes its WSG
listing for the WSG load/save choices and its WSR listing for the WSR
load/save choices. The enumerator builds a wildcard from `save\\*` and the
selected extension, creates `save\\` if it does not exist, strips the final
four extension characters from each returned filename, stores at most eight
names, then sorts the entries with a bytewise lexicographic comparator. The
selector wraps across indices 0–7. Empty entries render as `LIBRE` in the save
screen; the user's reported eight-slot screen agrees with this code.

For an empty save slot, the name editor starts with a cleared 32-byte entry.
When editing an occupied slot, the current basename is copied into the editor
and the handler first displays a two-choice confirmation state. The text
input handler accepts a character while its cursor index is at most 30, so a
name can contain up to 31 input bytes plus the terminating NUL. Backspace
removes a byte. Before writing, the handler compares all 32 bytes with the
other listed names (excluding the selected slot) and rejects a duplicate.
When replacing an occupied entry, it deletes the old file with the selected
extension before writing the entry under its edited basename. The save UI
therefore keeps WSG and WSR names in separate lists and applies duplicate-name
checks within the selected file type.

The writer opens in `wb` mode and the reader in `rb` mode. Open failure exits
the helper without a visible error result in these routines; the `fwrite` and
`fread` element counts are not checked here. The routines do not show a
temporary-file/rename step, checksum, or recovery mechanism.

This ordering makes replacement appear non-atomic in the static call path:
the old file is deleted before the new writer attempts to open its destination.
If that open fails, the writer returns and the file-selection handler does not
receive a success/failure result. This is a code-path inference; an actual
failed-save UI outcome has not been observed.

The five writes total 126 bytes. `NICO.WSG` is exactly 126 bytes, and its
contents split cleanly at those boundaries. This is strong evidence that this
file is the output of the `.WSG` writer. The writer uses raw blocks: no file
header, version, checksum, or field-by-field conversion is visible in these
routines. The read counts returned by `fread` are not checked here, so this
path does not visibly reject a short/truncated file before continuing.

## `NICO.WSG` field map

All multibyte integers below are little-endian, as on the Windows target.

| File offset | Size | `wpc.exe` destination/source | Observed contents | Interpretation |
|---:|---:|---|---|---|
| `0x00` | `0x14` (20) | `DAT_006f4654` | Five u32 values: `1, 1, 1, 1, 0` | Meaning not recovered. |
| `0x14` | `0x05` (5) | `DAT_006f4668` | `01 01 00 00 00` | Meaning not recovered. |
| `0x19` | `0x01` (1) | `DAT_006f466d` | `00` | Meaning not recovered. |
| `0x1a` | `0x50` (80) | `DAT_006f467c` | Twenty little-endian u32 records, listed below | A course-indexed timing/record table; packed field semantics are partly recovered. |
| `0x6a` | `0x14` (20) | `DAT_006f46cc` | Twenty zero bytes | Per-record bytes are read elsewhere; exact meaning is unknown. |

The 20 values at `0x1a` form a five-by-four table. The game checks
`DAT_006f41d8 < 5` and `DAT_006f41dc < 4`, and forms the record index as
`first_index * 4 + second_index`. The same indices select a WGM resource from
a 20-entry pointer table. Its five four-resource blocks are ordered by these
filename keys:

| First index | WGM resources for second indices 0–3 |
|---:|---|
| 0 | `cc\cc_t1.wgm` … `cc\cc_t4.wgm` |
| 1 | `sh\sh_t1.wgm` … `sh\sh_t4.wgm` |
| 2 | `dt\dt_t1.wgm` … `dt\dt_t4.wgm` |
| 3 | `ct\ct_t1.wgm` … `ct\ct_t4.wgm` |
| 4 | `dr\dr_t1.wgm` … `dr\dr_t4.wgm` |

This establishes the table shape and its resource-key order. The meanings of
the two indices in the championship flow, and their mapping to the French
championship/course labels observed in play, have not been recovered.

The 20 values at `0x1a` are:

```text
6000a410 6000dea8 6000dea8 600080e8
6000afc8 6000a9ec 600086c4 6000cd14
600086c4 60006f54 600057e4 6000b5a4
6000afc8 6000e484 6000cd14 6000c738
6000c738 6000f03c 6000639c 6000cd14
```

Race setup reads the selected record at
`DAT_006f41d8 * 4 + DAT_006f41dc`. Another routine iterates four consecutive
records for the current first index when checking/displaying record state.

At race-state setup (`FUN_004a2b50`, branch in the generated pseudocode), WPC
splits the selected u32 into its low 28 bits and high four bits. At a later
race event, it compares the existing low-28-bit value with a newly sampled
timer value. If the existing value is no better, it leaves the entry alone;
otherwise it stores the sampled value in the low 28 bits and a value read from
the active `WCtrlRoboCar` object at `+0x88` in the high four bits. The same
field indexes WPC's per-vehicle dynamics rows, and `FUN_004a8630` copies it from
the selected vehicle descriptor's `+0x2c` field. This identifies the nibble as
the vehicle table ID, rather than a finishing place. The time formatter
`FUN_00450e00` divides the timer by 300 to produce seconds, establishing 300
timer units per second. The lower-is-better comparison applies to the packed
record for the selected environment/track pair.

The native race loop now returns its completion flag, elapsed simulation time,
and selected vehicle ID to the menu. On a completed non-replay race, the menu
converts elapsed milliseconds to the recovered 300 Hz timer units and updates
the corresponding record in the active in-memory profile only when it is
faster. The 20-entry family order is `cc`, `sh`, `dt`, `ct`, `dr`, matching the
executable's WGM table; track variants map directly to indices 0–3. Marker
bytes and the opaque 26-byte prefix remain untouched. The record is written to
disk only when the user invokes the native save action. The native race loop
does not yet reproduce WPC's countdown or exact timer start point, so recorded
times are on the recovered scale but are not yet frame-matched to WPC.

For the first three serialized blocks (`DAT_006f4654`, `DAT_006f4668`, and
`DAT_006f466d`), the available generated pseudocode shows reads and writes in
the save/load routines but no direct non-I/O references. Their values cannot
yet be tied to unlock flags, championship points, or other gameplay state from
this static pass, so those meanings remain open.

### Fresh-profile initialization from Wine runtime

An xref search across the generated WPC decompilation finds the five profile
buffers passed to the `.WSG` reader and writer. The 20-record table has
additional gameplay reads and time-record updates, and the final marker table
has reads in record comparison code. The first 26 bytes have no direct
non-I/O references in this decompilation. No generated code initializes these
buffers to the values of `NICO.WSG` or to another known profile before a save.
The static pass therefore did not establish valid contents for a new,
unloaded profile. The isolated Wine runtime save described in
[`wine-fresh-profile-save-20261023.md`](wine-fresh-profile-save-20261023.md)
now supplies the exact 126-byte fresh-profile state. It matches the installed
`NICO.WSG` in all but byte `0x15`; the reason for that changed byte is still
unknown. The native port uses the observed fresh bytes and does not infer the
meaning of the opaque fields.

`DAT_006f46cc` is indexed alongside the same records. `FUN_0048b640` tests
those bytes (including a comparison with `0x7f`) while comparing the packed
low-28-bit values against a second table. This proves the final 20 bytes are
not just unused padding, but does not establish their exact UI or progression
meaning. They are all zero in `NICO.WSG`.

## `.WSR` replay recordings and demo `.pc` files

The same code cluster handles `.WSR` files separately:

- `FUN_004b7920` writes exactly `0x382f9` (230,137) bytes from the global
  buffer at `DAT_006abee0` to `save\\<name>.WSR`.
- `FUN_004b7a30` reads the same fixed-size block back into that buffer.
- `FUN_004b8030` deletes a selected `.WSR`; `FUN_004b7f60` deletes a selected
  `.WSG`.
- The file-selection handler has distinct WSG and WSR branches. Loading WSG
  reads the 126-byte profile/record structure above. Loading WSR reads the
  large buffer and calls the same `FUN_0045aff0` state-restore routine used
  by demo playback, then starts the corresponding track. Saving through that
  handler writes the same buffer to WSR.

The replay role is supported by the buffer's other code paths. Its fixed
address range is `[0x006abee0, 0x006e41d9)`, exactly `0x382f9` bytes. The demo
reader `FUN_0045afb0` reads the same number of bytes into the same base, and
both WSR and demo loading pass it through `FUN_0045aff0`. Runtime recording
code fills fields and arrays in this range: `FUN_0045b530`, called during the
active input/update path, samples a per-frame value, packs a two-bit state
value per frame, and accumulates run-length encoded input deltas for four
channels. `FUN_0045b310` flushes those channel streams and copies session
metadata into the buffer. Playback routines `FUN_0045ba30` and
`FUN_0045bce0` consume the stored channel events and per-frame state; when the
recorded data ends, the playback state is reset. This makes WSR a saved race
recording/replay, rather than a second career/profile save.

The beginning of the WSR/demo buffer can be mapped from the load and recording
code:

| Buffer offset | Size | Source/use observed | Meaning |
|---:|---:|---|---|
| `0x00` | 16 | Four u32 values copied to/from application-object offsets `+0x1a60` through `+0x1a6c` | Application/session fields; individual meanings unknown |
| `0x10` | 4 | `DAT_006abef0` / `DAT_006f41d8` | First WGM/record-table index |
| `0x14` | 4 | `DAT_006abef4` / `DAT_006f41dc` | Second WGM/record-table index |
| `0x18` | 1 | `DAT_006abef8` / `DAT_02a1cc74` | Flag/value; semantics unknown |
| `0x19` onward | Remaining buffer | Controller metadata, counters, per-frame values, packed states, and event streams | Partly recovered; full offsets and meanings remain open |

The writer dumps this buffer as one raw fixed-length block. No WSR-specific
header or version field is visible in the save/load path.

The WSR payload and installed `.pc` demos therefore use the same fixed-size
buffer and state-restore/playback machinery. Their byte layout is compatible
at the loader level: both are raw fixed-span reads into the same buffer, with
no extra header visible in these paths. No WSR sample was available to compare
payloads or determine whether user recordings and shipped demos differ in
optional metadata/content. The user's `NICO.WSG` remains the separate
126-byte profile/record save.

## What the two save captures establish

- The reported menu save name `NICO` appears as `save/NICO.WSG` with the
  extension expected by WPC's `.WSG` path builder.
- Its 126-byte length and all five block boundaries match WPC's exact
  `.WSG` write/read sequence.
- A fresh-profile Wine capture from an empty save directory produced the same
  20 packed records and all but one byte of the installed `NICO.WSG`; see
  `wine-fresh-profile-save-20261023.md`.
- Its 20-entry packed record table is used by race logic, including a
  lower-is-better timer comparison and a high-nibble metadata field.
- Several other fields remain opaque. The file does not visibly contain the
  text `NICO`, a magic signature, a version, or a checksum.
- The runtime capture establishes a valid fresh profile and the native menu's
  save path, but no native write/read round trip has been performed. The
  changed byte's meaning and gameplay-driven record updates remain unknown.

## Native C implementation status

`c-port/savegame.c` implements bounded reading, fresh-profile initialization,
and fixed-layout writing. The writer preserves the opaque 26-byte prefix and
20 marker bytes, encodes the 20 packed records as little-endian u32 values,
writes exactly 126 bytes through a synced same-directory temporary file and
atomic rename, and reports I/O errors. The native menu now lists the eight
slots, accepts a bounded ASCII name, checks duplicates, confirms replacement,
and writes the current session profile. This follows the executable's observed
20/5/1/80/20-byte raw block order without inferring unknown field meanings.
Native write/read round-trip verification remains open under the workspace
instruction not to add or run tests.
