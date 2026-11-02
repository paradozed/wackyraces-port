# Native race: Championship I, race 1 (`cc_t1`, LE GRAND CANYON 1)

Same-state Wine runs (Wine 11.20, 1024x768, French) confirm the flow:
Championnat → CHAMPIONNAT I → car → PISTE SUIVANTE / LE GRAND CANYON 1 /
APPUYER SUR ENTREE → `cc/cc_t1.wgm`. The single-race page also names course 1
"LE GRAND CANYON 1". After a single race's results the race restarts (in the
observed run, with rain). Captures stay in the session scratchpad; game images
are not committed.

## Menu path

`WMenuNodeSelectCShip 0x380` (page 667) owns car node `0x381`. Its children are
one `WMenuNodeSelectTrack` per championship; Championship I is `0x149`, page 518
("PISTE SUIVANTE"), with four `WMenuNodeLoadTrack` children. The native
controller now enters the championship page and routes that car page to the
selected championship's track node. It issues `LoadTrack` for Championship I
race 1 with environment 0 / course 0. The course table for later races and
championships is not recovered, so those requests remain pending.

## Course data

| Item | Source | `cc_t1` value |
|---|---|---|
| Laps | `GDWorld` `m_nLapNum` (`0x25`; `0x21..0x27` = PlayMode…IsGhost) | 3 |
| Sections | WMOD routes Snake, Triple, Seadman; row `ground_object_id` → `WGndBrick` → `0x174` WGNR | bricks 71/481/635, WGNR `0x291..0x293` |
| Ground frame | WGNR is brick-local; brick 71 is at y 448, so local 564 = start line 1012 | |
| Portals | route `+28/+30` | 70→69, 472→463, 634→632 |
| Loop | WMOD transform carrying exit 632 onto entry 70 | +73386.5, −0.13, −31976.5 |
| Start | route-zero `WStartLine` | `0x44` |
| Fog | `0x4ae060`: D3D linear fog from far×0.5 to far; Glide `0x433930` colors fog with the viewport background | 10000–20000, (222,165,82) |
| Sky | `GDViewport m_BackDrop` (`0xc8`) | `GDModel 0x19`, 5000-unit dome |

WGNR record flags seen on the course: `0x2` road and sand, `0x1` banks, `0x4`
the water surface at world y 560, and `0x800000` the riverbed below it. `0x4`
crosses the Snake road twice, so it is drivable. The car drowns on
`0x800000`: in Wine, holding Up runs into the river at the first bend and the
car vanishes in a splash, then returns near the start about 1.5 s later.

`0x482b30` tests a quad with polygon bit `0x20000000` as two sub-polygons with
consecutive planes. The native query had marked these incomplete. That made
gently sloped road read as holes and dropped the car through the world.
Triangles (0,1,2)/(3,1,2) are now tested against plane and plane+1.

## Vehicle (per 33 ms game frame)

`0x4ad540` busy-waits until at least 33 ms (`0x21`) have passed since the frame
started, then runs update and render. Wine measured 27 frames/s.

- Speed `+0x3ef`, keyboard branch of `0x466db0`. Below top speed: from
  reverse +0.023; otherwise `+= (max(top,0.8)−speed)×row[1]`. At or above top:
  −0.0096 and clamp to top. Brake: −0.023 to 0 (state 1), hold, release →
  state 2, then reverse toward −0.4. Coast: ±0.0096, stop under 0.015.
  `0x5129c4/cc` (±1/60) only scale the analog branches.
- Steering `+0x403` (`0x467640`): ±per-vehicle step (`0x4660b0`), clamp ±40°,
  ×0.8, deadband 0.017.
- Motion (`0x46a4a0`): displacement = (front probe − rear probe) × speed ×
  `+0x163`. Probes are from `0x468280` mode 2: ±102 longitudinal, the front one
  stretched by 1+0.09(speed−0.4) above 0.4. `+0x163` is
  `int(ms×0.06)` clamped to 2..4, ×0.4667, max 2 (`0x469550`), i.e. 0.933 at
  33 ms.
- Yaw: clamp(steer×0.7, ±5°) (×−1.05 in reverse) × |row[2] − min(speed,top)| ×
  0.8 × `+0x8c7` (1.0) × `+0x163`.
- Template tyres `0x11c..0x11f`: per vehicle (car-effects note); `m_gravity` `0xef` =
  0.05.

Provisional parts, not recovered:

- gravity scale (0.05×45 units/frame²);
- wall slide response (walls use the `0x01e000ff` movement-probe mask on the
  two leading probes);
- the steep-climb stop (normal.y < 0.7 and more than 25 units up);
- respawn after a short fall with no WGNR below;
- the 1.5 s drown delay;
- camera lag (the initial pose `0x477b60` is rebuilt every frame from a
  smoothed yaw).

## HUD

`WHudBrick 0x485b00` picks one-player layout row 0 from `0x52bd5c` (int16
pairs, 320×240 units): dial (263,207), needle (280,217), message (160,120),
lap (13,10), position (13,30), suffix (24,30), timer (13,50). Text uses the
general HUD font film 66333 at native glyph size. In the 1024×768 capture the
text is drawn 1:1 in screen pixels while the dial scales. Suffix frames:
63–66 ST/TH/ND/RD, 84 ER, 85 ÈME. Messages: strings 3/4 (PRET!/PARTEZ!, 61 + 12
updates of `0x4a2b30`), 2 wrong way, 5/6 win/lose, 7 press Enter.

The needle angle is speed×π/top − π/2, capped at 0.27. The dial's top-left
2·(x,y) − (`0xa1`,`0xa2`) and the needle pivot pixel (10,52) are fitted to the
Wine frame.

Not drawn: weapon/gadget icons, the winged boost gauge and the racer
progress strip (power-ups and AI are not ported).

## Rendering choices fitted to Wine

Render-setting bit `0x100` objects (flat grey slabs) are hidden, since the
Wine frame shows the mesas behind them. The backdrop is drawn around the eye,
unfogged, at 93% of the far plane.

## Progress, rank and laps

Superseded for Championship I by
[native Championship I](native-championship-i-20261028.md): the field now
measures distance to the exit, with surface layers and wall costs, and the
lap line reach is 5000.

There is no recovered route cursor. Section progress is a geodesic distance
over the section's own dry WGNR cells: 150-unit grid inside the WMOD row's
world bounds (row vectors 2/3), measured from the entry portal and normalized
by the exit's distance. A lap counts when the car crosses the start-line plane
forward with all sections visited. Grid opponents are parked; the player ranks
against their grid progress (7th at the start, as in Wine).
`tests/native_drive.cpp` drives a full three-lap race on `cc_t1` with a
test-only lookahead autopilot: about 193 s, three loop wraps.
