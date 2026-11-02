# Native Linux port progress — 2026-10-27

The current executable is a Linux x86-64 ELF using X11 and Vulkan 1.0.
It presents original startup cards, navigates menus, draws vehicle previews,
and loads/displays the selected original race course. It does not execute
WPC.EXE or use Wine at runtime. **Racing is not playable.**

The previous goal turn made concrete progress on navigation and textured
models. This turn connects course selection to original world resources and
verified native presentation. The full native-game objective remains active.

## Verified current state

- Configure/build in `build/linux`; six CTest suites pass: 332 asset,
  190 scene/string, 18 menu, 147 mesh, 90 material and 84 world/path checks
  (**861 synthetic checks**). The world suite also passes ASan/UBSan with
  representative original Crazy City, Creepy Town and Desert 4 archives.
- WGM ACTN uses ACCM classes, while BRTR/SCRT use BRCM. All 37 installed/disc
  archives parse, including actions and screens. The installed menu has 991
  bricks, 23 actions and three screen nodes. All seven string tables load.
- Menu traversal retains cursors and original selection enums, respects base
  availability/one-way/back behavior and defaults Quit to No. Specialized
  multiplayer, championship, profiles and media callbacks remain incomplete.
- `--disc DIR`/`WPC_DISC` joins the installed-file root for course lookup.
  The original 5x4 filename table selects race courses; battle uses `_b`.
  Windows names resolve component by component on Linux without regard to
  case. Missing course files leave menu navigation available.
- Exact full-ID resource joins preserve general-bank group-one IDs. Null-tag
  and count-zero materials remain intentionally untextured. All 5,532 GMATs
  and all 3,844 base TGAFs across 37 archives parse and resolve.
- All 7,010 GMDL/GHMS meshes across those archives load. GHMS uses recovered
  static segment mapping and parent translations. Five Desert 4 meshes retain
  finite CDCDCDCD UVs literally, following the original bounded byte-copy
  reader. No invented planar UVs replace them. Their extreme sampling remains
  unverified against the original renderer.
- All twenty race courses prepare model geometry, type-zero WGndSprite quads,
  materials, textures and a static serialized track camera. Course 1 has 410
  models/88 sprites/30,213 expanded vertices/44 textures; course 2 has
  398 models/121 sprites/32,259 vertices/61 textures. General templates are
  resolved as dependencies rather than all being drawn as world placements.
- X11 input smoke selects one-player race, Dingo enum 3, environment 0 and
  course 1 (cc_t2). It loads and captures the world, returns, loads it again,
  releases its textures and returns through retained menu cursors. Quit No
  and Yes still work. Escape from the world is a temporary return path.
- The 71-frame NVIDIA RTX 4070/Xvfb renderer test passes known pixels,
  alpha/clipping/depth/zero-alpha discard, all four U/V addressing modes,
  resize, input and close. A 150,003-vertex draw verifies allocation growth
  beyond four MiB while preserving submission lifetime rules.
- The latest 420-frame startup capture still matches both original BMPs pixel
  for pixel and verifies their two-second transitions.

Current evidence artifacts (generated, ignored):

- `build/linux/world-corpus.json`, `mesh-corpus.log`.
- `build/material-corpus-report.txt` (initial sentinel guard findings;
  `mesh-corpus.log` contains the later full-mesh pass).
- `build/linux/menu-smoke/course-world.png`, page captures and navigation log.
- `build/linux/main-menu*` startup captures/log.
- `build/linux/native-verification.json` source and executable hashes.

## Next work toward the complete game

1. Port original race bootstrap: WModManager is an action, not necessarily a
   BRTR class. Resolve its selected WMOD route row, choose the linked
   WStartLine, create participants from original general-bank templates,
   apply original grid offsets and ground queries, initialize shared
   WFollowCamera_2 actions and bind their cameras to course viewports.
2. Port collision, vehicle update/controls, AI, route/lap/rank state,
   countdown/clock, HUD, finish/results and return to menus. Existing recovered
   routines and older C-port research do not mean those systems execute here.
3. Complete specialized menus, profiles/settings, dynamic unlocks, independent
   two-player cursors/readiness and championship progression.
4. Implement audio, movie/demo/credits playback and persistence; verify
   representative complete races and modes against original behavior.

## Fidelity gaps

The world currently uses a static roadside camera. Course viewports contain
zero serialized camera references; the original initializes and binds shared
follow cameras at runtime. Five battle arenas have no serialized camera and
cannot yet complete the current static-view initialization. Ground-sprite
billboarding, GD3dSprite/other class-specific draws, lighting, TWOS culling,
render flags, transparent ordering and animated materials remain incomplete.
The course capture has visibly flat gray surfaces and is not a matched frame.

Menu previews also need original camera activation, lighting, WRot/MTNL
animation and dynamic visibility. Character timing, road entry/wrap, indexed
intermediate alpha, selection behavior and some layout details remain
provisional. The current build is a partial port, not a complete game.
