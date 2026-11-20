# WGeyser's model, fade and sounds (2026-11-18)

Follows `analysis/lap-line-geyser-20261106.md` ("Provisional or not ported": the model's own update `0x4c9850`,
`0x4c99a0`, the fade, the sounds).

## WGeyserModel

- Vtable `0x5156ec`. `0x4c9850` (slot 0) is only the destructor; the model's own code is slot 3, `0x4c99a0`.
- The data uses it for WGeyser (cc_t3 #23, dr_t4 #877) and WFountain (dt_t2 #795), as their first child.
- `0x4c99a0` draws the model **three times** when the owner (`[+0x10]`) is active (flag 2). Copy i, from the matrix
  it is given:
  1. turned about y by `[owner + 0x16f + 4i]` (`0x403279` -> `0x43b4a0`, the y turn the logs use);
  2. its rows scaled (x, y, z) = (0.75 x `[owner + 0x163 + 4i]`, `[owner + 0x157 + 4i]`, the same as x)
     (`0x402027` -> `0x43b190`; `0x511224` = 0.75);
  3. drawn through the renderer (`[0x5472b8] + 0x1838`, vtable `+0x40`).

## The owner's fields

- **The shared start** (slot 1, `0x4c84c0`, both WGeyser and WFountain), once at load. For i = 0..2:
  - the filter states `+0x13f`/`+0x14b`, the values `+0x157` and the latch `+0x163` are set to 1.0;
  - the turns `+0x16f` are set to i x 0.5235988 (`0x51563c`): 0, 30, 60 degrees;
  - then the filter's coefficients are built (`0x403cf1`).
  So both start at full height and width. The port started them at 0.
- **WGeyser state 2** (`0x4c910a`): each value goes to `+0x157` and also `+0x163`; its turn grows (i + 1) degrees
  (`0x511518`).
- **WGeyser state 3**: the values move towards 0.6, the turns keep growing, and `+0x163` is not written. At its end
  (`0x4c9474`) the values, the filter states and the chosen car `+0x19b` are cleared, not `+0x163` or the turns.
- **WFountain** (`0x4ca355`, `0x4ca3ec`): the values and the turns only. Its `+0x163` stays 1 from the start.

## The fade

Renderer 1 only (`[esp+0x1c]`: the column's vertex data when `0x6f5958` = 1, 0 otherwise).
- State 3 (`0x4c93ad..0x4c9460`): f = min(counter x 255 / 50, 254), with the counter after its increment (the magic
  `0x51eb851f` is the division by 50).
  - The model's material `+0x60` is set to (255 - f) x 1/255 (`0x513014`).
  - Every vertex colour is set to `(255 - f) << 24 | 0xffffff`.
  - The mesh gets flag `0x40`, and `+0x14` is set to 4.
- At the next aim (state 0 to 1, `0x4c8f63..0x4c8f9a`): the material's `+0x60` = 0 and every vertex 0xffffffff
  (white, opaque), the flag set again.

## Sounds

None.
- The update calls no sound routine.
- At the change to state 2 (`0x4c9031`) it activates `[+0x94]`, the brick's first action (`0x404edf` -> `0x41ba60`).
  In the data that is the geyser's **WPalAnim** (cc_t3 #22, dr_t4 #876), whose step has no visible effect
  (`analysis/course-objects-census-20261117.md`).
- Neither geyser has a sound node.

## Native

- `Geyser::model3`, `wide` (+0x163), `turn` (+0x16f) and `alpha` (-1 until the first eruption).
- The start values are set at load for WGeyser and WFountain.
- `column_frames()` gives the three copies; `RaceView` draws them. Once `alpha` is set, it draws a copy of the
  column's batches with white vertices of that alpha, translucent.
- WVolcano (dr_t2) is not one of these classes, so it is unchanged.

## Checks

- Captures on dr_t4, from 700 north of the geyser facing it. Wine: `gcap2_dr4.sh`, the car hopped there; native:
  `WPC_PLACE`. Both show the column rising, standing as overlapping cloudy copies of the same width (about 110 px of
  a 400 px tile), fading translucent, then gone. The phases fall at other times in the two builds, so the
  comparison is by eye, not by pixel.
- `native_drive_test`: all 20 courses exit 0 (dr_t4's Wine lift profile unchanged; dt_t2 as before: 1 robot
  trapped at the fountain). ctest 14 of 14.
- Not compared: the first eruption after the load (the filter starts from 1, not from 0), and the material's `+0x60`
  as such (only the vertex alpha is drawn).
