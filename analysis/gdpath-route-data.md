# GDPath data in course archives

## Direct observations

The scene reader resolves BRTR class names from the installed CLAS table. A
bounded survey of all 33 `.WGM` files on the mounted ISO found path records in
ten archives. The 20 course archives named `_t1.WGM` through `_t4.WGM` contain
either class in nine files; `cc_b.WGM` supplies the tenth.

| Archive | Class / object ID | Property `0x7c` count(s) | Value length(s) |
| --- | --- | ---: | ---: |
| `cc/cc_b.WGM` | `GDPath` `0xc` | 2 | 28 bytes |
| `cc/cc_t1.WGM` | `GDPath` `0x11` | 2 | 28 bytes |
| `cc/cc_t2.WGM` | `GDPath` `0x1c` | 2 | 28 bytes |
| `cc/cc_t4.WGM` | `GDPathQ` `0x195` | 4 | 52 bytes |
| `ct/ct_t2.WGM` | `GDPathQ` `0x1e9` | 16 | 196 bytes |
| `dr/dr_t2.WGM` | `GDPathQ` `0x3d6`, `0x3d7` | 2, 2 | 28, 28 bytes |
| `dr/dr_t4.WGM` | `GDPathQ` `0x15` | 4 | 52 bytes |
| `dt/dt_t1.WGM` | `GDPath` `0x21f`, `0x371` | 2, 2 | 28, 28 bytes |
| `dt/dt_t4.WGM` | `GDPath` `0x39` | 39 | 472 bytes |
| `sh/sh_t3.WGM` | `GDPathQ` `0x7e`; `GDPath` `0x270`–`0x274` | 18; 6, 6, 7, 7, 6 | 220; 76, 76, 88, 88, 76 bytes |

Every listed property value has the observed shape `u32 count` followed by
exactly `count * 3` little-endian float words. For example, `dt_t4` has 39
triples and ends with X `15915.904`; `sh_t3` has five separate objects, each
with six or seven triples. Several Y values in `dt_t4` and `cc_t1` are zero,
while the `sh_t3` coordinates span varying elevations. These are byte-level
observations; the meaning of the triples has not been confirmed.

The executable's reflection setup provides a class-specific link: registration
stub `0x00444190` associates `GDPath` with member `m_Points` at object offset
`+0x84`; stub `0x004441f0` registers `m_bClosed` at `+0x90`. In the course
files, `GDPath` property `0x7c` has the counted point-array shape, and adjacent
property `0x7d` is four bytes (zero in every surveyed path). This strongly
supports those property/member pairings, although the serializer's property-ID
lookup has not been traced end to end. The native reader retains `0x7d` raw
instead of assuming its boolean encoding.

`GDPathQ` is a distinct reflected class present in five archives. Its property
`0x7c` uses the same count-plus-triplets length pattern. In `sh_t3.WGM`, object
`0x7e` has 18 points and a composed node origin of
`(176973.844, 5120.041, 38.379)`. The shared property IDs suggest a related
point-list representation, but inheritance and the queue's behavior have not
been proven. The same file has a `WLogRide` object (`0x7c`) and a `WLogJam`
object (`0x267`). No direct reference from either object to `GDPathQ 0x7e` has
been established, so a relationship to the ride is only a possibility.

The same `sh_t3` archive contains nine `WLogBase` and six `WLogModel` nodes
with `GMDL` links in property `0x67`. The native scene reader now loads those
models at their serialized initial transforms; the inspector reports 221 model
placements for that archive. `WLogRide` and `WLogJam` do not carry a `GMDL`
link in their records, and the six `WLogModel` meshes have no recovered update
or path association yet. Other archives pair `GDPathQ` with a `WLogModelQ`
node whose `0x67` GMDL link is null, which is additional evidence that these
objects belong to an animated model system, but it does not identify the path
consumer.

The inspector applies each composed node transform to the raw points and
queries the track's ground surfaces within a 10,000-unit vertical window. In
`sh_t3`, transformed `GDPathQ 0x7e` points all obtain a ground sample (18/18)
with a mean vertical gap of 42.192 units; its five `GDPath` sets have 6/6 or
7/7 samples and mean gaps of 248–399 units. In `dt_t4`, all 39 points sample
ground with a mean gap of 154 units. Other archives produce gaps from about 5
to 2,849 units and one two-point path has no ground samples. This supports
spatial alignment between at least some paths and course terrain, but the
10,000-unit search window, approximate ground sampler, and unverified transform
convention make it insufficient to classify the paths as racer routes.

The executable registers reflected names `GDPath` and `GDPathQ`. The data
supports interpreting property `0x7c` as a counted XYZ-like sequence, but the
coordinate frame, path direction, ownership, and link between the classes are
still unknown. Archives without either class may use another route resource or
may not require one. There is not enough evidence to use these values as
opponent routes or ordered lap checkpoints.

## Native port

`wpc_world_scene_load` recognizes `GDPath` and `GDPathQ` properties `0x7c` and
`0x7d`, checks the point count and exact record length before allocation, and
rejects non-finite or out-of-range point coordinates. It retains the raw triples,
composed node transform, and raw `0x7d` value through
`wpc_world_scene_paths`; `--inspect-wgm` reports each path's count, node origin,
serialized endpoints, transformed endpoints, and exploratory terrain proximity.
No gameplay behavior is derived from them yet.
