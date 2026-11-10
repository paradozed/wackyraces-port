# CLAUDE.md

@AGENTS.md

## Hard rules

- **No game content in git, ever.** Original executables, WGM/GSW/SND/AVI
  files, `Wacky.iso`, extracted disc trees, captures and screenshots stay out
  of the repo (`/build/` and `/Wacky.iso` are ignored). Tests take asset paths
  as arguments; never copy original bytes into sources or fixtures beyond
  recovered tables/constants.
- **Never publish or host a web build.**
- Never commit and never push, even after a significant change; leave the work
  in the working tree.
- Original assets are read-only. Never write into the install directory, the
  mounted disc or the user's Wine prefix; copy the prefix into a scratch
  directory before running the original.

## Asset locations (this machine)

- Install: `~/.wine/drive_c/Program Files (x86)/Infogrames/Wacky Races`
- Disc: `/run/media/nicolas/Wacky` when mounted; courses, `Music/`, `Movies/`
  and `Win/` extracted to `build/original-disc` (see README for `bsdtar`).
- Original game files are read-only inputs; they are never bundled with the
  project.

## Native port

`src/port/` — C++17, X11 + Vulkan 1.0. Reads original game files directly
(WGM, SND, AVI, BMP, TGAF, …). No Win32 calls, no PE execution, no Wine.

Optional deps:
- libavcodec (FFmpeg) for Indeo 5 movies (`WPC_HAVE_LIBAVCODEC`)
- libpulse-simple for audio output (`WPC_HAVE_PULSE`)

## Build and test

```sh
cmake -S . -B build/linux -DWPC_NATIVE_PORT=ON
cmake --build build/linux -j
ctest --test-dir build/linux --output-on-failure        # 14 native suites
./build/linux/wackyraces --data "<install>" --disc build/original-disc \
    [--lang 3] [--skip-startup] [--frames N]
```

The game reads `~/.config/wackyraces/config.ini` (or `$XDG_CONFIG_HOME/wackyraces/`,
`$WPC_CONFIG`, or `--config FILE`); copy
[`config/wackyraces.example.ini`](config/wackyraces.example.ini).

Runtime env: `WPC_DATA`, `WPC_DISC`, `WPC_LANG`, `WPC_JOYSTICK1/2`, `WPC_AUDIO=silent` (mute —
automated scripts must set this so they never play on the user's speakers),
`WPC_AUDIO_CAPTURE=out.wav` (record the mix instead of playing).

Command-line options override environment variables, which override the config
file. Unknown keys are reported with their line. At startup the log names the
config file and where the data and disc paths came from.

## Reverse-engineering workflow

- Ground every behavior in the original: cite the routine/data address in a
  comment (`// 0x4705ca: ...`) and in the matching `analysis/` note. Mark
  anything inferred or provisional as such; never present a guess as recovered.
- Runtime comparison: run the original under Wine from a scratch copy of the
  prefix on its own Xvfb display (e.g. `:77`), the native build on another
  (`:78`), and drive both with XTest key scripts. Compare captures for the same
  state; state when no comparison was made.
- Audio is verified through WAV captures and the game log, not by ear, unless
  the user listens.

## Code conventions

- Match the surrounding dense style in `src/port/`: namespace `wpc`, compact
  one-line bodies, short comments that explain original behavior.
- Avoid names that collide with `std::` via ADL (e.g. `apply`).
- New port files go in `WPC_PORT_SRC` in `CMakeLists.txt`; new test targets
  get a `ctest` entry.
- Bounded parsing: reject truncated/implausible input with exceptions and add
  negative cases to the tests.

## Documentation upkeep

After a feature lands, update together:
- `README.md` (native port description, verification commands, remaining work),
- `TODO.md` (check items off, add the discovered gaps with addresses),
- a dated note `analysis/<topic>-YYYYMMDD.md` with recovered addresses,
  formulas and what was and was not verified.

## Native port status (keep current)

- Startup: `legal.avi` (own soundtrack) → L2/L3 cards → `Movies/intro.avi`
  with `Voice/iaudio.avi` → menu. Soundtrack position clocks movie frames.
- Menu: recovered node graph, character selection, textured previews, menu
  music `Music/Cc_t1.snd`, per-item voice from `Menu.GSW`, volume sliders.
- Race: Championship I (`cc_t1`, `sh_t1`, `dt_t1`, `ct_t1`, recovered order
  and race-to-race progression), single player and practice (alone, lap
  splits, the course record `0x6f467c`, finish state 0xf), WackyAuto physics
  (with surface modifiers), rotated WMOD loop seams (weapons cross them, `0x4a23c2`), section culling (player's section +-1, `0x4a8680`), jumps, HUD, countdown,
  course music and race sounds.
- Wall contacts, opponents (`WCtrlRoboCar`), weapons (turbo, shield, missile,
  breath, tank gun, boomerang, TNT, storm cloud, detour sign, landmine; the hit smoke, tint, stars, paint pool and the tank shell's mound and chunks,
  `analysis/hit-smoke-20261107.md`; rocket tails, `analysis/rocket-tail-20261108.md`;
  TurboSpeed/FlyPower jet flames (WFire, `analysis/wfire-20261109.md`); FlyPower's rotor (vehicle 6), parachute (5) and per-vehicle flight ends;
  race-side animated GDHModels (the breath's model, `analysis/breath-head-20261108.md`; the cars'
  body sway, controller 0, `analysis/mtnl-motion-20261028.md`); backward use
  with look-back held (`+0x5c`); dropped weapons filed in their slot row (`0x4a6d20`)),
- trams on `dt_t1`, pause in course, profiles/saves (8 slots), two players in
  split-screen, battle on `*_b.wgm` arenas.
- Course objects: dt_t1's trams, sh_t3's WLogRide, cc_t2's WMisty bridge
  (`analysis/misty-bridge-20261106.md`), WGeyser, WFountain, WLogRollingB,
  WDynamiteHole, WRolling, WChiselCrossover and WBlowUp
  (`analysis/rolling-chisel-blowup-20261106.md`), WLogJam
  (`analysis/log-jam-20261106.md`), WRotoHill, WMagnet, WVolcano, WBumSteer, course WRot
  (`analysis/roto-magnet-volcano-20261106.md`), WSaw, WJunkyard, WConstructionSite,
  WShiftedRoad, WGoldenGate (`analysis/saw-crane-gate-road-20261107.md`), WDicksTrick2,
  UVScroll (`analysis/dickstrick-uvscroll-rooms-20261107.md`); other courses' classes are in
  `TODO.md`. Laps follow the original's start line rule `0x49a2f0`
  (`analysis/lap-line-geyser-20261106.md`).
- Not ported: force feedback and joystick rebinding (a joystick is picked on the options page and drives races and menus), parts of the power-up effects and their visuals, part
  of the race-world rendering. All 20 courses pass
  `native_drive_test` (its driver: the port's RoboDriver on the player's keys).
  The port is not a complete game; see `TODO.md`.

See `README.md` for the full implemented/remaining list and verification
commands.
