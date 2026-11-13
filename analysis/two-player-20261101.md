# Two players (2026-11-01)

## The car page (`WMenuNodeTwoPlayer`)

**Class and nodes.**
- Class record `0x6f3df0` (`0x48ded0`). The instance vtable is `0x513dac` (constructor `0x48e090`, 0x13e bytes).
- The nodes are `0x147` (start code 14) and `0x146` (code 15), both with page 61 and `m_bSelectLeftRight`.
- The child of `0x147` is the environment page (`WMenuNodeSelectScenario 0x236`).

**State.** Player 1 has cursor `+0x12a` and ready flag `+0x136`; player 2 has cursor `+0x132` and ready flag `+0x13a`.

**Activation (slot 15, `0x48e1c0`).**
- Both cursors are placed by `0x40605f`.
- The car items' selection marks `0x800000` and `0x1000000` are set.
- Both players start not ready (`0x405f79` hides the ready texts).

**Input (slot 16, `0x48e320`).**
- A player who is not ready moves their cursor with their own input (`0x40316b`, mask `0x800000` or `0x1000000`).
- Pressed bit `0x10` (`0x52aad8`; `0x6e42b0` for player 1, `0x6e42b2` for player 2) makes a ready player not ready, with a sound (`0x4041fb`). For player 1 who is not ready, it returns 1, which leaves the page.
- Pressed bit `0x40` (`0x52aada`, accelerate) makes the player ready (`0x405f79` shows SELECTIONNE, text 171, which is serialized inactive).
- With both ready, each player's car item takes its selection (`+0x4c` with the mark: game `+0x1a68` and `+0x1a6c`). The node then goes on (`+0x54`), returning 2.

**Availability (slot 18, `0x48e2d0`).**
- A row held by both cursors is closed, so the cursors skip each other's row.
- Row 0 needs Satanas (`0x40425f`).

**Wine (2026-11-01, scratch prefix).**
- The page is split: player 1 in the top half and player 2 in the bottom half. Each half draws the page at its normal positions for that player's cursor, cut at the half; player 2's is moved down 240.
- The 3D preview sits inside the half.
- The cursors start on Dingo (row 1) and Tocard Tank (row 2).
- Up (player 1) and G (player 2) made both ready, and the environment page followed.

## The race

- **Roster (`0x49d580`, `+0x1a64` = 1).** Player 2's car is `0x6f2ce0[+0x1a6c]`, or the spare `0x6f2cfc` when both chose the same vehicle. Every other car's node flag 2 is cleared and flag 1 set, so there are no robots. Under Wine only `0x6f2ce0[1]` and `[5]` were set.
- **Grid (`0x49eb70`, two-player branches).** Player 1 takes slot 0 (−450 across, −300 along) and player 2 slot 3 (+450, −300). Wine on cc_t1 had (2845.1, −6675.9) and (3735.3, −6543.8), exactly those slots.
- **Screen.** Split top and bottom, one follow camera and one HUD per player. The HUD rows 1/2 are those of the native HUD note: lap at (13, 30), rank at (33, 50), and the power-up panel.

## Native

- `MenuController::two_move`, `two_ready` and `two_back`, `two_cursor`, and `availability` follow `0x48e2d0`.
- `linux_main` maps each player's MenuPrefs keys (left, right, accelerate = ready, brake = back; provisional for bit `0x10`); Escape backs out player 1.
- `MenuView` draws the split page: the halves with clipping, the preview squashed into each half (provisional), and SELECTIONNE only for a ready player.
- `native_menu_test` covers the cursors, the skipping, ready, back and the advance.

**The race (native, 2026-11-01).**
- `RaceSession(..., vehicle2)`: player 2 is `opponents()[0]` with `human` set, at slot 3; there are no robots, and the robots' gravity doubling does not apply.
- `drive_human` runs player 2 through the player's speed, steering and move. Its items use the robots' slots (no roulette) and are spent on a new press of player 2's gadget or weapon key.
- Player 2 has its own lagged follow yaw (`follow_frame2`), `lap2` and `rank2`. The race finishes once both players have finished (provisional).
- `RaceView::draw` splits the screen: `draw_view` once per player, with the two-player follow pose. Each half keeps the pixel scale: the middle band of the view, moved by a quarter frame (provisional; the original viewport projection is not traced).
- `hud2` draws lap (13, 30), rank (33, 50) and suffix (44, 50), and the message at (160, 60). The timer (160, 216) falls outside the half, as under Wine. Player 1's power-up panel is shown; player 2's is not.
- `linux_main` reads player 2's MenuPrefs keys (G, B, V, N, Z, X by default).
- `native_drive_test`: no robots, slots 0 and 3, player 2 driving on its own input and leading.
- An Xvfb capture shows the two halves with their own cameras and HUDs.

**Not ported or provisional:** the viewport projection, player 2's item roulette and HUD panel, the standings for two players, the two-player audio listener (player 1's camera), the pause per player (198/199), two-player championships (start codes `0xe` + mode).

## Items and panel (2026-11-02)

- `0x4d2c80` draws per viewport (`viewport+0x13a`): 0 takes player 1's car (`+0x11e`), the needle at (273,93) and its angle state `+0x13e`; otherwise player 2's (`+0x122`), the needle at (276,95), state `+0x142`. The gauge (230,68), gadget (250,10) and weapon (290,10) are the same. `0x546cf0` is 1 or 2 in two players while `+0x146` draws (0 in battle and state 5); that element is not identified.
- Player 2 is a human (`+0xc0` = 0, `0x49eb20`), so `0x4679d0` runs the human roulette for it (the port used the robots' slot path).
- Native: `RaceSession::human_items` for both players (`items2_`), `RaceView::powerup_panel(human)`. Headless capture (`--race cc/cc_t1.wgm --vehicle2 1`): both panels with their own items. Not compared with Wine.

## The half viewport against Wine (2026-11-08)

### Wine

- **Path to a two-player race.** Main menu Return (DEMARRER), Down + Return (2 JOUEURS), Return
  (COURSE). On the car page, Up (player 1 ready) and G (player 2 ready). Then Return
  (environment), Return (course): `cc_t1`, Dingo (vehicle 3) and Tocard Tank (vehicle 5).
- **Size.** At 1024×768 the top half shows the tower car 205 × 70 pixels. The native port drew it
  290 × 100, the full-screen pixel scale cut to the half. The ratio is 0.707 = 1/√2 on both axes,
  with no squash.
- The projection `0x435410` depends only on the camera's FOV and ratio (`+0x15e`). A ratio halved
  would give x × 0.707 and y × 1.414, onto half the height: 0.707 on both axes. Where the original
  halves it is not traced (the `+0x15e` writes found belong to other classes).

### Native

`RaceView::draw_view` scales the half's clip x and y by 1/√2 and centres the view in the half.
At the same moment the top halves then match Wine's framing: car size, the canyon wall, the
palm, the water tower (a few pixels apart in height).

Not compared: the bottom half in motion; the two-player follow camera's own distance.

## The camera ratio (2026-11-11, static reading)

- `0x435410` builds the projection from the camera: f = 1 / tan(`+0x156` × 0.5); s = √(`+0x15e`
  (m_nRatio, int) × `0x511220` (0.01)); m00 = f × s, m11 = f / s, the depth terms from `+0x166`/
  `+0x16e`. Its caller `0x4354a0` passes the viewport's camera (`[[this+0xbc]]+0x186`).
- So the two-player fit (×0.707 in x, ×1.414 in y onto half the height) is what a ratio of 50 gives.
- `general.wgm` has two follow cameras under the world, `0x1b7` and `0x1ba` (fov 0.9599, near 50,
  far 20000), both with `m_nRatio` 100; the menu's camera `0x12d` also 100. The constructor
  `0x4208f1` writes 100. No other store to a camera's `+0x15e` and no use of `m_nRatio`'s member
  descriptor (`0x544f00`, registered at `0x420730`) was found; the 79 readers of app `+0x1a64` show
  no ratio write nearby. Still not traced; a Wine read of `[camera+0x15e]` in a two-player race would
  settle it.
- Tried 2026-11-11: a two-player race reached under Wine (DEMARRER, 2 JOUEURS, COURSE, Up and G
  held 0.6 s then Return on the car page, two Returns). `/proc/<pid>/mem` is refused (ptrace scope
  1); a winedbg attach and detach left a `winedbg --auto` child on WPC.EXE (later attaches fail with
  error 5, and stopping that child ends the game); in script mode a breakpoint at `0x43543b` does not
  stop before the next command (each `cont` returns in the attach thread, `$eax` = 0). The camera
  objects' vtable is `0x5108e0` (constructor store `0x4208e1`), for a later memory scan.
- A second try (one session, prints only, over the object table `[[0x52a504]+0xc]`): winedbg's
  attach thread faults at once in WoW64 (`page fault on read access to 0xffbc11b0 in wow64 32-bit
  code`), and the script stops there. Under this Wine (11.20, WoW64 only), attaching winedbg to the
  32-bit game is not usable; a memory read needs another route (a 32-bit Wine prefix, or a native
  debugger with ptrace rights).

## The camera ratio, read under Wine (2026-11-12)

Memory reads work when the reader launches Wine itself as a child subreaper
(`prctl(PR_SET_CHILD_SUBREAPER)`, so WPC.EXE stays its descendant and Yama scope 1 allows
`/proc/<pid>/mem`). A scan for the GDCamera vtable `0x5108e0` in a two-player `cc_t1` race found three
cameras: the menu brick's (fov 1.0472, ratio 100, near 0.5, far 20000) and the two follow cameras
(fov 0.9599, ratio 100, near 50, far 20000). So the original does **not** halve the camera ratio in
two players; the port's fitted 1/√2 (m00 × 0.707, m11 × 1.414 onto half the height) must come from
the viewport path. **Found (same day):** the Direct3D renderer's projection `0x4aec30` reads the
device viewport (`IDirect3DDevice7::GetViewport`, vtable `+0x3c`) and takes
s = √(dwHeight / dwWidth × the camera's ratio `+0x15e` × 0.01 (`0x512fcc`)), then m00 = f × s and
m11 = f / s with f = cot(fov / 2) (`0x4aec48..0x4aece6`). Full screen 1024 × 768: s = √0.75 = 0.866
(the port's 0.0075 factor); a half 1024 × 384: s = √0.375 = 0.612, i.e. the full screen's × √0.5:
x × 0.707 and y × 1.414 onto half the height, exactly the earlier fit. (`0x435410`, the frustum's
projection, keeps √(ratio × 0.01) without the viewport.) `RaceView::draw_view` now derives the factor
from these two aspects instead of the fitted constant (the same value). In the main menu the scan
found two cameras.

## The audio listener (2026-11-12, static reading)

`0x427620(camera)` (the sound manager, listener `+0x94`) runs only when the camera passed in is the
first viewport's (`[[renderer+0xbc]]+0x186`, checked again through the list `+0xb4`); it then copies
the camera's frame into `SetPosition` (`+0x38`, `0x4276ce`) and `SetOrientation` (`+0x34`, `0x4276fa`).
In two players viewport 0 is player 1's top half, so the listener follows player 1's camera, as the
port's `RaceAudio` does (`session.camera()`). Nothing to change.

## The finish (2026-11-12)

- The lap pass `0x4a4170` returns 1 when player 1's `+0x19f` passes the lap count (`app+0x1a70`,
  `0x4a42a1..0x4a42a9`) and, in two players, 2 as soon as player 2's does (`0x4a43aa..0x4a43b9`).
- Its racing-state caller (`0x4a2dbf`) goes on at once when the result is above 0 (`0x4a2dcf`):
  state `+0xf30` = 4, and both players' cars are stopped and timed (`0x403bd4`(car, 2, 1), `+0x19b` =
  2, the finish time from `0x6ab2d0` into `+0x18b`; `0x4a2dee..0x4a2e60` for `+0x38`, then `+0x3c`
  when `app+0x1a64` is set).
- So the first player to finish ends a two-player race. The port had waited for both
  (provisional); `RaceSession::count_lap` now ends it at the first human finish.
- `native_drive_test` (`cc_t1`): player 1 on the test driver, player 2 idle: finished after 4338
  updates, laps 3 and 0. Control (both must finish): not finished after 18000 updates, player 1 at
  12 laps.

## The standings and championships (2026-11-12, static reading)

- State 4 (`0x4a2ea2..0x4a2ed1`): a single race (`app+0x1a60` = 0) goes to state 5 with counter 120
  (`0x78`), a championship (3) to 6 with the points of `0x52cef0` by place; nothing depends on
  `app+0x1a64`, and the rank table (`+0x7f0`, `+0x7cc` entries) holds the two players. The port's
  `finish()` builds the same table.
- The film draw `0x499b30`: modes 0 and 3 take the strip/standings branch `0x499c38`, which in states
  5..7 skips the viewport whose `+0x13a` is set (`0x499c7d..0x499c83`), so the standings appear in
  the top half only; mode 1 (battle) draws the counts (`0x499b96`); mode 2 draws nothing. The port's
  `hud2` draws them for viewport 0 only. Not compared on screen.
- `menu.WGM`'s start codes (property `0x19d`): 0..15 and 35..44; 14 is `WMenuNodeTwoPlayer` `0x147`
  (race), 15 is `0x146` (battle). There is no two-player championship or practice node, so the
  "two-player championships" of the earlier list do not exist; AUTRE COURSE's `0xe` + mode
  (`0x491d1e`) only returns to 14 or 15.

## The car page's ready texts and Return (2026-11-12)

- **SELECTIONNE.** The page's first child, WMenuText `0x384` (string 171), has the flags
  `0x10046012` of any shown text, so the port's inactive-bit rule drew it in both halves from the
  start (pre-existing). The original hides it per player: the activation `0x48e1c0` calls
  `0x48cec0`(`0x800000` and `0x1000000`, off, first 0, count 1), clearing each player's selection
  bit on child 0, and getting ready sets it again (`0x48e40d`, `0x48e447`). `MenuView::draw_page` now
  shows page child 0 in a half only while that player is ready. Checked natively: none on arrival;
  Up shows player 1's only.
- **Return.** The slot `0x48e320` readies a player on input bit `0x40` (`0x52aada`) and un-readies or
  leaves on `0x10` (`0x52aad8`), for each player's word (`0x6e42b0`, `0x6e42b2`). Binding list 3 maps
  Return to `0x40` and Escape to `0x10` on the keyboard, which both keyboard players read, so under
  Wine one Return on the page readies both and goes on to the environment page (2026-11-12, `0x146`).
  `linux_main` now treats Return as both keyboard players' ready and Escape as their back (player 1
  first: not ready, it leaves the page). Checked natively: one Return goes from `0x146` to `0x24b`.
  (Up and G, the accelerate keys, still ready each player; under Wine they gave no SELECTIONNE in this
  run, unlike the 2026-11-01 note.)
