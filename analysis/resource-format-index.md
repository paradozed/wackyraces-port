# Recovered format index

This index separates layouts established from bytes or executable behavior
from payloads that remain only inventoried. Primary files were the installed
game directory and the mounted retail ISO at `/run/media/nicolas/Wacky`.
Executable-specific claims refer to `wpc.exe` SHA-256
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`.

## WGM files on the installed disc

The four installed archives and 33 WGM archives on the mounted ISO were
checked: 37 files total, and the native `--inspect-wgm` command succeeds on all
37. All have `WPCG`, version `0x02020000`, nine
length-prefixed top-level records in the order
`GNRL, WDIM, RSPR, CLAS, SBNK, RSRC, BRTR, SCRT, WMOD`, and framing that ends
exactly at EOF. Every record boundary is four-byte aligned. All 37 `RSRC`
payloads parse as repeated `tag[4] | length:u32le | payload` records with no
truncation or trailing bytes. This confirms framing across the WGM files on
this disc; it does not establish compatibility with another game release.

Aggregate `RSRC` inventory:

| Tag | Count | Recovered layout / evidence |
|---|---:|---|
| `GMDL` | 6,945 | Eight-byte resource prefix, then `MATR`, `MATS`, `VERT`, `TANG`, `TSET` records. The C mesh reader validates position, UV, index, material, and TSET ranges, and assigns each triangle the material selected by its TSET row. The five TSET fields are inferred as material index, vertex start/count, and triangle start/count; all 7,010 GMDL/GHMS resources in the installed and mounted WGM corpus satisfy those bounds, material-index, and full triangle-coverage checks. Five ground meshes are empty placeholders with zero `MATS`, `VERT`, `TANG`, and `TSET` counts, which the group loader skips. Five meshes in `dt_t4.WGM` contain the exact `0xcdcdcdcd` uninitialized-heap pattern in UVUV; the decoder recognizes this sentinel family and derives planar UVs from the two largest vertex axes as a fallback inference. |
| `GHMS` | 65 | Executable registers this tag as `GDHMesh` (class token `0x0131079b`). Child loader reads two HMTS header u32s and then 8-byte rows; SEGS reads a count and 36-byte rows. The native inspector validates each framing independently; installed general meshes show that the HMTS and SEGS row counts are not required to match. HMTS row fields remain unnamed. |
| `GMAT` | 5,532 | Thirteen nested records: `TEXP`, `REFL`, `BUMP`, `COLS`, `SHIN`, `SILL`, `TRAN`, `CHRO`, `SMOO`, `TWOS`, `CLAM`, `COLT`, `TXAN`. The texture link is identified; most material properties remain
uninterpreted and the C port does not yet parse them. |
| `GTEX` | 3,844 | `TGAF` and `PALS`; nine track textures additionally have `TTXA`. Their observed payload is a 20-byte header, a bounded u32 frame-index table, full indexed image bytes, and a bounded u32 sequence table. The native race loader validates and decodes the nine observed 8-bit indexed cases. Header word three and the original playback cadence remain unknown; see `texture-animation-ttxa-20261023.md`. TGAF variants are validated/decoded only for documented subsets. |
| `FILM` / `FILQ` | 32 / 16 | Resource ID, count, and typed resource links; see `film-resources.md`. |
| `REEL` / `REEQ` / `FRMQ` | 38 / 173 / 6,116 | Placement rows or typed links; menu film placements are documented, while world animation semantics remain open. |
| `GSMP` | 282 | Four-byte sound reference; group/ID lookup is recovered. |
| `MTNL` | 47 | Registered as `GDMotionLib`; loader reads five u32 scalars (first and fifth remain opaque), then count-driven arrays: rows with five serialized u32s (24-byte destination stride), a u32 array, and rows with one u32 plus a 12-byte vector (16-byte destination stride). All 47 resources match the exact serialized span with no trailing bytes. Channel c drives segment c/3 (translation, rotation in degrees, scale − 1): [`mtnl-motion-20261028.md`](mtnl-motion-20261028.md). |
| `WGNR` | 80 | Registered as “Wacky Ground”; serialized form is resource ID, class token `0x0130e39a`, blob length, and exactly that many bytes. The native inspector and race-world loader validate relocation tables, linked rows, 28-byte records, spatial-grid cells, polygon-link chains, polygon spans, vertex/plane arrays, and referenced indices without applying PE32 pointers. Static consumers use the class for spatial collision/ground queries. World-space transforms and native collision response remain unresolved; see `disassembly/wpc-wgnr-collision.md`. |
| `GTXA` | 9 | Registered as `GDTextureAnimRes`; payload is one u32. Runtime callers pass the decoded field to the resource lookup path, establishing it as a resource link key; the target class is not assigned. The native WGM inspector now enforces the observed four-byte payload. |

The inventory confirms that track and victory WGM files contain data families
absent from the installed menu-only samples. In particular, `GHMS`, `MTNL`, and
`WGNR` are present in disc WGM files. `MTNL`/`WGNR`/`GTXA`, world `BRTR`, and
the semantics of several `GHMS` fields remain unrecovered. Track
loading/rendering cannot be implemented faithfully from the menu formats
alone. The menu `BRTR` tree parser has not been shown to cover world `BRTR`
variants.

See `wgm-container.md`, `film-resources.md`, `tgaf-menu-film.md`, and
`disassembly/wpc-wgm-metadata.txt` for chunk-level evidence and the
implemented and experimental subsets.

For the relationship between these world resources and the unresolved race
flow, see [`race-gameplay-and-worlds.md`](race-gameplay-and-worlds.md).

## Sound bank archives

The installed `Menu.GSW` and eight ISO victory `*.GSW` files all use `SBPC`
plus a counted directory of seven `(language_id, absolute_offset)` entries.
Across these nine archives, 63 language banks were accepted by the bounded
bounded format parser. They contain 1,134 sample rows total. The bank table/body layout,
sample spans, group lookup, language selection, and PCM properties for these
installed banks are described in `sbpc.md`. All rows in these archives use
group `0x80`, 22,050 Hz, and flag 2 (16-bit mono PCM); per-bank sample counts
vary from 5 to 37. No body gaps or overlapping rows were reported. Zero or two
bytes can follow a declared bank body and are retained as unexplained padding.

WGM `SBNK` tables were also parsed from all 37 WGM files: 99 rows in total,
with record counts of 0, 1, 2, 3, or 46 per archive. Row addressing and
`GSMP` references are validated. Nonzero extension fields are absent in the
checked banks, so their use remains unknown.

## Other file families

| Family | Established | Still unknown |
|---|---|---|
| `STRINGS.DAT` | Seven language records, 233 `(string_id, offset)` entries each in the installed file, NUL-terminated strings, executable XOR-`0x7f` transform (preserving raw `0x7f`), registry-selected language ID and menu key joins. | General non-ASCII encoding/glyph mapping; the seven language names are inferred from text. See `wpc-strings.md`. |
| `.pc` | `demo1.pc`–`demo3.pc` have exact size `0x382f9`; the native bounded reader parses the two controller streams, four two-byte RLE input streams, packed per-frame states, float samples, course indices, and trailer counts. The menu Demo action now plays all three installed demos in sequence through the native race loop. `credit.pc` is a u32 count followed by 50-byte records. | Exact vehicle-table to menu-model mapping, other control-bit roles, float-sample semantics, reserved trailer fields, and user `.WSR` selection/playback. Do not conflate `.pc` demos with persistent profile saves: `.WSR` recordings use the same buffer and playback path, while `.WSG` is a separate 126-byte record/profile save. See `pc-formats.md`, `save-format-and-logic.md`, and `disassembly/wpc-save-load.txt`. |
| `.WSG` / `.WSR` | `save/NICO.WSG` is 126 bytes and matches WPC's five-block `.WSG` reader/writer. Its 20-entry array is used by race timing/record logic and is organized as five groups of four resources. `.WSR` reads/writes the fixed `0x382f9`-byte replay buffer; the recorder appends frame state/input events and the WSR loader uses the demo playback state-transfer path. | Meanings of most WSG fields, precise record units, and full replay-buffer field/offset schema remain open. See `save-format-and-logic.md` and `disassembly/wpc-save-load.txt`. |
| `.snd` | Four-byte installed header pattern (u16 marker, u16 rate); executable has IMA ADPCM encode/decode tables and stateful decoder routines. | Whether marker bits describe channels, channel/nibble layout, initial state, exact framing, voice-event mapping, and verified music mixing. The SND channel and framing interpretation remains exploratory. See `snd-observations.md`. |
| `FILM` / font | Menu film and glyph mapping structures are parsed and bounded; byte-to-glyph index mapping is tied to the executable's `-0x20` rule. | Draw timing, complete screen placement, and meaning of all variants. See `film-resources.md` and `menu-text-properties.md`. |
| `TGAF` | Executable validates embedded uncompressed TGA-like indexed 8-bit and truecolor 24/32-bit inputs. The C animation reader handles indexed, two-byte-short indexed, a specifically validated footer-bearing indexed layout, strict bounded 24-bit input, and bounded 32-bit input. No installed 24-bit sample is available to check the C wrapper assumption. | Other payload variants, color-key/threshold meaning, backend compositing, and complete scene texture coverage. See `tgaf-menu-film.md`. |
| AVI / BMP | Installed headers and stream metadata are recorded in `bitmap-avi-observations.md`; AVI playback was observed under Wine with the required Indeo decoder available. | WPC display integration and selection behavior outside the captured startup path. |

## Scope closure

The reusable parsers are bounded around observed framing and reject truncation
or invalid spans. There are no test fixtures or automated tests because the
workspace instructions prohibit adding tests unless requested. The installed
files and mounted ISO serve as analysis evidence, not as asserted support for
all future versions.

Outer-container and per-file resource inspection now succeeds for all 37
installed/disc WGM files. This does not close the opaque fields listed above:
packed WGNR relocation entries, MTNL vector semantics, HMTS row meanings, and
broader TGAF variants remain unresolved.
