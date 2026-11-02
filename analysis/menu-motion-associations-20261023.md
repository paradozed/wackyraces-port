# Menu vehicle motion links

> Update (2026-10-28): the channel mapping, playback and the menu preview pose
> are recovered in [`mtnl-motion-20261028.md`](mtnl-motion-20261028.md). The
> word `FUN_004403f0` scans is the first serialized clip word (runtime `+0x04`,
> the controller index); the one described below as the second is the chained
> clip.

A bounded walk of the `menu.WGM` BRTR tree followed each `WMenuCarItem`'s
`GDModel`/`GDHModel` children and found these `MTNL` links in property `0x65`:

| Page | Car item | `GDHModel` | `MTNL` |
|---|---:|---:|---:|
| 28 | `0xc5` | `0xb8` | `0x7f8` |
| 28 | `0xd5` | `0xca` | `0x7f9` |
| 28 | `0xf7` | `0xea` | `0x7fa` |
| 61 | `0x64` | `0x5a` | `0x7f8` |
| 61 | `0x71` | `0x68` | `0x7f9` |
| 61 | `0x8d` | `0x82` | `0x7fa` |

The Dingo-Limousine is item `0xb3`; its `GDHModel 0xa7` links to `GHMS 0x7ae`
but has no `MTNL` property. The three libraries are shared between the
single-player and multiplayer pages. This is direct WGM evidence that three
other vehicle previews have motion data; it does not establish the playback
trigger or which fields animate the GHMS segments.

The parser in `c-port/wgm_motion.c` validates the MTNL signature, five header
words, and exact lengths of its 20-byte rows, u32 value array, and 16-byte
vector rows. WPC's `FUN_0043fa60` confirms those serialized spans and loads the
rows into 24-byte runtime records. Its update path (`FUN_0043feb0`) uses values
as indices into the vector array and interpolates three components over time.
The executable's `FUN_00429910`, called while reading each vector, checks that
12 bytes can be read; this is a length check, not a finite-float or semantic
validation of the components.

## Recovered pose application path

`FUN_004385c0` builds the static hierarchy from each `GDHModel`'s segment
table, obtains the linked motion library from the model object, and initializes
the motion state. `FUN_004403f0` scans the second word of each MTNL row to size
the motion-channel controller array. In the three page-28 libraries, that word
is `0xffffffff` in `0x7f8` and `0x7f9`; `0x7fa` has two rows with value `1` and
one with `0xffffffff`. These observations identify a channel selector used by
WPC, but do not yet name its domain.

`FUN_00440650` applies the controller output to each GHMS segment. Its runtime
buffer has nine sentinel/value pairs per segment. The first three values are
added to the serialized SEGS translation. The next three values are passed
through three axis-rotation helpers after multiplying by the executable's
`0.0174533` degrees-to-radians constant. The last three values are added to
one and scale the segment basis. WPC then composes the animated segment with
its parent before transforming mesh vertices. This establishes the output
contract needed by the native renderer, but does not determine how a serialized
row addresses one of the nine slots.

The native port still needs to decode how MTNL row fields select a segment and
which of the translation, rotation, or scale triplets each curve supplies.
Until that mapping is recovered, applying the curves would risk assigning
valid animation data to the wrong bones or transform channels.
