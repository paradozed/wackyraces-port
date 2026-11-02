# TGAF payload observations in the menu text film

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

This note checks the 102 `GMAT` placements referenced by `FILM 0x7d1` ->
`REEL 0x7a3` in the installed `menu.WGM`. The RSRC records consistently link
each material through `TEXP` to a `GTEX` containing a `TGAF` chunk. This
confirms the resource chain only; it does not establish that the placements
are glyphs or that they are drawn as a single text run.

## Header and payload checks

For all 102 referenced TGAF payloads, the u32 at offset 12 is `8`. The u32 at
offset 16 differs from the containing TGAF payload length, and the bytes after
the 32-byte header do not equal `width * height * 4`. Eight payloads have a
zero width or height according to the width/height fields at offsets 4 and 8.
The first placement, material `0x468`, texture `0x1cc`, reports 16x4 but has
840 bytes after the 32-byte header. Placement 1, material `0x469`, texture
`0x1ce`, reports 12x25 and has 1,076 such bytes. These observations do not
support treating this set as uncompressed RGBA data.

The same scan over every `GTEX` in the installed menu WGM found 662 TGAF
payloads: 518 with nonzero dimensions and value `8` at offset 12, 100 with
nonzero dimensions and value `32`, and 44 with a zero dimension and value `8`.
None of the 662 payload tails equals only `width * height` bytes or only
`width * height * 4` bytes. Instead, the 100 value-`32` chunks have exactly 32
bytes of extra data beyond the four-byte-per-pixel size. For value-`8` chunks,
390 have exactly 776 extra bytes beyond the one-byte-per-pixel size; 116 have
774 extra bytes, six have 777, five have 800, and one has 775. The values at
offset 12 match the observed per-pixel byte counts, though interpreting them
as bits-per-pixel remains an inference.

The TGAF payload has a 20-byte wrapper followed by an embedded TGA stream.
The stream begins at offset 20; its 18-byte TGA header places the 256-entry
24-bit palette at offset 38 and its indexed pixels at offset 806 (for a
zero-length image ID). This is confirmed by the TGA header fields and the
embedded byte length at offset 16. The earlier offsets 40 and 808 were two
bytes too late. For example, the selected menu animation texture `GTEX 0x142`
has dimensions 79x72 and embedded length 6474, exactly `18 + 768 + 79*72`;
the containing TGAF payload has two additional trailing bytes. This explains
why the earlier decoder shifted the palette and rendered corrupted colors.

The executable copies palette bytes as stored, then its Glide upload path
interprets them as BGR. The C renderer converts those BGR triplets for SDL.
The index-zero transparent treatment is a presentation choice for this menu
sprite based on the supplied port capture; the original conversion routine
instead keys exact all-zero palette colors and applies a per-texture alpha
threshold to other indices. That runtime threshold has not been recovered for
this particular film object.

The earlier contact sheet used the shifted palette/index offsets, so its color
and two-index-short interpretation is superseded by the embedded-TGA layout
above. The native C reader now validates the TGA header, color-map fields,
dimensions, and declared image-data length before decoding. It accepts the
two-index-truncated case by padding the final two indices with zero, though
the earlier cases that prompted this accommodation are complete when parsed
from the correct TGA pixel offset.
The eight zero-width records remain
unresolved, but the font map narrows them to byte codes `0x78`–`0x7b` (`x`–`{`)
and `0x82`–`0x85`. All eight placement records have offset `(0, 102)` and
texture dimensions `0 x 106`; this common shape is an observation, not proof
that the records are deliberate blank glyphs. The combined 94 nonzero-size
previews are saved at
`tgaf-reel-7a3-preview.png`; blank slots correspond
to the eight zero-width records.

Three additional sprite textures used by the scene (`GTEX 0x16`, `0x18`, and
`0x1a`) use the same 256-entry RGB palette and indexed pixels, followed by a
24-byte TGA-signature trailer. Their duplicated descriptor bytes are `08 08
00 00` (pixel-size byte 8, descriptor byte 8, then a zero u16). Their exact
payload sizes match `40 + 768 + width * height + 24`. The native bounded C
decoder accepts this variant when the indexed pixel extent, either eight
zero offset bytes or the older two `0xff` bytes plus six zeroes, and TGA
signature match. `GTEX 0x12` uses the all-zero offset form. It does not infer
palette attribute semantics from the duplicated descriptor.
This explains the three indexed materials previously skipped in scene
previews; it does not establish what the descriptor's attribute bits mean for
the palette.

Across all 100 menu-WGM TGAF records with value `32` at offset 12 and nonzero
dimensions, the embedded TGA starts at offset 20. Its 18-byte header has
matching dimensions, 32-bit pixels, and descriptor `8`; the raster therefore
starts at offset 38. The file extent is exactly `20 + 18 + 4 * width * height
+ 26`, where the final 26 bytes are the TGA 2.0 footer. This supersedes the
earlier offset-40 interpretation: bytes 38–39 are the first two raster bytes,
not a key field or footer padding. Converter `0x0044e950` reads the raster
pointer returned by helper `0x0044e0c0` (`TGA header + 18 + ID length`) and
copies four bytes per 32-bit pixel unchanged. Glide upload code at
`0x00434520` treats the source dword as `AARRGGBB` on little-endian x86,
packing bytes 0–2 as BGR and using byte 3 for alpha-bearing formats. Thus the
native SDL decoder uses BGRA byte order, starts at byte 38, and normalizes the
bottom-left TGA origin to SDL's top-left surface coordinates. No per-pixel
color-key comparison occurs in the 32-bit converter; the former candidate
u16 at offset 38 was the first half of the raster and was a false lead.
The descriptor value `8` has no top-origin flag, so the SDL decoder reverses
the stored row order into its top-left surface convention. The truecolor row
conversion itself is a copy; the row reversal is performed by the native SDL
adapter to preserve the TGA image orientation.

All 100 of the 32-bit records end in the standard 26-byte TGA 2.0 footer:
eight zero offset bytes followed by `TRUEVISION-XFILE.\0`. This accounts for
the exact `64 + width * height * 4` TGAF size. The separate 24-byte trailer
described above belongs to three indexed records with a nonstandard footer
shape. The [Library of Congress TGA format note](https://www.loc.gov/preservation/digital/formats/fdd/fdd000180.shtml)
documents the standard footer signature. The signature supports a TGA-derived
serialization path; the executable converter and Glide packer establish the
32-bit pixel byte order and alpha use independently.

The sprite screen transform is a separate question from TGAF row orientation.
The selected `GDFilm2` node's serialized object position and 240-line menu
coordinate context support the existing anchor conversion for that overlay;
the FRMQ offsets are not an extra translation in the matching reference
composition. This does not establish the parent-to-screen transform for the
static `GDSprite` group or `WMenuSprite` nodes. Their 48-byte matrices are
identity and properties `0x9f`/`0xa0` are candidate coordinates, but the
common group's display transform and draw order still need a renderer call-site
or a usable original runtime frame.

The native C animation decoder also has a strict 24-bit truecolor path. It
requires an uncompressed type-2 embedded TGA, zero ID length, matching
dimensions, no interleave bits, and the wrapper extent `64 + 3 * width *
height`; its assumed raster start is byte 40. The executable supports 24-bit
source pixels, but no 24-bit TGAF record occurs in the installed WGM files, so
that wrapper layout remains an assumption awaiting an asset comparison.

The former offline texture parser's assumption that TGAF pixels are RGBA
starting at offset 32 did not match this film. The earlier experimental preview
also mislocated the embedded TGA palette and pixels by two bytes. The native C
menu-animation reader follows the embedded TGA header, decodes indexed
textures for the selected film queue, and accepts the specifically validated
footer-bearing indexed layout described above. It does not yet decode all
menu textures or render the full scene. Executable disassembly confirms BGR palette
triplets, zero-color transparency, and that Glide's
alpha-bearing packed formats consume the fourth byte of 32-bit pixels as alpha.
The exact per-texture alpha threshold, DirectDraw interpretation, and two-pixel
padding remain unresolved. This decoder does not determine placement
geometry or establish runtime text draw order. Experimental page `0x1c`
preview now renders all 16 sprite nodes; the full-scene preview renders all
298 nodes with no skipped materials. Neither preview has been validated
against an original runtime frame. The candidate page preview is saved as
`analysis/page-1c-sprites.ppm`; the unfiltered preview is
`analysis/menu-all-sprites.ppm`.

## Next evidence needed

- Identify the TGAF handler/decompressor and its call sites in `wpc.exe`.
- Determine whether the 22 two-index-short menu glyphs are intentionally
  trimmed or incomplete, and explain the eight zero-dimension records.
  The zero-width placements are byte codes `0x78`–`0x7b` and `0x82`–`0x85`,
  all with the same `(0, 102)` offset and `0 x 106` texture dimensions.
- Confirm how the backend interprets the preserved truecolor byte order and
  fourth byte against an original runtime frame.
- Confirm indexed palette byte order, zero-color transparency, and the
  `0xff`/`0x80` threshold behavior in a runtime frame; the converter copies
  palette triplets in order and applies those output-byte values statically.
- Validate any recovered decoder against multiple known menu textures and an
  original runtime capture before wiring it into the menu renderer.

The counts and sizes above were checked against the installed `menu.WGM`
resource records on 2026-10-22. Their meaning remains unresolved.

## Executable leads

Radare2's string scan finds the class name `GDTexture` in `.data` at VA
`0x0052a654`, with a registration reference at `0x0044dc24`. It also finds
`GTEX`-shaped tag comparisons in `.text` around `0x00449ead`, `0x00449f87`,
`0x004bd0d2`, and `0x004bd16f`, and a `TGAF`-shaped comparison around
`0x0044e1dc`. These are static code locations, not recovered function
semantics. Radare2 did not resolve a direct xref from the inline tag bytes to a
function, so the loader/decoder and the meaning of the header remain open.

A focused disassembly clarifies part of that lead. The `TGAF` comparison at
`0x0044e1dc` is a branch in a chunk-dispatch routine within the large function
starting at `0x0044d850`; the adjacent branches compare other nested tags such
as `TTXA` and `PALS`. Its TGAF branch at `0x0044e3b7` invokes the shared
`0x00402d2e` dispatcher on object fields at offsets `0x28`, `0x1c`, `0x20`,
`0x24`, and `0x30`, then calls helpers at `0x004022a2`, `0x004041ec`, and
`0x00404322`. These are jump thunks: `0x004022a2` to `0x004ad810`,
`0x004041ec` to `0x00429b60`, and `0x00404322` to `0x0044eef0`;
`0x00402d2e` jumps to `0x004294f0`. The TGAF branch allocates a buffer using
the value read from object offset `0x30`, stores its address at `0x2c`, then
calls the resource method at `0x0044eef0`. That method performs resource-state
checks and virtual calls. This trace supports structured field loading and
resource lifecycle handling, but alone does not prove the serialized fields'
meanings.

A second pass followed the registered texture object's nearby methods and
found a TGA-header validator at `0x0044dd10`, plus helpers at `0x0044e0c0` and
`0x0044e0f0` that calculate the first pixel/index address from the TGA header.
The validator accepts only uncompressed type-2 truecolor images with no color
map and 24- or 32-bit pixels, or type-1 color-mapped images with 8-bit indices,
a nonempty color map, and 24-bit palette entries. Both paths require nonzero
width and height. It rejects descriptor interleave bits `0x30`; it does not
check the vertical-origin bit or determine alpha/key behavior. The data-start
helpers calculate `18 + ID-length` bytes for truecolor and add
`color-map-length * (color-map-entry-depth / 8)` for indexed images. These
checks strongly identify an embedded TGA-compatible image buffer and confirm
that the executable handles both the 8-bit palette and 24/32-bit truecolor
families found in menu resources. The focused instruction dump is
[`wpc-tgaf-header.txt`](disassembly/wpc-tgaf-header.txt).

Following the validator's thunk at `0x0044e99f` reaches the conversion routine
starting at `0x0044e950`. It reads source dimensions and bit depth, applies
dimension limits and optional power-of-two sizing, then constructs a type-2
truecolor output image. Source 8-bit indices are expanded through the 24-bit
palette into 32-bit output pixels. Source 24-bit pixels are copied as three
bytes and 32-bit pixels as four bytes, so this routine preserves their stored
channel byte order rather than swapping channels. The indexed path sets the
fourth output byte to zero for an all-zero palette color; otherwise it sets it
to `0xff` or `0x80` according to a runtime threshold. The threshold is not a
fixed palette property: the converter uses fallback 300 when the texture
descriptor field at `+0x3c` is `0xfe`. Otherwise it resolves the descriptor's
`+0x38` value through a global object table, requires linked-object fields
`+0x1c == 256` and `+0x60 == 1`, scans sixteen dwords beginning at `+0x20`,
and derives a cutoff from the first nonzero slot and object field `+0x6c`.
Other states take a separate count-based formula. Thus the threshold source
is now mapped as linked runtime object state, though the linked object's class,
serialized meaning, and visual effect remain unknown. Whether the 32-bit
fourth byte is interpreted as alpha by each backend also remains unresolved.
The routine builds coordinate lookup
tables for the output dimensions and samples the source through those maps; it
does not visibly swap channels or flip rows. The focused conversion dump is
[`wpc-tgaf-convert.txt`](disassembly/wpc-tgaf-convert.txt).

The caller at `0x00434520` takes that output through Glide-specific format
selection and texture upload. The loader at `0x0043345e` resolves
`_grTexDownloadMipMap@16` into global `0x0054732c`, which the caller invokes at
`0x00434a20`. Its packing branches convert BGR triplets to RGB332, RGB565,
ARGB1555, or ARGB4444 layouts; the high byte feeds the alpha field in the
alpha-bearing formats. The 24-bit input path synthesizes an opaque high byte
before packing. This confirms Glide's interpretation of truecolor channels and
alpha. A separate DirectDraw upload routine at `0x004b0050` calls a virtual
method at offset `+0x58` on an incoming surface-like object to retrieve a
124-byte surface description, then calls `+0x18` on another COM object to
create a texture surface. After locking surfaces through vtable offset
`+0x64`, it reads destination red/green/blue/alpha masks, computes each mask's
bit shift and scale, and packs converted samples into 16-bit destination
words. It handles a destination flag at description byte `+4` by averaging
scaled channels; when all source channels are zero, it writes zero. This
documents mask-based channel conversion, but does not establish the COM
interface identities, alpha blend state, or how indexed `0xff`/`0x80` source
bytes affect rendered opacity. See
[`wpc-glide-texture-upload.txt`](disassembly/wpc-glide-texture-upload.txt) and
[`wpc-directdraw-texture-upload.txt`](disassembly/wpc-directdraw-texture-upload.txt).

The DirectDraw setup at `0x004b14b0` performs three virtual pixel-format
enumeration passes through a callback at `0x004b13c0`. The callback filters
pixel depth and descriptor flags, and checks 16-bit alpha-mask candidates
against `0x8000` and `0xf000` before storing a candidate descriptor. If the
last pass yields no candidate, it displays `Not Find Alpha Texture Format`.
The callback and call setup are preserved in
[`wpc-directdraw-format-selection.txt`](disassembly/wpc-directdraw-format-selection.txt).
This shows that WPC seeks an alpha-capable format, but the exact winning
format and runtime blending behavior are not demonstrated by static analysis.
