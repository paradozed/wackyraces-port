# Menu sound bank format and selection

Evidence: installed `wpc.exe` SHA-256
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`,
`Menu.GSW`, and the four WGM files. This is static executable analysis and
file parsing, not a captured playback run. Focused assembly is preserved in
`analysis/disassembly/wpc-sound-bank-loading.txt`.

## Corrected directory

`Menu.GSW` is 34,897,400 bytes. Its layout is:

```text
+0x00  "SBPC"
+0x04  entry_count:u32le (=7)
+0x08  entry_count * { selection_id:u32le, absolute_bank_offset:u32le }
```

Function `0x0044c510` (thunk `0x00401cee`) reads the signature through the
u32 stream helper `0x004294f0`, compares it with its expected-tag argument,
reads the count, and scans exactly that many ID/offset pairs. It returns `-1`
for the wrong tag or `-2` when the requested ID is absent. A matching offset is
passed to the stream's seek method with origin 0, then the manager's virtual
method at `+0x40` loads that bank.

An initial interpretation incorrectly treated 7 as a version, treated the
first entry `(0, 64)` as other header fields, and started the directory at
`+0x10`. This omitted bank 0 and interpreted bank data as a spurious entry.
There is no version word or zero-ID terminator in this directory.

| Selection/language ID | Bank offset | Body length | Bytes after declared bank before next bank/EOF |
| ---: | ---: | ---: | ---: |
| 0 | `0x00000040` | `0x004b8f02` | 2 |
| 1 | `0x004b9368` | `0x004e8376` | 2 |
| 2 | `0x009a1b04` | `0x004c410a` | 2 |
| 3 | `0x00e66034` | `0x004b43f2` | 2 |
| 4 | `0x0131a84c` | `0x004b5cba` | 2 |
| 5 | `0x017d092c` | `0x004a5cb8` | 0 |
| 6 | `0x01c76a08` | `0x004d0fca` | 2 |

## Shared SBPC/SBNK bank layout

Each bank starts with two lengths followed by a table and a sample body:

```text
bank+0x00  table_size:u32le       // excludes these two length words
bank+0x04  body_size:u32le
bank+0x08  group:u32le
bank+0x0c  next_link:u32le        // stored zero in these files
bank+0x10  sample_count:u32le
bank+0x14  body_pointer:u32le     // stored zero in these files
bank+0x18  sample_count * 28-byte rows
bank+8+table_size  body[body_size]
```

Concrete loader `0x00426bd0` is reached through thunk `0x00401e0b` in slot
`+0x40` of vtable `0x00510b80`. It reads the two lengths, allocates their sum,
and reads that many bytes **after** the length prefix. At `0x00426c35` it
sets loaded-table `+0x0c` to `loaded_table + table_size`; it then sets table
`+4` to the former bank-list head and installs the table at manager `+0x78`.
This confirms both the body offset and the runtime roles of the two pointer
words. The recovered bounds model preserves the original bytes and treats offsets
relative to the documented table/body spans.

`table_size = 16 + sample_count * 28`. All seven SBPC banks have table size
`0x41c`, group `0x80`, and 37 rows. The body begins at bank offset `0x424`,
not `0x41c`. The former extraction command shifted every span eight bytes earlier: the
first clip began with the last eight table bytes, and every extracted clip
included eight preceding bytes and omitted its final eight bytes. Existing extracts should be regenerated.

Each row is seven little-endian u32 values:

| Row offset | Meaning established so far | First row in bank 0 |
| --- | --- | ---: |
| `+0x00` | Sample ID (lookup compares the low 24 bits of a reference) | `0x64` |
| `+0x04` | Relative body offset | 0 |
| `+0x08` | Byte length | 270,030 |
| `+0x0c` | Audio parameter, copied to sample object `+0x28` | 22,050 |
| `+0x10` | Audio parameter, copied to sample object `+0x18` | 2 |
| `+0x14` | Raw field, copied to sample object `+0x20` | 0 |
| `+0x18` | Raw field, copied to sample object `+0x24` | 0 |

Rows start at bank `+0x18`, not `+0x10`. The formerly reported per-row count
was actually the bank header's sample count. The row IDs range from `0x64` to
`0x88`; spans cover the declared SBPC bodies exactly with no gaps/overlap.
All 259 rows have final four words `(22050, 2, 0, 0)`. DirectSound sample setup
at `0x00427dc0` tests row flags at sample-object `+0x18`: bit 0 selects stereo,
and bit 1 selects 16-bit samples. The resulting `WAVEFORMATEX` is initialized
with format tag 1 (PCM) in `0x004279c0`; channels, rate, block alignment, byte
rate, and bit depth are filled from the sample fields. The sample rate comes
from object `+0x28` (row `+0x0c`). The DirectSound buffer path copies sample
bytes directly into the buffer (`0x00427ce0`). Installed SBPC rows with flag
2 therefore use 16-bit mono PCM at 22,050 Hz. This is static evidence; no
runtime capture was made.

Rows `+0x14` and `+0x18` are copied to object fields `+0x20` and `+0x24`.
When sample `+0x20` is nonzero, `0x00427dc0` computes additional bytes as
`(sample[+0x24] - sample[+0x20]) * 2` and appends that many bytes from the
sample's tail to its temporary buffer, subject to a size cap. Installed rows
set both fields to zero. Their intended loop or padding semantics remain
unknown; the earlier WAV export wrote each stored clip once and did not
reproduce that extension. The 0/2 trailing bank bytes are outside sample data.

The native C port now reads the installed `Menu.GSW` SBPC directory, validates
all seven banks and their sample rows/spans, selects an exact language ID, and
loads that bank's 37 PCM samples. It rejects duplicate language or sample IDs,
bank overlap, unsupported parameters, and sample overlap. For main-menu page 4,
the C WGM reader collects GSMP links attached directly to WMenuItem nodes and
resolves their low 24-bit IDs against the selected language bank. Local WGM
SBNK resources, general-scene references, alternate row extension values, and
other pages remain unsupported. Gaps and trailing bytes remain data rather
than presumed audio.

## Menu filename and language selection

At `0x00431d69` and `0x00431d7b`, calls to `0x00503600` compare the scene
filename with `MENU.WGM` and `SWMENU.WGM`. This is a case-insensitive string
comparison helper, already documented in `analysis/wpc-runtime-signatures.md`;
these calls do not attempt to load either file. A zero comparison result
branches into the `MENU.GSW` open path at `0x00431d87`.

When the GSW open succeeds, `0x00431d9b` reads the selected `LANG` value from
`0x006f46e8`, and `0x00431db1` calls the SBPC selector with that value and tag
`SBPC`. It selects the directory entry with the exact matching ID. Thus bank
0 is a real language bank, and LANG 3 selects the bank at `0x00e66034`.
Language names inferred from STRINGS.DAT content are recorded separately in
`analysis/wpc-strings.md`. The previous claim that GSW was loaded only after
both menu WGM loads failed was incorrect.

## Sample references and binding

Initializer `0x0044ae00` registers class `GDSample` with tag `GSMP` and class
vtable `0x00511d6c`. Factory `0x0044ae40` allocates `0x30` bytes, invokes
constructor `0x0044af00`, and returns an adjusted pointer. The constructor
installs instance vtable `0x00511d7c` and clears five fields.

The instance's sample-binding method `0x0044af90` passes its ID to manager lookup
`0x0044c640` through thunk `0x004055b5`. The lookup walks the bank list at
manager `+0x78`. It matches the reference's high byte against the bank group
at descriptor `+0`, then scans `sample_count` at descriptor `+8`, starting at
descriptor `+0x10` in 28-byte steps. It compares row `+0` against
`reference & 0x00ffffff`. A match calls `0x0044b000` through thunk
`0x004059e8` with `(bank_descriptor, sample_row)`; a miss clears the sample
binding and returns zero.

**Stack detail:** `0x0044b000` initially obtains the row from `[esp+8]`.
After `push esi` at `0x0044b00d`, `[esp+8]` denotes the first argument, the
bank descriptor. Consequently `0x0044b029`–`0x0044b036` compute
`object[+0x2c] = bank[+0x0c] + row[+0x04]`: body base plus relative offset.
The previous note claiming this was the sum of two row fields failed to
account for the saved register and is superseded. The byte length is copied
to object `+0x1c`; the other four words are copied as shown in the table.

Both menu WGM files contain 36 GSMP references in group `0x80`, matching
36/37 samples in the language-selected bank (`0x6e` is unreferenced). Each
also has one group-0 local SBNK sample: `0x7f7` in `menu.WGM` or `0x803` in
`SWMenu.WGM`. All 46 GSMP references in each general WGM resolve to group-0
local rows. `WMenuSound` property `0x94` carries tagged GSMP references.

An earlier bounded inspection followed this selection and lookup, reporting
source-file offsets and lengths for all GSMP references. It reports missing IDs/languages instead of substituting another
bank. For `menu.WGM` with language 3 it resolves all 37 references. Its output
associates each `WMenuSound` object with its direct parent scene object, raw
property `0x1bc`, tagged GSMP value in property `0x94`, and resolved file offset
and length. In installed
`menu.WGM`, the observed property `0x1bc` values are 0 and 2; their meaning is
not established, and scene attachment alone does not prove when playback
starts or stops.

## Format cross-checks

An earlier WAV export of `Menu.GSW` language 3 clip `0x64` was recognized as
22,050 Hz mono 16-bit PCM; its data section matched the source clip bytes.
Both menu scenes resolved all 37 references for each installed language ID
(0–6), and both general scenes resolved all 46 local references. Extracted
clip files were compared by SHA-256 with the corrected source spans. These
checks establish file addressing, not audible output or menu event timing. The
current C port implements the `Menu.GSW` SBPC directory and PCM rows, plus
main-menu page 4 `WMenuItem` GSMP link resolution as described above.
`--inspect-wgm` validates local SBNK rows and cross-checks `GDSound` and
`WMenuSound` GSMP links by group/sample ID. Local SBNK playback and general-scene
group-0 samples with installed 16-bit mono PCM parameters can now be played by
`--play-wgm-sample WGM sample-id`. Scene-trigger dispatch, spatial mixing,
nonzero extension values, and general-scene GSW-to-WGM resolution remain
unimplemented.

## Additional GSW files on the mounted disc

The installed `Menu.GSW` was compared with the eight victory archives under
the mounted ISO's `Win/` directory (`DickOff.GSW` and seven `Win*.GSW` files).
All nine have the same `SBPC` counted-directory layout and seven selections;
a bounded parser accepted all 63 banks. Across those banks, the table
row counts range from 5 to 37 and total 1,134. Every row uses group `0x80`,
the parameter tuple `(22050, 2, 0, 0)`, and a sample span inside the declared
body; no row overlap or body gap was reported. Some banks are followed by two
bytes outside the declared body, matching the previously observed padding in
`Menu.GSW`.

This establishes that the GSW files on this disc share the parser's observed
format and PCM parameters. It does not establish when each victory bank is
selected or whether other releases use the same layout. The eight victory WGM
files have empty local SBNK tables and 5–20 GSMP references apiece; the exact
runtime pairing between each WGM and corresponding GSW remains to be traced.

## Remaining work

- Trace resolved samples into the audio backend to establish remaining row
  field meanings and loop behavior.
- Map menu events to sound start/stop and compare output with a captured run.
- Original menu trigger event, gain, and overlap behavior remain unverified.
  The native app currently plays page-local `WMenuItem` links on selection for
  text-backed pages; static-label pages do not yet map their selectable object
  IDs. The separate `.snd` music path is implemented in the native runtime.
