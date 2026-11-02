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
