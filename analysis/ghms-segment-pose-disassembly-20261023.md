# GHMS segment pose path

For a readable, field-named reconstruction of the static helper chain, see
[`decompilation/ghms-pose-named.c`](decompilation/ghms-pose-named.c). It is
explanatory pseudocode and leaves uncertain engine/resource member names
explicitly marked; the generated Ghidra export remains unchanged.

This note records executable behavior recovered from the installed `wpc.exe`
and checks the native interpretation against the four WGM archives currently
installed beside it. Addresses refer to the executable's 32-bit image; this is
static evidence, not a runtime trace.

## Recovered behavior

> Correction (2026-10-27): the fill and the TSET rewrite below run inside the
> *same* descending iteration, as the named reconstruction shows. A segment's
> TSETs see its own span before lower segments overwrite it. The earlier
> native loader filled the whole map first, which caused the `GHMS 0x7ae`
> "missing tower". See `menu-character-selection-reverse-20261027.md`.

`FUN_00437040` constructs a per-mesh vertex map from the `SEGS` rows. The
second `HMTS` header word is the map length. For each segment, its row fields
at `+0x0c`, `+0x10`, and `+0x14` provide a count, source-vertex start, and map
start. The function fills map entries in descending segment order, storing the
source vertex index and segment index for each entry. The mapping can contain
overlapping spans; the descending loop means lower numbered rows overwrite
those entries later.

The same function walks each segment's TSET span (row fields `+0x04` and
`+0x08`), rebuilding each set's triangle indices around unique source-vertex
indices. Its lookup compares the source index only and retains the segment
index from the first occurrence within that TSET. TANG indices therefore
address the HMTS map, not the raw VERT stream directly. This explains why
drawing raw VERT positions with TANG indices produced malformed GHMS surfaces.
The focused assembly at `0x00436ff0` confirms that the helper returns the
reused/appended output-slot index in EAX; the generated Ghidra fragment types
the helper as `void`, although its caller consumes that return value. The
named reconstruction records the return contract explicitly.

`FUN_004385c0` builds one transform per segment in ascending row order. The
parent index is the first row word; `0xffffffff` denotes a root. The three
floats at row offsets `+0x18`, `+0x1c`, and `+0x20` initialize the local
translation. Child matrices compose with their earlier parent using
`FUN_0043c620`. The final pass transforms each remapped source vertex by its
segment matrix using `FUN_0043b900`.

The inherited object-copy path does not reverse the `SEGS` rows. In
`FUN_00436e90`, the `0x1000` deep-copy branch passes the source row pointer and
count to `FUN_00436f00`; that routine allocates `count * 0x24` bytes and copies
the rows forward, word by word. The non-deep-copy branch retains the source
pointer and count directly. This rules out a row reversal in this clone path.
The initial serialized reader is now traced as well. The method beginning at
`0x00436c30` dispatches the `SEGS` and `HMTS` FourCCs. For `SEGS`, it reads the
row count, allocates `count * 0x24` bytes, then reads each row's six u32 fields
and trailing three-float translation while advancing the destination by
`0x24` per iteration. The loop increments its row index from zero to count,
so the initial materialization also preserves file order. `HMTS` pair records
are likewise read into an eight-byte row array in ascending order. Together,
the parser and clone paths establish that the runtime SEGS order matches the
serialized order.

The C loader now follows these steps in `pose_height_mesh()`: it validates
map, vertex, parent, and TSET bounds; fills the map in the observed reverse
order; preserves per-TSET source-vertex sharing and first-occurrence segment
selection; composes translations; and rewrites
the mesh positions and triangle indices. This applies the static segment pose.
MTNL animation and the game's lighting/render backend are separate paths and
remain unimplemented.

## Installed archive consistency check

The current `menu.WGM`, `SWMenu.WGM`, `general.wgm`, and `swgeneral.wgm`
contain 58 GHMS resources. For all 58, the second HMTS header word equals one
past the greatest VIND index, all SEGS map spans cover that entire index range,
all source-vertex spans fit VERT, every parent is a root or earlier segment,
all segment TSET spans are in range, each TSET is assigned once, and TSET
triangle intervals cover each TANG triangle exactly once. This confirms the
native map bounds, ownership, and hierarchy assumptions against the available
corpus; it does not by itself prove pixel-perfect output.
