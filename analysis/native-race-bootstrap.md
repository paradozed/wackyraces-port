# Original race bootstrap: participants, grid and camera

Target: installed `WPC.EXE`, SHA-256
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`.
Findings come from the existing Ghidra export, focused radare2 disassembly,
executable data reads, and installed/extracted WGM bytes. They are static
recovery, not original-runtime spawn observations. This document does not
implement the bootstrap or establish playable racing.

## Corrected scene inventories

Earlier race notes walked BRTR children without decoding ACTN and described
`cc_t1.WGM` as lacking `WModManager`. The complete walk finds
**WModManager action ID 8 attached to course root ID 1**. Its serialized
property names are established directly by the `CLAS/PRIM` name/owner pairs:

| Property | Reflected member | `cc_t1` value |
| --- | --- | ---: |
| `0x1e8` | `m_MainBrickID` | 0 |
| `0x1e9` | `m_idStartLineModel` | `0x3d3` |
| `0x1ea` | `m_nStartWait` | 0 |
| `0x1f5` | `m_nMaxRoboNum` | 0 |

`m_idStartLineModel` is not the start marker or a WMOD resource link. The
course's actual `WStartLine` brick is ID **0x44**.

The shared `general.wgm` already supplies two race-follow cameras:

| Camera brick | Action | Class |
| --- | --- | --- |
| `0x1b7` | `0x1b8` | `WFollowCamera_2` |
| `0x1ba` | `0x1bb` | `WFollowCamera_2` |

Both cameras serialize FOV approximately **0.959931 rad (55 degrees)**,
integer ratio **100**, near plane **50**, and far plane **20000**. A separate
shared `GDCamera 0x12d` exists under folder `0xc3` with different projection
settings; it must not be chosen merely because it appears first.

`cc_t1` SCRT supplies `GDViewport 0x14f` and `0xeb`, both with serialized
camera reference zero. The original bootstrap binds shared follow-camera
instances at runtime. The course's `WTrackCamera` nodes are separate camera
candidates, not evidence for a replacement chase camera.

## Shared participant templates and registry

The shared `general.wgm` WackyAuto templates and their serialized property
`0xed` vehicle enum are:

| Vehicle enum | Template node |
| ---: | ---: |
| 0 | `0x28` |
| 1 | `0xaf` |
| 2 | `0xc8` |
| 3 | `0xe1` |
| 4 | `0xb` |
| 5 | `0x90` |
| 6 | `0x76` |

The registry scan **0x00483a20** clears eight racer slots at `0x006f2ce0`,
then visits runtime object IDs below `0x600`. Objects matching the WackyAuto
class getter **0x00464540** are placed into the slot selected by their
descriptor's vehicle value at `+0x88`. A duplicate vehicle instance can occupy
the reserved extra slot `0x006f2cfc`, supporting two players choosing the same
vehicle. The scanner also finds WModManager, WFollowCamera_2, HUD/controller
objects and associated AI actions.

Roster routine **0x0049d580** selects the primary racer from the enum stored
at application `+0x1a68`, through the registry. The secondary enum is at
`+0x1a6c`; application `+0x1a64` is zero for one player and one for two.
For a normal one-player race it retains other occupied racers up to the
configured opponent limit, stores them in manager records beginning at
`+0x60` with stride 12, and shuffles their order. Grid position therefore does
not imply a fixed opponent vehicle enum. The exact RNG lifecycle has not been
reconstructed here.

## Manager initialization and WMOD dependency

The runtime WModManager constructor is **0x0049d2d0**; its small serialized
descriptor constructor is **0x0049d3d0**. Initialization method
**0x004a0550** reads application `+0x18`, checks that object's resource class
FourCC is `WMOD`, and stores it at **manager+0x2c**. It checks that the WMOD
has at least three route records and more than two transform entries.

This method overrides descriptor `m_nStartWait` to **60**. For race and
championship modes, it overrides `m_nMaxRoboNum` to
`*(int*)0x0052ceb0 - 1`, which is **6** in this executable. Serialized zeros
in the course action are consequently not the effective startup values.

The main initialization call sequence is:

```text
0x004a0550
  0x004a8450  prepare WMOD route-linked objects and their transform groups
  0x004a3ba0  accumulate route state
  0x0049d580  select and order participants
  0x0049dc90  select route state, start marker, viewports
  0x0049eb70  write participant spawn matrices
```

This is substantially more state than copying the first visible track mesh
and selecting its first roadside camera.

## Route-selected start marker

For normal race/championship, **0x0049dc90** selects the first route object
from the WMOD route-pointer table. The active route's first nested record is
stored at **manager+0x8d0**. It scans that record's encoded linked-object list:

```text
count = *(u32*)(route_row + 0xce4)
links = *(u32**)(route_row + 0xce8)
for each encoded link:
    if (link & ~3) != 0 and (link & 3) == 0:
        object = lookup_runtime_id(link >> 2)   // helper 0x0049b0e0
        if object matches WStartLine class getter 0x0049a270:
            manager.start_line = object       // manager+0x30
            break
```

The code therefore selects a start line linked to the chosen WMOD route,
rather than selecting the first `WStartLine` in an arbitrary scene walk.
The native WMOD reader still needs to expose these route-row object links
and runtime transform-group assignments before that selection can be fully
reproduced. A sole course marker is consistent with the selected marker but
does not, by itself, verify the WMOD join.

If no linked marker is found, the code uses a WMOD route-associated frame.
That fallback and battle route-frame selection are distinct paths and should
not be replaced with an invented start-line transform.

## Exact normal-grid matrices

Spawn writer **0x0049eb70** has a concrete WStartLine-present branch. It copies
the marker's active affine into a temporary 13-word matrix, preserving its
basis, then changes its translation. The recovered affine helpers are:

| Address | Operation |
| --- | --- |
| `0x004200e0` | Obtain active object transform |
| `0x0043bea0` | Copy complete 13-word runtime affine |
| `0x0043bed0` | Extract X basis |
| `0x0043bf30` | Extract Z basis |
| `0x0043bf60` / `0x0043bf90` | Read / write translation |

The executable's seven-entry float tables are:

```text
0x0052ceb8, lateral: [-450, -150, 150, 450, -300,    0,  300]
0x0052ced4, forward: [-300, -300,-300,-300, -600, -600, -600]
```

For marker basis columns X/U/Z and translation P, grid entry k is:

```text
M = marker affine
candidate = P + lateral[k] * X + forward[k] * Z
```

The basis is **not** refitted to a path or replaced by an arbitrary heading.
In the normal one-player seven-racer case the primary player uses **k=6**,
while the six ordered opponents use **k=0..5**. Training uses **k=2**.
Two-player branches have their own placement rules and special global-state
cases; the one-player rule must not be applied to them indiscriminately.

Before committing a WStartLine-based candidate, the writer makes a vertical
ground query **0x004a4830** from `candidate + (0,150,0)` down to `candidate`,
using query mask **0x018000ff**. On a hit it uses the reported hit point;
otherwise it retains the candidate. The actor write is explicit at
**0x0049f09a..0x0049f0a0**: destination **primary_actor+0x2c**, complete affine
copy via thunk `0x0040644c -> 0x0043bea0`. It then marks the actor/descendants'
transforms dirty through **0x0041eeb0**. Opponent records receive analogous
copies and control/progress initialization.

For `cc_t1`, serialized marker `0x44` has:

```text
X = ( 0.989176691, 0, 0.146730438)
U = ( 0,           1, 0)
Z = (-0.146730438, 0, 0.989176691)
P = (3246.199707, 1012.059326, -6313.069336)
```

Thus its primary-player **pre-query** translation is approximately
**(3630.990977, 1012.059326, -6862.556219)**, with the marker basis unchanged.
This is a derived value from the original table algorithm and file transform,
not a runtime-captured or ground-snapped final position. The actual final
matrix depends on the original ground query result.

## Follow-camera binding and initial pose

The registry scanner identifies WFollowCamera_2 runtimes through class getter
**0x004770c0**, stores them at `0x006f2d04/0x006f2d08`, and orders them using
their parent-camera sibling relationship. It discovers the race viewports in
the renderer's viewport list, then calls **0x00453e90**. At
**0x00483d76..0x00483d7f**, the first call passes the parent camera's ID wrapper
at `[follow_action+0x10]+0x1c`. The binder sets viewport camera reference
`+0x152`, resolves it through the object registry, and stores active camera
at viewport **+0x186**. This supplies the missing runtime link absent from
the course's zero camera-reference fields.

The WFollowCamera_2 runtime vtable is **0x00512d88**. Its initialization slot
`+0x10` reaches **0x00477ea0**, and update slot `+0x08` reaches
**0x00478070**. Initialization chooses the selected participant from the
vehicle registry, sets its camera-owner pointer, overrides several serialized
offsets according to player mode, and calls **0x00477b60(1)** to build the
initial camera matrix.

For one player, the effective initial eye offset is **(0,400,-900)**, look-Y
scalar **150**, and look-Z scalar **0**. These are initialization outputs,
not guesses from the old serialized camera position. Method `0x00477b60`
uses the actor affine at **actor+0x8eb**:

```text
eye = actor_frame.point(0,400,-900)
aim = actor_frame.position
      + 150 * (actor_frame.U + world_up)
      + 0 * actor_frame.Z
forward = normalize(aim - eye)
right   = normalize(cross(actor_frame.U, forward))
up      = normalize(cross(forward, right))
camera  = affine(right, up, forward, eye)
```

The normalization and cross-product bodies are **0x00452e20** and
**0x00452ed0**. Camera construction writes the parent camera's affine at
`+0x2c` through **0x0043aec0**, then marks its transform dirty.

WackyAuto method **0x00468620** maintains the actor's `+0x8eb` follow frame,
copying a complete 13-word affine when its argument equals one. It can use
an alternate yaw path depending on actor state. A native bootstrap must
establish that frame and its initialization order; assuming `+0x8eb` always
equals the visible car frame omits a real state dependency. The ongoing
follow update at `0x00478070` includes additional interpolation/state behavior
not ported by merely reproducing this initial pose.

## Remaining verification

The current evidence establishes the shared actor/camera sources, roster
selection, concrete grid offsets and writes, ground-query gate, WMOD-linked
marker selection, and viewport-to-follow-camera binding. It does not yet
prove the full serialized WMOD route mapping, the original ground hit for a
specific initial car, exact RNG ordering, two-player special branches, actor
follow-frame initialization timing, or frame-matched camera updates. Those
requirements remain open before a native race bootstrap can be called faithful.
