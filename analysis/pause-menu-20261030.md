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
