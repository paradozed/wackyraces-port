# Native scene and localized strings

`src/port/game_scene.cpp` reads the installed WGM scene into owned `Node`
values. Class names come from the counted length-prefixed strings in `CLAS`
`BRCM`. Each node stores its ID, class name, raw property values, child nodes,
and opaque auxiliary chunks. Serialized node class tokens select the class by
their low 12 bits; for example `0x0104c02b` selects class 43, `WMenuPage`.
Property tokens select keys by their low 16 bits.

The bounded BRTR reader follows the directly inspected framing:

```text
node := class_token:u32 | node_id:u32 | PRPS | CHBR* | auxiliary_chunks*
PRPS := tag/length | PROP*
PROP := tag/length | property_token:u32 | raw_value
CHBR := tag/length | node
```

The actual reader consumes bounded node-local chunks, retaining auxiliary
chunks wherever encountered. The installed menu has 991 nodes, 990 `CHBR`
records, 991 `PRPS` records, and 23 `ACTN` records; the maximum node depth is
eight. All node IDs and per-node property keys are unique in this file.
`ACTN` bytes are retained and also decoded through the separate ACCM class
namespace into `Node.actions`. SCRT is decoded as a separate screen/viewport
tree. The menu controller now interprets the WMenu navigation subset; the
remaining action classes are data, not executed behaviors. See
[navigation recovery](native-menu-navigation.md).

The reader rejects duplicate IDs/properties, missing or duplicate `PRPS`,
invalid class references, truncated nested chunks, and trees exceeding 128
levels or 65,536 nodes. Failed reloads preserve the prior decoded scene.
Scalar helpers require exactly four bytes; a resource-link helper requires
the expected four-byte tag followed by a u32 ID. Link helpers do not establish
that a referenced resource exists; resource lookup supplies that check.

Read-only observations from the installed `menu.WGM`:

- Root ID 1 has class `GDWorld`.
- Node 4 has class `WMenuPage`, properties `0x197=160`, `0x198=70`.
- Node 7 links property `0x35` to `FILM 0x7d1` and string key 14 at `0x4a`.

These are successful data reads, not evidence that scene actions or the
original game loop have been reconstructed.

## Strings

`Strings` reads the counted language triples `(ID, entry_count, absolute
entry_offset)`, then `(key, absolute_text_offset)` rows. It bounds all tables
and NUL-terminated strings, rejects overlapping tables/duplicate language
IDs, and decodes the selected table with XOR `0x7f`, preserving raw `0x7f`.
Language selection falls back to the first table if the requested ID is absent,
matching the decompiled lookup at `0x0044db50`.

All seven installed tables have **233 rows but 231 distinct keys**: IDs 254
and 999 occur twice in each language. The first matching entry wins, matching
the sequential lookup at `0x0044dba0` (exported under its thunk in
`analysis/decompilation/wpc/functions-006.c`). Treating duplicate keys as
malformed would reject the original file. This refines the earlier row-count
description in `wpc-strings.md`; no string encoding beyond the observed byte
transform is claimed.

Observed key 14 by language ID is `START`, `START`, `INICIO`, `DEMARRER`,
`INIZIA`, `START`, `INICIAR` for IDs 0 through 6 respectively.

Repeat 190 synthetic checks and optional installed-file inspection:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Isrc/port \
  src/port/game_assets.cpp src/port/game_scene.cpp tests/native_scene.cpp \
  -o /tmp/wpc-native-scene-test
/tmp/wpc-native-scene-test /path/to/menu.WGM /path/to/STRINGS.DAT
```

The fixtures exercise class/property decoding, typed links, opaque action
retention, duplicate IDs, malformed spans, depth limits, language selection,
duplicate-key behavior, non-ASCII bytes and every truncated prefix of a
representative string file. Fixtures contain no copied game data.
