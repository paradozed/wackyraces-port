# Main-menu sprite transform findings

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

This note records the correction to the native road-sprite transform, based on
the installed `menu.WGM`, the installed `wpc.exe`, and a native render using
only decoded WGM assets. The user-provided `analysis/wpc-mainmenu.png` was not
loaded by the renderer.

## Executable and resource observations

- `GDSprite::Draw` is `FUN_0044d660` in `wpc.exe` (instance vtable slot `+0x0c`).
  It maps the serialized X and Y origins through the active viewport helpers
  at `0x00454130` and `0x004541a0`, then subtracts the active clip origin.
- The live main-menu viewport capture records a 320x240 logical scene mapped
  to a 640x480 client, with zero transform offsets. The common menu `GDSprite`
  offsets in `menu.WGM` are zero. Thus the observed page-4 origin mapping is
  `screen_x = 2 * sprite_x` and `screen_y = 2 * sprite_y`, before any clip
  subtraction.
- The `GDSprite` constructor `FUN_0044d2b0` initializes its scale field to
  `1.0`; the seven common road sprites serialize the same value in property
  `0xa9`. Their decoded TGAF source sizes range from `56x70` to `132x48`.
- The seven serialized car origins are `(-280,28)`, `(-210,25)`, `(-80,22)`,
  `(60,20)`, `(120,16)`, `(170,23)`, and `(320,17)`. Mapping them by 2 places
  four origins at or beyond the horizontal viewport boundary, so they are
  clipped by the 640-pixel client.
- The selected 26-frame GDFilm2 queue has exactly one placement in every
  frame. Each placement starts at `(-62,61)` and measures `79x72`, `79x74`, or
  `79x76`. This queue changes Satanas's head image; it contains no arm/body
  pose animation. The per-frame native composition sheet is
  `analysis/mainmenu-film2-frame-sheet-20261023.png`.

## Native change and comparison

The native renderer previously placed the seven cars at `sprite_x + 320`,
`2 * sprite_y + 18` and halved each texture. Those values were a visual fit,
not values recovered from WPC. The renderer now maps their origins through the
observed viewport and retains the default scale and source dimensions. This
produces three visible cars rather than seven undersized cars. The output is
`analysis/native-mainmenu-viewport-20261023.png`.
`analysis/menu-viewport-comparison-20261023.png` places that native renderer
frame beside the supplied reference at the same 640x480 canvas size.

## Remaining unknowns

The original static preview at
`analysis/native-mainmenu-viewport-20261023.png` showed three cars at a
different phase from the supplied reference. That preview predates road
animation. The later six-second Wine sequence shows the cars moving left, and
the six-second native sequence reproduces those positions: a road-strip search
aligns Wine samples 6–12 with native samples 1–7, with zero horizontal shift
preferred in the vehicle-colored pixels for all seven pairs. This supports the
current 160-pixel/second motion estimate and right-edge re-entry behavior.
A longer 30 fps Wine capture now shows the road strip repeating most closely
at an 8.233-second lag. The previous native renderer repeated at 7.5 seconds,
confirming that its 1200-pixel period was too short. The native renderer now
uses 1320 pixels: 165 eight-pixel steps at 50 ms predict 8.25 seconds. This
matches Wine within one capture frame. The 60-logical-unit margin beyond the
serialized anchor span and the absolute menu-entry phase remain inferred.

## Follow-up hypothesis from the supplied 640x480 reference

The supplied reference shows a clipped vehicle at the left edge and vehicles
near x=136, x=350, and x=490. The WGM origins mapped by the recovered 2x
viewport produce x values `-560, -420, -160, 120, 240, 340, 640`. The native
renderer moves these anchors left by eight screen pixels per 50 ms and wraps
them over 1320 pixels. The measured period matches the long Wine capture; its
60-logical-unit margin and initial phase remain inferred. Recover the sequence
controller's attachment and compare per-sprite visibility rules against
synchronized captures.
The raised arm is absent from the selected 26-frame GDFilm2 head queue, but its
art is present in `FILQ 0x7cf` sequence 12 (`REEQ 0x79d`). The C port now layers
that 19-frame gesture sequence over the head queue using the shared anchor and
inferred speed 0.5. Its rendered raised-claw pose matches the supplied reference;
sequence 12's selection by the original WPC update path remains inferred. See
`analysis/mainmenu-satanas-gesture-20261023.md` and
`analysis/menu-gesture-comparison-20261023.png`. Car movement/visibility and
the gesture's original scheduling remain open, so the complete main-menu
animation is not yet matched.
