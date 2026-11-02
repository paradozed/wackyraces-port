# Race HUD resource map

## Observed scene data

The bounded native inspector reads `general.wgm` as a 6,116,072-byte WPCG
archive and finds one `WHudBrick` instance, object `0x1a4`. A direct bounded
walk of the installed BRTR tree finds two `GDSprite` children:

| Object | GMAT | Serialized X/Y | Serialized width/height | Current role evidence |
|---|---:|---:|---:|---|
| `0x1a5` | `0x10193` | `(160, 50)` | `(32, 24)` | Dial; GMAT `0x10193` is the 124x48 speedometer material recovered by the native HUD loader. |
| `0x1a7` | `0x10194` | `(160, 47)` | `(6, 24)` | Needle; GMAT `0x10194` is the 20x64 needle material recovered by the native HUD loader. |

The WGM records also include render property `0x1a` values `0x10046012` and
`0x10040012`, RGB values of 255, alpha zero, and scale 1.0. The WHudBrick's
serialized transform translation is approximately `(-18584.502, 0, 67065.82)`;
both child transforms are identity. These are direct archive observations.
The sprite coordinates, dimensions, and render-state fields have not yet been
mapped to screen-space destination rectangles or source-rectangle cropping.
In particular, interpreting `(160, 50)` as the dial's top-left screen pixel
does not match the Wine race capture.

## Executable draw path

The `WHudBrick` vtable at `0x005132d0` routes slot `+0x0c` through thunk
`0x0040106e`, wrapper `0x00486670`, and `0x00402e41` to the body at
`0x00485b00`. That body reads the selected participant's speed value from the
runtime HUD table and the maximum-speed field at `owner+0x88`; it computes
`angle = speed * PI / max_speed - PI/2`, then caps the upper angle at the
executable constant `0.27000001` radians. It writes the result to the needle
sprite's dynamic angle field. These operations are visible in the disassembly
dump at [`disassembly/wpc-whudbrick-draw.txt`](disassembly/wpc-whudbrick-draw.txt).

The native race state uses the executable-derived normalized speed target
table. The renderer now applies the recovered angle equation using that target
as `max_speed`; equivalence with the runtime object's `owner+0x88` value is
not yet established. It preserves the prior image-space pivot and fitted HUD
rectangles while that remaining coordinate mapping is investigated.

The same draw routine selects an 80-byte HUD layout row from the static table
at `0x0052bd5c`. With an active participant it selects row 1 or row 2; both
begin with sprite positions `(160,50)` and `(160,47)`, matching the serialized
dial and needle origins in `general.wgm`. The no-participant row begins with
`(263,207)` and `(280,217)`. The `WHudBrick` itself is a BRTR root and has no
serialized parent; its WGM properties do not include the runtime `owner+0x88`
field. Thus the normal-race position mismatch is downstream of the HUD's
selected logical coordinates, in the active viewport/clip or draw-record
transform. This narrows the remaining coordinate investigation.

Static disassembly identifies `FUN_0044d660` as the `GDSprite` draw method;
the GDSprite vtable at `0x00511fdc` points to it through thunk `0x00403643`.
The method sends the serialized X and Y through the active graphics viewport's
virtual X/Y mapping functions, then subtracts the caller's clipping origin.
This confirms that sprite positions depend on the active viewport and clip
context, rather than being raw output pixels. The method also reads the
sprite's render properties and the resolved texture surface dimensions before
submitting the draw record through graphics vtable slot `+0x44`.

The call path establishes the coordinate transform and draw context, but the
exact `GDSprite` field-to-draw-record mapping and which viewport/clipping state
is active for `WHudBrick` have not been recovered. The Wine screen capture is
not synchronized with a debugger capture of those values. Applying the main
menu's 320x240-to-640x480 transform to the HUD's `(160,50)` would place the
dial at `(320,100)`, which does not match its observed lower-right position;
that mismatch is evidence that the HUD uses another context or another
position-selection path, not grounds for a hand-tuned alternative transform.

The original 640x480 race frame shows the speed dial at the lower right. The
native race renderer already decodes both material textures from `general.wgm`
and draws the dial and a rotated needle at manually fitted rectangles. That
proves the artwork can be loaded. The angle equation is now recovered, but the
native renderer still uses fitted screen rectangles and a fitted image-space
pivot. Recover the active HUD viewport, the needle pivot, and the runtime
meaning of `owner+0x88`, then replace those approximations with the serialized
WHudBrick/GDSprite behavior.

`general.wgm` also has one `WCarFxStorage` (`0x197`), 28 `WPowerUpSprite`
instances, 30 `WMenuSprite` instances, and seven other `GDSprite` instances.
These belong to other parents and should not be folded into the HUD without a
class/linkage trace. The exact race-frame evidence and current game-loop status
are in [`race-gameplay-and-worlds.md`](race-gameplay-and-worlds.md).
