# On-screen checks against Wine: rock, crane, loop seam, sky, logs (2026-11-07)

Setup: the original under Wine from the scratch prefix on Xvfb `:77`, the native build on `:78`, both at 1024×768.
The original's player was moved by hops along robot 1's logged line (`wmem hops`), then held at the pose;
the native one was placed with `WPC_PLACE=x,z,yaw`. Captures and memory logs stay in the session scratchpad.

## WRolling rock (`cc_t4`, node #403)

Pose (25120, −1945), yaw 1.6823, facing the rock's path.

- On screen: in both builds the rock swings across the view, with the same look.
- Memory: the WRolling brick (vtable `0x515508`) holds the rock object at `+0x11e`. Its state is read every
  33 ms over 20 s (`+0x152` state, `+0x15a` velocity, `+0x15e` distance, `+0x162` segment, `+0x166` spin),
  with the parameters it reads at `0x4c5030` (`[[+0x10]+0x18]`: `+0x98` g 9.81, `+0xa0` drag 0.999,
  `+0x90` radius 350). The path's directions and lengths match the native ones (2499.3, 1957.3, 4228.9).
  The points differ only by a constant offset (2632.9, 0, 4060.5), the brick-space origin.
- The native rock (the `rk2` harness, 3000 updates) starts at the top of segment 0, with a peak speed of 152.
  The original was measured about 51 s after its start. From the native series' 51.2 s onwards, the
  peaks are 71.9, 74.0, 72.8, 73.1, 68.8, 68.0, 68.9, 67.9, 64.0, 63.7 and 58.4. Wine gives the same values,
  in the same order. The half-swings last 1.63–1.70 s in both builds (Wine 1.62–1.72 s).

**Not compared:** the rock hitting a car, the rock blowing up.

## WJunkyard crane (`dr_t3`) and the loop seam

Pose (−821, 2805), yaw 0.0095, just past the start, facing the crane.

- Wine shows sand, junk piles and the crane's tracks. The native view also had a pale floor, a pale-blue
  wall and a town building over that ground. They hid the tracks.
- Hiding node-id ranges found the source: town models from the end of the course (#363 and others under
  WGndModel #304; texture 6 has a mean of (216, 210, 192), the floor's colour). The native view drew the whole
  world a second time through the WMOD loop transform (`wrap()` near entry 0, `unwrap()` near the last exit).
  That laid every other section of `dr_t3` over the start, and the town over the junkyard.
- Fix (`RaceView`): only one section is drawn across the seam. Through `wrap()` it is the last section;
  through `unwrap()` it is section 0. A batch's section comes from its node's WMOD row links
  (`RaceTrack::node_sections`, inherited from the nearest linked ancestor). Scenery has no links (about
  95% of the batches), so its section is the section of the ground under the batch's bounding-box centre,
  or of the nearest ground on rings up to 3000. **Provisional:** this is a native rule. The same day,
  it was replaced by the original's enabling (`section-culling-20261107.md`). Only section-less batches
  kept the ground rule at first. Later that day the seam itself was traced (group table `0x5472d0`), and
  the ground rule was dropped.
- Checked under Wine:
  - `dr_t3` start (above): the native view now matches.
  - `dr_t3` at the end of lap 1, pose (11845, 4956), yaw 1.5708: Wine shows the start banner and the
    junkyard past the last exit. The new native view matches. The old one drew a town building across the road.
- Checked natively only, old vs new at a point 2500 before the last exit: `sh_t1`, `cc_t1`, `dt_t1` and
  `ct_t1` are unchanged (0.02–0.4% of pixels differ; the start line is still visible past the seam).
- The crane's arm was at another point of its cycle in the two captures. Its angles were compared before
  (`saw-crane-gate-road-20261107.md`); the arm squash is still not ported.

## Backdrop origin and fog (`dr_t2`, WBumSteer runners)

Pose (−21166.6, −4486.9), yaw 2.1108, beside the level crossing (gate 0 at (−20702, −3906)). The runner path
leads into a tunnel.

- **Sky.** Wine showed an orange sky with clouds; the native one was flat orange. `dr_t2`'s backdrop node #25
  sits at (8898, 20, 60420). The native dome was scaled about the eye from world coordinates, which threw it
  out of view, and its "radius" came out as 65344.
  - Now the dome is drawn in its node's frame less the translation: v → eye + k (v − t).
  - The old fit, which lowered it by 262/2505 of the radius, was `cc_t1`'s node translation (0, 262, 0), so
    `cc_t1` draws as before.
  - Other courses' translations: `cc_t3` (0, 262, 1464); `ct_t1`/`ct_t2`/`ct_t3`/`dr_t4` y 541; `sh_t2` y 651;
    `dt_*`, `sh_t1`, `sh_t3`, `sh_t4` and `dr_t3` y 20. Those last ones now sit higher than with the fit.
  - Wine-checked: `dr_t2`'s clouds now match in place; `sh_t1` and `dt_t1` at their start poses match (the
    sky corner and the roofs).
- **Fog.** The tunnel's inner vertices have VCOL 0. Wine draws them black; native drew (115, 82, 22), which is
  about 47% of the fog colour (242, 168, 34). The tunnel is 11000–14000 away.
  - `0x4ae060` picks the start from the device raster caps (`[+0x2d5fd]+0x44`), and `0x4b1c65` sets the mode:
    - with bits 0x100 (`D3DPRASTERCAPS_FOGTABLE`) and 0x100000 (`WFOG`): table fog
      (`FOGTABLEMODE` = `0x52d3e0` = 3, linear), start = far − 3000 (`0x514d7c`), end = far;
    - otherwise: vertex fog from far × 0.5.
  - Native now uses far − 3000 = 17000 to 20000, per pixel by clip w.
  - Wine, same pose, mean rgb per box (Wine / native after):

    | Box | Wine | Native |
    |---|---|---|
    | tunnel | 0, 0, 0 | 0, 0, 0 |
    | right hill | 94, 120, 31 | 98, 124, 40 (was 149, 143, 45) |
    | left hill | 92, 132, 16 | 93, 132, 21 |
    | mid ground | 170, 170, 86 | 173, 170, 88 |

  - **Limit:** the caps are what Wine reports. A card without table W fog would get the far × 0.5 vertex fog.
- **Runners and gates.** The crossing barriers were down natively at 00:00 and raised in Wine at 02:29. The
  timing was not compared. A native car placed on the rails was crushed by the runner leaving the tunnel; it
  was not compared with the original.

## WLogJam logs (`sh_t3`, node #615)

Pose (232800, −4600), yaw −1.107, on the road's west leg in section 2, facing the jam. In Wine the
player was moved there by hops along robot 2's logged path (sections 0, 1, 2 in 36 s) and held.

- Memory: the six WLogModels sit 0x1b0 apart from `0x4689324` in both runs (ids 0x268–0x26a and
  0x26c–0x26e; #619 does not exist). Their world matrix is at `+0xc0` (translation) and `+0x98` (rotation).
  The brick frame is turned by π about y: brick (x, z) = −(world − node) less the path translation (52, 472, 0).
- Resting logs, Wine against the port's `CourseObjects` after 240 updates:

  | Slot | Wine world | Native world | Rotation (Wine = native) |
  |---|---|---|---|
  | path 2, n − 3 | (236343.0, 6523.7, −2679.5) | the same | 0, −1, 0, 0.999, 0, −0.037, … |
  | path 1 | (237292.8, 6523.7, −1944.9) | the same | …, 0.994, 0, −0.108, … |
  | path 0 | (238336.0, 6526.3, −2454.1) | the same | …, 0.999, 0, −0.050, … |
  | path 3 | (235712.9, 6523.7, −2271.9) | the same | …, 0.950, 0, 0.312, … |
  | path 4 | (239262.5, 6526.3, −3343.3) | the same | …, 0.816, 0, −0.578, … |

- One cycle on path 4, 24 Wine samples 0.54 s apart (capture, then a 0xa20-byte read):
  - The rolling log's positions are the port's at u866, u937 and u951: (237704, 7821, −7380),
    (238392, 7077, −5156), (238703, 6681, −4261).
  - The hand-off is at (239193, 6530, −3439) and (239300, 6530, −3290) in both.
  - The struck log's positions are the port's at u980, u995 and u1009: (239814, 6580, −2564),
    (240359, 6341, −1812), (240868, 5505, −1113).
  - The struck log restarts at the top on the next sample.
  - Wine sampled every 14–15 updates. At a 0.54 s sample interval, that is about 26 updates/s under Xvfb.
- On screen: with five logs resting, the four on the left match pixel for pixel in place, size, end
  cap and bark texture. The rolling log comes over the crest right of the path-4 slot, and the two logs
  overlap at the hand-off. Both builds show the same.
- **Not identified:** in a first run, at 02:00 with robots past the jam, Wine showed a seventh log-like
  object on the left road edge for at least 12 s, while the cycle ran. There are only six WLogModels.
  At about 00:30 in the second run, from the same pose, it was absent. It is perhaps a robot's item; not traced.
- **Not compared:** a car hitting or riding a log; the path drawn at each restart (Wine took path 4
  twice, then path 1; the port's generator is provisional).

## Side note

Placing the native car in `dr_t1`'s river (drowning surface `0x00800000`) splashes it and returns it to the
section-2 entry after 45 updates. Captures placed there 6 s later show that entry, not the river.
