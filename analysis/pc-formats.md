# `.pc` file format observations

Findings below are from the installed files and static disassembly of
`wpc.exe`. The `.pc` extension covers at least two different observed layouts;
the demo data is not the same structure as the credits table.

## Demo files

Installed `demo1.pc`, `demo2.pc`, and `demo3.pc` are each 230,137 bytes
(`0x382f9`). Their SHA-256 hashes are:

| File | SHA-256 |
| --- | --- |
| `demo1.pc` | `5dec45e2a0305eb03f9852dc1813079fbdfd32426ddff774a58fd440b2e2a07f` |
| `demo2.pc` | `eb23f56a50f29a8a96a23d2548f986e34dc1a3a448713c4396144f47797a7d7b` |
| `demo3.pc` | `f0e72fbdba4b45b9e19aed8e02e9926bfb93850cf21808c22209f8980d78d7b2` |

The first 0x50 bytes of each file contain many common little-endian values,
though the first dwords differ. The files have no obvious text signature.
Static disassembly at `0x0045afb0` opens a path argument in `rb` mode and calls
the CRT read function with element size `1` and count `0x382f9`, storing into
the global buffer at `0x006abee0`. This exactly matches each installed demo
file's length. Code then cycles through the first three filename table entries
and calls `0x0040613b`, which copies the first four buffer dwords into object
fields at `+0x1a60`–`+0x1a6c` and initializes additional game state. A second
path reloads the next demo when an object counter passes a duration-like field.
This links these files to repeated demo-state loading, but not to a recovered
input, scene, or record schema. Full call-site evidence is in
`analysis/disassembly/wpc-demo-playback.txt`.

The filename pointer table at `0x0052beac` contains `demo1.pc` through
`demo4.pc`. Code around `0x0048a543` and `0x0048fcc7` indexes this table using
a counter, passes the selected filename to `0x004028f6` (a thunk to the fixed
size reader at `0x0045afb0`), and wraps the counter after index 2. The
references demonstrate a three-entry selection cycle in those call paths.
`demo4.pc` is referenced by the table but was not present in the installed
directory inspected here. This does not prove no other path can load it.

## Recovered replay-buffer regions

Static data movement in `FUN_0045b310`, `FUN_0045b530`, `FUN_0045aff0`,
`FUN_0045ba30`, and `FUN_0045bce0` now establishes the main fixed-buffer
regions. Offsets below are relative to the `0x006abee0` buffer base:

| Offset | Capacity | Observed use |
|---:|---:|---|
| `0x00` | 16 bytes | Four session values copied to/from application offsets `+0x1a60`–`+0x1a6c` |
| `0x10` | 4 bytes | First course/record-table index |
| `0x14` | 4 bytes | Second course/record-table index |
| `0x18` | 1 byte | Session flag/value; meaning unresolved |
| `0x19` | 48 bytes | Two interleaved controller stream descriptors; exact field meanings unresolved |
| `0x49`, `0x275f` | `0x2716` bytes each | Two controller streams; recorder emits 3-byte run/state entries |
| `0x4e75` | Four `0x2716`-byte regions | Four input channels; recorder emits 2-byte run/delta entries |
| `0xeacd` | 10,000 bytes | Two-bit per-frame state array, maximum 40,000 frames |
| `0x111e3` | 160,000 bytes | 40,000 per-frame float samples |
| `0x382e3` | 8 bytes | Four u16 input-channel stream lengths |
| `0x382eb` | 8 bytes | Unresolved trailer fields/reserved data |
| `0x382f3` | 4 bytes | Two u16 controller-stream lengths |
| `0x382f7` | 2 bytes | u16 frame count; ends the file |

The native inspector was run read-only on the three installed demo files and
reported:

| File | Track indices | Session values | Flag | Frames | Controller bytes | Channel bytes |
|---|---:|---|---:|---:|---:|---:|
| `demo1.pc` | `(0,0)` | `0,0,4,0` | 1 | 2767 | `1038,0` | `22,0,22,0` |
| `demo2.pc` | `(1,0)` | `0,0,1,0` | 0 | 2824 | `894,0` | `24,0,24,0` |
| `demo3.pc` | `(2,2)` | `0,0,6,0` | 0 | 1760 | `630,0` | `14,0,14,0` |

The executable indexes the five-by-four track filename table at
`0x0052bef0` as `variant + environment * 4`. Reading its 20 pointer targets
recovers rows `cc`, `sh`, `dt`, `ct`, and `dr`, each with variants 1 through 4.
Thus the demo pairs `(0,0)`, `(1,0)`, and `(2,2)` select `cc_t1`, `sh_t1`, and
`dt_t3`. The native replay launch maps all 20 table entries to their installed
track archives. See `analysis/disassembly/wpc-track-resource-table.md` for the
pointer values and filenames.

Static routine `FUN_00477ea0` selects a vehicle pointer from the table at
`0x006f2ce0` using session field 2 for the first participant, or field 3 for
the alternate participant. The demo values 4, 1, and 6 therefore identify
vehicle-table indices; their localized vehicle names have not been joined
here. The native replay launch maps these values to the seven menu models using
the current installed-asset association, which remains an inference.

The channel streams end exactly where the packed state array begins. The
sample array ends exactly at the four-channel length table. The controller
stream lengths are multiples of three; channel stream lengths are multiples
of two, and the sum of their RLE durations matches the frame count for every
active stream in all three installed demos. This establishes array framing and
bounds, but not the meaning of every event code, the four float-sample meanings,
or the eight middle trailer bytes.

The native C reader in `c-port/replay.c` validates the exact fixed size, reads
the counts and session indices, bounds each event stream and frame array, and
exposes accessors for the two-bit frame state, float sample, controller
run/state triples, and channel run/delta pairs. Its playback cursor advances
the streams using WPC's run counts, state mask, channel accumulation, and
packed-state `+2` conversion. `--inspect-replay` decodes the complete event
timeline and reports the number of input frames consumed. The main-menu Demo
action now cycles repeatedly through all three installed demos in the native
race loop, matching the observed three-entry filename-table wrap. In the native
port, Escape returns to the menu; the original demo-exit key behavior remains
unverified. The observed track mappings are `(0,0) -> cc_t1`, `(1,0) -> sh_t1`, and
`(2,2) -> dt_t3`; session field 2 contains vehicle-table entries 4, 1, and 6.
Mapping those table entries to menu model IDs remains inferred. First-controller
state bits `0x0040` and `0x0080` feed speed increase/decrease, and
`0x8000`/`0x2000` feed opposite steering directions, matching the default
masks in `wpc.exe`. The controller-state masks across all three demos include
the expected controls plus bits `0x0001`/`0x0002`, whose roles remain unknown.
User `.WSR` selection/playback remains unconnected; float-sample meanings,
other event-code meanings, and some trailer fields are still unresolved.

## Credits file

`credit.pc` is 15,054 bytes (`0x3ace`). Its first little-endian u32 is `0x12d`
(301). In `wpc.exe` routine `0x0047f840`, the file is opened as `rb`, the first
four bytes are read into a count, and the code computes count times 50 before
reading records with element size 50 and element count from the header. The
remaining size is exactly `301 * 50 = 15,050` bytes. This supports a layout of
a four-byte record count followed by 301 fixed-width 50-byte records.

Many records contain NUL-terminated ASCII-compatible credit strings, while
other records appear zero-filled or contain non-text data. Text includes
section headings, names, and voice cast credits. The bytes `[` and `k` appear
in places where accented characters may be expected, but the encoding or
glyph substitutions have not been established. Record field meanings,
rendering behavior, and the significance of non-text records are unresolved.

Inspection of all 301 records shows a consistent observed row convention: byte
0 is 0 in 224 records, 1 in 59, and 2 in 18; bytes 1–49 hold NUL-terminated
text and zero padding. Marker 0 occurs with names and ordinary credit lines,
marker 1 with section headings such as `GAME CONCEPT` and `TRACK DESIGN`, and
marker 2 with title/credits headings such as `CREDITS FOR WACKY RACES` and
`APPALOOSA CREDITS`. These role associations are inferred from content; the
renderer that consumes the marker has not been traced. The native C port now
provides `--inspect-credits FILE`, which validates the exact count/length
relationship, marker range, and NUL-terminated text field. It preserves
non-ASCII bytes as hex escapes; the credits renderer and character encoding
remain unresolved.

## Persistent WPC saves

The `.pc` demo and credits structures are distinct from the persistent save
files. A user-provided `save/NICO.WSG` matches WPC's 126-byte `.WSG` writer and
reader layout. The separate `.WSR` path reads/writes a fixed `0x382f9`-byte
race-recording buffer; its loader passes the buffer to the same state-transfer
and playback routines used by `.pc` demos. Both use the same fixed-size global
buffer. Per-frame state and compressed input-event stream fields are partly
recovered, but the complete byte-offset schema is not. See
[`save-format-and-logic.md`](save-format-and-logic.md) and
[`disassembly/wpc-save-load.txt`](disassembly/wpc-save-load.txt).
