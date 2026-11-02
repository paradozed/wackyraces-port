# `STRINGS.DAT` decoding evidence

`wpc.exe` contains an indexed loader for `STRINGS.DAT`. The string reference is
at VA `0x0044d97d` in the function radare2 labels `0x00404e21`; the routine body
starts at `0x0044d950`. It reads the file through a shared file helper, then
walks the language records and their offset entries. For each string payload,
it XORs bytes with `0x7f`, except raw byte `0x7f`, which it preserves. The table
payload has raw NUL terminators. The current C text inspector applies that
transform to menu strings.

Decoded ASCII examples identify the language IDs by their contents:

| Language ID | Example for string 2 | Inferred language |
| --- | --- | --- |
| 0 | `WRONG WAY!` | English |
| 1 | `FALSCHE RICHTUNG!` | German |
| 2 | `\x82SENTIDO CONTRARIO!` | Spanish |
| 3 | `MAUVAISE DIRECTION!` | French |
| 4 | `CONTROMANO!` | Italian |
| 5 | `VERKEERDE RICHTING!` | Dutch |
| 6 | `CAMINHO ERRADO!` | Portuguese |

The language names are content-based inferences, not decoded language labels
from the file. The transform yields readable ASCII across the tables, but the
non-ASCII byte encoding/font convention is not yet established (for example,
Spanish string 2 begins with decoded byte `0x82`).

## Language selection evidence

`wpc.exe` has the literal `LANG` at VA `0x0052ab84`. The settings code around
`0x004603c0` opens the base product key and queries `Prefs`, `MenuPrefs`, and
`LANG` as separate values. The language value is passed with global storage
at `0x006f46e8` to the registry query helper, which reaches `RegQueryValueExA`
at `0x00460308`. The base key path is embedded at `0x0052ab28` as
`Software\\InfoGrames\\Wacky Races\\1.00.000\\`. `MenuPrefs` is a separate
384-byte value; it is not the subkey containing `LANG`.

After `STRINGS.DAT` is loaded, code at `0x0044d8e4` reads the global value at
`0x006f46e8` and passes it to the language lookup at `0x0044db50`. The lookup
compares that value with each language record's ID and selects the matching
record. If none matches, it selects the first record. This establishes a
static configuration path from the product key's `LANG` value to the
language-table selector. The current Wine prefix's
`Software\\Wow6432Node\\Infogrames\\Wacky Races\\1.00.000` entry contains
`LANG=dword:00000003`, which agrees with the content-inferred French table
(ID 3). This is evidence about the current prefix only; it does not establish
the value written by setup or the default used on a clean installation.

The native runtime now reads optional `WPC_LANG` as a decimal language ID
0–6, passes it to menu and vehicle string lookup, and selects the same
language ID from `Menu.GSW`. With the variable unset it defaults to ID 3 to
match this installed prefix. Menu labels preserve decoded bytes in the
installed font's `0x20..0x85` range and use `?` outside it. This covers the
French `0x80` œ glyph. Spanish `0x82` occurs four times at the start of
messages, but its font placement has a zero-width texture and does not render;
Portuguese `0xc7` in `CONFIGURAÇÕES` exceeds the font range and displays as
`?`. Native save prompts are still French.

A full resource survey of the installed `menu.WGM` found 204 `WMenuText` nodes,
of which 187 have nonzero candidate keys. The native C command
`--inspect-menu-text` currently joins text keys for one selected menu page.
The key-to-text relationship is supported by the matches, but exact on-screen
rendering and font handling remain open.

The seven installed language tables each contain 233 strings. Decoding each
string with WPC's observed XOR-`0x7f` rule shows French byte `0x80` in
`PENELOPE JOLI C\x80UR` (objects `0x107` and `0xbe`); the `FILM 0x7d1`
contact sheet shows that byte as a glyph shaped like uppercase `Œ`, consistent
with French “CŒUR”. Spanish strings contain `0x82` four times at message
starts. Portuguese contains one `0xc7` in `CONFIGURAÇÕES`. The menu font reel
`REEL 0x7a3` has 102 placements, exactly the byte-index range `0x20..0x85`;
`0x82..0x85` are zero-width placements. Thus the native renderer can map the
French glyph, but the Spanish opening punctuation has no visible bitmap and
Portuguese `0xc7` has no placement. Other byte values outside the font range
are displayed as `?` rather than indexing past the placement array.
