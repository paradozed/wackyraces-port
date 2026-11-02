# Menu film-resource observations

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

The installed `menu.WGM` `RSRC` top-level chunk has resource records with a
four-byte tag and payload length. The following nested link/payload patterns
were recovered from its `FILM`, `FILQ`, `REEL`, `REEQ`, and `FRMQ` records.
They are structural findings; animation timing and screen-space interpretation
remain unknown.

The executable also registers the matching chunk handlers: `FILM` as
`GDFilmRes` at `0x0042b810`, `REEL` as `GDReelRes` at `0x0042b950`, `FILQ` as
`GDFilmRes2` at `0x0042f5c0`, `REEQ` as `GDReelRes2` at `0x0042f700`, and
`FRMQ` as `GDFrame2` at `0x0042f840`. These registrations corroborate the
resource tag/class names; they do not reveal the full runtime draw lifecycle.

## Container records

- `FILM` / `FILQ` payloads begin with a u32 resource ID and u32 link count,
  followed by `count` eight-byte records (`tag[4]`, u32 target ID).
- A `FILM` in this file has one `REEL` link. Six such resources were observed:
  `0x7cc -> 0x78e`, `0x7cd -> 0x78f`, `0x7ce -> 0x790`,
  `0x7d0 -> 0x7a2`, `0x7d1 -> 0x7a3`, and `0x7d2 -> 0x7a4`.
- `FILQ 0x7cf` links 17 `REEQ` resources, IDs `0x791` through `0x7a1`.
- `REEQ` payloads use the same ID/count/eight-byte-link pattern and link to
  `FRMQ` resources.

## Placement sequences

Each observed `REEL` and `FRMQ` payload has an eight-byte header (u32 ID, u32
entry count) followed by exactly `count * 12` bytes. Each 12-byte entry
contains a signed little-endian 16-bit x value, a signed little-endian 16-bit y
value, the four-byte `GMAT` tag, and a u32 material ID. Every material
referenced by the recovered sequences resolves to a `GMAT` in `menu.WGM`.

The six `REEL` resources contain 100, 100, 9, 99, 102, and 110 placement
entries, respectively. The 17 `REEQ` resources link to 594 `FRMQ` sequences in
total. The resource counts and links were checked against their serialized bounds, and
prints the first eight placements of each sequence.

`WMenuText` nodes in the main menu candidate page reference `FILM 0x7d1`,
which resolves to `REEL 0x7a3` and 102 `GMAT` placements. This establishes the
resource chain. Static analysis of the `WMenuText` draw routine at
`0x0042de80` shows bytes at or above `0x20` are reduced by `0x20` to index
12-byte placement records, linking text bytes to the `REEL` sequence. The
A prior font-resource survey recorded that byte/index/material mapping. This supports
the glyph-table interpretation for `0x7a3`; cursor advance, final placement,
color, and font encoding remain unresolved. Likewise, `FILQ` / `REEQ` / `FRMQ`
suggest a queued sequence from their names and references, but timing and
playback semantics are not proven by the payload layout alone.

## Native C animation slice

The native C loader resolves three `GDFilm2` objects from `FILQ 0x7cf`:
object `0x220` uses queue slot 0 at `(66,60)` with one frame, object `0x21f`
uses slot 13 at `(220,66)` with one frame, and object `0x21b` uses slot 3 at
`(66,60)` with 26 frames. These are observed serialized values from the
native loader's WGM walk. The port now renders both single-frame queues as
four-piece character compositions and draws the 26-frame queue over Satanas.
The four placements in slot 0 use materials `0x3e0` through `0x3e3`; slot 13
uses `0x3f8` through `0x3fb`. Drawing them from their serialized coordinates
replaces the hand-scaled character cutout sprites and visually improves their
size and palette match.

The 26-frame queue resolves to `REEQ 0x794`. Its records refer to four
indexed `GMAT` textures, of which three appear in the 26-frame sequence. The
TGAF data contains an embedded TGA whose bottom-left origin is flipped for
SDL. Each frame has one material placement at local coordinate `(-62,61)`.
The native renderer maps the node anchor through the observed 2x menu
viewport, then adds this local FRMQ coordinate; for object position `(66,60)`
the texture origin is `(70,181)`. The same transform draws the four static
placements from queues 0 and 13. Their rendered compositions improve the
visible character size and palette match in
`analysis/menu-film-composition-comparison-20261023.png`. This transform is
visually checked but still needs a live GDFilm2 vertex capture. The selected
node's adjacent properties `0x5a`–`0x5d` decode as `2`, `0`, `24`, and `0.5`.
The executable registers `m_AnimType`, `m_FirstFrame`, `m_LastFrame`, and
`m_Speed`; their runtime offsets are `+0x16a`, `+0x172`, `+0x17a`, and
`+0x182`. Matching registration order and runtime field descriptors strongly
maps those properties to mode, first frame, last frame, and speed. Therefore
`0x5c = 24` is the last-frame bound, not a frame rate. Exact timing and
transparency behavior remain unresolved. The separate `GDFilm` sibling at
`0x23d`/`0x18` is not rendered.
Its material sheet shows portrait pieces for multiple drivers, and a trial
overlay on Satanas only duplicates his head; it does not explain the raised
claw pose in the Wine menu. See
`analysis/gdfilm-reel-790-materials-20261023.png`.

### Raised-arm gesture sequence

The 17 entries of `FILQ 0x7cf` include queue index 12 -> `REEQ 0x79d`, which
contains 19 `FRMQ` records (`0x6d9`–`0x6eb`). Unlike the explicit 26-frame
head queue, these frames contain the static Satanas body pieces plus changing
head/arm textures. The hand and arm materials include `GMAT 0x3ea` and
`0x3ed`–`0x3f7`; their decoded art is visible in
`analysis/menu-satanas-gesture-20261023.png`. At the same `(66,60)` film anchor,
the sequence produces the raised-claw pose in the supplied menu image. The C
port decodes this queue as a startup gesture over the static character and head
animation. It inherits the head node's playback settings as an inference
because sequence 12 has no serialized `GDFilm2` node of its own.

The serialized common group has no separate `GDFilm2` node selecting sequence
12: its three nodes select 0, 13, and 3. Thus the existence and contents of
`REEQ 0x79d` are direct resource observations, while pairing it with the main
menu is an inference supported by the rendered pose match. The executable path
that selects this additional sequence and its exact timing remain open. Six
fresh Wine menu captures show the arm extended in the first image and lowered
in the next five. A private 30 fps Wine recording bounds the raised pose to
about 1.0-1.1 seconds after the menu transition begins. The native port clamps
the sequence to one play and hides its overlay after an estimated 1,064 ms,
using a fitted 28 ms gesture tick and the serialized 0.5 speed. This matches
the visible interval but does not establish WPC's timer origin or trigger.
Captures are recorded in
`analysis/menu-animation-times/wine-menu-contact.png` and
`analysis/menu-satanas-idle-comparison-20261023.png`; the earlier raised-pose
comparison remains `analysis/menu-gesture-comparison-20261023.png`.

### Muttley animation queues

The same `FILQ 0x7cf` has queues 14, 15, and 16 with 48, 109, and 4 frames.
Their materials overlap static Muttley queue 13 (`GMAT 0x3f8`–`0x3fb`) and
add Muttley-specific head/body parts. Decoding their `FRMQ` placements shows
queue 15 contains the open-mouth laugh poses seen in the Wine main-menu
capture. These resource links and frame contents are direct observations.

The serialized Muttley `GDFilm2` object `0x21f` still selects queue 13, with
mode 2, frame range 0–0, and speed 0.5. No serialized node selects queue 15.
A 100-second Wine capture now records two complete laughter cycles in the
main menu. A mouth-only crop finds bursts at 52.367–53.933, 55.033–56.167,
60.300–61.867, and 63.000–64.100 seconds. The repeated first bursts are about
7.93 seconds apart, and the two bursts within each cycle start about 2.67
seconds apart. The previous 13-second excerpt measured similar individual
burst durations and onset gap. In queue 15, the matching frame groups are
16–37 and 54–69. At speed 0.5, a 35 ms update interval predicts 1.54 s and
1.12 s durations and a 2.66 s onset gap. The repeated Wine sequence supports
wrapping the complete queue. The native menu loops its 109 frames at
object `0x21f`'s anchor using mode 2, then shows the static queue-13 pose for a
fitted 355 ms tail pause. The resulting 7.95-second cycle matches the Wine
measurements; a fresh native capture measures both repeated burst starts
7.966 seconds apart. Queue selection and start trigger remain inferred. See
`analysis/menu-muttley-runtime-fit-20261023.md` and the timed clips
`analysis/menu-animation-times/wine-menu-muttley-sequence-20261023.mp4` and
`analysis/menu-animation-times/native-menu-muttley-sequence-20261023.mp4` and
`analysis/menu-animation-times/wine-mainmenu-long-startup-20261023.mp4`, and
`analysis/menu-animation-times/native-menu-muttley-loop-refined-20261023.mp4`.
Decoded frames are in `analysis/menu-muttley-sequences-20261023.png`.

### GDFilm2 playback update path

Static disassembly identifies the `GDFilm2` virtual table at `0x00511014`:
its slot `+0x08` thunk (`0x00405254`) reaches `0x00430110`. The method updates
an integer frame at object offset `+0x162` from an accumulator at `+0x196`,
using a float increment at `+0x182`. It reads a playback-mode field at `+0x16a`
and frame bounds at `+0x172` and `+0x17a`. In the three branches, it either
marks completion and clamps at the upper bound, resets the accumulator to the
lower bound, or reverses the increment at a bound. In mode 3, passing the
upper bound negates the increment and resets the accumulator to zero; a later
negative step below zero negates the increment again and resets it to the upper
bound. This behavior creates endpoint jumps, rather than a continuous
triangular-wave traversal. A separate virtual method
at slot `+0x10` reaches `0x004300d0` and clears the completion state while
resetting the accumulator to the lower bound.

The member registration stubs identify `m_AnimType`, `m_FirstFrame`,
`m_LastFrame`, and `m_Speed`. Their registered offsets plus the class
subobject adjustment (`+0x9e`) match runtime offsets `+0x16a`, `+0x172`,
`+0x17a`, and `+0x182`. In `menu.WGM`, properties `0x5a`–`0x5d` hold mode
`2`, first frame `0`, last frame `24`, and speed `0.5`. The 26th queue frame
(index 25) is therefore outside the configured playback range. The mode-2
updater adds speed to its accumulator and resets it to the first-frame bound
after passing the last-frame bound. The other branches clamp and mark
completion, or reverse direction at a bound.

The C port now interprets these properties as playback state instead of
treating property `0x5c` as a frame rate. It advances at the native menu
scheduler's 16 ms interval, applies mode 2's upper-bound reset, and reproduces
mode 3's accumulator resets and direction changes. The original engine tick
duration and whether its menu invokes this updater on every tick remain
unknown, so this native cadence is an approximation. The inferred gesture
sequence inherits the 0.5 speed, clamps its bound to its frame count, and now
plays once based on fresh Wine menu captures. Its original call site and exact
duration remain unknown.
