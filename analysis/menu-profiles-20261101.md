# Profiles, dynamic availability and the save screen (2026-11-01)

Read from the disassembly of `WPC.EXE` (objdump). The save screen was checked
in the native build; nothing was compared with Wine in this pass.

## The profile object `0x6f4568`

The `.WSG` writer `0x004b7b40` writes five blocks of this object, and the
reader `0x004b7cd0` reads them back:

| Offset | Size | Contents |
|---|---|---|
| `+0xec` | 20 | Single races open: byte `[environment*4 + variant]`, bit 0 |
| `+0x100` | 5 | Championships 0..4 open, bit 0; `+0x104` doubles as practice's dr flag |
| `+0x105` | 1 | Satanas et Diabolo selectable |
| `+0x114` | 80 | 20 records: low 28 bits a time in 1/300 s, high 4 bits the vehicle |
| `+0x164` | 20 | Per race, bit v set when vehicle v won it |

Fresh state, `0x496590`:
- the tracks block is cleared, then variant 0 of cc, sh, dt and ct is set;
- championship I is open, the other flags and Satanas are cleared;
- the records copy `0x52cac0` | `0x60000000` (vehicle 6), and the wins are cleared;
- weather is 2 (`+0xe4`) and dust 0 (`+0xe8`).

This matches the earlier save capture, which differed from `NICO.WSG` only at
byte `0x15` (`+0x101`, championship II).

## Availability (page entry, slot `+0x3c`)

Three node classes override the layout slot. They set each row's state
through slot `+0x44` (`0x48c030`: descriptor `+0x90`), and set its colour
through `0x48cec0`: texts and sprites `0xff` when the row is available,
`0x40` when it is not.

| Class (vtable) | Callback | Rule |
|---|---|---|
| `WMenuNodeSelectScenario` (`0x514084`) | `0x490d60` | Row 4 (dr) is open if `0x496a40(+0x1a60)`: game mode 2 reads `+0x104`; the others need any dr variant |
| `WMenuNodeSelectCShip` (`0x514130`) | `0x4910a0` | Rows 0..4 are open per `+0x100+i` (`0x496af0`) |
| `WMenuNodeSelectTrack` (`0x5141dc`) | `0x4913f0` | Rows 0..3. In a championship (mode 3), only row `0x6f41dc`. Otherwise `0x496a80(env, i, mode)`: mode 2 reads `+0x100+i` (`+0x104` for dr), the others `+0xec + env*4 + i` |

The cursor skips closed rows (`0x48cab0`), as it already did for Satanas
(`0x48dde0`).

## Unlocks after a race (`0x491c98..0x491da0`)

Single race (`0x6f41cc` = 2), not battle, one player, rank (`0x6f41c4`,
1-based, from the car's `+0x1af`) at most 3: `0x496c50(env, course, rank)`.
With rank 1, it:
- opens the next variant of the environment (`+0xed + env*4 + course`, or `+0xfd + course` for dr);
- marks the vehicle in `+0x164`.

Championship, after its last race (environment 3, or course 3 for
championship 4): `0x496b80(championship, rank)`. It needs rank ≤ 3 and the
player's vehicle (`+0x1a68`) to be `0x496d90`'s winner. That winner is the
first vehicle with the most points in `+0x10d..+0x113`, or the player's
vehicle when it has as many. Then:
- championship c < 4 opens `+0x101+c` and variant c of cc, sh, dt and ct;
- c = 3 also opens Satanas (`+0x105`);
- c = 4 opens every dr variant.

`0x496b40` (all 16 cc..ct races won by some vehicle) has no effect.

`0x486fe0` (from `0x4a3533`, on a new press of the results' key once
`+0xf34` reaches 0) is the race's exit to the menu:
- In a single race (`+0x1a60` = 0), it calls `0x496b40`, then `0x496c50(env,
  course, rank)`. When the first call returned 0, it calls `0x496b40` again.
  Both results are discarded. It then loads `MENU.WGM` (`0x431c20`).
- In a championship (mode 3), `0x496d10` adds the race's points (`+0x106..`)
  to the totals (`+0x10d..`).
  - With the player's vehicle as `0x496d90`'s winner, rank ≤ 3, and the last
    race (environment 3, or environment 4 with course 3), it loads
    `win\win*.wgm` from `0x52bf54`, indexed by the vehicle (`wintur`,
    `winpen`, `winred`, `wincrep`, `winmobi`, `winarmy`, `winmean`).
  - Otherwise it loads `MENU.WGM`.
  - The winner scene is not ported.

## The file screen (`WMenuNodeLoadSave`)

Each node's `0x1bd` gives its mode:

| `0x1bd` | Nodes | Action |
|---|---|---|
| 0 | `0x111` | Load a `.WSG` profile |
| 1 | `0x118`, `0x33c`, `0x363` | Save a `.WSG` profile |
| 2 | `0x119` | Load a `.WSR` replay |
| 3 | `0x23f`, `0x11e` | Save a `.WSR` replay |

The node has no page. `0x495df0` lists the files (`0x4b78e0` → `0x4b7690`):
at most eight names, sorted, into `0x2746d50` (8 × 32 bytes), with the count
in `0x2746d4c`. The screen then opens on its list (`+0x13a` = 0).

**Input, `0x495f30`.**
- Rows wrap over 0..7 (`+0x12e`), and OUI/NON toggle `+0x132`.
- Name entry takes printable bytes up to 31, BackSpace and space.
- Back leaves the screen from every state.
- Enter depends on the state:
  - **List.** A save mode on an occupied row asks to overwrite (state 1, NON first). Loading a profile loads it (`0x4b7e30`), then shows the message (state 2). A save mode on a free row starts name entry with an empty name (state 3). Loading a replay restores it (`0x4b7a30`, `0x45aff0`) and starts the race.
  - **State 1.** OUI keeps the name for editing and flags a replacement (`0x6f4230`). NON leaves.
  - **State 3.** A name already listed on another row (`0x495ec0`) gives state 4. Otherwise the old file is deleted if this is a replacement (`0x4b7f60` for `.WSG`, `0x4b8030` for `.WSR`), the file is written (`0x4b7ca0` / `0x4b7920`), and state 2 follows.
  - **State 2** leaves; **state 4** goes back to name entry.

**Drawing, `0x495790`.** `0x4015e6` draws at x = 160, y in 320×240 space,
alignment 4 (centred). The font is the first text object of class 13
(`0x42e3a0`): `GDPsText` `0x117`, cream (248, 222, 172).
- **Title** at y 10, red: `0x92` ENTRER NOM DE FICHIER while typing, else `0x8e` CHOISIR SAUVEGARDE or `0x8f` CHOISIR FICHIER DE REDIFFUSION.
- **Rows** at y = 30 + 22i: the name, or `0xe9` LIBRE past the count; the selected row in (240, 240, 0), the others with colour 0, which draws white (Wine capture, 2026-11-01).
- **Name entry**: the row is drawn character by character from 160 − width/4, with `|` at the cursor in a colour whose red and green step ±20 between 60 and 240 (`0x52c034`).
- **Footer**: y 206 shows `0xc2` (save) or `0xc1` (load, occupied row); y 220 shows `0xcf` while typing, else `0xcb`.
- **Overwrite**: `0x8d` in red at y 40, OUI and NON (`0x75`/`0x76`) at y 90/110, `0xca` at y 220.
- **Messages**: `0x8b` (load) or `0x8c` (save) at y 110, `0xea` for a taken name, then `0x91` at y 150.

## Native port

- `game_profile`: `Profile` (the five blocks, `fresh()`, `.WSG` serialize/parse with a 126-byte check), the availability predicates, `race_won`, `championship_won`, `list_saves`, `save_directory` and atomic `save_profile`.
- Saves go to `$XDG_DATA_HOME/wackyraces/save` (default `~/.local/share/...`), never under the install directory. The original writes `save\` there.
- `MenuController`:
  - `MenuSession::profile` replaces the Satanas flag;
  - `availability()` follows the three callbacks, and `greyed()` gives the 0x40 rows;
  - `race_finished` and `championship_finished(points, rank)` apply the unlocks;
  - the file screen (`FileScreen`) has `type()` for name entry.
- `MenuView::draw_files` draws the screen. Its glyphs load in `prepare()`, since textures cannot upload during a frame.
- `wpc_input_text` gives the typed text (`XLookupString`, BackSpace as `'\b'`).

**Native choices (provisional).**
- Names are upper-cased, since the font has capitals. The key-to-character table `0x426a50` is not traced.
- An empty name is refused.
- The file is written and then renamed. The original deletes the old file first.
- The screen is drawn on the menu's clear colour. The original's background under this page-less node was not captured.

**Not ported.**
- `.WSR` replays (record and playback; load and save report "not ported").
- The `0x496e10` unlock-all cheat, and `0x496b40`'s effect.
- The `MenuPrefs` registry record.

**Checks.**
- `native_menu_test`:
  - the fresh blocks, the round trip and size check, and the predicates;
  - the unlocks by rank and points;
  - fresh availability (cc_t1 only on the track page, closed rows greyed and skipped; the championship page after Championship I is won);
  - the save screen on a scratch directory: name entry with BackSpace, a 126-byte `.WSG`, the listed slot, overwrite with NON first, a duplicate name refused, and loading.
- A native run on Xvfb (`XDG_DATA_HOME` in the scratch area) showed the list, the name entry and the SAUVEGARDE CORRECTE message, and wrote `NICO.WSG`.

**Wine check (2026-11-01).** CHARGER PARTIE with an `ALL.WSG` placed in the scratch prefix shows the same title, rows, footer and blue background as the native screen, then CHARGE AVEC SUCCES. The unselected rows are white, not cream; the native colour is fixed. With that profile, the environment page shows the dr row available and Le Grand Canyon's four courses.

## Credits and WACKY RULEZ (2026-11-01)

**The credits page.** CREDITS enters `WMenuNode` `0x21e`, start code 0x2c, which has no page.
- On entry, `0x48c7e5` loads `credit.pc` (`0x47f840`): a u32 count, then 50-byte rows. Each row is a style byte (0 name, 1 heading, 2 title) followed by NUL-terminated text, with accents in the font's own codes. The scroll `0x6f25ec` is set to 256 (`0x47f8f0`).
- Each menu update (`0x48fc54`), `0x47f910` draws 17 rows 16 apart, from row −scroll/16, at y = scroll mod 16 − 16 + 16k. Rows are centred at x 160 with `0x4015e6`'s font, in colours from `0x6f25c0`: white, yellow (255, 255, 0) or red (255, 0, 0), set at `0x47f7c0`. The scroll then decreases by 1, and restarts at 256 once the first row is past the count.

**The cheat.** Also on that page, `0x48f180` follows the 11 DirectInput codes at `0x52bfc4`: W A C K Y Space R U L E Z. Each new press of the expected key advances `0x6f4220`; any other new press restarts it. The eleventh key calls `0x496e10`, which opens every course, every championship and Satanas, and marks every race won by all seven cars (`0x7f`).

**Native.** `parse_credits`, `MenuController::credits_update` (the scroll, and the sequence from typed characters, so the word works on any keyboard layout) and `MenuView::draw_credits`. Covered by `native_menu_test`: the installed `credit.pc` has 301 rows, scroll 256 then 255, the unlock, and a wrong key restarting the sequence. A native capture on Xvfb shows the heading, title and name colours and the accents. Not compared with Wine.

## The controls page (2026-11-01)

**Nodes.** OPTIONS (node `0x8`) leads to two `WMenuNodeControllerConfig`
nodes, `0x23b` (+0x1a2 = 0, player 1) and `0x247` (+0x1a2 = 1, player 2),
both with page 560 (`0x230`) and start code 41. The page's eight
`WMenuControllerItem`s carry the control item in +0x196 (0 ACCELERER, 1
FREINER, 2 and 3 the turns, 4 CAMERA, 5 VUE ARRIERE, 6 the gadget, 7 the
weapon); RETOUR DE FORCE is a `WMenuSetValueItem`.

**Binding, `0x4969a0`.** For items below 10, when both devices (+0x18 and
+0x1c) are keyboards (-1), a key that the other player holds is refused.
Otherwise the item that had the key takes the old one, and the item takes
the new key. The pairs are (DIK code, action bit) from +0x24 (player 1) or
+0x84 (player 2).

**Native.**
- `set_control` ports `0x4969a0`.
- `dik_to_linux`/`linux_to_dik`: equal codes through F12 (0x58), and a table
  for the extended keys (0x9c..0xd3).
- `MenuController::bind_key`: Enter on an item waits for a key (shown
  `?`). The next key press binds it; Escape cancels.
- The race reads the player 1 table (`wpc_input_code_down`).
- `MenuPrefs.bin` keeps the table.

**Provisional.**
- The key names are native French names. The original asks DirectInput.
- The original's wait-for-key loop and its drawing were not traced.
- Joysticks and the controller-presence texts are not ported.

**Checks.**
- `native_menu_test` covers the mapping round trip, the swap and refusal, and
  the page reached from the root. On it: Up shown, the `?` wait, a binding,
  an Escape cancel, and player 2's G refused.
- A native Xvfb run showed the page with HAUT/BAS/…/MAJ DROITE, then W
  after binding. Escape cancelled without leaving the page. QUITTER wrote
  `MenuPrefs.bin` with ACCELERER = 0x11.
- No Wine comparison was made.

## Key names (2026-11-01)

**Original.** `0x48eebd` gets each control's name through the keyboard device's vtable `+0x1c`, which is `0x426930(dik)`:
- it finds the code's index i in the 106-entry table `0x526a4c`;
- in French (`0x6f46e8` = 3) with flag 2 of `0x526f44[i]`, it returns the French name `0x526d9c[i]` (the keypad, NumLock…);
- code `0x45` returns `0x527d14`;
- otherwise it tries `GetKeyNameTextA(dik << 16, …)` and uppercases the result (`0x403deb`), falling back to the French (language 3) or English (`0x526bf4[i]`) table.

**Arrows.** The extended codes (≥ 0x80) are not found by `GetKeyNameTextA` with that argument, so the table gives them. The arrows' entries are single font glyphs: `0x7f` up, `~` down, `}` left, `|` right.

**Native.**
- `KeyNames` reads the four tables from the installed WPC.EXE (`ExeImage::u32_at`), so the sources carry no game text.
- For codes below 0x80, the native French names stand for a French Windows' `GetKeyNameTextA`; in English the table names stand in.
- Without WPC.EXE, the native names are used.
- An Xvfb capture of the controls page shows the four arrow glyphs, then ENTREE, ".", "/" and MAJ DROITE.
- Not compared with Wine (whose `GetKeyNameTextA` follows X11).

## Option values (2026-11-01)

**Glyph advance.**
- `0x42dc50`/`0x42de80` advance each glyph by its texture width (`+0x1c`) less the text descriptor's `+0x150` less 1, times `+0x152` and the screen factor.
- `0x42dd99` uses less 4 instead when `0x5299b0`, `0x6e4585` and `0x6f5958` are all 0. Wine's widths fit less 1.
  (2026-11-09: `0x6f5958` is the renderer, 1 under Wine, as the port draws, so the -4 branch belongs to
  renderer 0 only and is not ported. `0x5299b0` is written at `0x431cbe`, `0x6e4585` at `0x4316aa`,
  `0x431fab`, `0x4903aa` and `0x490553`; not traced further.)
- The descriptor maps property k to `+0x130 + 8 × (k − 0x4a)`. So `+0x150` is property `0x4e`.
- In `menu.WGM`, `0x4e` is 4 only on the controller values #552 and #178 (CLAVIER); 0 elsewhere.

**LUMINOSITE.**
- It is WMenuSetVolumeItem #196: type `0x193` = 3, default `0x18d` 50, maximum `0x18e` 100, step `0x18f` 1. It has no notches.
- Its second child, WMenuText #212 (film 2002, serialized text `0x48` "50"), shows the value. Wine shows "50".
- Natively, the value is the rounded `MenuController::volume` (prefs `+0x20`).

**Checked against Wine:**
- "CLAVIER" now has Wine's glyph positions;
- ALEATOIRE/INACTIVE were already equal;
- the main menu's centred labels match to the pixel.
- Right-aligned labels are 5–9 px (in a 1024-wide frame) left of Wine's. That is a constant shift and is not explained.

## Controls page against Wine (2026-11-02)

- The page's GDPsText children with a string are drawn like fixed WMenuTexts: #650 string 250 at (160, 200), #551 string 251 at (160, 210) and #656 string 105 at (160, 226). They are blue (128, 128, 255) and centred. Wine shows the same three lines.
- RETOUR DE FORCE (WMenuSetValueItem type 2), label and value, is grey under Wine: 115 where the other rows' texts are 239.
  - `MenuController::grey_level` gives 0x80 there (native 124).
  - Profile-closed rows keep 0x40 (`0x48cec0`).
  - The pause does not grey the row; it only skips it.
- Open: the port draws a spinning W model over the keyboard that the Wine capture lacks. Wine's key names are English (DirectInput under Wine).

## Key help line (2026-11-01)

**Original.**
- `0x48ba10` (a page shown with flags) ORs the flags into the page and into its `+0x132` object, then calls `0x48bb20(node +0xa0)`.
- `0x48bb20(n)`: when n > 0 and the world's first child is a GDPsText (`0x40413d`), its string (`+0x130`) becomes `0xc8` + n.
- In `menu.WGM` that child is GDPsText #279 at (160, 220), cream (248, 222, 172), font FILM 2000.
- The n values come from the pages' property `0x19a`:
  - main page 4: 2, string 202 "SELECTIONNER: ~ ACCEPTER:ENTREE";
  - options page 10: 4, string 204 "SELECTIONNER: ~ ACCEPTER:ENTREE RETOUR:ECHAP".
  - Both match Wine captures. The `+0xa0` / `0x19a` correspondence is inferred from these values.

**Native.**
- `MenuView::prepare` adds the line when the page's `0x19a` > 0.
- The line is centred on x 160 as Wine shows. Its `0x4b` is `0x80800`; the WMenuText rule (`0x800` keeps x) would left-align it, so the GDPsText alignment is not decoded.

## LUMINOSITE's effect (2026-11-02)

- The slider (`0x489bb3`, `0x489d19`) stores the rounded value in `0x6f4588` (prefs `+0x20`) and calls the renderer's slot `+0xc0` (`app+0x1838`) with `x = (b × 5 − 250) × 5 × 32 = (b − 50) × 800`; renderer init does the same (Direct3D `0x4aeba0`, Glide `0x4ba206`).
- Direct3D slot `+0xc0` = `0x4b5a80` → `0x4abf50(gamma control +0x2d5f9, x)`: for i = 0..255, `ramp[i] = clamp(i × (65535 − |x|) × 1/256 + (x > 0 ? x : 0), 0, 65535)` (`0x513100`, `0x51121c`), the same table for red, green and blue, then `IDirectDrawGammaControl::SetGammaRamp(0, ramp)`. Glide's slot is `0x4bca00` (not traced).
- The ramp is linear: the output is the picture blended toward white (x > 0) or black (x < 0) by |x| / 65535 (0.61 at 0 and 100).
- Native: `wpc_vk_set_brightness` (every frame from MenuPrefs `+0x20`); `wpc_vk_end_frame` draws that blend over the whole frame. Not compared with Wine (its windowed DirectDraw may not apply gamma ramps).
