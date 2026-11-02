# Wine comparison: Championship I races 2–4

The original ran under Wine on Xvfb (1024×768, French, a scratch copy of the
prefix, disc on D:). The native build ran on a second Xvfb display, muted.
Both used Dingo-Limousine. Races were started as single races, COURSE SIMPLE →
environment → course 1, so race 1 did not have to be won: LES COLLINES
JOYEUSES 1 (`sh_t1`), the town course (`dt_t1`) and AU PAYS DES FRISSONS 1
(`ct_t1`).

Each run held Up from a standstill and captured a frame per second. Captures
stay in the session scratchpad and are not committed. The Wine race had
started before the input, so its timer leads the native one; only the
elapsed time from the first input is compared.

## Matches

- Start grid views on all three courses: road, banner, scenery and the
  wooden tunnel mouth on `sh_t1`. The player is 7th at the start.
- `sh_t1`: holding Up drifts right onto the grass in both, and the car
  reaches the tunnel mouth about 4 s after the input in both. Acceleration and
  straight-line motion agree over this stretch.
- `dt_t1`: both cars hit the right-hand building at the street's bend about
  3 s after the input.

## Differences in motion (WackyAuto contact response, `0x46a4a0`)

| Course | Original | Native |
|---|---|---|
| `sh_t1` tunnel mouth | glances past the right post and keeps going along the grass; MAUVAISE DIRECTION appears when it faces the forest wall | stops against the post. Its left probe meets the post face square on (normal −1,0), its right probe the slanted cliff (−0.46,−0.89); the slide on the post removes all forward motion |
| `dt_t1` bend | stays stuck against the building | slides along the facade past the pink-awning shop |
| `ct_t1` start | slides on the snow: drifts down the slope and along the bank to a rock, still moving | hits the bank after about 3 s and stays stuck facing it |

So the original stops on the steep `dt_t1` impact but slides past the
glancing `sh_t1` one, and it lets the car slide down snow slopes. The native
slide does neither consistently. Recovering the response in the
2,468-line `0x46a4a0` pseudocode
(`decompilation/wpc/wackyauto-update-0046a4a0.c`) is the next step for
handling.

## Rendering fixed during the comparison

- **Sky.** `ct_t1` drew a plain background where the original shows its cloud
  dome. Backdrop domes differ per course: radius 2505 on `cc_t1`, about 5000
  on `sh_t1`/`ct_t1`, 6420 on `dt_t1`. The view scaled every dome by
  `cc_t1`'s radius, pushing larger ones past the 20,000-unit far plane. It now
  scales each dome by its own radius; the 93% and 10.5% placement factors
  fitted to `cc_t1` are kept.
- **`GD3dSprite`.** These were not drawn at all (155 on `ct_t1`, 205 on
  `sh_t1`, 62 on `dt_t1`). They carry the same `0x09..0x0f` sprite fields as
  `WGndSprite`. `ct_t1`'s start-line pines have serialized planes edge-on to
  the start camera, yet Wine shows them face-on, so they are drawn as
  billboards turned about their up axis toward the camera each frame. The
  pines now appear in the same place as in Wine but paler (fog/lighting not
  recovered). The original orientation routine is not traced.

- **Sprite texture orientation.** Every world sprite (`WGndSprite` and
  `GD3dSprite`) was drawn upside down: image row 0 (the top) went to the
  quad's lower corners. `sh_t1`'s trees and mountains and `cc_t1`'s cacti were
  inverted. The upper corners now take v = 0; the anchor fraction `0x0c`
  stays measured from the bottom, so `ct_t1`'s pines (`0x0c` = 0) stand on
  their base. Checked in close-up screenshots on `sh_t1` and `cc_t1`.

## Not ported, seen in the original

- HUD: weapon and gadget icons, the winged boost gauge, the racer progress
  strip.
- Pause menu on Escape: CONTINUER, EFFETS, MUSIQUE, VOIX, RETOUR DE FORCE,
  RECOMMENCER, QUITTER, with an ES-TU SUR(E)? OUI/NON confirmation. QUITTER
  returns to the player-count page.
- Falling snow and tyre tracks on `ct_t1`; AI opponents driving off.

## Not compared yet

- Lap times and complete laps. Driving a lap blind through XTest was not
  attempted.
- `sh_t1`'s ramp jumps (no way to place the car in the original).
- The championship's race-to-race flow in the original (race 1 would have to
  be won).
