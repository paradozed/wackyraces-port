# Native world material loading

`src/port/game_material.cpp` reads the material state established by the
installed `WPC.EXE` and WGM files. Target executable SHA-256:
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`.
Executable findings below are static decompilation/disassembly observations;
corpus counts are read-only file observations, not rendered-frame comparisons.

## API and resource identity

`Material` exposes `textured`, full `texture_id`, `diffuse[3]`, raw
`transparency`, `clamp_axes`, and `two_sided`.

- `parse_material(const Archive&, id)` reads a local GMAT without requiring
  its GTEX to be local.
- `resolve_resource(const ArchivePool&, tag, id)` finds the exact four-byte tag
  and complete 32-bit resource ID across supplied archives.
- `parse_material(const ArchivePool&, id)` resolves the GMAT, parses it, and
  verifies the typed GTEX reference across the same pool.

Missing and ambiguous identities fail explicitly. Duplicate pointers to the
same archive do not create an ambiguity. Archive order does not silently
override another archive's resource. The caller must choose its active world
and matching general bank rather than supplying alternative menu/general
variants together.

The original link lookup **0x0044aaf0** compares the referenced resource type
and complete ID against resource records. High resource-ID bits must therefore
remain intact. The checked general-bank resources use IDs in group one
(`0x0001xxxx`), while ordinary course resources use group zero. Stripping the
high bits would join distinct resources incorrectly. The older
`Archive::material_texture()` convenience method requires a local textured
link; world code should use the new material/pool APIs for flat materials and
cross-bank references.

## TEXP and the null-slot case

The GDMaterial reader **0x0043a710** checks format token `0x0130e079`, then
dispatches its nested chunks. `TEXP` begins with a u32 slot count followed by
that many eight-byte resource links `(tag[4], ID:u32)`.

For each slot, reader **0x0044ac30** reads the tag and ID, clears its current
resource through **0x0044abe0(0)**, and resolves a new resource only when the
tag is nonzero. This establishes that a count-one slot with a zero tag is
untextured. The ID does not change that null-tag branch. A nonzero `GTEX` tag
with ID zero is a typed lookup, not the same null representation.

Direct inspection of **ct/ct_t1.WGM, GMAT 0x77** found:

```text
TEXP: count=1, tag=00000000, ID=0
middle COLS RGB: approximately (0.192157, 0.474510, 0.545098)
TRAN: approximately 0.8
TWOS=0, CLAM=0
```

This is a legitimate flat-color material. It must not cause a missing-texture
failure or receive an invented texture. Count-zero `TEXP` materials are also
flat. Multiple texture slots are explicitly unsupported by the new parser;
none occurred in the checked 37-archive inventory.

## Color, transparency, culling and wrapping

`COLS` contains three RGB float triplets. The material reader stores them at
`+0x30`, `+0x3c`, and `+0x48`. Glide state updater **0x00433d40** reads the
middle triplet at `+0x3c..+0x44`, multiplies it by 255, and packs it into the
constant color. `Material.diffuse` exposes this middle triplet directly.

The same updater computes constant alpha as **1 minus TRAN** from material
field `+0x60`. The parser preserves the stored transparency without silently
clamping it. How that constant participates in every backend/draw-flag mode
remains unresolved: the initialized Glide alpha path can be texture-driven,
and draw flags select blend modes. Parsing `TRAN` does not prove that simply
multiplying every textured pixel by `1-TRAN` matches every original draw.

The updater masks `TWOS` with one for culling state. It passes `CLAM` bit zero
and bit one separately to the texture U/V clamp state. The parser therefore
returns `two_sided = (TWOS & 1) != 0` and `clamp_axes = CLAM & 3`. Other material
chunks remain bounded by the archive reader but their rendering semantics are
not invented here. See also `disassembly/wpc-material-state.md`.

## Corpus observations

The user-provided `Wacky.iso` has been extracted as read-only source inputs
under **`build/original-disc/{cc,sh,dt,ct,dr,Win}`**. Revalidation of those 33
WGM files plus four installed files observed **5,532 GMATs**:

| TEXP form | Materials |
| --- | ---: |
| One `GTEX` link, group zero | 4,503 |
| One `GTEX` link, group one | 742 |
| Zero links | 286 |
| One null-tag slot | 1 |

All format tokens were `0x0130e079`. `CLAM` was zero in 5,526 materials and
three in six; `TWOS` was zero in 4,550 and one in 982. All nine COLS components
were finite in the range zero through one in that inventory.

All **3,844 GTEX** records contained `TGAF` and `PALS`; nine additionally had
`TTXA`. Their base TGAF images are available independently of animation.
Frame selection, TTXA playback and its original cadence remain separate work.

The complete current corpus probe reported:

| Parser | Result | Observed failure |
| --- | ---: | --- |
| Scene, including ACTN/SCRT | 37 / 37 | None |
| Material and typed texture references | 5,532 / 5,532 materials | None |
| TGAF decoding | 3,844 / 3,844 textures | None |
| Mesh, initial strict-sentinel probe | 7,005 / 7,010 individual meshes; 36 / 37 complete archives | Five GMDL resources in `dt/dt_t4.WGM` contain exact `0xcdcdcdcd` UV sentinels |

The five rejected mesh IDs are **0x16e, 0x16f, 0x19b, 0x19c and 0x19d**.
There were no other mesh decoding failures. The report is retained at
`build/material-corpus-report.txt`. No planar UV reconstruction or guessed
replacement was introduced. All corpus MATS-to-GMAT references resolve within
their containing archive (zero cross-bank MATS references); pooled loading
still supports typed full-ID references across banks.

Direct inspection shows that all five affected meshes use textured material
**GMAT 0xa2 -> GTEX 0x38**, not a flat-color material. Their corrupt UV vertices
are referenced by triangles: respectively four, four, three, two and two bad
vertices among eight, eight, twelve, four and four vertices. A flat-material or
unused-vertex exemption therefore does not explain these records.

The shared texture is a nonuniform **64x64 indexed image**, with fourteen
referenced palette colors; a constant-texture exemption also does not apply.
All five scene placements are `WGndModel` objects, with model property `0x68`
equal to `0x00480062`. No generated-UV repair path has been established for
these placements.

The original mesh reader **0x0043da50** handles `UVUV` by allocating eight
bytes per vertex, storing that array at resource `+0x2c`, and calling
**0x004297c0**. The latter checks only whether the stream's virtual read can
provide `count << 3` bytes; failure throws `Bad UV.` It does **not** inspect
the float values or reject `0xcdcdcdcd`. That bit pattern represents finite
float **-431602080.0**. Therefore preserving its raw value follows the observed
loader more closely than rejecting it or generating replacement UVs. This
does not prove equivalent Glide/Vulkan sampling at such extreme coordinates.
The five failures reported above described the native decoder's initial
explicit sentinel guard, not a truncation or original-loader rejection.
The decoder now preserves those finite UV values and counts affected source
vertices in `Mesh::uninitialized_uv_vertices`. Revalidation with
`native_mesh_test` accepts **7,010 / 7,010 meshes across all 37 archives**;
`build/linux/mesh-corpus.log` retains that later result. No replacement UVs
were introduced. The count records the source anomaly; it does not establish
original renderer sampling at those coordinates.

The four installed archives remain available and were rechecked with the new
parser and matching menu/general pool pairs:

| Archive | Materials | Textured | Flat | Texture links |
| --- | ---: | ---: | ---: | --- |
| `menu.WGM` | 667 | 665 | 2 | All resolved |
| `SWMenu.WGM` | 673 | 673 | 0 | All resolved |
| `general.wgm` | 350 | 343 | 7 | All resolved |
| `swgeneral.wgm` | 408 | 399 | 9 | All resolved |

Their mesh MATS references are all local: respectively 41, 39, 184 and 244
links. Texture references, unlike these MATS references, include the 742
group-one links identified above.

## Verification

`tests/native_material.cpp` performs **90 synthetic checks** for null slots,
count-zero and typed-zero-ID distinctions, middle-triplet selection, raw
transparency, clamp/culling masks, malformed/truncated fields, NaNs/infinities,
complete group IDs, cross-bank joins, missing references and ambiguities.
Fixtures contain no copied game data.

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Isrc/port \
  src/port/game_assets.cpp src/port/game_material.cpp tests/native_material.cpp \
  -o /tmp/wpc-native-material-test
/tmp/wpc-native-material-test /path/to/menu.WGM /path/to/general.wgm
/tmp/wpc-native-material-test /path/to/SWMenu.WGM /path/to/swgeneral.wgm
```

GCC warnings-as-errors and ASan/UBSan passed. Leak detection was disabled for
the host's ptrace environment. Parsing and reference resolution do not prove
equivalent lighting, transparency modes, texture animation or playable racing.
