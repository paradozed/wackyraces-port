# In-race pause menu (20261030)

## The graph (general.wgm)

The pause is a WMenu graph in `general.wgm` under `WMenuBrick` (id 195). Its `WMenu` action is the graph's manager. Classes are registered at `0x490700` (`WMenuNodePause`, name `0x52c740`, method table `0x513fb4`, factory `0x490750`), `0x48d8d4` (`WMenuNodeResumeGame`) and `0x48fec4` (`WMenuBrick`).

| Node | Page | Contents |
|---|---|---|
| `WMenuNodePause` 196 | 243 | texts 179 "JEU EN PAUSE", 147 "PRESS ENTER FOR PAUSE MENU" |
| `WMenuNode` 216 | 247 | items CONTINUER (180), EFFETS (29, `WMenuSetVolumeItem` type 1), MUSIQUE (30, type 0), VOIX (31, type 2), RETOUR DE FORCE (41, `WMenuSetValueItem`), RECOMMENCER (181), QUITTER (99) |
| 216's children | | `WMenuNodeResumeGame` 217, four `WMenuNode`s (218..222, availability 2), `WMenuNodeLoadTrack` 235, `WMenuNodeYesNo` 240 → page 293 |
| `WMenuNodeYesNo` 240 | 293 | "ES-TU SUR(E)?" (116), OUI (117), NON (118); child `WMenuNodeLoadFile` 241 |

The string ids are WMenuText property `0x4a`, from `STRINGS.DAT`. Each volume item has ten `WMenuSprite` notches.

## Native

- **Opening.** `MenuController` runs on the general scene's root, which is the graph's root, `WMenuNodePause`. Each Escape starts a new pause on that root page (see `WMenuNodePause` below); Enter opens the option node with `open_first_child`.
- **Events.** `MenuController::enter` returns `ResumeGame` for `WMenuNodeResumeGame` and `LoadFile` for `WMenuNodeLoadFile`.
- **The loop** (`linux_main`):
  - the race stops while paused;
  - Up/Down move, Left/Right adjust the volumes (`apply_volume_item`), Enter accepts;
  - Escape goes back, and from the option node resumes;
  - RECOMMENCER reloads the same course with the same vehicle, as `0x48d7b0` does (below);
  - QUITTER → OUI returns to the menu, as finishing a race does.
- **Drawing.** `MenuView` draws over the race view (`set_overlay`, no background clear). `MenuModels` has no models when the scene has no camera viewport, as in `general.wgm`.
- **Tests.** `native_menu_test` with `general.wgm`:
  - the root page has no items, and the opened page has seven;
  - CONTINUER resumes;
  - RECOMMENCER reloads the track;
  - QUITTER asks with NON selected, and OUI loads the menu file.

**Checked headless:** the menu over the `sh_t1` race (HUD visible), the restart and the quit, with its question.

**Not ported or different:**
- page 243;

Not compared with Wine beyond the earlier capture.

## Volume notches (`0x489e30`)

`WMenuSetVolumeItem` (method table `0x513674`, instance vtable `0x513698`) refreshes its display through `0x489e30` (thunk `0x4045f7`), after each change and at setup.

**Lighting.**
- The routine walks the item's children (`+0x60`, then `+0x14`) and keeps the `WMenuSprite`s with flag `0x200000` (property `0x189`).
- A threshold starts at 0 and grows by 0.1 per notch (`0x511624`).
- A notch below the item's value takes the item's colour (`+0xb0/+0xb8/+0xc0`, properties `0x190..0x192`, 255 in both files). The others keep their own (`+0xc0/+0xc8/+0xd0`, properties `0xa5..0xa7`, 64).
- Type 3 (the 0..100 value) prints its number instead (`0x4fe5e0`).

**Native.**
- **Layout.** `MenuView` advances the layout cursor after a positioned `WMenuSprite` by `0x187`/`0x188`, as it does after a text by `0x181`/`0x182`. The ten notches now stand in a row; before, they were stacked.
- **Lighting.** Each notch is lit or not at draw time from `MenuController::volume`.

This applies to the pause and to the main menu's options. **Checked headless:** the pause shows ten notches per slider.

## RETOUR DE FORCE (type 2, `0x48a6ef`)

The option callback `0x48a660` dispatches on `WMenuSetValueItem` type − 1 through `0x48aa1c` / `0x48aa04`:

| Type | Callback | Option |
|---|---|---|
| 1 | `0x48a68c` | MONO/STEREO, `0x6f457c`, strings 12/13 |
| 2 | `0x48a6ef` | RETOUR DE FORCE |
| 5 | `0x48a845` | not traced |
| 8 | `0x48a773` | METEO |
| 11 | `0x48a7ea` | POUSSIERE |

**Type 2.**
- **Toggle.** It flips `0x6f4568` (indexed by the player `0x6f41d4`); in a one-player game it flips both players' copies `0x6f4568`, `0x6f456c`, `0x6f46e0` and `0x6f46e4`. It then plays the cursor sound (`0x4041fb(0)`).
- **Display.** It shows string `0x9a` + value: INACTIVE or ACTIVE.
- **Default.** `0x496590` sets the value to 1, so ACTIVE, as in the Wine capture.

**Native.**
- `MenuSession::force_feedback` (default true) holds the value; `MenuController` toggles it and gives the value text.
- `MenuView` now keeps a value text whose `0x4a` is 0, as the pause's item 288.
- **Checked headless:** "RETOUR DE FORCE ACTIVE", then INACTIVE after Enter.
- **Not ported:** force-feedback output.

## REDEMARRER CHAMPIONNAT (`0x48c7a8`)

The node activation at `0x48c750` (WMenuNode, `+0x18` template) reads the node's code `+0x98` (property `0x19d`). With code `0x21` and game mode `app+0x1a60` = 3 (championship), it walks the page's children (`+0x60`, then five `+0x14`): the sixth item's first child's string (`+0x130`, member `0x4a`) becomes 200, REDEMARRER CHAMPIONNAT (`0x48c7db`).

The pause's option node 216 has code 33, and its sixth item is RECOMMENCER (289, text 290). Code `0x2c` calls `0x4052b3` and `0x40177b` there (not traced).

**Native.** `MenuController::set_championship_race` and `text_override`. `MenuView` labels keep their source text and ask for an override at draw time. `native_menu_test` checks the championship and single-race cases. Not checked on screen.

## RECOMMENCER: `WMenuNodeLoadTrack` (`0x48d7b0`)

The class is registered at `0x48d5a0` (method table `0x513b7c`, instance vtable `0x513ba0`, slot 21 = `0x48d7b0`). The activation does the following:
1. It sets the load mode `0x6f41cc`: 3 in a championship (`app+0x1a60` = 3), 2 in a single race (mode 0), else 1.
2. With force feedback on (`0x6f4568`/`0x6f456c`), it resets each player's device (`0x417180`).
3. It loads `0x486f90(0, −1)`, the current course's file from `0x6f41d8`/`0x6f41dc` (`0x52bef0`), through `0x431c20`.

So in a championship, REDEMARRER CHAMPIONNAT reloads the current race, not race 1. The native restart, which keeps the championship flag and the points, follows that.

## `WMenuNodePause` (instance vtable `0x513fd8`)

Constructor `0x4908d0` (thunk `0x403413`).

**Activation (slot 15, `0x490950`).**
- It sets the page's first text (`+0x130`) to 179, JEU EN PAUSE.
- In a two-player game (`app+0x1a64` = 1) it uses 198 or 199 instead, by the node's player (`+0xc0`); both strings also read JEU EN PAUSE in French.
- It then runs the generic activation `0x48c750`.

**Input (slot 16, `0x4909f0`).** It reads the player's pressed bits (`0x6e42b0`, high byte):
- bit `0x800` clears flag 2 on its object (the game resumes);
- bit `0x100` returns 2 with the option node (`+0x12a`).

So the pause first shows page 243: JEU EN PAUSE, and text 303 "PRESS ENTER FOR PAUSE MENU" (string 147, English in the French file too). The option list follows on the accept key. The earlier Wine capture showed the list, probably after Enter.

**Native.**
- Escape opens page 243, Enter the option page, and Escape on page 243 resumes.
- Which keys the two bits are is not traced: Enter and Escape are assumed.
- On the option page and on OUI/NON, Escape resumes (provisional).

**Checked headless:** JEU EN PAUSE with PRESS ENTER FOR PAUSE MENU, then the options on Enter, and the resume.

## Input bits `0x100` and `0x800`

`0x4399b0` builds each player's current input word `0x6e42a0` from the device state (`+0xc` of the device entry). Pressed bits `0x6e42b0` = current & ~previous (`0x45c205`).

| Device state | Input bit |
|---|---|
| `0x1000` | `0x100` |
| `0x2000` | `0x800` |
| `0x100000` / `0x200000` / `0x800000` / `0x1000000` | the configurable masks `0x52aad0`, `0x52aacc`, `0x52aac8`, `0x52aad4` |
| axes beyond 0x40 / 0xc0 | the direction bits `0x4000`, `0x1000`, `0x2000`, `0x8000` |

- Bit `0x100` is also the accept test of other menu pages (`0x48e932`, `0x48ea27`), whose prompt reads ACCEPTER:ENTREE. Bit `0x800` is tested only by the pause (`0x4909f9`).
- The native Enter/Escape assignment fits this reading, but the keyboard's own device-state mapping is not traced.

## The keyboard bits (2026-11-01)

**Fixed bindings.** `0x40c5a3..0x40c739` add the keyboard's fixed bindings through `0x40645b` → `0x4392a0` (player, list 0, DIK, device bit). Each one is an object (vtable `0x51139c`) that sets its bit in the device state `+0xc` while the key is down:

| Keys | Device bits |
|---|---|
| Left, Right, Up, Down | 1, 2, 4, 8 |
| Z, C, S, X | 0x20, 0x10, 0x40, 0x80 |
| O, P, Q, W | 0x100, 0x200, 0x400, 0x800 |
| `\` (0x2b) | 0x1000 |
| Return | 0x2000 |

The MenuPrefs control table follows (`0x40c73e`, when the device `0x6f4580` is −1). Through `0x4399b0`, `\` gives input bit `0x100` and Return gives `0x800`.

**Wine (2026-11-01, `cc_t1` single race, scratch prefix).**
- Escape shows the option list over the race, with no cursor movement.
- Return resumes, whatever row is shown.
- `\` makes the cursor active: Down then highlights EFFETS, then MUSIQUE.
- The JEU EN PAUSE text was not seen.

**Native.** On the root page, `\` opens the option node; Return and Escape resume (Escape is provisional). The native port still draws page 243's text.

**Escape (2026-11-01, Wine, `cc_t1` single race).**
- On the root, Escape restarts the pause: the list slides in again (the x shift seen earlier) and the timer stays stopped.
- On the option node it does nothing; the cursor stays and keeps moving.
- On OUI/NON (NON selected first) it does nothing either. Enter on NON returns to the option node with QUITTER selected.
- The cursor cycles through six rows: RETOUR DE FORCE (item 222, `0x19c` = 2 in the file) is skipped. Its availability is cleared at run time, presumably for want of a force-feedback device. The helpers `0x48cd10`/`0x48cd80` write a row's state through slot `+0x44`; their caller is not traced.
- The device bindings: list 3 (`0x40d0e6..`) maps Return to device bits `0x2000000` (accept, input mask `0x52aada` = `0x40`) and `0x1000`, and Escape (DIK 1) to `0x4000000` (back, `0x52aad8` = `0x10`) and `0x2000`. Which list is active during the pause is not traced.

**Native.**
- Escape on the root re-prepares the view; elsewhere it is ignored.
- `MenuController::availability` returns 0 for a type-2 `WMenuSetValueItem`.
- `native_menu_test` checks the six-row cycle.

**Display (2026-11-01).** The native port follows the Wine run. In a one-player game, the root (`WMenuNodePause`) is drawn with its option node's page 247 and no cursor (`MenuController::shown_page`, `cursor_shown`). `\` then opens the option node with its cursor. Page 243's texts (179 JEU EN PAUSE, 147 PRESS ENTER) were not seen under Wine in one player. The native port still uses page 243 in two players, provisional. The original code path that skips page 243 in one player is not traced.

## Two-player pause (2026-11-02)

- The trigger `0x432b15..0x432b79`: unless the manager state `+0xf30` is 5, 6, 7 or 10, player 1's pressed bit `0x800` (`0x6e42b0`) gives player 0; in two players (`app+0x1a64` = 1) player 2's (`0x6e42b2`) gives 1 and wins. With `0x6e457f` and `0x6e4585` clear, the pause brick (`+0xc` |= 2) gets the player in `+0x12b` (constructor `0x490130` sets 0); `0x4903da..0x490419` copy it into the pause nodes' `+0xc0`.
- `0x490950` then shows 198 (player 0) or 199 for text 179. English: PAUSE / GAME PAUSED / GAME PAUSED; the other languages use the same text for all three.
- Native: `MenuController::text_override` (two players), `set_pause_player(0)` (the port pauses on player 1's Escape). Checked in `native_menu_test general.wgm`.

## Player 2's pause key (2026-11-08)

- **Bindings.** `0x40645b` is called with (device, list, DIK, device bit). In list 1 (the race),
  device bit `0x2000` (input `0x800`) is bound only to Escape (DIK 1) for player 1's keyboard
  (`0x40c87d` with the MenuPrefs keys, `0x40c9ce` with the fixed keys used when player 1 has a
  joystick).
- **Player 2.** Its lists (`0x40ca65..0x40cc79` from MenuPrefs, `0x40ccb7..0x40cd9e` fixed: V, N, G,
  B, left Shift, A, X, Z, and `0x40ce03..0x40cf22`) bind the eight controls only.
- **So:** on the keyboard only player 1 pauses. Player 2's trigger (`0x6e42b2`, `0x432b15`) needs a
  joystick button on device bit `0x2000`; the joystick bindings are not traced.
- **Native:** it pauses on player 1's Escape (`set_pause_player(0)`), which matches.
- A static reading of the binding calls; not checked under Wine.

## RETOUR DE FORCE's availability (2026-11-08)

- The controls page's class has vtable `0x513e48` (constructor `0x48e77a`). Its slots `+0x38`,
  `+0x3c` and `+0x40` (`0x48e810`, `0x48e850`, `0x48e890`) call `0x48eff0`.
- `0x48eff0` reads the player's device (`0x6f4580[player]`, the MenuPrefs `+0x18`/`+0x1c`):
  - **−1, keyboard:** two `0x48cec0` calls (the rows' grey levels), then `+0x50` = `0x48cd80`(8).
    It clears row 8's availability, RETOUR DE FORCE.
  - **A joystick:** two other `0x48cec0` calls. If the device entry (`0x54190c[device]`) has bit
    `0x100` at `+0x36f` (force feedback), it calls `+0x60` = `0x48cd10`(2, 8).
- This is the caller the earlier note left open. Natively, `MenuController::availability` gives 0
  for that row (keyboard only), as the original does with a keyboard.

## The dim behind the pause (2026-11-11)

**Seen under Wine** (`analysis/menu-profiles-20261101.md`, "Selection pages against Wine"): the race
behind the pause list goes to about half (sky 0.55, sand 0.50 of the frame before; 255 → 132,
231 → 117, 115 → 58); the list itself is not dimmed.

**Original.**
- The pause key (`0x432b15`) flags the world's first child, the WMenuBrick, active (`+0xc` | 2) and
  stores the player in `+0x12b`. Its activation `0x4902b0` ends with, in one-player races only
  (app `+0x1a64` = 0), the renderer's slot `+0xd0`(1); the pause's end (`0x4905dc`) calls slot
  `+0xcc`(1). The renderer init (`0x4aebc3`) calls `+0xcc`(0).
- Direct3D vtable `0x514c74`: `+0xcc` = `0x4ae6b0`, `+0xd0` = `0x4ae6f0`. Both keep the time
  (`0x2ad475c`, in `0x2a1bb60`) and the argument (`0x2a1cc6c`); `+0xd0` sets `0x2a1cc64` = 1, `+0xcc`
  sets it to 0.
- The frame end (`0x4aef69`) keeps T = now − start in `0x2a1caf0`. The draw, slot `+0xc8` =
  `0x4ae730`, runs at the frame end (`0x4af075`) or, while `0x6e4585` is set (the pause sets it,
  `0x4903aa`), after the sprite list (`0x4b332f`):
  - with an argument and the flag 0: nothing (so `+0xcc`(1) ends the dim at once); with an argument,
    T doubles;
  - flag 1: T = 1200 − T (`0x4b0`); T ≥ 4800 (`0x12c0`): nothing;
  - cells of 20 pixels, (W / 20 + 1) × (H / 20 + 1) (`0x66666667`, `sar 3`), W and H the renderer's
    `+0x2d8a5`/`+0x2d8a9`; each cell (i, j) is a black quad (alpha in the top byte, `shl 0x18`) with
    alpha min(255, 25 × ftol(√((cols/2 − i)² + (rows/2 − j)²)) − T / 2 + 255) >> argument
    (`0x4ae8e4..0x4ae956`; 20.0 is `0x51152c`).
- So the pause's dim closes in from the edges over 600 ms and holds at 127, half black. With
  argument 0 (the init), the black opens from the centre as T grows.

**Native.** `pause_dim` (`linux_main.cpp`) draws the same cells after the race view and before the
pause menu, in one-player races, from the pause's start (Escape on the root page restarts the list,
not the dim). Measured on `dt_t1` (`:78`): after 2 s the sky, the road and the HUD are 0.50 of the
frame before; 0.15 s in (the capture lags by about a second), the corners are 0.50 and the centre
0.92. The opening wipe is ported as well (below). Wine's HUD is not all dimmed (below).

## The opening wipe at each scene (2026-11-11)

- The scene loader `0x431690` (menu, course, winner files) clears `0x6e4585` and calls the
  renderer's slot `+0x1c` (`0x4316c2`), whose Direct3D routine `0x4aeb70` sets the display, the
  brightness (`+0xc0`) and then `+0xcc`(0) (`0x4aebc3`): the wipe with no shift and no closing flag,
  drawn at every frame end (`0x4af075`) until T ≥ 4800. The black opens from the centre: alpha
  min(255, 25 × d − T / 2 + 255) is 0 at the centre after 510 ms and in the corners (d ≈ 32 cells at
  1024×768) after about 2.1 s. The other slot `+0x1c` caller is `0x4ad104`.
- Native: `screen_wipe(…, false, 0)` at the frame end in the menu and winner stages (a race is
  drawn in the menu stage), from `wipe_start`, set when the main menu is entered, a course has
  loaded, a winner scene opens, and the race ends (menu.wgm loads again). A pause clears it, as
  `+0xd0` replaces the state.
- **Compared with Wine:** a 10 fps x11grab of each build, from Enter on CHOISIR CIRCUIT (Le Grand
  Canyon 1). Grey means of a centre box, a box at (200..240, 300..340) and the top-left corner:
  - Wine: black 0.4 s after Enter (frame 5); the centre opens over frames 6..13, the box over
    14..15, the corner over 20..25;
  - native: black at frame 9 (its load is longer); the centre over 9..14, the box over 16..19, the
    corner over 24..29.
  From the first black frame, both open the box 0.7–1.0 s later and the corner 1.5–2.0 s later.
  The original starts T before its load, so a little of the wipe passes during it (provisional:
  the native start is the load's end).

## What the pause's dim covers (2026-11-11)

Wine, the frame before the pause against the paused frame (median pixel ratio by element, `cc_t1`
on the grid): lap text 0.51, the progress strip 0.50, the gadget and weapon icons 0.64 and 0.64,
the power-up gauge (with its needle) 1.0, the speedometer 1.0. So the gauge and the speedometer are
drawn after the wipe, and something in the icon boxes too (a fully dimmed element measures 0.50);
the order inside the original's sprite lists is not traced.

Native (provisional order): `RaceView::hud` draws the strip, the texts and the messages, calls
`set_late_dim`'s function, then `late_panel(0)` (the gauge and the gadget and weapon icons, which
are translucent, alpha 0.6, while uncharged) and the speedometer. Measured: texts and strip 0.50,
gauge and speedometer 1.0, icons 0.75 and 0.74 on `cc_t1`. The icons are not compared on the same
picture: before the pause, the pixel at (900, 60) is 255/206/90 under Wine and 166/143/102 natively.

A bug found here: the wipe's start of −1e9 after a pause overflowed `int(seconds × 1000)` and drew a
black frame after resuming; `screen_wipe` now clamps the elapsed time at 10 s.

## The two-player pause against Wine (2026-11-12)

Wine (`cc_t1`, two players, Escape a few seconds after the start): the option list (page 247, CONTINUER ...
QUITTER) at once, as in one player, over the undimmed split race (the dim is one-player only); no page 243
(JEU EN PAUSE, PRESS ENTER FOR PAUSE MENU). The port had shown page 243 in two players (provisional);
`MenuController::shown_page` now shows the option node's page at the pause root in both modes (the
`text_override` of page 243's first text, strings 198/199, is no longer drawn). After: the same list, at
the same x; Wine's list sits about 20 pixels higher in two players (in one player the two matched), not
traced. The code path that skips page 243 is still not traced.

**The two-player offset, narrowed (2026-11-12, later).** Wine recordings at 20 frames a second from the
Escape on, one and two players (`cc_t1`):
- The list does not slide. It is drawn at its place from the first frame of the dim or the race and
  stays there over 4 s, in both modes. So the 20 px is not an animation caught mid-way.
- The bands of rows 2..4 measured on full-size frames, as y ranges in px:

  | | EFFETS | MUSIQUE | VOIX |
  |---|---|---|---|
  | Wine, two players | 288–308 | 338–357 | 389–408 |
  | Port | 307–326 | 358–377 | 410–429 |

  A constant 19–21 px. In one player, Wine's list sits about 20 px lower than in two players (from
  quarter-size frames, ±4 px), where the port's.
- Page 247 (`general.wgm`) has `m_nPosX` 160, `m_nPosY` 80. They are id members: the values are at
  `+0x122`/`+0x12a`, the constructor `0x48b2e0` defaults 160/100, and they are read at `0x48b510`.
- The layout `0x48bab0` places the items from (x, y) and adds each item's step, as
  `MenuView` does. So the page data are the same in both modes, and the offset comes from a draw or a
  write not found.
- Not found: no writer of a page's `+0x12a` outside its read `0x48b510`. The other writers listed
  (`0x48b6a8`, `0x490a1d`, `0x490e00`, ...) belong to other classes. The only two-player tests in the
  pause nodes are `0x490435`/`0x4905c7` (the renderer's `+0xd0`/`+0xcc`, the one-player dim and
  wipe) and `0x49096f` (strings 198/199/179).

