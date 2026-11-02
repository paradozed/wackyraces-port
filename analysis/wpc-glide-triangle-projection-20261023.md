# Glide triangle projection and texture coordinates

This note records static evidence from `wpc.exe`'s software-side mesh
projection routine. It is not runtime evidence and does not yet establish that
the native SDL sampler uses the same effective texture origin.

## Observed in `FUN_004354a0`

The function transforms three input vertices, projects them to the active
viewport, and packs Glide-compatible vertex records before calling the
dynamically loaded `grDrawTriangle` entry at `DAT_0057e690`. The record fields
are consistent with the Glide `GrVertex` layout: screen x/y, depth and color,
followed by perspective texture coordinates and reciprocal depth.

For texture unit `param_1[0x64b0]`, the function writes the source S value
multiplied by the configured S scale and by reciprocal camera depth. It writes
the complement of the source T value, also scaled and depth-weighted:

```c
sow = local_150 * s_scale * reciprocal_depth;
tow = (1.0f - fStack_14c) * t_scale * reciprocal_depth;
oow = reciprocal_depth;
```

The variable names above are descriptive labels for the recovered operations;
Ghidra's local names are not source names. This proves that the executable
complements T before submitting each vertex. It does not by itself determine
the effective top/bottom orientation, because that also depends on how the
texture decoder/upload path lays out rows. `FUN_0044e0f0` computes a pointer
to stored pixel bytes from the texture header without reordering them.
`FUN_0044efc0` and `FUN_0044e950` copy source rows in ascending order, with no
vertical reversal in those stages. The installed TGAF for `GTEX 0x5e` has a
bottom-origin TGA descriptor. WPC therefore submits the original bottom-up
pixel rows and complements source T (`1-T`) before Glide sampling. The native
decoder converts bottom-origin rows into SDL's top-left row order and samples
source V directly. These paths are equivalent by inference: source `V=0`
addresses the top row in both. Glide's
[3.0 Programming Guide](https://www.bitsavers.org/components/3dfx/Glide_Programming_Guide_3.0_199806.pdf)
defines the texture coordinate origin at `(s,t)=(0,0)` and the first texel
center at positive half-texel coordinates. The equivalence also agrees with
the earlier preview
experiment, where an additional native V inversion made the image worse. The
texture-orientation path does not explain the Dingo preview fragmentation; no
renderer change is needed here.

## Relevance to the vehicle preview

`GHMS 0x7ae` and its `GMAT 0x301` texture load successfully. The preserved
`Dingo UV atlas overlay` shows the
decoded UV triangles over the 256x256 atlas. The atlas chart is complex and
contains long cross-region triangles. Given the matched origin convention,
those links are part of the serialized UV mapping rather than evidence of a
vertical-orientation bug. The Dingo item `0xb3` has no `MTNL` link; its missing
tower/driver appearance is not attributable to a missing animation resource
on that item.

## Camera-to-viewport path

Static call-path inspection shows that `FUN_004354a0` obtains the active camera
from the current viewport at offset `+0x186`. It calls `FUN_00435410` to build
the perspective matrix from that camera and `FUN_00435290` to build its view
matrix from the engine's transform stack. The viewport binding routine
`FUN_0044a1f0` stores the viewport at graphics-context offset `+0xbc`; when the
viewport has a camera, it pushes the camera transform through the graphics
backend's virtual method at `+0x50`. This explains why directly passing the
serialized 12-float camera matrix to the native preview is not a full port of
WPC's camera path: WPC composes it with the current transform stack and then
uses a camera-specific projection matrix.

The two root cameras on page `0x1c` are candidate viewport cameras, but the
page-to-viewport binding and exact camera selected during the car preview are
not yet identified. The native selection renderer still draws the car into a
fixed screen-space viewport with a fitted local camera. No page camera has
been wired into the C renderer.

## Next evidence needed

The texture orientation is accounted for. The next task is to recover which
page camera and projection scale WPC applies to the vehicle group, then compare
the resulting transforms and same-state native/Wine crops. The remaining
unknown is model/camera placement and projection, not TGA row order.
