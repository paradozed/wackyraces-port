# TTXA animated texture observations

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

## Binary layout evidence

The installed track WGM corpus has nine `GTEX` resources with a nested `TTXA`
chunk. Each was checked against its sibling `TGAF` indexed image. The observed
TTXA payloads have a 20-byte header followed by a u32 frame-index table,
fixed-size indexed images, and a u32 sequence table. Their extents match
`20 + data_frame_count*4 + data_frame_count*frame_size + sequence_count*4`.
Both tables contain in-range indices in all nine samples. In every sample,
the first indexed image in TTXA is byte-identical to the TGAF raster.

| Archive | GTEX | Image dimensions | Frames | Indexed bytes per frame |
|---|---:|---:|---:|---:|
| `cc_t1.WGM` | `0x42` | 128x64 | 11 | 8192 |
| `cc_t2.WGM` | `0x62` | 128x64 | 11 | 8192 |
| `cc_t2.WGM` | `0x65` | 8x8 | 9 | 64 |
| `dr_t1.WGM` | `0x3c` | 128x64 | 11 | 8192 |
| `dr_t2.WGM` | `0x69` | 128x64 | 11 | 8192 |
| `dt_t3.WGM` | `0x85` | 128x64 | 11 | 8192 |
| `sh_t1.WGM` | `0x7b` | 128x64 | 11 | 8192 |
| `sh_t2.WGM` | `0x76` | 128x64 | 11 | 8192 |
| `sh_t3.WGM` | `0x66` | 128x64 | 11 | 8192 |

For example, `cc_t1.WGM` `GTEX 0x42` has TTXA header words
`(11, 11, 71, 8192, 0)` and payload size 90,220 bytes. The two index arrays
are both `0..10`; its first frame exactly matches the TGAF pixels. Rendering
the indexed frames with the sibling TGA palette produces changing cloud
shapes. A contact sheet is saved as
`ttxa-cloud-frame-sheet-20261023.png`.

The first and second header words are treated as sequence and image counts
because they size the two arrays and match the executable's allocation and
lookup operations. The fourth word matches the indexed frame byte count. The
third word (`71`) and fifth word are not assigned semantic names here. Static
analysis shows WPC builds texture handles from the frame data and palette,
then selects handles through a sequence table. The original frame cadence has
not been recovered.

## Native implementation

`c-port/wgm_anim.c` validates the TTXA dimensions, counts, exact payload span,
both index tables, indexed TGAF palette layout, and first-frame consistency
before creating SDL surfaces. `c-port/world_scene.c` switches animated
materials during race drawing. It currently uses a 100 ms frame interval as a
visible provisional cadence; this is an implementation estimate, not a
recovered WPC timing value. Race time pauses with the native simulation.

The cloud texture is referenced by 21 ground-model instances in `cc_t1.WGM`,
so this adds its serialized moving-cloud images to the course. Other scene
geometry, horizon materials, draw ordering, and original animation scheduling
still need comparison against WPC.
