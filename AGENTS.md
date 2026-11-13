# Project guidance

A file for guiding AI coding agents.

## Project Overview

The project's goal is a native Linux x64 port of the 2000 PC game Wacky Races
(`src/port/`, C++17, X11 + Vulkan 1.0, reading original game files directly).

1. No game content in git, ever
2. Never publish or host a web build
3. Never commit and never push: leave every change in the working tree

## Debugging

- Debug by printing measured values to stdout. Scratch harnesses (small C++
  programs in the session scratchpad, linked against the port's object files)
  and tests print one line per case: the course, the update, the positions,
  the states and the counts that decide the result.
  - Example: `dt_t1: sections 4 player 3, alive 60 updates, crossed 1, after 55`.
- Run the same harness on all 20 courses rather than on one, so a rule that
  fits one course and breaks another shows up at once.
- Keep a control. Run the harness once with the change disabled, to show that
  the check fails without it; then restore the source and rebuild.
- Compare printed values with the original's (Wine memory reads, logged update
  by update), and quote the lines in the `analysis/` note.
- Scratch harnesses and their outputs stay out of the repository.

## Processes

- Stop Wine, WineDbg, Xvfb and the native game by their process number
  (`kill PID`), never with `pkill -f`. A loop such as
  `for p in $(pgrep -f PATTERN)` also matches the agent's own shell.
- Stop every Xvfb and Wine process you started before the end of a task.

## Native captures

- `--race` takes the course archive's path under the disc, e.g.
  `--race sh/sh_t1.wgm`, not the course name.
- Debug variables:
  - `WPC_PLACE=x,z,yaw`: player 1 on the ground there at the start of a
    `--race`;
  - `WPC_RACE_PLACE=x,z,yaw`: the same for any race;
  - `WPC_WEAPON=11..25`: player 1 holds that weapon, charged;
  - `WPC_GADGET=0..10`: player 1 holds that gadget, charged (0..6 FlyPower, 8 TurboSpeed);
  - `WPC_LAST_LAP=1`: player 1 starts on its last lap, so its first crossing ends the race;
  - `WPC_WEATHER=0/1/2`: overrides METEO (off, on, random);
  - `WPC_WINNER=vehicle,environment,course`: opens that winner scene;
  - `WPC_HIT=TYPE,UPDATE`: hits player 1 with that WackyAutoBomba type (e.g.
    `0x80`) at that race update.
  - `WPC_DEBUG_BREATH=1`: logs the breath weapon model's world box each draw.
- `import -window root` takes about a second per frame. For anything that
  moves fast (a projectile, a hit), record with
  `ffmpeg -f x11grab -video_size 1024x768 -framerate 30 -i :78 -t 2.5`
  and extract the frames.

## Reverse-engineering method

- The thunks at `0x401000..0x406xxx` are `jmp` instructions to the real
  routines. A call goes through its thunk, so map each thunk to its target to
  name the routine called.
- A routine reached only through a vtable has no direct caller. Search
  `.rdata` for its thunk's address to find the vtable and its slot.
- Before porting a field's rule, list every instruction that writes the field
  and group them by routine. The group field `+0x104` has 57 writers: the
  section enabling, the tracker, the node copy, the car effects and the
  weapons.

## Verification

- `ctest` runs only the synthetic checks (about 0.1 s). After a change to the
  race, also run `native_drive_test` on the 20 courses:
  - pass each `*_t?.WGM` path as an argument;
  - run them in parallel and in the background (`xargs -P 7`), with
    `WPC_AUDIO=silent` and a timeout of 1200 s per course (about 10 minutes
    in all);
  - check every exit code and the summary line of each course.
