# Native menu model rendering

`src/port/menu_models.cpp` connects original menu scene models to the native
Vulkan textured-triangle interface. It uses decoded asset geometry and
serialized transforms. Meshes, expanded triangle batches and material textures
are prepared before a frame; drawing changes the matrix and submits cached
vertex spans without decoding files or uploading textures.

## Asset and transform observations

Both `GDModel` and `GDHModel` use property **0x67** for their typed `GMDL` or
`GHMS` resource link in the installed menu. Some `GDModel` nodes are containers
with an eight-byte zero link; their transforms and descendants remain relevant.
Treating a null container link as a missing mesh would discard its children.
Property `0x63` is not used for the observed GHMS resource links.

The renderer composes property **0x1b** along the complete BRTR ancestor path.
Its 48 serialized bytes contain three XYZ basis columns and an XYZ position,
without the reserved runtime word in the 52-byte `WpcAffine` view. The resource
`MATR` is then composed with the resulting node transform. All 39 installed
menu meshes have identity `MATR`; the parser preserves the matrix and its flag
word independently.

`load_mesh` applies the recovered GHMS static segment mapping and hierarchy
before the scene transform. The map overwrite order and per-TSET source sharing
follow **0x00437040** and **0x00436ff0**; static parent translations follow
**0x004385c0**. See `ghms-segment-pose-disassembly-20261023.md`; the map fill and
TSET rewrite are interleaved per segment (corrected 2026-10-27). `WRot` type-2
rotation is applied; MTNL motion is not. See
`menu-character-selection-reverse-20261027.md`.

Direct models under a page are common to its selections. Model descendants of
each direct menu item receive that item's selection index. The renderer draws
only the selected item's group alongside the common models. For page `0x1c`,
the common container `0x31` contains the original `GMDL 0x7ca` and `0x7c9`
planes, using materials `0x2b4` and `0x2b3`. These planes are included with their
serialized transforms, not replaced by a fitted background.

## Camera source and projection

The earlier `menu-camera-link-20261102.md` walk searched BRTR alone and reported
no serialized viewport. Including the separate **SCRT** tree finds `GDScreen 3`
with `GDViewport 0x25` and `0x3a`. Their property **0xc4** camera references are
**2** and **0x3c**, respectively. This directly establishes serialized links;
it does not prove which viewport the original runtime binds on every page.

The native model renderer selects the first camera-bearing GDViewport in SCRT,
which is `0x25 -> 2` in this archive. It composes and inverts that camera's
actual serialized affine, retaining scale/shear rather than assuming a rigid
transpose. Its projection uses the camera's serialized properties:

| Property | Meaning | Installed camera 2 |
| --- | --- | --- |
| `0x2c` | FOV, f32 radians | `1.04719758` |
| `0x2d` | Ratio, **integer** | `100` |
| `0x2e` | Near plane, f32 | `2` |
| `0x2f` | Far plane, f32 | `20000` |

The matrix formula from **0x00435410** is:

```text
q = sqrt(ratio * 0.0075)
P00 = q / tan(FOV / 2)
P11 = 1 / (q * tan(FOV / 2))
P22 = far / (far - near)
P32 = -(far * near) / (far - near)
P23 = 1
```

The Vulkan adapter negates projected Y and scales clip X/Y to a centered
640x480 letterbox in the client area. Original positive-Z depth and perspective
W are retained. No bounds fitting, guessed rotation, or replacement chase
camera is used. The original renderer's viewport-binding path is
**0x004ae060 -> 0x0044a1f0**, while **0x004354a0** reads active viewport/camera
state. A runtime observation must still confirm the active binding on the car
page and the second viewport's two-player composition.

## Verification and boundaries

A mock rendering backend exercised the current installed menu with all seven
selections on each of pages `0x1c` and `0x3d`. Both pages prepared **38 batches
and 13,068 expanded vertices** across all selections. Each selected group
submitted seven batches, except Turbo Terrific's eight. Ten material callbacks
were shared across both pages; instrumented drawing invoked none of them.
Every projected vertex calculation was finite. This verifies preparation,
selection separation and dispatch, not displayed pixels or a matched original
frame. The model source compiles with `-Wall -Wextra -Werror`.

The resource parsers separately passed their synthetic mesh and scene suites
and installed-asset checks. The Vulkan renderer supplies perspective UVs,
depth testing and zero-alpha discard. This module leaves material render-state
selection, original lighting, animation scheduling, dynamic visibility, and
two-player viewport activation for further recovery. Material callback errors
and absent GPU textures remain explicit failures; no arbitrary texture fallback
is synthesized here.

## Material addressing and displayed verification

The material loader at **0x0043a710** reads `CLAM` into material offset
`+0x70`. The Glide state updater **0x00433d40** passes its low bit and
next bit independently as S/T clamp modes to `grTexClampMode`. The original
[Glide header](https://github.com/sezero/glide/blob/master/glide3x/h3/glide3/src/glide.h)
defines zero as wrap and one as clamp. The native adapter therefore keeps
four sampler/descriptor combinations for each shared texture image and
selects them per draw using the material's low two CLAM bits.

The native X11 pixel test verifies all four modes using out-of-range UVs on
one shared two-by-two texture. It also verifies near/far depth ordering,
zero-alpha fragments not writing depth, and 2D UI drawing over 3D.
`tools/smoke_native_menu.py` captures the actual 640x480 Dingo selection page
under `build/linux/menu-smoke/dingo-vehicle.png`. This is displayed native
evidence, not an original-frame match. The model's composition/orientation,
original lighting, motion and runtime camera state still need comparison.
