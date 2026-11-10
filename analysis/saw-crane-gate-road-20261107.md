# WSaw, WJunkyard, WConstructionSite, WShiftedRoad and WGoldenGate (2026-11-07)

Native: `src/port/race_objects.{h,cpp}` (`Saw`, `Crane`, `Conveyor`, `ShiftedRoad`, `GoldenGate`),
`RaceSession` (robots' side state, doors' contact), `RaceTrack` (route links), `RaceView`.

## WSaw (sh_t2)

Action: instance vtable `0x5150b0`, init `0x4c0890`, update `0x4c0a90`, end `0x4c0960`, wave `0x4c0a00`,
`0x4c0a40`. On GDModel #647, child of the spinning GDModel #646. Members (`0x207..0x20f`): `m_24` spin axis 3,
`m_2c` step -0.349, `m_34` slide axis 3, `m_3c` amplitude 300, `m_44` step 30, `m_4c` range 1000, `m_54` reach 220,
`m_5c` gain -1.4, `m_64` hold 30.

- The parent spins; in the hold, by (hold - timer) x gain / hold more.
- State 0: the first car of its section within range becomes the target (never cleared), the wave follows its coordinate
  while they differ by `m_44` or more. Within `m_54` + 200 on x and 100 on z: state 1. Without a target it runs on its own.
- State 1: the car stays where it was caught, `+0x1c7` = 0, for the hold. Then a slot: from the saw to the car's last
  position, horizontal, length `m_4c`, decay 0.3. State 2 for 15 updates.
- Robots with `+0x1c7` = 0 passing at x below the saw's get zones from wave value and offset (167, -167): 7 or 8, `+0x1cb` = 3;
  near mode then steers +-0.1745 for that time (`0x4e75c0`).

**Compared under Wine:** idle wave and offset (phase 360 -> z 36.4 + 60), a capture of the player and of a robot, timer 30
then 15, the throw (-973, 0, -231, length 1000), the car landing about 1600 away. **Not compared:** the robots' side states;
sounds not ported (`+0xb5`, `+0xb9`).

## WJunkyard (dr_t3)

Vtable `0x515b48`, init `0x4ce1d0`, update `0x4ce430`. Three nested arms (#10, #705, #706), biquads `(m_90, m_98)`, `(m_a8, m_b0)`,
`(m_c0, m_c8)`. Phases `+0x122`: 0 waits `m_a0`, then arm 1's target 5; 1 waits `m_b8`, then -55; 2 waits `m_b8`, then `m_a0` and
arm 0's target = rand x 180 - 150. Arm 0 turns about y, arm 1 about z, arm 2 about -z (folded at 0.541). Arm 2's target stays 0:
`+0x11e` stays 3 and nothing sets 4.

**Compared under Wine:** the phases and targets (-56.2, -139, 26.2: within -150..30), arm axes by the turn.

**The third arm's squash (2026-11-08).**
- **The primitive.** The arm `+0x12f` is node 706, whose PRIM is a GDPrimSphere: type word
  `0x20003002`, centre (−3.145, −121.940, 17.785), radius 215.089, name "PrimSphere", sensor group
  `0x10000`. The parser already read it; the earlier "radius 0" came from arm 1 (node 705), whose
  PRIM is a "PrimBox" (key `0x8b`, six floats), which is not parsed.
- **The squash (`0x4ce65b`).** After the arms' frames are set, `0x4040e8(arm 2, query)` tests the cars
  (mask `0x100`). Only the first result is looked at: if its `+0x369` node has flag 2 and its
  WackyAutoBomba (`+0x36d`) type `+0x2c` is 0, the type becomes `0x200` and the bomba is activated
  (`0x404edf`).
- **Native.** `Crane::prims` holds the arm's PRIM. The update tests each visible car's 100 sphere
  against it (in the arm's world frame) and stops at the first car touched, which is hit unless it
  is busy.
- **Checks.** `native_drive_test` on `dr_t3`:
  - a car at the sphere's centre is squashed, one 2000 above is not;
  - a busy car found first shields the next one.
  - Control: with the hit removed, the first check fails.
  - All 20 courses pass; `dr_t3`'s drive is unchanged (105.7 s).
- **Not compared with Wine.** The order of `0x4040e8`'s results is not traced (natively: the
  player, then the robots); arm 1's PrimBox is not used by this update.

## WConstructionSite (dr_t3)

Vtable `0x515db0`, init `0x4d1080`, update `0x4d11a0`. Two belts: child to its first child, unit x `m_88`. Each car of its section whose
`+0x8a3` has bit 26 + belt moves by it; `+0x89f` is cleared. Native takes the contact flags of `RaceCar::surface` for `+0x8a3`
(provisional: `+0x8a3` is set once per clearing, from the first hit).

**Compared under Wine:** the vectors (40, 0, 0), (0, 0, 40). **Not compared:** a car on a belt. `WPalAnim` is not ported.

## WShiftedRoad (sh_t4)

Instance vtable `0x515900`, init `0x4cb900`, update `0x4cbb60`, end `0x4cbae0`. Timer `m_88` (120): chosen `+0x142` = rand % 3, previous
`+0x146`, swing timer 30. The chosen slab goes to 90 degrees about x; the previous one back once the swing timer is 0. Slab k's
link (`+0x172`, towards nodes 0x20, 0x23, 0x28 from node 1) weighs 0xffff when chosen, 0 otherwise, when links below 2 are written,
at the draw only. A car on a slab is squashed and makes it chosen, with both timers reset, without touching the weights. Signs 3..5 swap homes.

**Compared under Wine:** link indices 0, 1, 2; weights (0, 0, 0xffff), (0xffff, 0, 0xffff), (0, 0xffff, 0xffff) with link 2 untouched;
timers 120 and 30; the slab's y axis to 0 and back to 1. **Not compared:** a squash on a slab; the signs.

## WGoldenGate (sh_t4)

Instance vtable `0x51549c`, init `0x4c6610`, update `0x4c6b10`, end `0x4c6750`, hit `0x4c67d0`, choice `0x4c69b0`, static table `0x52d7c8` (9 x 25
bytes). Table: kinds 1, 2, 1, 0, 0, 2, 1, 2, 3; states 1, 2, 1, 1, 2, 2, 1, 2, 1; angles / 5: 90, 54, 72. `m_88` 30, `m_90` 9 doors an update,
biquad (`m_98`, `m_a0`), `m_a8` 50, `m_b0` 0.75.

The table is static in the original, so its states carry on across activations. Native keeps them per object.

**Compared under Wine:** door states and targets in play (450, 270), the change 16 updates after a choice (timer 14), the chosen door's value. Sequences
differ, the draws being a provisional generator. **Not compared:** a door hit by a car (the door's axis is z, `0x4024fa`, read from the code);
robots' `+0x1c7` = 1 is set, but the vehicles' own gadgets are not ported, so it has no effect.

## Not seen on screen

None of these objects has been seen on screen in the native game.
