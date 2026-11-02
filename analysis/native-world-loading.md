# Native course loading — 2026-10-27

The current X11/Vulkan executable loads a selected original course from the
menu and renders its static geometry. This is an initialization step toward
the game, not playable racing. Sources are `game_files.cpp`, `game_world.cpp`
and `world_view.cpp` under `src/port/`.

## Files and resources

`course_file()` follows original accessor 0x486f90 and its twenty-entry table
at 0x52bef0: environments `cc`, `sh`, `dt`, `ct`, `dr`, with variants `_t1`
through `_t4`. Mode one chooses the corresponding `_b` battle file. All
other modes use the track table. The menu's selected variant remains zero
based. Bounds are checked before indexing.

`game_file()` searches the installed root and optional `--disc`/`WPC_DISC`
root in order. It resolves names component by component, accepts Windows
backslashes, and rejects ambiguous case matches and invalid relative names.
An unavailable course reports a load error and leaves the menu usable.

The selected course and installed `general.wgm` form an exact `(tag, full
32-bit ID)` resource pool. General-bank IDs retain their group-one bits.
Only course-tree placements are collected; drawing all general templates
would place unrelated cars, HUD and menu objects into the world.

## Geometry and rendering

Model property 0x67 links GMDL or GHMS. The loader composes ancestor, local
property-0x1b and mesh MATR transforms, retains per-triangle material IDs,
and uses the recovered static GHMS segment pose. Nested ground models are
included. Material and texture state is described in
[world material loading](native-world-material-loading.md).

Type-zero `WGndSprite` geometry uses serialized pivot, scale and aspect, with
texture H/W when aspect is zero. Current checked placements have zero angle;
nonzero angles fail explicitly until their original rotation convention is
ported. The camera-dependent facing of 0x419b90 is ported (see
`sprites-20261029.md`). `GD3dSprite`, runtime
power-ups, animated objects and other class-specific draws remain incomplete.

The course viewport supplies the clear color. Its serialized camera handle
is zero in the checked courses. Until the shared race/follow-camera lifecycle
is ported, the first serialized WTrackCamera supplies a static initial view.
The loader exposes this distinction rather than claiming a runtime binding.
Projection uses recovered 0x435410 with original FOV, integer ratio, near and
far planes. The shared general-bank cameras/actions are not roadside cameras.

Vulkan draws use perspective UV interpolation, depth testing, zero-alpha
discard and per-material U/V addressing. Flat materials use a constant-color
primitive. Textured lighting, draw flags, TWOS culling, transparent ordering,
billboarding and animated materials still need original-state translation.
The captured Crazy City course has visibly flat gray surfaces; it is not a
matched reproduction of the original renderer.

The frame vertex allocation grows before frame recording, waits for existing
GPU submissions before retiring old storage, and preserves the old allocation
if growth fails. World uploads and destruction occur outside begun frames.
Escape currently releases the static view and returns to the course menu;
this temporary path does not implement the original race pause lifecycle.

## Verified evidence

- Six display-free CTest suites pass, including 84 world/path checks and
  90 material checks. World fixtures exercise cross-bank mesh materials,
  full IDs, null-tag flat materials, nested/resource transforms, affine
  inversion, sprite pivot/scale/aspect, camera links, missing references,
  filename tables and case-sensitive Linux filesystem behavior.
- The extracted ISO and four installed archives contain 7,010 meshes across
  37 archives; all now load. Five textured desert meshes contain finite
  `0xcdcdcdcd` UV values. They are preserved literally, as original reader
  0x43da50 -> 0x4297c0 does. No planar UV replacement is generated. Extreme
  coordinate sampling equivalence with Glide remains unverified.
- All 20 `_tN` worlds prepare geometry, materials, textures and a static camera
  with `native_world_test`. All five `_b` worlds lack a serialized camera and
  cannot yet complete the current static-view initialization. Evidence:
  `build/linux/world-corpus.json`, `build/linux/mesh-corpus.log`.
- Crazy City 1: 410 models, 88 sprites, 670 batches, 30,213 expanded vertices,
  44 textures, static camera 0x4. Crazy City 2: 398 models, 121 sprites,
  624 batches, 32,259 vertices, 61 textures, static camera 0x43.
- Native input smoke selects mode zero, one player, Dingo enum three,
  environment zero and variant one. It loads `cc_t2.WGM`, captures textured
  world output, releases it, loads it again, returns through retained menu
  cursors and quits through default-No/Yes confirmation.
  `build/linux/menu-smoke/course-world.png` and `navigation.log` retain this
  observed run. It verifies loading/presentation/lifetime, not gameplay.
- The 71-frame GPU integration test passes on NVIDIA RTX 4070 under Xvfb,
  including a 150,003-vertex draw beyond the original four-MiB allocation,
  in-frame growth rejection and final presented pixels.

Race participant creation, collision queries, follow-camera execution,
vehicle simulation, AI, lap/rank/timer/HUD/results and audio remain required
before this can be a playable native port.
