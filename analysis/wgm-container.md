# WGM container observations

This note records the outer WGM layout and top-level chunk inventory in the
four installed `.WGM` files. Counts and lengths were read from the files on
2026-10-22. The names and meanings of opaque payloads are not inferred from
their tags alone.

## Outer framing

Each file begins with the four ASCII bytes `WPCG`, then a little-endian u32
version (`0x02020000` in all four files). The rest of the file is a sequence of
records:

```text
tag[4] | payload_length:u32le | payload[payload_length]
```

All nine records in each installed file are read to the exact end of the file
using this framing; no trailing padding is needed to reach the next record.
Every observed record start is four-byte aligned. The C inspector checks the
header and bounded chunk lengths, rejects incomplete headers and payloads
extending past EOF, and requires the sequence to consume the full file. These
observations do not establish how other WPCG versions handle alignment or
compression.

## Top-level inventory

Every installed file has the same record order:
`GNRL`, `WDIM`, `RSPR`, `CLAS`, `SBNK`, `RSRC`, `BRTR`, `SCRT`, `WMOD`.
The table gives payload sizes in bytes.

| Chunk | `menu.WGM` | `SWMenu.WGM` | `general.wgm` | `swgeneral.wgm` | Evidence status |
| --- | ---: | ---: | ---: | ---: | --- |
| `GNRL` | 8 | 8 | 8 | 8 | Fixed size; fields not fully interpreted |
| `WDIM` | 40 | 40 | 40 | 40 | Fixed size; dimensional fields not fully interpreted |
| `RSPR` | 56 | 56 | 56 | 56 | Fixed size; embeds `SPRA`-shaped records |
| `CLAS` | 26,416 | 26,416 | 26,416 | 26,416 | Class-name table used by the scene inspector |
| `SBNK` | 5,864 | 5,864 | 2,082,340 | 2,082,340 | Indexed sample table and body; several field meanings remain unknown |
| `RSRC` | 7,813,256 | 7,727,656 | 3,860,464 | 3,699,956 | Fully traversed as nested tag/length records |
| `BRTR` | 377,424 | 367,348 | 146,268 | 146,268 | Scene tree; menu variants differ |
| `SCRT` | 1,060 | 1,060 | 348 | 348 | Fixed per menu/general pair; contents not fully interpreted |
| `WMOD` | 52 | 52 | 52 | 52 | Fixed size; contents not fully interpreted |

Within each menu/general pair, `GNRL`, `WDIM`, `RSPR`, `CLAS`, `SCRT`, and
`WMOD` payloads are byte-identical. `SBNK`, `RSRC`, and `BRTR` differ. This is
direct byte comparison of the installed files; it does not prove what causes
the variants or which variant the game selects in each runtime mode.

## Fixed metadata chunks: additional byte observations

The menu and general variants were compared field by field. `RSPR` consists of
two 28-byte `SPRA` records. Each record has a `0x14` length, the same
pointer-shaped dword `0x0130e2d8`, then a four-byte tag and three dwords:
`PSXG, 0, 320, 240` and `GLID, 0, 640, 480`. The tags and dimensions strongly
suggest renderer-specific output sizes (320x240 and 640x480), but the zero
field and pointer-shaped value are not explained by the bytes alone. These
values are identical in all four archives.

`WDIM` is 40 bytes in all files. After a pointer-shaped dword `0x0130e010`,
the next six little-endian f32 values decode to three `-500000.0` values and
three `1000000.0` values; the final three dwords are `1`. This resembles a
three-dimensional minimum/maximum extent plus flags, but the precise field
names and use are unverified.

`GNRL` is eight bytes: its first dword is `0x0130e02b`; the second is
`0x416` for both menu archives and `0x410` for both general archives. The
executable's chunk reader compares the first value against `0x0130e02b` and
uses the error text `Invalid general chunk version.` on mismatch, so it is a
validated format/version token (its odd numeric value is not a file pointer
in this context). The second dword is consumed, but its role is unknown.
`WMOD` is 52 bytes and
is identical in all four archives; it begins with `0x01312ea1`, repeats the
same six f32 extent values and three `1` dwords as `WDIM`, then ends with
three zero dwords. The relationship between `WDIM` and `WMOD` is not
established.

This fixed-size observation applies to the four installed menu/general WGM
archives only. The mounted CD has 30 course/battle WGM files whose `WMOD`
payloads are variable length (2,380–8,100 bytes), plus eight 52-byte victory
archives. In each variable course payload, the first 40 bytes match the shared
version/extent/flag prefix, followed by a u32 value of 3 or 4. For
`cc_t1.WGM`, that value is 3 and the following bytes begin with the
zero-terminated text `Snake`, padded with `0xcd` bytes. The variable tail is
not yet parsed by the native WGM inspector.

### WMOD reader evidence

Static decompilation identifies `FUN_0049b3b0` as a reader for this serialized
shape: after a minimum-version check it reads two three-float vectors, three
integer values, and a record count. Each record begins with a 32-byte read
(`FUN_00429b60(stream, record, 0x20)`), followed by an integer, two
versioned/virtual reads, another integer, and a nested count. The reader
allocates `nested_count * 0xcf0` bytes and processes each nested row with
additional field reads and version gates. This agrees with the mounted files'
common 40-byte prefix, count at offset 40, and first padded name at offset 44.
The first names include `Snake` (`cc_t1`), `Tripple` (`cc_b`), and `whoop`
(`cc_t2`). The nested row semantics, serialized row boundaries, and
relationship to `WModManager::m_nStartWait` remain unresolved. This is static
executable evidence plus direct file-byte observations; no `WMOD` runtime
behavior is claimed.

`SCRT` begins with bytes `33 40 03 01` followed by dword `3`. Walking the
remaining bytes as bounded `tag[4] | length:u32le | payload` records consumes
the chunk exactly. Menu archives contain `PRPS` (length `0x14c`), then two
`CHBR` records (each length `0x15c`); general archives contain only the
`PRPS` record. The PRPS payload contains repeated `PROP` records, including
two `0x34`-byte payloads; the child payloads contain property records as well.
The code at `0x0041bf80` dispatches `SCRT` to the parser at `0x0041fe80`
(thunk `0x00401eec`), whose code recognizes `PRIM`, `ACTN`, and `CHBR` while
walking nested records. The same parser is also used for the outer `BRTR`
chunk. Its BRTR path creates class objects and reads their `PRPS` records
through member descriptors; details and the main-menu text-property
association are in `analysis/wpc-brtr-property-loading.md`. These observations
are limited to the installed version and do not justify a general-purpose
parser yet.

The checked corpus also contains two observed legacy headers: `CLAS` version
`0x740` has 116 class names and pairs with a `s@` BRTR marker in seven victory
archives; version `0x74c` has 117 names and pairs with the `tP` marker in the
other files. Both retain the same eight-byte BRTR header and bounded child
record framing. The native parser accepts only these observed class-table and
BRTR marker pairs.

## SBNK sample index

The four installed `SBNK` payloads use the same bank layout as SBPC:
8 bytes containing table/body lengths, a 16-byte table header, then fixed
28-byte sample rows. The header contains group at payload `+8`, a next-link
word at `+12`, sample count at `+16`, and a body-pointer word at `+20`.
Rows start at `+24`. The declared table size is `16 + count * 28`, excluding
the initial two length words; the body therefore starts at `8 + table_size`.
All installed WGM groups are 0 and the stored pointer words are zero.

Rows contain ID, relative offset, byte length, and four raw audio parameter
words. See `analysis/sbpc.md` for the corrected layout and executable evidence.
Earlier notes and inspectors started rows and bodies eight bytes too early.
The corrected table layout has been checked against installed bank sizes,
sample bounds, duplicate IDs, and overlap. The native `--inspect-wgm` reader
now validates this table and cross-checks scene `GDSound`/`WMenuSound` GSMP
references against its group and sample IDs. `--play-wgm-sample` plays an
installed group-0 16-bit mono PCM row. Scene-trigger dispatch and row extension
semantics remain unimplemented.

Static disassembly of the WGM tag dispatcher at `0x0041bf80` compares the
`SBNK` tag and sends it through a virtual resource-manager method at
`0x0041c05a`; `RSRC` follows a different manager call.

| File | Table size | Body length | Rows | Local GSMP references matched | Body gaps / tail | Bytes after body in SBNK |
| --- | ---: | ---: | ---: | ---: | --- | ---: |
| `menu.WGM` | `0x2c` | `0x16b4` | 1 | 1/37 | 0 gaps, `0x76` tail | 0 |
| `SWMenu.WGM` | `0x2c` | `0x16b4` | 1 | 1/37 | 0 gaps, `0x76` tail | 0 |
| `general.wgm` | `0x518` | `0x1fc102` | 46 | 46/46 | 45 gaps, `0x74` tail | 2 |
| `swgeneral.wgm` | `0x518` | `0x1fc102` | 46 | 46/46 | 45 gaps, `0x74` tail | 2 |

General file IDs enumerate `0x1031f`–`0x1034c` and `0x1035b`–`0x10388`.
The menu local IDs are `0x7f7` and `0x803`. The other 36 GSMP references in
each menu have group byte `0x80` and sample IDs matching the language-selected
Menu.GSW bank; only sample `0x6e` from that bank is unreferenced. Executable
lookup `0x0044c640` splits references into high-byte group and low-24-bit ID;
The recorded bank-resolution results use this exact language selection.
The 103 `GDSound` nodes in `general.wgm` each link one local group-0 sample;
their 46 distinct IDs cover all 46 local SBNK rows. The direct parent-class
map is in `analysis/general-gdsound-map.md` and its TSV. These object links do
not establish the runtime trigger or playback behavior.

Across all 94 local rows, raw field `+0x0c` is `6000`, `8000`, `11025`, or
`22050`, field `+0x10` is 2, and the last two fields are zero. These resemble
audio parameters; playback evidence is still needed for their precise meaning.
General sample spans have gaps, so the body must not be treated as a simple
concatenation of clip payloads. The unreferenced body tails and the two bytes
following general-bank bodies remain unexplained.

## Nested resource records

The complete `RSRC` payload in each file parses as repeated
`tag[4] | payload_length:u32le | payload` records with no truncation or trailing
bytes. The observed tag counts are:

| Resource tag | `menu.WGM` | `SWMenu.WGM` | `general.wgm` | `swgeneral.wgm` |
| --- | ---: | ---: | ---: | ---: |
| `GTEX` | 662 | 670 | 275 | 277 |
| `GMAT` | 667 | 673 | 350 | 408 |
| `GMDL` | 32 | 30 | 86 | 86 |
| `FILM` | 6 | 6 | 10 | 10 |
| `FILQ` | 1 | 1 | 0 | 0 |
| `REEL` | 6 | 6 | 13 | 13 |
| `REEQ` | 17 | 17 | 0 | 0 |
| `FRMQ` | 594 | 594 | 0 | 0 |
| `GSMP` | 37 | 37 | 46 | 46 |
| `GHMS` | 7 | 7 | 22 | 22 |
| `MTNL` | 3 | 3 | 17 | 17 |

Format-specific resource findings and bounds checks are documented in
`film-resources.md`, `tgaf-menu-film.md`, and `sbpc.md`. The C port validates
outer WGM framing and menu scene properties, and now decodes the selected
`GDFilm2` queue for the menu animation preview. Model geometry, full-scene
composition, and most other resource tags remain unimplemented or opaque
there.

The executable registers `GHMS` as `GDHMesh` with class token `0x0131079b`.
Its loader reads the first two HMTS u32s as header fields, then reads the mesh's
declared count of 8-byte rows (two u32 values per row). This matches every HMTS
payload in the 65-resource corpus: 8 header bytes plus an integral number of
8-byte rows. `SEGS` stores a u32 row count and 36 bytes per row. All 65 checked
GHMS resources have equal HMTS and SEGS row counts. The native inspector now
checks the class token, nested chunk bounds, HMTS row alignment, SEGS count and
length, and the count match. The row fields and terrain meaning are still
unknown. `MATR` is 52 bytes, and `MATS` is a count followed by 8-byte links.

Static analysis of the resource readers recovered additional schemas: `MTNL` is registered as `GDMotionLib` and consumes
five scalar u32 fields followed by three count-driven arrays. The reader
serializes five words for each 24-byte destination row, then a u32 array, then
one u32 and a 12-byte vector per 16-byte destination row. `FUN_00429910`
confirms that the vector read consumes 12 bytes. All 47 MTNL resources across
the four installed archives and mounted disc match the exact serialized span,
with no trailing bytes. The native inspector now uses 16 bytes per third-array
row. `WGNR` is registered as Wacky Ground and has an
explicit blob length; the executable relocates offset-based pointers inside
that blob, including linked records and 28-byte rows. The native inspector
validates its class token and outer blob length but does not run the pointer
relocation pass. `GTXA` is registered as `GDTextureAnimRes`; its sole u32 is
used as a resource lookup key. These findings recover stream framing and
runtime relationships, not the game-level meanings of every field.

## Cross-check against the mounted ISO

The mounted disc contains 33 additional WGM files under the track and victory
directories. Together with the four installed archives, 37 WGM files were
checked for outer framing and nested `RSRC` record bounds. All 37 use version
`0x02020000`, the same nine-tag order listed above, reach exact EOF, and have
four-byte-aligned record boundaries. Their nested `RSRC` records also consume
each payload exactly. This broadens the observed format boundary to the WGM
files on this disc, but is still one release/version sample.

The aggregate nested resource inventory is:

| Tag | Records | Notes |
|---|---:|---|
| `GMDL` | 6,945 | All begin with `MATR`, `MATS`, `VERT`, `TANG`, `TSET`; geometry framing is observed, but the C port does not yet decode meshes. |
| `GHMS` | 65 | Same five records followed by `HMTS`, `SEGS`; HMTS has two header words and two u32s per row, while SEGS is a count plus 36-byte rows. |
| `GMAT` | 5,532 | Thirteen records per material; only selected links/fields are interpreted. |
| `GTEX` | 3,844 | `TGAF` and `PALS`; nine also contain `TTXA`. |
| `FILM` / `FILQ` | 32 / 16 | See `film-resources.md`. |
| `REEL` / `REEQ` / `FRMQ` | 38 / 173 / 6,116 | Link and placement framing is known; world animation semantics remain open. |
| `GSMP` | 282 | Four-byte sample references; lookup is documented in `sbpc.md`. |
| `MTNL` | 47 | Five scalar u32s and three count-driven arrays: 20-byte, 4-byte, and 16-byte serialized rows; all 47 match the exact declared payload size. |
| `WGNR` | 80 | Resource ID, class token, blob length, packed pointer-relocation blob. |
| `GTXA` | 9 | Four-byte resource lookup key. |

The 37 WGM `SBNK` chunks were checked against the recovered table layout. There
are 99 sample rows total: 31 archives have no local samples, two have one, and the
others have 2, 3, or 46. All installed WGM groups and matching `GSMP` links
remain within the documented parser bounds. The eight victory WGM files have
zero-row local banks while their resource trees contain 5–20 sound references;
their corresponding GSW relationship is not resolved by this local-bank
check. The two track WGM files with nonempty local banks have 3/3 and 2/2
local references matched. Only the four installed WGM files were included in
the earlier detailed chunk-size table; the disc-wide counts and per-tag
inventory are summarized in `resource-format-index.md`.

## Remaining work

- Continue recovering field meanings and validation rules for `GNRL`, `WDIM`,
  `RSPR`, `SCRT`, and `WMOD`. `RSPR` has two observed backend-tagged
  resolutions; `WDIM`/`WMOD` contain repeated extent-like float values.
  `SCRT` record boundaries and parser dispatch are established, but property
  IDs and child class meanings remain unknown. These remain byte-level or
  content-based interpretations, not confirmed runtime roles.
- Complete the `SBNK` audio field interpretation; its framing is distinct from the
  ordinary `RSRC` records. Its index/body layout and `GSMP` ID correspondences
  are now validated, but the raw record fields, gaps, trailing bytes, audio
  representation and playback behavior remain unresolved; group/ID and
  language selection are now traced.
- Document every `RSRC` payload type, reference, compression mode, and version
  variant. Current notes cover only selected texture, model, film, and sound
  resources.
- Compare these disc files with another WGM version before treating the
  observed record order, alignment, or sizes as rules across releases. The
  current cross-check covers only the mounted disc's `0x02020000` files.
