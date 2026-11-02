# Native race HUD (one player)

Recovered from the executable, then compared with a same-course Wine capture
(`sh_t1`, 1024×768). Captures stay in the session scratchpad. All positions
are in 320×240 HUD units; the native port draws in 640×480 space at twice
these values.

## WHudBrick (`0x485b00`)

The update positions the children of `WHudBrick 420` from the one-player row
of the layout table `0x52bd5c` (20 int16 pairs per row). It hides a child with
flag `0x100`. Its data is the per-player block at `0x6f3148`, 0x54 bytes per
player, filled by `0x4a4650`.

| Child | Element | Position | Source |
|---|---|---|---|
| 0, 1 | speedometer dial and needle | (263,207), (280,217) | speed `+0x00` |
| 2 | message | (160,120) | `+0x18` |
| 3 | lap `%d/%d` | (13,10) | lap = `car+0x19f` clamped 1..laps (`app+0x1a70`) |
| 4 | rank | (13,30) | `car+0x1af` |
| — | rank suffix | (24,30) | string `0xb6+rank`, drawn as text; the GDFilm child 5 stays hidden |
| 6 | timer | (13,50) | `+0x14` |
| 7, 8 / 10, 11 | racer ahead / behind: head (FILM 66332) and up / down arrow (FILM 66275 frames 7 / 8) | (28,176)+(54,176) / (28,216)+(54,216) | vehicle ahead `+0x30` and behind `+0x28`; shown only when the brick's `+0x122` < 0 |
| 15 | bend arrow (FILM 66331) | (160,100) | `+0x38` |

- **Ahead/behind portraits:** the dial is shown only while `+0x122` is 0, so a
  normal one-player race never shows them. Wine's frames (7th place) agree.
- **Suffix strings:** each is one HUD-font character. English `_ a b `` are
  ST/ND/RD/TH, French `t u` are ER/ÈME, German uses `.`.
- **Bend arrow (`0x4a6890`):**
  - The car checks the `WBendSign` objects linked from its current WMOD route
    grid. A sign counts when the car is within the sign's radius (`0x1e0`,
    integer components) and the car's heading, atan2(−x, z) of its forward
    axis, is within ±60° of the sign's. The result is the sign's type
    (`0x1e1`), or −1.
  - The value is refreshed only while racing (manager states 2, 3, 14).
  - Frames: types 0/1 use frame 0, 3/4 frame 1, 2 frame 2; types 0 and 3 are
    mirrored (flag `0x1000000`, serialized property `0x43`).

## Power-up panel (`WFirstPowerUp`, draw `0x4d2c80`)

Race setup (`0x483a20`) takes `WFirstPowerUp`'s children out of normal
rendering and draws them directly: big dot (GMAT 66010), small dot (66011),
needle (66013), winged gauge (66012), the `WModMngrFilm` (FILM 66334), then
27 `WPowerUpSprite` icons (origin `0xa1`, `0xa2` = 64, 0).

- **Gauge:**
  - The sprite is at (230,68); the needle turns about (10,29) at (273,93).
  - The car's charge level `car+0x399` (0..3) selects −56°, −30°, 23° or 56°
    (table `0x52debc`). The needle starts at −45° and moves at most 3° per
    update: within 3° it jumps to the target.
- **Icons:**
  - The gadget (`car+0x371`) is at (250,10) and the weapon (`car+0x375`) at
    (290,10).
  - `0x4679d0` state 0 sets the gadget to icon `m_iVehicle` and the weapon to
    icon `m_iVehicle+11`; vehicle 6 gets a random weapon, 17 + rand() % 9.
    For the Dingo-Limousine these are the bat wings and the dragon, as in the
    car page and Wine.
  - While the charge is 0, both materials get transparency 0.4.
- **Strip (single race and championship, one player):**
  - Big dots at x 18 and 235 (y 220); small dots every 6 from 26 to 230
    (y 222).
  - `WModMngrFilm` (`0x499d90`) draws each racer's head (frame vehicle+2) at
    (x+20, 216), scale 0.65.
  - x = 220 × distance into the lap ÷ lap length (`car+0x1a3` ÷
    `manager+0xf38`); a finished racer is at 220. The shown x (`car+0x1ab`)
    moves by 0.6 of the difference, truncated, unless the jump is 150 or more.
  - In states 5–7 the strip becomes the finish standings slide-in, and the
    gauge and icons are not drawn.

## Native

- `RaceTrack::bend_signs` and `RaceSession::bend_arrow`/`strip`/
  `charge_level`.
- `RaceView::powerup_panel`, `progress_strip` and the bend arrow in `hud()`.
- The gauge and strip easing advance once per game update; the original steps
  them per 33 ms HUD frame.
- Power-ups are not ported, so the charge stays 0: dimmed icons and the needle
  at −56°, as Wine shows before any pickup.
- The lap fraction comes from the progress field (route sections), not
  `car+0x1a3`.
- Opponents stay on the grid, so their heads stay near the start.

## Wine comparison (`sh_t1`)

- **Matches:**
  - lap, rank, suffix and timer positions;
  - the gauge (wing tips within a pixel at 1024×768) and the gadget frame
    (same x range);
  - the dot row, pixel for pixel;
  - head size and height.
- **Differs only through unported systems:** Wine's weapon slot showed a
  picked-up oil slick with full-opacity icons, and its AI racers spread along
  the strip.
- **Bend arrows:** checked natively with the car placed by `WPC_RACE_PLACE`
  short of `cc_t1`'s signs 54 (type 1) and 626 (type 0, mirrored). No Wine
  frame at a sign has been captured.

## Not done

- **Power-ups:** pickup, charge, the roulette (`0x4679d0` states 1–2) and the
  spinning icons.
- **Results table** after the standings, and the unlocks.
- **Other modes:** the two-player, battle and practice HUDs; see "Other
  layouts" below. The port has none of these modes yet.

## Finish standings (WModManager `0x4a2b30` states 4–7, `WModMngrFilm` `0x499c38`)

| State | Behaviour |
|---|---|
| 3 → 4 | the race has ended; each car's final place (`+0x1b3`) comes from the rank table `+0x7f0` |
| 4 | single race (mode 0): state 5. Other modes: each car gets 10/6/4/3/2/1/0 points by place (bytes at `0x52cef0`, into `0x6f466e[vehicle]`), then state 6. Both set the counter `+0xf34` to 120 |
| 5 | the counter counts down; at 0, Enter leaves the race |
| 6 | the same, then Enter selection-sorts the rank table by total points (`0x6f4675` + `0x6f466e`) into `+0x810`: state 7, counter 120 |
| 7 | at 0, Enter leaves the race |

**Draw (`WModMngrFilm`):** for place p, t = (counter + 8p − 85)², at least 0.
Odd places come in from the right (x = 45 + min(t, 800)), even places from the
left (x = 45 − min(t, 400)). Places stack at y = 65 + 20p, head frame
vehicle+2 at scale 0.65. State 7 draws the `+0x810` order. Place 0 arrives
35 updates after the finish, and each later place 8 updates after the one
before. Meanwhile the power-up panel and the strip are not drawn.

**Native:**
- `RaceSession` holds the states, the standings order and the points. The
  player's place is `rank()`; the parked opponents keep their grid order
  behind it.
- `RaceView::standings` draws the column.
- `MenuSession::points` carries the championship totals from race to race,
  cleared when a championship is chosen.
- Checked in the game with a temporary forced finish (not kept), and by
  `native_drive_test` (single race, and a championship race with a leading
  vehicle) and `native_menu_test` (points accumulate).

### Finish text (race manager draw `0x4a8f30`, states 5–7)

The states set HUD mode `0x6f3164` to −1, and `WHudBrick` then hides every
child: lap, rank, timer, dial, message and bend arrow are gone. State 4 had
already cleared the message, so strings 5/6 are not shown at the finish. Each
standings row is drawn with `0x42e330` (the HUD font, flag `0x800`: no
alignment), sliding with its head:

| Item | Position | Text |
|---|---|---|
| place | x+20, y−2 over a black copy at x+22, y | `%d`: place + 1; in state 7 the total `0x6f4675` + `0x6f466e` |
| name | x+60, y−2 over a black copy at x+62, y | string `0x52cf0c[vehicle]` = 60, 57, 58, 55, 59, 56, 54 |

- **Layout:** x is the head's slide (±t, as above); y = 62 + 20 × place.
- **Colour (font tint, 0..255):** rows with y < 122 (y < 82 in state 7) are
  (255,128,128), the others white. The player's vehicle (`app+0x1a68`)
  pulses, g = b = 192 + 63 sin(GetTickCount × 0.01) (`0x512fcc` float,
  `0x514be0`/`0x514bd0` doubles).
- **Prompt:** string 7 centred at (160,200) once the counter is below 5; at
  (160,100) in two-player.
- **Gates:** in state 5, the rows draw only if `0x6f2cc0` is 0 and the
  viewport's `+0x13a` is 0.
- **Environment 3 (`0x6f41d8`, a championship's last course), state 7:** when the counter is below 5
  and the player is not leading, three lines are centred at y 5, 20 and 35.
  They are in the executable, not in STRINGS.DAT: Spanish, French or
  Portuguese by language `0x6f46e8` (2, 3, 6), else English (`0x52d0c4`…
  `0x52d1b4`).

**Native:**
- `RaceView::standings` draws the rows, `hud()` hides everything else and
  shows the prompt.
- `ExeImage` (`game_exe`) reads the championship lines from the installed
  `WPC.EXE`, so the sources carry no game text. Without the file the lines
  are skipped.
- The original draws each coloured string before its black copy. The port
  draws the black copy first, as a drop shadow; the original's sprite order
  has not been compared with Wine.
- The pulse uses the wall clock, as GetTickCount does.
- Checked in the game with a temporary forced finish (not kept): state 6 with
  place numbers, state 7 with the sorted totals, and the French
  championship lines.

## Other layouts (not ported)

The two-player screen uses layout rows 1 and 2 of `0x52bd5c` (identical; one
per viewport, chosen by `+0x13a`), and the HUD block of the second player
(`0x6f3148` + 0x54). The dial is hidden in two-player.

| Child | Row 0 | Rows 1/2 |
|---|---|---|
| 0, 1 dial | (263,207), (280,217) | hidden |
| 2 message | (160,120) | (160,60) |
| 3 lap | (13,10) | (13,30) |
| 4, 5 rank, suffix | (13,30), (24,30) | (33,50), (44,50) |
| 6 timer | (13,50) | (160,216) |
| 7, 8 ahead | (28,176), (54,176) | (28,196), (54,196) |
| 10, 11 behind | (28,216), (54,216) | (292,196), (266,196) |
| 13, 14 battle counts | (14,216), (306,216) | (14,105), (306,105) |
| 15 bend arrow | (160,100) | (160,70) |

- **Battle (mode 1):** children 3–12 are hidden. Children 13 and 14 show
  `%d` of `+0x20` and `+0x24`.
- **Practice (mode 2, `app+0x1a60` = 2):** child 3 shows `%d/%d` of `+0x0c`,
  `+0x10`; child 8 shows frame `0x6f3188` at (23,220). `0x4a8f30` adds times
  at (13,45) and (40,215).

## Practice ported (2026-11-01)

**Original.**
- **Roster.** `0x49d580` mode 2 places only the player, at grid slot 2 (`race_roster`).
- **Setup.** `0x49e0a1` sets the splits `0x6f318c`/`0x6f3190`/`0x6f3194` to −1 and `0x6f3180` to −1. It loads the record `0x6f467c[env × 4 + course]` (the profile's `+0x114`) into `0x6f3184` (low 28 bits, 1/300 s) and `0x6f3188` (vehicle, the high 4 bits).
- **Splits.** `0x4a33ac..0x4a33f6` store each lap's time as the timer (`0x4040ca` on the car's `+0x18b`) minus the earlier laps.
- **Finish.** When the lap count `+0x19f` reaches 4, state `0xf` begins. If the time beats the record, `0x4a3473` writes time | vehicle << 28 and sets `+0xfa4`.
- **HUD (`0x4861xx`).**
  - Child 8 shows the holder's frame `0x6f3188` at (23, 220).
  - Two `0x4015e6` texts follow: the split sum `0x6f3198` at (13, 45), and the record at (40, 215).
  - Both are formatted by `0x450f00`: t / 300 seconds as `%02d:%02d` (`0x52a700`).

**Native.**
- `RaceSession(..., mode)`, with `lap_splits`, `split_total` and `set_record`.
- `linux_main` passes the menu's game mode for the requested course and loads the record from the profile.
- At the finish, `MenuController::practice_time` updates the record.
- `RaceView::hud` hides the rank and the timer in mode 2 and draws the two times and the head.

**Checks.**
- `native_drive_test`: alone at slot 2, splits at −1.
- `native_menu_test`: an equal time is no record; a faster one is.
- An Xvfb capture of practice on cc_t1 showed 1/3, 00:00, and the record 02:20 with Dick Dastardly's head.

**Not traced:** which other HUD children mode 2 hides (the native progress strip stays), the head film's frame mapping, and `+0xfa4`'s use. Not compared with Wine. (Traced 2026-11-01, below.)

## Practice, remainders traced (2026-11-01)

**Original.**
- **HUD children (`0x485b00`, mode 2):**
  - child 3: the laps, `%d/%d`;
  - children 4, 5 and 6 (rank, suffix, timer) are hidden;
  - child 7 (GDFilm 429, film 66332, the neighbours' heads) shows frame `0x6f3188` at (23, 220). The frame is the vehicle itself, as for the neighbours, whose frame is the car object's `+0x88`;
  - children 8–14 are hidden;
  - child 15, the bend arrow, is as in a race.
  - The earlier "child 8" was a miscount.
- **Strip.** `0x499c38` draws it when `0x6f2cc0` is 0, that is with one player (`0x48a425` sets it to 1 for two), in states 1–3. States 5–7 draw the standings instead.
- **Finish.**
  - When the lap count reaches 4 (`0x4a3421`), practice enters state `0xf` (not 5) with counter `+0xf34` = 60 (`0x4a3464`).
  - `+0xfa4` tracks the last lap count (`0x4a3415`). It becomes 1 on a new record (`0x4a34d5`), otherwise 0 (`0x4a34e4`).
- **State `0xf`:**
  - `0x4a3504` counts down; at 0, Enter (DIK `0x1c`, newly pressed) leads to state `0x10`.
  - The HUD mode stays 2 (`0x4a8f4c`).
  - `0x4a9ccf`: with `+0xfa4` set, the HUD message (`0x6f3160`, child 2 at (160, 120)) is string `0xe7` (231). Below 5, string 7 is drawn at (160, 150).

**Native.**
- `faces_` is WHudBrick child 7's film; the record head uses frame = vehicle.
- `RaceSession::finish` in mode 2 sets state 15, counter 60 and `new_record()`, and replaces the record.
- `RaceView::hud` keeps the practice HUD in state 15, hides the strip there and draws strings 231 and 7.

**Checks.**
- `native_drive_test` (cc_t1): practice with record `0xfffffff` ends in state 15, counter 60, with no standings and a new record for vehicle 3. Enter is accepted after 60 updates.
- An Xvfb capture shows the record head (Dick Dastardly, vehicle 0) from film 66332.
- Not compared with Wine.
