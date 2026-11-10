# Wacky Races for Linux

The goal is a native Linux x64 port of the 2000 PC game. The native executable
displays the original startup cards, navigates original menu pages and renders
textured vehicle previews, and races the four courses of Championship I
(`cc_t1`, `sh_t1`, `dt_t1`, `ct_t1`) with a single player. Movies, menu and race
sound effects, voices and music play. **AI opponents and power-ups are not
fully ported.** Build and run instructions are in the [native port
section](#native-linux-port). Original game assets are required and are opened
read-only.

`analysis/` contains dated reverse-engineering notes for the native port
(recovered addresses, formulas, what was and was not verified). The Ghidra
project, decompilation exports and disassembly dumps are machine copies of the
game's code, kept locally and ignored by git (no game content in git).

## Native Linux port

The x86-64 Linux executable uses X11 and Vulkan 1.0. It reads the original
`menu.WGM`, `general.wgm`, course WGMs, `STRINGS.DAT`, `l2.bmp`, and `l3.bmp`
directly. It also reads a few interface strings that exist only in the
installed `WPC.EXE`, as data; without it, those lines are skipped. No Wine
process, PE execution, screenshots, or extracted replacement artwork are used
by the native runtime. The original files are not bundled with the project.

### Build and run

Requires CMake, a C++17 compiler, X11 development files, the Vulkan loader
library, and a working Vulkan driver. Vulkan headers and precompiled shaders
are included. libavcodec (FFmpeg) is optional, found through pkg-config. It
decodes the Indeo 5 movie bitstream; without it the movies are skipped.
libpulse-simple is optional too. It sends sound to PulseAudio (or PipeWire's
pulse server); without it the game runs silently. `WPC_AUDIO=silent` mutes a
build that has it, and `WPC_AUDIO_CAPTURE=out.wav` records the mixed output
instead of playing it.

```sh
cmake -S . -B build/linux -DWPC_NATIVE_PORT=ON
cmake --build build/linux -j
./build/linux/wackyraces --data "/path/to/Wacky Races" --disc "/path/to/disc/root"
```

To avoid passing the paths every time, put them in a config file. The game
reads `~/.config/wackyraces/config.ini` (or `$XDG_CONFIG_HOME/wackyraces/`,
`$WPC_CONFIG`, or `--config FILE`); copy
[`config/wackyraces.example.ini`](config/wackyraces.example.ini):

```ini
[paths]
data = ~/.wine/drive_c/Program Files (x86)/Infogrames/Wacky Races
disc = /run/media/you/Wacky   # where the CD is mounted

[game]
language = 3
skip_intro = true             # open the menu directly (as --skip-startup)

[video]
resolution = 1280x960         # window size (as --resolution); default 640x480
fullscreen = true             # full screen (as --fullscreen, WPC_FULLSCREEN=1)
```

On the options page, left/right on JOUEUR 1/2 picks the keyboard or a Linux joystick
(`/dev/input/jsN`, shown as number:name), as the original does; `[input] joystick1` / `joystick2`
(or `WPC_JOYSTICK1` / `WPC_JOYSTICK2`) set it at start. In races the stick steers and accelerates or brakes past half
deflection, and the buttons follow the original's joystick defaults (4 accelerate, 5 brake,
0 camera, 1 rear view, 2 gadget, 3 weapon). The keyboard keeps working beside it.

The menu options (volumes, METEO, POUSSIERE, RETOUR DE FORCE and the control
table) are kept as the original's `MenuPrefs` record in
`$XDG_CONFIG_HOME/wackyraces/MenuPrefs.bin`, read at start and written at exit.

Command-line options override the environment variables below, which override
the config file. Unknown keys are reported with their line. At startup the log
names the config file and where the data and disc paths came from.

`WPC_DATA` supplies installed files and `WPC_DISC` supplies the disc root.
Course files are searched in both, installed directory first. Asset paths are
matched component by component without regard to case, including original
Windows separators. If using the user-provided ISO in this workspace, extract
its course directories into the ignored build directory:

```sh
mkdir -p build/original-disc
bsdtar -xf Wacky.iso -C build/original-disc cc sh dt ct dr Win Movies Music
```

Then use `--disc build/original-disc`. Original assets and the ISO remain
untracked. The default language is English (`0`); `--lang 0..6`
or `WPC_LANG` selects another original language. Use `--lang 3` for French.

Startup follows `0x4acad0`. `legal.avi` plays first (installed directory),
then L2 and L3 show for two seconds each, then the disc's `Movies/intro.avi`,
then the main menu. A movie shows the frame due at
`elapsed × rate / scale`, ends after its last frame, and Escape skips it.
The intro (320×208) is centred in a 320×240 surface that fills the 4:3 area,
as the original's display-mode change does. SEQUENCE D'INTRODUCTION in the
main menu replays it. `legal.avi` plays its own soundtrack and the intro plays
`Voice/iaudio.avi`; the soundtrack position clocks the frames. See
[movies](analysis/native-movies-20261027.md) and
[audio](analysis/native-audio-20261028.md).
The menu plays `Music/Cc_t1.snd` and each item's spoken name (language bank
of `Menu.GSW`) when the cursor moves onto it. EFFETS, MUSIQUE and VOIX set
the effect, music and voice gains. Races play the course's music, the
engine loop (pitch from speed), the narrator's 3-2-1-GO, wall hits, the
drowning splash and the "took the lead" voice line.
`--skip-startup` opens the menu immediately. Up/Down
select list items; Left/Right select vehicles and adjust implemented slider state. Enter accepts and Escape returns through the recovered node graph.
The Quit confirmation starts on No, as in the original. The window close button
also exits. Some specialized actions remain pending, as described below.
`--frames N` exits after N presented frames for bounded runtime checks.
`--race COURSE.wgm` (for example `sh/sh_t1.wgm`, with `--vehicle 0..6`, and `--vehicle2 0..6` for two players)
starts a single race on a course archive, skipping the startup and menu.
`WPC_RACE_PLACE=x,z,yaw` then puts the player at a world position, for
screenshots of a given spot. `WPC_MENU_MOTION=N` (debug) plays MTNL clip `N`
on the car-selection previews that have one; the original never starts a clip
there ([MTNL motion](analysis/mtnl-motion-20261028.md)). `WPC_PLACE=x,z,yaw` (debug) puts player 1 there at the start of a `--race`, for captures. `WPC_WEAPON=11..25` (debug)
gives player 1 that weapon with a full charge, and `WPC_GADGET=0..10` (debug) that gadget. `WPC_HIT=TYPE,UPDATE` (debug) hits player 1 with
that WackyAutoBomba type (e.g. `0x80`) at that race update.

### Implemented and remaining work

- Bounded WGM chunk/resource lookup, BMP and embedded TGAF decoding, material
  texture links, BRTR scene trees, and original language-table decoding.
- Original banners, road vehicles (`WSpriteMove`, `0x492f80`: 3 units an
  update, back to x 320 past −380), character compositions, localized page
  labels, item visibility and multiple original fonts. The main-menu characters
  follow the recovered `WMenuAnimation` controller (`0x4942b0`) and `GDFilm2`
  player (`0x430110`) at 30 updates per second: random claw gestures and laughs,
  and Satanas talking in step with each item's voice, which waits 20 updates as
  in the original ([menu character animation](analysis/menu-character-animation-20261028.md)).
- Menu graph traversal, retained page cursors, default-No quit confirmation,
  player/mode/vehicle/environment/course selection and a retained race request.
  Championships follow the recovered order (`0x48de20`, `0x4928f0`,
  `0x491cf7`): Championship *N* races the *N*th variant of `cc`, `sh`, `dt`,
  `ct` in turn, so Championship I is `cc_t1`, `sh_t1`, `dt_t1`, `ct_t1`.
  Finishing a race and pressing Enter returns to PISTE SUIVANTE with the next
  race; Escape abandons without advancing. Points, unlocks and the post-race
  pages follow the original. A won championship plays its winner scene
  (`Win/Win*.WGM`, `WWinnerMovie` `0x494be0`, [championship](analysis/native-championship-i-20261028.md)),
  then DickOff after Championship IV, or the outro and credits after
  Championship V. Profiles and saves, demos, replays, credits and the
  keyboard controls page are ported, as is practice (ENTRAINEMENT: alone on
  the grid, lap splits, the course record and the finish state 0xf) and two players (page 61's
  cursors, then a split-screen race without robots;
  [two players](analysis/two-player-20261101.md)), including the two-player
  battle on the `*_b.wgm` arenas (hits received, the end at five;
  [battle](analysis/battle-20261102.md)).
- Racing on the Championship I courses `cc_t1`, `sh_t1`, `dt_t1` and `ct_t1`:
  WMOD route sections, WGNR ground in each brick's frame, the rigid loop
  transform (a translation on `cc_t1`/`ct_t1`, a rotation on `sh_t1`/`dt_t1`; across the seam each
  section, each car and each weapon is drawn in one place, as the original's group table `0x5472d0` picks;
  projectiles, TNT and storms cross the seam through the transform,
  [note](analysis/section-culling-20261107.md)), section culling
  (only the player's section and the two beside it are drawn, as the original's `0x4a8680` enables
  each row's brick and linked nodes; [note](analysis/section-culling-20261107.md)),
  `m_nLapNum` laps, the PRET!/PARTEZ! countdown, the recovered WackyAuto
  speed/steering/motion equations, drowning in the deep river, the follow
  camera (`WFollowCamera_2` `0x478070`: spring, wall and ceiling rays, the
  Return view toggle, the '.' rear view, the near mode and the swing before
  the countdown; [camera](analysis/follow-camera-20261102.md)), viewport fog and backdrop, and the recovered one-player HUD: lap,
  position (the WModManager route cursor, `0x4a3937`) and suffix, timer, speedometer, bend arrows, the power-up gauge and
  the vehicle's gadget/weapon icons, and the racer progress strip
  ([HUD](analysis/native-hud-20261028.md)). Power-up pickups spin on the
  course; taking one adds a charge and runs the gadget or weapon roulette, and
  the MenuPrefs keys ('/' gadget, right Shift weapon by default; OPTIONS > JOUEUR 1 rebinds them) use it; the item effects are below
  ([power-ups](analysis/native-powerups-20261028.md)). Up/Down accelerate and brake/reverse, Left/Right steer, Escape
  leaves the race, and Enter after the finish returns to the menu. Opponents
  follow the recovered `WCtrlRoboCar` driver along the WGNR route layer
  ([RoboCar AI](analysis/robocar-ai-20261028.md)); they touch other cars, take
  and use items, and away from the player slide along the route (laps within
  about 5% of the original's on `cc_t1`); outside the player's previous, current and next
  sections they are hidden and run on along the route rows until they show again at a
  section's entry portal (`0x4a1c8b`, dt_t1), and steer round the car ahead to the side with room (`0x4e69bd`, road
  edges `0x468890`; [avoidance](analysis/robot-avoidance-20261030.md)). The turbo (gadget 8) and the shield (gadget 10) work. The missile, breath, tank gun and boomerang fire guided projectiles, the TNT
  bounces towards a target, the storm cloud strikes cars along the route, the
  detour sign turns a car round, and the landmine (icon 19) drops behind the
  car and arms after 15 updates. Holding look-back while using a weapon fires it backward, as the
  original's `+0x5c` flag does: projectiles and the TNT leave behind, and the storm follows the route
  back (not compared with Wine). A dropped weapon takes the section of the slot row whose box holds
  it (`0x4a6d20`). A car that is hit is held
  for 25 updates (`WackyAutoBomba`, then a doubled acceleration;
  [weapons](analysis/weapons-bomba-20261029.md)); a TNT or mine hit fades the car to black and
  raises a smoke column (`flamesmoke`), a boomerang hit circles it with stars, a tank shell buries it in a
  pulsing mound that bursts into chunks (the cars' bodies also sway with their accelerations,
  GDHModel controller 0 with the per-vehicle gains, matching Wine frame for frame; the
  tower car's own gadget spreads two flapping wings, and vehicle 6's unfolds a spinning rotor
  that folds back at the 70th update, vehicle 5's opens a parachute, the bodies play their gadget clips, MonsterTires grows four big tyres, the wheels roll and the front ones steer, the tank's turret turns to its shot, the boot kicker's arm stretches out and kicks, each vehicle's flight ending
  by the original's rule, not compared with Wine; TurboSpeed and FlyPower light the
  WFire jet flames of vehicles 0, 2, 5 and 6, [WFire](analysis/wfire-20261109.md)), and paint leaves a blue
  pool under it (all compared with Wine; [hit visuals](analysis/hit-smoke-20261107.md)). Missiles and
  the breath trail WRocketTail's fins and glow, and the breath flies as its fireball film
  ([tails](analysis/rocket-tail-20261108.md)). Every weapon icon now has an effect, some provisional, and FlyPower and
  MonsterTires only partly (see `TODO.md`). On dt_t1 the level-crossing tram runs on its light cycle and the
  shuttle tram on its path; both squash the cars they touch, and robots wait at the red light
  (compared with Wine; like the original, each runs only while the player is in or next to a
  section whose WMOD row links it)
  ([trams](analysis/trams-20261030.md)). On `cc_t2` the bascule bridge (`WMisty`) opens and shuts
  at its sensors, carries cars and redirects the robots
  ([bridge](analysis/misty-bridge-20261106.md)). `WGeyser`, `WFountain` (`dt_t2`), `WLogRollingB` (`sh_t2`, player cars only) and
  `WDynamiteHole` (`dr_t1`) are ported too, as are `WRolling` and `WChiselCrossover` with `WBlowUp` (`cc_t4`, `ct_t2`, `dr_t4`; [note](analysis/rolling-chisel-blowup-20261106.md), `cc_t4` compared with Wine), `WLogJam` (`sh_t3`, [note](analysis/log-jam-20261106.md), compared with Wine in memory and on screen), and `WRotoHill`, `WBumSteer`, `WMagnet`, `WVolcano` and course `WRot` (`dr_t2`, `dr_t3`, [note](analysis/roto-magnet-volcano-20261106.md)), and `WSaw`, `WJunkyard`, `WConstructionSite`, `WShiftedRoad` and `WGoldenGate` (`sh_t2`, `dr_t3`, `sh_t4`, [note](analysis/saw-crane-gate-road-20261107.md)), and `WDicksTrick2` (`cc_t3`) and `UVScroll` ([note](analysis/dickstrick-uvscroll-rooms-20261107.md)); the fountain and the log were compared with Wine
  ([notes](analysis/lap-line-geyser-20261106.md)). Escape opens the original in-race pause menu
  (general.wgm's `WMenuBrick`) over the race: resume, volumes, restart and
  quit with its confirmation ([pause](analysis/pause-menu-20261030.md)). Race audio covers every car's engine and skid loop,
  placed in 3D around the follow camera (DirectSound's distance law with the
  original's rolloff 5 and the GDSound ranges; provisional), the narrator
  countdown, wall hits, the car-contact and rock voice lines, the drowning
  splash, the leader voice and the course music. Every sound follows the original volume law: its volume capped by
  its slider (EFFETS, or VOIX for GDSound `0x9d`), through DirectSound's
  −40..0 dB curve; the sliders start at the option defaults (MUSIQUE 1,
  EFFETS 1, VOIX 0.75). Power-up voice lines and saved options are not
  ported
  ([audio](analysis/native-audio-20261028.md)). The
  vertical motion follows `0x46a4a0` (the step along the probe axis, the
  m_gravity × airtime² fall, the four contact rays), checked against a Wine
  capture of `sh_t1`'s ramps ([vertical](analysis/vertical-motion-20261030.md));
  the return trigger and progress field are provisional native rules (the
  return point is the original's, Wine-checked). All 16 other course archives
  load; all 20 courses pass every `native_drive_test` check. The test's driver is the port's route
  follower (RoboDriver) on the player's keys, with a look-ahead planner for stalls
  ([note](analysis/test-driver-20261107.md)). Laps follow
  the original's start line rule (`0x49a2f0`), and `cc_t3`'s geyser throws cars
  up ([laps and geyser](analysis/lap-line-geyser-20261106.md)). See
  [native race](analysis/native-race-cc-t1-20261027.md) and
  [Championship I](analysis/native-championship-i-20261028.md).
- Race worlds draw each course's own backdrop dome, in its node's frame about the eye and scaled by its radius
  (fog from far − 3000, [checks](analysis/on-screen-checks-20261107.md)), and
  `GD3dSprite`/`WGndSprite` trees, cacti and signs turned toward the camera
  about the world up axis as `0x419b90` does ([sprites](analysis/sprites-20261029.md));
  they draw at the texture's colour, Wine-checked. A same-input Wine comparison of
  Championship I races 2–4 is recorded in
  [Wine comparison](analysis/wine-comparison-championship-i-20261028.md).
- Bounded GMDL/GHMS decoding, per-triangle materials and static segment poses.
  Vertex colours (`VCOL`, `0x43dd67`) and TRAN set textured vertices as the
  Direct3D cache `0x4b21ae` does (alpha TRAN, not 1 − TRAN), Wine-checked on
  `ct_t1` ([vertex colours](analysis/vertex-colours-20261101.md)); 3D draws
  sample bilinearly as `0x4b1ad0` sets.
  Vehicle previews use original ancestor/resource matrices, page-level planes
  and the serialized SCRT viewport/camera link; no fitted replacement camera.
- Vulkan texture uploads, alpha blending, sprite clipping, streamed draws,
  swapchain image selection, synchronization, resize handling, and cleanup.
  Textured 3D includes depth, zero-alpha discard, per-material U/V addressing
  and frame vertex storage that grows outside frame recording.

The menu is an incomplete reconstruction. Road wrapping,
indexed alpha thresholds, selection animation/colors, viewport binding
and lighting still need original-runtime comparison.
The character page follows the recovered `WMenuNodeCarSelection` lock: Satanas
et Diabolo stays unavailable until its profile unlock (winning Championship IV),
so the page opens on Dingo-Limousine and Left/Right wrap past it. The profile
(`game_profile`, the original's `.WSG` blocks) also opens the environment,
championship and course rows: closed rows are skipped and drawn dark, a won
single race opens the next course and a won championship the next one.
CHARGER/SAUVEGARDER PARTIE use the original's eight-slot save screen, with name
entry, and keep `.WSG` files in `$XDG_DATA_HOME/wackyraces/save`
([profiles](analysis/menu-profiles-20261101.md)). Previews use
the recovered GHMS segment pass and each group's serialized `WRot` (5° per
30 Hz frame about the group origin). Same-state Wine captures of all six
playable cars match in order, icons, geometry and shadow; lighting and exact
pixel parity are unverified. See
[character selection](analysis/menu-character-selection-reverse-20261027.md).
Specialized node lifecycle
callbacks other than demos, replays, credits and the keyboard controls page,
multiplayer readiness, joysticks in the menus, complete race-world
rendering, the rest of the wall/contact response and the item effects, the championship results and progression remain to be ported. Recovered routines elsewhere
in `src/` and legacy native stubs do not imply those systems execute in this
build. The port is not a complete or playable game.
Race rendering uses the shared follow camera's projection and the ported
`0x478070` follow camera (not compared with Wine). Lighting, culling, billboard orientation and
animation still require porting and comparison. Translucent parts follow the
recovered per-model queue in the menu and in races, with the additive and
no-depth-write draw flags ([transparent draw order](analysis/transparent-draw-order-20261028.md)).
Every car throws dust by surface, leaves tyre tracks on sand, ripples on deep
water and splashes when it drowns; cc_t1's cacti burst into pieces, rocks stop
the car and push it back, and the shadow follows the ground
([car effects](analysis/car-effects-20261028.md)). The menu's POUSSIERE and
METEO options reach the race (`[game] dust` sets POUSSIERE's starting value);
braking and sliding leave skid marks on roads; a slide start kicks the yaw by `m_farolbendmod` and throws the player's car outward; sand, grass, snow, dirt and ice scale top speed, acceleration and steering by the recovered per-vehicle rows; METEO draws rain on cc and dr_t1 and snow on ct, dr_t4 and dr_b (`0x4b6190`, `0x4b6510`); rain also halves the textures and darkens the sky and fog, as the original does (compared with Wine on cc_t1). Walls use the original contact response: four probe rays push the car out along the wall normal, turn it along the wall and cost 5% speed; three hits on stopping
surfaces halt it and a wall ends a running turbo; the rays meet every surface, ramp faces included, and robots rest on the ground through the original's single ray along their up axis, levelling out in the air; a car that falls off or drowns returns at its section's entry (the start line in the first), and steering in the air is cut to 30% ([wall contacts](analysis/wall-contacts-20261029.md)). There is no edge guard: as in the original, a car may leave the course area, hang off a ledge on one probe or fall; the out-of-bounds trigger and gravity scale are provisional.

The loader and its observed limits are documented in
[asset loading](analysis/native-asset-loading.md) and
[scene/string loading](analysis/native-scene-loading.md),
[menu navigation](analysis/native-menu-navigation.md), and
[model rendering](analysis/native-menu-model-rendering.md),
[world loading](analysis/native-world-loading.md), and
[world materials](analysis/native-world-material-loading.md).

### Native verification

```sh
ctest --test-dir build/linux --output-on-failure
./build/linux/wackyraces --data "/path/to/Wacky Races" --inspect-assets
./build/linux/native_assets_test "/path/to/Wacky Races/menu.WGM"
./build/linux/native_scene_test "/path/to/Wacky Races/menu.WGM" "/path/to/Wacky Races/STRINGS.DAT"
./build/linux/native_menu_test "/path/to/Wacky Races/menu.WGM"
# Car previews: projected box, batches and mean vertex colour per car (page 28);
# --print regenerates tests/native_vehicle_expected.inc after an intended change.
./build/linux/native_vehicle_test "/path/to/Wacky Races/menu.WGM"
./build/linux/native_mesh_test "/path/to/Wacky Races/menu.WGM"
./build/linux/native_motion_test "/path/to/Wacky Races/menu.WGM" "/path/to/Wacky Races/general.wgm"
./build/linux/native_material_test "/path/to/Wacky Races/menu.WGM" "/path/to/Wacky Races/general.wgm"
./build/linux/native_world_test "/path/to/Wacky Races/general.wgm" build/original-disc/cc/cc_t1.WGM
./build/linux/native_drive_test build/original-disc/cc/cc_t1.WGM build/original-disc/sh/sh_t1.WGM \
    build/original-disc/dt/dt_t1.WGM build/original-disc/ct/ct_t1.WGM
./build/linux/native_movie_test "/path/to/Wacky Races/legal.avi" build/original-disc/Movies/intro.avi
./build/linux/native_audio_test "/path/to/Wacky Races" build/original-disc
xvfb-run -a python3 tools/capture_native.py --data "/path/to/Wacky Races" --disc build/original-disc --startup
```

`native_audio_test` checks the mixer (resampling, looping, gains, pan,
clipping, clock), `.snd` IMA decoding through the recovered `0x40bdd0`
routine, SBPC/SBNK banks, music names and AVI PCM. With
`"/path/to/Wacky Races" /path/to/disc` it also checks the seven `Menu.GSW`
banks, `general.wgm`'s 46 samples, `Music/Cc_t1.snd` and the narrator order.

With an archive, `native_scene_test` also checks the property IDs the port
reads against the archive's own `CLAS/PRIM` member table. The disc `Win/*.WGM`
tables are shifted from ID `0xd5`; give a reference archive as the third
argument (`native_scene_test Win/WinTur.WGM "" menu.WGM`) and the scene is
mapped onto its IDs by member name.

`native_motion_test` checks the MTNL reader, the clip controller's
interpolation and chaining (`0x43feb0`) and the segment pose (`0x440650`).
Given archives, it parses every MTNL, checks that each animated `GDHModel`'s
library has as many segments as its GHMS, and plays every clip to its end.

`native_movie_test` checks the RIFF/AVI reader (stream headers, `rec ` lists,
padding, dropped frames, malformed files) and the YUV 4:1:0 conversion. Given
movie paths, it decodes every frame.

`native_drive_test` checks the recovered speed, brake/reverse, steering,
frame-factor and yaw equations. For each course argument it also checks that
the loop transform joins the last exit to the first entry, that every
section's progress field connects, the grid and the countdown lockout (plus
`cc_t1`'s river drown/respawn). A test-only lookahead driver then races three
laps, or one lap on `sh_t1`, whose tunnel hairpins it drives slowly. The
four-course run takes about three minutes.

The six native suites cover 861 synthetic checks, including truncation,
overflow, image orientation/channel order, typed action and screen trees,
mesh index/material/segment bounds, language lookup, cursor traversal and
selection state, cross-archive materials, course transforms and Linux path resolution.
Optional original-file arguments perform additional read-only
corpus checks; no game assets are needed by the synthetic tests.

The GPU integration test verifies actual X11-presented pixels, alpha, clipping,
multiple draws, 71 frames, resizing, 3D depth ordering, zero-alpha discard,
all four U/V addressing combinations, vertex-buffer growth, keyboard input and close events. It
requires an X11 display; Xvfb can provide one when a headless Vulkan driver is available:

```sh
xvfb-run -a ./build/linux/native_render_test
xvfb-run -a python3 tools/smoke_native_menu.py --data "/path/to/Wacky Races" --disc build/original-disc
xvfb-run -a python3 tools/capture_native.py --data "/path/to/Wacky Races" --lang 3 --startup
```

The menu smoke command sends real X11 events through player/mode/vehicle/course
selection, loads/displays the selected world twice when `--disc` is supplied,
checks retained cursors, then exercises
No and Yes in the quit confirmation. It saves page captures and a log under
`build/linux/menu-smoke/`.

The capture commands require ImageMagick and Pillow for startup/world image checks. The
startup capture command saves the menu and card captures under `build/linux/`, compares both presented cards
against the original BMP pixels, and checks the two-second transitions.
To include the GPU test in CTest, configure with `-DWPC_RENDER_TESTS=ON` and run
CTest inside an X11 session. GPU tests are off by default for display-free builds.

Shader sources are in `src/port/shaders/`. To regenerate their checked-in
SPIR-V header, install `glslangValidator` and `spirv-val`, then run:

```sh
python3 tools/build_native_shaders.py
```
