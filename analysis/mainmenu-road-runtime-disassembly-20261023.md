# Main-menu road sprite update path

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

This note follows the installed `wpc.exe` GDSprite vtable to determine whether
the seven common-group road vehicles move themselves.

## Evidence

- The two preserved original captures are independent frames rather than a
  timed sequence: `wine-mainmenu-20261022.png` shows road vehicles near x=0,
  250, and 560, while `wine-mainmenu-clean-20261022.png` shows visible vehicles
  near x=160, 300, and 560 in the 640x480 game area. They confirm that visible
  car sets differ between captures, but do not give a reliable direction,
  speed, loop period, or mapping from image to serialized object IDs.
- A fresh Wine launch produced one additional live main-menu frame at
  `wine-mainmenu-road-live-20261023.png`.
  It shows the yellow vehicle near x=195–255, a blue/red vehicle near
  x=430–505, and a red vehicle near x=538–610. The window then entered its
  automatic demo before a synchronized series could be captured, so this frame
  adds one position sample but no movement rate or identity mapping.
- A dedicated six-second Wine capture is now available as
  `menu-animation-times/wine-road-6s.mkv`,
  with twelve evenly spaced 0.5-second frames in
  `menu-animation-times/wine-road-strip-contact.png`.
  The menu remained active through the capture. Vehicle positions and visible
  sets change repeatedly while the viewport stays 640x480. Coarse scenery
  comparison across the saved frames has its best alignment at zero shift,
  indicating that current evidence supports moving vehicle layers over a
  stationary scenery layer. Individual moving silhouettes are not yet mapped
  to the seven serialized GDSprite IDs. In the first four samples, the blue,
  red, and pink cars each move left by roughly 75–85 screen pixels per
  0.5-second interval until they clip at the left edge. This is approximately
  150–170 pixels/second, or 4 logical units per 50 ms at the observed 2x
  viewport.
- The serialized menu graph places common visual group `GDStdBrick 0x1b`
  under `GDStdBrick 0x145` and top-level menu root `GDStdBrick 0x1a`.
  `0x1b` directly owns the seven road GDSprites described in
  `mainmenu-sprite-transform-20261023.md`. This confirms the resource parent,
  but not the runtime value of `DAT_005472b8 + 0x0c` used by the callback.

- The GDSprite constructor at `0x0044d2b0` installs instance vtable
  `0x00511fdc`.
- Vtable slot `+0x0c` points through thunk `0x00403643` to
  `GDSprite::Draw` at `0x0044d660`.
- Slot `+0x18` points through thunk `0x00405aa6` to `0x0041ee30`. That method
  calls base routine `0x004012a8`, then walks two child-list heads at offsets
  `+0x94` and `+0x10a`; for each child it invokes that child's virtual slot
  `+0x18`. Its visible behavior is recursive child processing. It does not
  write this object's X or Y fields (`+0x12a` and `+0x132`).
- The other nearby GDSprite vtable entries include several stubs that return
  immediately (`0x004115d0`, `0x004115f0`). No per-sprite horizontal movement
  was found in this class's update slot.
- `GDSprite::Draw` reads the serialized/current X and Y members and passes
  those values through the viewport. It does not advance them.
- A separate routine, `FUN_00494ab0`, does write the GDSprite X field. It walks
  the child list at `parent + 0x60`, compares each child's type descriptor with
  the descriptor returned by `FUN_0044cdb0`, and subtracts its first argument
  from the matching sprite's X at `+0x12a`. Its other branch similarly shifts
  another sprite type's X field at `+0x1a2`.
- `FUN_00494be0` calls that routine with a step of 4 while its byte state at
  `+0x2c` is active, increments a counter at `+0x2d`, and clears the active
  state at 40 ticks. This proves a finite 160-unit leftward sprite translation
  exists in a menu-related callback path. It does not prove that this callback
  is the main-menu road's continuous animation: the state activation and exact
  owner of this callback have not been resolved.
- The callback is vtable slot `+0x08` in the table beginning at `0x00514610`;
  its entry is thunk `0x00405f1f` -> `0x00494be0`. Factory
  `FUN_004945d0` allocates `0x39` bytes, calls the common initializer, and
  installs that table. The class's semantic name is not present in the
  recovered symbols. The second allocator `FUN_00494640` creates a separate
  0x40-byte object and is not shown to be the callback owner.
- Factory thunk `0x00403148` is a method pointer in the separate vtable
  beginning at `0x005145d8`; it jumps to `FUN_004945d0`. This links the event
  object to a virtual factory path, but the registering class and the WGM
  page/object that invokes it have not been identified. It does not establish
  that the callback runs for `GDStdBrick 0x1b`.
- Inside the callback, the 40-tick shift is activated when its progress field
  reaches a threshold read from the context at `this + 0x18`. Thus the shift
  is one bounded phase in a larger menu sequence. The threshold/phase table and
  whether this particular object is attached to main page 4 remain unresolved.
- The surrounding callback is a sequence controller, rather than a per-car
  updater: it reads its context at `this + 0x18`, advances a phase counter at
  `this + 0x35`, initializes state from a referenced WGM object through
  `FUN_00486ef0`, and selects numbered child states through `FUN_00493f10`.
  `FUN_00493f10` resolves the selected child from the global WGM object table
  and resets the sequence object's frame/state fields. When the controller
  requests a new WGM, `FUN_00494b20` chooses `DickOff.wgm` for state pair
  `(3,3)`, `MENU.WGM` for `(4,3)`, and otherwise reloads `MENU.WGM` in the
  default language path. This ties the 40-tick translation to a larger
  menu-resource/sequence transition, but still does not identify the active
  WGM parent at the moment of the shift or prove that it animates the idle
  road scene.
- The same callback advances its own progress counter at `this + 0x31`; when
  that counter equals the context's `+0x2c` threshold it sets the active-shift
  flag at `this + 0x2c`. On subsequent callback passes, the shift branch runs
  before the ordinary sequence-state work and returns until its 40 ticks end.
  So the short shift is triggered by a milestone in this object's own sequence,
  rather than by a direct per-frame update in each GDSprite.

## Consequence

GDSprite's own update method does not move the cars. A separate menu sequence
controller does shift matching children by four logical units for 40 updates,
but its attachment to page 4 remains unresolved. The video independently
supports a shared leftward speed near 160 screen pixels/second. The native
renderer uses 8 screen pixels per 50 ms tick. A long Wine road-strip search
finds its closest repeat at 8.233 seconds; the former 1200-pixel native period
repeated at 7.5 seconds. The renderer now wraps the seven anchors over 1320
pixels, or 165 measured steps, predicting an 8.25-second period. The 60-unit
margin beyond the serialized `-280..320` logical anchor range remains an
inference, as do per-sprite visibility rules. An RGB absolute-difference search
over 30 fps frames of the road strip (y=80..111) found its minimum at 8.233 s
for Wine (mean channel error 6.01; the 7.5 s candidate scores 19.03). The old
native strip instead favored 7.5 s (3.38 versus 18.66 at 8.233 s). A new
current-native capture favors 8.233 s (4.80 versus 18.38 at 7.5 s), confirming
that the longer period reproduces the Wine road cycle. The capture is
`menu-animation-times/native-menu-road-period-20261023.mp4`. A six-second
native capture at matching 0.5-second sampling
intervals is preserved in `menu-animation-times/native-road-review-01.png`
through `native-road-review-12.png`, with contact sheet
`menu-animation-times/native-road-review-contact.png`. It confirms leftward
motion and repeated re-entry at the right edge. A coarse all-pairs search over
the road strips identifies a continuous alignment: Wine samples 6–12 match
native samples 1–7. Comparing saturated vehicle pixels in each of those seven
pairs prefers zero horizontal shift. The matched frames show the same vehicle
groups at the same positions across the full three-second overlap, including
cars entering from the right edge. This supports the native direction, speed,
and wrap behavior. Capture start times were not synchronized, so it does not
recover the absolute menu-entry phase. The controller-to-page link remains
inferred.
The visual pairs are shown in
`menu-road-phase-alignment-20261023.png`.

The function addresses and fields above are static disassembly observations;
they do not establish which parent/controller is active on the menu page.

## WSpriteMove recovered (2026-11-01)

The road cars move themselves after all, through their `WSpriteMove`
action. The callback discussed above is a different class: `0x494be0` is
`WWinnerMovie`'s update (`m_nWinnerMovieType`, `m_nStartMoveIn`,
`m_nStartOtherSpeak`), the championship winner scenes `win\*.wgm`. Its
4-unit, 40-update shift moves those scenes' sprites, not the menu road.

**The class.**
- Its record is `0x6f3498` (`0x492a50`, record vtable `0x514464`).
- Its members are registered at `0x492c90..0x492e1f`: m_nStartPosX, m_nEndPosX, m_nRandomIntervallum and m_nIntervallumMin, all ints, and m_nStepMultiplier, a float.
- The template constructor `0x492e50` (0x48 bytes, vtable `0x5144c0`) sets the defaults at `+0x24`/`+0x2c`/`+0x34`/`+0x3c`/`+0x44`: 320, 0, 10, 1 and 1.0.
- The runtime object comes from `0x492aa0`: 0x2c bytes, vtable `0x51448c`. It holds the sprite at `+0x10`, the template at `+0x18`, the float X at `+0x24` and the step at `+0x28`.

**Init and update.**
- Init, `0x492f10` (slot `+0x10`): X = the sprite's `+0x12a`; step = (`rand()` % m_nRandomIntervallum + m_nIntervallumMin) × m_nStepMultiplier. `rand()` is the shared `0x430e40`.
- Update, `0x492f80` (slot `+0x08`): X −= step. When `_ftol(X)` ≤ m_nEndPosX, X = m_nStartPosX and a new step is drawn. The sprite's `+0x12a` = `_ftol(X)`.

**menu.WGM values.** All seven road cars (GDSprites `0x140`, `0x12`, `0x11`, `0x36b`, `0x369`, `0x36a`, `0x366`) carry the same action: 0x1b3 = 320, 0x1b4 = −380, 0x1b5 = 1, 0x1b6 = 3 and 0x1b7 = 1.0.
- `rand() % 1` is always 0, so every step is 3 logical units. The "random intervals" never vary.
- A car loops in 700 / 3, that is 234 updates.
- At the native 30 updates a second, that is 7.8 s. Wine's measured 8.233 s period gives ~28.4 updates a second. That agrees with the attract timing: 800 updates in 27–28 s.

**Native.**
- `SpriteMove` (`menu_animation`) is kept per sprite in `MenuView::road_` across pages and advanced once per menu update. It replaces the measured 8-pixel, 1320-pixel wrap.
- `native_menu_test` checks the step of 3, the first restart, and the 234-update loop.
- A native Xvfb capture shows the cars moving left and re-entering on the right.
- The original's menu update rate (≈28.4 Hz under Wine against the native 30) is not resolved.

**Update rate (2026-11-01).** `0x4ad540` reads the time (`0x2ad475c`) at each frame and stores ⌊Δms × 0.06⌋ (`0x514c68`), clamped to 2..4, in `0x5299b4`. That is the race motion's frame factor (`frame_time_factor` natively). The menu objects, `WSpriteMove` among them, step once per frame without it. Their speed therefore follows the frame rate: about 28.4 Hz under Wine. The native port runs them at 30 Hz, the factor's lower bound (2/60 s).
