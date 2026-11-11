# TODO

Open work on the native port. Rebuilt on 2026-11-06 from the last known list
(2026-11-02; the file was missing after the move to this repository), so
finished items are not kept. Some entries may predate later work: check the
code before starting one. The follow camera (`0x478070`) has since been ported
(`analysis/follow-camera-20261102.md`). Addresses refer to the original WPC.EXE.

- [x] sh_t1's barrier against Wine (done 2026-11-06, wall-contacts note): falling
  onto the barrier and running over the ramp now follow Wine within 1 or 2 units,
  speeds identical until the first divergence. Three causes: the wall rays' centre
  (last update's frame), the player's ground rays (x 1.3), the slot tilt (after
  the step). The "dead stop" did not reproduce in 11 traced falls: the only zero
  write was the glide's end `0x472490`; a glide does occur under Wine.
- [ ] On-screen check of the objects ported 2026-11-06/07 (2026-11-07): every course now starts
  in the native game (20 of 20 reach the race); before, a draw of any new object threw
  (`map::at`: their model groups were not registered for translucency), so the game failed
  on cc_t3, cc_t4, ct_t2, dr_t2, dr_t3, dr_t4, sh_t2, sh_t3 and sh_t4 at the first frame
  of such an object. Captures with `WPC_PLACE` show the chisel wall, the saw's shed, the
  golden gate's doors, the shifted road's slabs; the runners and logs were not framed. The
  cc_t4 rock matches Wine update for update (swing peaks and half-periods), and the dr_t3 crane
  scene matches after the loop-seam fix (`analysis/on-screen-checks-20261107.md`). The golden doors
  were compared with Wine too: they were yellow (COLS[1]) and are
  now brown like the original (untextured parts with VCOL use the Direct3D diffuse,
  `analysis/vertex-colours-20261101.md`). UVScroll was seen moving in both builds on `dr_t1`'s
  river (speed not measured; `analysis/dickstrick-uvscroll-rooms-20261107.md`).
- [ ] Loop seam rendering (2026-11-07), `analysis/section-culling-20261107.md`:
  - Sections come from the original's enabling (`0x4a8680`: each row's `+0x58` brick and its linked
    top-level nodes), and only the player's section ±1 is drawn.
  - Across the seam, each group (`+0x104`) is placed by the table at `0x5472d0` (`0x432690`,
    `0x4200e0`), for the world, the cars, the course objects and the pickups. Wine-checked on `dr_t3`
    and `sh_t1`.
  - Weapons and car effects (traced statically, not compared with Wine):
    - Projectiles, TNT and storms follow their section's group, and cross the seam through the loop
      transform (`0x4a6b30`, `0x4a23c2`, `0x4a25b5`).
    - Dropped weapons are culled and placed by their section (`0x4a6d20`).
    - Splashes and ripples follow their car.
  - Left:
    - The crossing test is the far robots' route rule, not the original's portal test `0x4a1a20`.
    - The skid marks (`0x47a7c0`, matrix from `0x40250e`) stay in place.
    - A dropped weapon outside every slot box keeps 0xffff in the original (always drawn).
    - With two players, both enabled sets are drawn (the second tracker is not traced).
    - Robots and weapons across the seam were not compared with Wine.
- [ ] Runners/gates on `dr_t2` (2026-11-07): the scene matches Wine (sky and fog fixed:
  `analysis/on-screen-checks-20261107.md`). Not compared: the gates' timing, or a car crushed by the runner.
- [ ] `sh_t3` logs on screen (2026-11-07): the resting logs, the hand-off and the struck log match Wine
  (frames and the six WLogModels' world matrices `+0xc0`, update for update). Once, at 02:00 with
  robots past, Wine showed a seventh log-like object on the left road edge for at least 12 s. It is
  not identified (six WLogModels only; perhaps a robot's item).
- [ ] Test driver (2026-11-07, `analysis/test-driver-20261107.md`): all 20 courses pass `native_drive_test`.
  - The port's RoboDriver now steers the player's car through its keys, and releases the throttle in
    turns past 80% of the 40-degree lock. The look-ahead planner takes over after 300 flat updates.
  - `cc_t3` and `sh_t3` were the driver falling off spiral ramps, not WDicksTrick2 nor the physics.
  - `ct_t2` runs 9 laps (`m_nLapNum`), so its finish is not driven.
  - Open:
    - On `dt_t2`, a robot's hit can throw the car onto route polygon 2/594. Its goals lead into a
      grass pocket closed by a wall (z ≈ 10) with the route beyond.
      - Traced 2026-11-08: no object closes it. The pocket (x 55657..57600, z ≈ 10..1090, grass
        0x10, no route polygon) is a box closed on its four sides by section 2's own WGNR walls
        (record 75, resource `0x33d`, flags 0x200000, over 3000 high). The route's link points
        (594 → (57716, 1159) → 582) pass east of it, on the road (0x2).
      - The walls rise to at least 8000 with no roof, so the car entered the box through a wall.
        Not reproduced (2026-11-08, scratch harnesses):
        - the player placed on the road north of the box (x 56000..57400, six headings), driving,
          hit by each WackyAutoBomba type at update 30: 0 of 600 runs enter it;
        - all robots near for 6000 updates on the 20 courses, a ray at 40 up from each car's last
          to its new position: no wall crossed except returns (`0x4a6730`) and cars grinding
          along a wall (1 to 6 units an update, the face crossed back and forth).
      - The original has no stuck reset: `0x4a6730` is called only by the car's reset (vtable
        `0x51295c` slot 2, `0x469550`) and by the 0x800000 surface return (`0x47014a`).
    - Passing proves a course can be finished, not that it plays like the original.
- [ ] The geodesic field (`RaceTrack::build_field`) is not monotonic where a route's
  U runs beside itself behind a thin wall (`sh_t4`, around (3500, 22000): the wall
  cost 4000 is below the detour). It feeds a hidden robot's distance and the
  progress before a car's first cursor hit, not compared with Wine.
- [ ] Barrier, small remainders: one wall hit more under Wine on the ramp run
  (update 14, (-14992, -3032)); a yaw step of 0.007 at the glide's start (Wine
  -2.6992 -> -2.6921, native keeps it); distance 0.13 % short per update in the
  air (82.45 against 82.57 at speed 0.4324). The airborne upward ray `0x46c6cf` is not ported
  (2026-11-08, tried and withdrawn):
  - Read: with `+0x8a7` > 1, a query (`0x402199`, settings at `0x6f12b0`: mask `0x1e000ff` and two
    1.1 factors) runs from the wall rays' start before the push (the last frame's x and z, the mean
    of the FL and RR probe heights, `0x46bafe`) to 200 (`0x512a44`) above. A hit adds the overlap
    to `+0x28c`.
  - Ported as a plain ray, it fired once in `native_drive_test`'s always-near pack on `dr_t3`
    (update 511, a downward face with flags 1, 96 above the probes). Robot 5 then stood still at
    y 299 over an object for 180 updates, and the road-edge check fell from 98.4% to 92.7%.
  - Withdrawn until the query's settings are traced and the case is checked under Wine.
- [ ] Test driver (2026-11-01, after the follow lock, route cursor, contact
  turn sign and the new weapons): cc_t1 3 laps 116.6 s (1st), sh_t1 1 lap
  97.0 s, dt_t1 112.2 s (1st), ct_t1 105.3 s (1st); sh_t1's robots best 2
  laps in 120 s; the championship check takes the points of its place.
- [ ] Slides and skid marks not compared with Wine; the item-effect case
  (`+0x385` = 0) of the slide.
- [ ] Hit visuals (2026-11-07, `analysis/hit-smoke-20261107.md`):
  - Done: the smoke clones of 8/0x40/0x80 (`0x47ed6e`, flamesmoke `0x418310`) and the tint of
    every batch of the car model. Compared with Wine on `cc_t1` (a hooked 0x80 hit on the player):
    the same look; the timing and the puff count were not aligned.
  - Films with SILL and SMOO now blend INVSRCALPHA/INVSRCCOLOR (`0x4b5550` mode 10), and every film
    draws without depth write.
  - Done 2026-11-08: the stars of 0x10/0x100 (`m_BoomerangHit` `0x391`) and the paint pool of type 4
    (`m_bluepool` `0x1c1`), both compared with Wine (same look).
  - Done 2026-11-08: the tank gun's hit (type 2, `0x47e277`): the mound `0x393` with its segment
    overrides (`0x437f70`), the car hidden through counter 41, the eleven WReszecske chunks, the
    flattened mound. Compared with Wine (same look; timing and random draws not aligned).
    Open: whether the shadow hides with the car (`+0x32` not identified).
  - Not drawn: the stars' WPalAnim; whether `0x43b190`'s squash persists past stage 2.
  - The storm cloud's `+0x152` is its GD3dSprite's angle about the view axis (`0x419eef` ->
    `0x43b690`), so the cloud turns rather than fades in state 2 (2026-11-08, not compared with Wine).
- [x] The traps' object section (2026-11-08, `analysis/section-culling-20261107.md`): a dropped
  weapon takes the section of the first enabled slot row whose box (row `+0x1c..+0x30`, copied by
  `0x4a89c0`) holds its x and z (`0x4a6d20`; slots: each player's previous, current, next row).
  - Fired weapons (projectiles, TNT, the storm) take their slot row's section the same way
    (`0x4a6b30`).
  - Open: none holding it, the original keeps 0xffff for a dropped weapon (always drawn, and the
    `0x4a7e70` section test never matches?) and registers no tracker for a fired one; natively the
    dropper's or thrower's section. Not compared with Wine.
  - (The actions' `+0x2c`/`+0x30` are the icon's WPowerUpSprite `m_range`/`m_visangle`, written by
    `0x4d2970`, since 2026-11-08: the detour, the FireSiren, the tree and the wall use them.)
- [ ] FlyPower remainders (`analysis/weapons-bomba-20261029.md`, 2026-11-08): vehicle 3's wings are
  drawn (compared with Wine) and vehicles 3, 4 and 6 halve the gravity again. Since 2026-11-09 the
  per-vehicle rules of the update (table `0x4d5b8c`) are ported: vehicles 1 and 2 take × 0.3 at the
  init (`0x4d5337`); vehicles 0 and 1 end once down (`0x4d59ed`), vehicle 5 halves its gravity again
  at update 10 (airtime 4, `0x4d56a2`) and ends past 17, the others past 95; vehicle 5's parachute
  (GDHModel 0x1c8, clip 0 frozen, at the car + 100) is drawn from update 10. Not compared with Wine
  (the native counter starts one update ahead of the original's `+0x64`, provisional). Open:
  - vehicle 4's model is drawn since 2026-11-08 (GDHModel 0x1c2, clip 0, at the car + 160 up;
    not compared with Wine);
  - vehicle 6's rotor is drawn since 2026-11-09 (brick 0x1c3's GDHModel 0x1c5: clip 0 frozen at
    its end, the segment swap before update 20, the fold (clip 2) at 70, its WRot spin; at the
    car's position + 100 with the brick's own axes). Not compared with Wine. The brick's
    WPU_Propeller action does nothing (vtable `0x516398`: init `0x4d80f0` clears `+0x48`/`+0x4c`,
    update `0x4d8110` is a bare `ret`). (The GDHModel's vtable `+0x4c` call at the init builds its controllers, as for the car bodies.)
    Its GDSound 0x1c7 is never played (2026-11-09: GDSound's vtable `0x511f38` overrides only the
    destructor, and FlyPower's one call of the play `0x44c380` is the car's `+0x3df` sound at
    `0x4d4dd6`);
  - the body animations of vehicles 0, 1, 2 and 5 are ported since 2026-11-09 (`RaceView::body_gadgets`:
    0 and 1 play body clip 0, 2 turns segment 5 about z by 153° an update, 5 locks segment 6's x
    rotation at 35°; TurboSpeed: vehicle 1's segment 1 scale lock, vehicle 4's clip 0 then 2 at the
    end). Captures show vehicle 4's clubs swinging and vehicle 1's umbrella opening; vehicle 2's
    propeller is not seen from the follow camera. Not compared with Wine. Open: TurboSpeed's `+0x70`
    = 1 for vehicle 1 (no reader traced); the body steps once per update (the original per draw);
  - the per-vehicle ends (`0x4d5ba8`): the WFire hiding of vehicles 0 and 5 is followed since
    2026-11-09 (`analysis/wfire-20261109.md`); the body clips of 2 and 5 at the end are not;
  - the robots' own gadget (2026-11-08, a static reading): the controller gives a robot its own
    gadget (`0x2ab078c[vehicle]`, at least one charge) only when `+0x1c7` = 2 (`0x4e59a4`), and no
    shipped code writes 2. The immediate writers set 3, 5, 7, 8, 0xa and 0xb; the register writers
    are the car's constructor, the hiding (0) and WGoldenGate's 1 (`0x4c6e35`), which nothing in the
    controller reads. `0xb` (WRotoHill) only triggers MonsterTires (ported). So the robots seem
    never to fly; to confirm under Wine.
- [ ] Weapon visuals (the breath's GD3DFilm and the WRocketTail fins and glow are drawn since
  2026-11-08 and compared with Wine, `analysis/rocket-tail-20261108.md`; `WExplosion` since
  2026-11-09 as the original's disc, update and draw traced, `analysis/weapons-bomba-20261029.md`). WPU_Breath's model on the car is drawn since 2026-11-08
  (`analysis/breath-head-20261108.md`; the race-side animated GDHModel `RaceView::AnimatedModel`),
  its attach point read under Wine, but it stays lower than Wine's (hidden behind the car's tower
  from the follow camera; the cause is not traced). Drawn since 2026-11-09 (not compared with Wine): FlyPower's
  models and body animations, MonsterTires, the FireSiren's light, the bees (WBeeHive `0x474650`),
  the BootKicker's and the Flipper's arms, the wall's and tree's camera shake (they have no fall
  animation beyond their m_rGravity drop). (The wall's and tree's
  placement query uses the icons' WPowerUpSprite range and angle since 2026-11-08.) Still open: the
  Wine comparisons of all of these. (`WPU_Boomerang`'s model was already drawn, and its DragonBreath's rotangle is 0: it does not
  spin; the breath's film now turns by its rotangle, 2026-11-09).
- [ ] WFire (2026-11-09, `analysis/wfire-20261109.md`): the jet flames drawn for TurboSpeed (vehicles 0,
  2, 5, 6) and FlyPower (0, 5), with the original's random shape and colours each draw. Open: not
  compared with Wine (the culling of mode 8 hides one triangle of each fin pair; the native capture
  shows thin flames); the draw's rand() is view-local, not the shared one. (TurboSpeed's body clips
  are ported, see the FlyPower item.)
- [ ] Wheel spin (2026-11-09, `analysis/wheel-spin-20261109.md`): ported; the wheels roll and the front
  pair steers. Open: the sign test reads the speed copy `[esp+0x44]` (written at `0x46ae50` from
  `+0x3ef`, provisional), the step's length is the native forward step (the probes' pitch left out);
  not compared with Wine; far robots (no motion block) keep their angle.
- [ ] `WPU_Detour`'s `m_aEffectAngle`/`m_aMaxSteerMod`/`m_nTrapTime`: no reader found (2026-11-08,
  static scan; the init sets `m_nTrapTime` to 5, `0x4e32a4`; `analysis/weapons-bomba-20261029.md`).
  A Wine read watchpoint on `[action+0x18]+0x54/+0x5c/+0x64` would confirm it.
- [ ] Backward use (2026-11-08, `analysis/weapons-bomba-20261029.md`): look-back held at the use sets
  the action's `+0x5c` (`0x4d24f0`). Projectiles, the TNT and the breath head turn by π; the storm
  takes backward links. Open:
  - the kicker is ported since 2026-11-09 (its arm, aim, kick and offsets `0x2ab10b0`, see the
    note's "BootKicker's arm" section); the flipper too (its reach, clip hold and hit, see the
    note's "Flipper's arm" section); the flipper's wall ray `0x4d96d0` is ported since 2026-11-09
    (the result's fraction at `+0x44` is a provisional reading; no test reaches a wall);
  - the tank gun's turret `+0x3ab` is ported since 2026-11-09 (see the note's "turret" section);
  - not compared with Wine.
- [ ] WPaintShot and WPU_Paintball are ported since 2026-11-09 (the note's "Paintball" section): five
  shots, the scatter, the type-4 paint, the blue explosion `0x3af`, the body's clip 2. Open: the solid
  objects' handler answer (`0x402b26` is ported as a touch of the port's solid spheres, 2026-11-09),
  (the player's paint on screen does not exist in the shipped game,
  2026-11-09: the WPaintFx class is never instantiated, and `+0x3a7` has no reader), not compared with Wine. (The brick flags 0x100000/0x200000 of `0x499243` are
  the loop seam's crossing marks: the shot's velocity turns by the loop transform, ported 2026-11-09.)
- [ ] Items still missing (2026-11-01): MonsterTires' physics (since 2026-11-09 its stages, 289 updates,
  the tyre models, the hidden wheels and the raised model `+0x92f` are ported; the contact spread
  `+0x15b` = `0x52b800[vehicle]` (end: `0x52b7f8`), the wheel records `+0x44b` = 24 (end:
  ftol(`0x52b804`)) and the original's `+0x385` = 0 are not); every weapon icon 11..25 now has an effect (Tnt, Storm,
  Detour, Flipper, BootKicker, Breath, Missile, TankGun, Boomerang,
  Landmine, OilSpill, BeeHive, FallingTree, BrickWall, Paintball), and the
  robots use them.
- [ ] Weather remainders: (the flake texture is none, 2026-11-09: renderer `+0x23969` is never
  written, so the flakes are plain white squares, `analysis/car-effects-20261028.md`), D3D line width (native ~1 px strips), the
  shared rand() and per-frame stepping (native per race update).
  - The Zartter shelter cut (`0x4565e0`/`0x456180`) is ported (`RaceTrack::sheltered`, 2026-11-02; its
    section rule is provisional).
  - The rain's lighting is ported (2026-11-08, `analysis/car-effects-20261028.md`) and compared with
    Wine on `cc_t1`: textures at half intensity (`0x4b08b1`), the clear/fog colour (`0x4ae364`), the
    dome's vertex colours (`0x4b4347`).
  - The rain streaks and the snow were not compared with Wine.
- [ ] WKBumm not compared with Wine (cactus not reached with key scripts);
  the shadow sits further from the airborne native car than in Wine, and the
  native car still drowns on the bank.
- [ ] Winner scenes, remainders: not compared with Wine; the scene's music
  (no `Music\Win*.snd`; native silent), the GDCamera, the WMenuItem's own placement.
- [ ] GDHModel remainders. Done 2026-11-08 (`analysis/mtnl-motion-20261028.md`): controller 0 on
  the cars' bodies (`SwayController`; rotation gains `+0x17e` by vehicle from the car init `0x469bf0`;
  compared with Wine, exact on segment 0), and the scale overrides `0x437f70` (the tank hit). Open:
  - (the overrides `0x4381a0`, translation, and `0x4383b0`, rotation, are ported since 2026-11-09:
    `AnimatedInstance::moves` for the kicker's and the flipper's arms, `CarSway::turn_locks` for the
    gadgets' bodies);
  - the race-side clip starts (`0x47394b`, the `WPU_*` actions `0x4d4f08`…);
  - compare a playing clip with Wine.
- [ ] Compare original and native lighting, shading, alpha, and material state;
  implement only states confirmed by the original render path. Done
  2026-11-01: vertex colours and TRAN for textured parts (`0x4b21ae`,
  Wine-checked on `ct_t1`). Texture filtering: `0x4b1ad0` sets stage 0
  MAG/MIN/MIPFILTER LINEAR; the 3D material draws now sample bilinearly
  (Wine-checked). Open: untextured parts; whether 2D draws filter (native
  keeps nearest); the other `0x4b1ad0` states (dither, specular) not compared. Fog follows the
  table-W-fog branch, far − 3000 to far (`0x4ae1c3`, `0x4b1c65`; Wine-checked on `dr_t2`, 2026-11-07).
- [ ] Recover WackyAuto wall/contact response (`0x46a4a0` after the probe
  queries), the gravity scale, the vertical/ramp response, out-of-bounds and
  respawn rules and `0x478070`; replace the native edge guard, ramp launch
  and stepped respawn. Wine evidence: the original slides past `sh_t1`'s
  tunnel post but stops dead at `dt_t1`'s first bend, and slides down
  `ct_t1`'s snow slopes (`analysis/wine-comparison-championship-i-20261028.md`).
- [ ] Vertex colours, remainders: the Direct3D colour of untextured parts
  (no DIFFUSE without VCOL, ALPHAOP disabled; native keeps COLS[1] and
  1 - TRAN from Glide `0x433d40`); out-of-range VCOL (the cache truncates
  with ftol, native clamps; `ct_t1` GMDL 289/361, `dt_t1` 786); the splash and
  WalkOnWater (additive, TRAN x VCOL) and the 2D icons' "transparency 0.4"
- [ ] Route cursor remainders: how the original updates `+0x2c5` (native:
  the player's route query each update, the robots' driver module), the `0x4a394e` fallback
  without a polygon, the start-line test `0x4a3f28` (forward-linked polygon
  and `+0x2b9`); the race distance uses the rows' `+0x34` (see the rank item).
- [ ] Rank scaling remainders (2026-11-08):
  - The race distance and the HUD strip now use the rows' route lengths `+0x34`, as the original's
    lap length does (`0x4a3ba0`, manager `+0xf38`) and its distance in a row (`0x4a3937`). Before,
    the native distance field's lengths were used, 0.9× to 1.74× apart per lap. The fraction inside a
    row is still the cursor's or the native field's.
  - The switch first broke two checks:
    - `dr_t2`'s far robots lost a lap, from a native bug now fixed: a hit taken while far kept its
      spin and hold until the robot came near again (see the robot item).
    - `ct_t1`'s avoidance aims fall to 88.9% on the route layer: the early-race window (progress
      under 20000) now ends sooner on the road, in the original's units. The test allows 85% there,
      as on `dr_t2`.
  - (The gravity `+0x98` × `+0xb4` and the robots' doubled gravity `0x4e45e0` are applied in the
    ported vertical motion.)
- [ ] Far robots and hits (2026-11-08, provisional): the car update `0x46a4a0` consumes its slots
  (`0x4727b0`, `0x472890`) and the motion counter without testing the far state, so a hit taken
  while far now runs out as the robot slides. Before, it waited for the return to near mode (`dr_t2`:
  76 s later the robot spun in place, turned round and fell).
  - Checked under Wine (2026-11-08, cc_t1): robot 0, about 40000 from the player on the grid, hit
    with 0x80 through the one-shot hook. Its WackyAutoBomba counter `+0x82` rose by one a frame
    (1..80 in 3.5 s), `+0x36` was set at 80 and the type cleared at 81, while the robot stayed far
    (40000 to 60000 away). So the bomba runs in far mode, as natively.
  - The hold stops a far robot (2026-11-08). Under Wine, robot 0 hit far away:
    - its position froze for about 25 frames (speed 0);
    - it then accelerated by 0.0216 a frame with the car physics;
    - it glided again (79 a frame) once the hit ended.
    The cause: the robot controller (`0x4e53df`), in the far state 0x1000, skips the far step while
    the bomba has a type. Natively a far robot with a running hit now goes through the car update
    with its last control. Checked on all 20 courses (`native_drive_test`): 0..115 units of creep
    during the hold, against about 1600 for a glide.
  - Not checked: the angles (frozen in far mode under Wine).
  - The 0x80 hit's end (Wine at 80, native 73) is explained and fixed (2026-11-08): Wine runs
    renderer 1, whose `0x47d890` fades packed bytes with a truncating step (33 updates a fade). The
    native tint now follows that path and matches each logged mode change
    (`analysis/weapons-bomba-20261029.md`).
- [ ] Robot items, remainders (the weapons' and FireSiren's aims are each icon's WPowerUpSprite
  `m_range`/`m_visangle` since 2026-11-08, read under Wine, `analysis/robocar-ai-20261028.md`):
  FlyPower use on `+0x1c7` 1/`0xb`,
  battle mode (`+0x34`, `0x4e9050`), the section-cell object list.
- [ ] Avoidance remainders: not compared with Wine; the target-speed angle
  while avoiding; stale `+0x315/+0x321` with fewer than two shared
  vertices; edges across sections; the two-player gate (`0x6f2d00+0xf30`).
- [ ] Robots trapped on `dt_t3`'s steep ramp in section 2 (2026-11-08): around (-40200, -2200, 23800)
  the road climbs about 250 over 500 toward -x out of a sandy hollow. A robot hit at its foot climbs
  at about 0.19, reaches y -2024 and slides back, over and over (always-near test pack). 2 robots were
  trapped with TNT's aims, 4 with the WPowerUpSprite aims (more hits); `native_drive_test` allows 4
  there.
  - Compared with Wine (2026-11-08, `dt_t3`): the player was held above the ramp at (-40200, -1700,
    23800) so the robots near it drive physically, and six robots were logged every 100 ms for 100 s.
    The original's robots are trapped too:
    - robot 0 entered the hollow at 35.6 s and was still there 64.6 s later, climbing to y -1833 and
      sliding back;
    - robot 6 went in and out 17 times in 100 s, reversing at -0.29 (the unstick), before climbing out
      at (-41646, -1575, 23692); robot 1 got out the same way after 4 passes;
    - only the robots taking the ledge at y -1269 passed cleanly.
    So the trap is original behaviour, and the test's allowance follows it.
  - Traced natively (robot 0, always-near pack):
    - u1317: on the flat floor about 130 before the slope, turning toward the corner of the ramp and
      the hollow's south side, its speed drops from 0.49 to 0. This is the two-walls-at-a-sharp-angle
      stop of the contact response (dot < 0.4), not a hit.
    - From rest it turns up the ramp (yaw 1.6). Its movement probe meets the ramp face (n (0.43,
      0.90, 0); `contact` counts ramp faces, as Wine showed for the player on `sh_t1`), and the
      correction lifts it 37 in one update.
    - Its single contact ray (the car's up axis × 1.3, 208 above to 39 below, `0x46c7f2..`) then
      misses the floor under its centre, since the slope starts ahead of it. It counts as airborne
      (airtime 2..29), its speed stops updating past airtime 6, and it slides up the face 70 above the
      surface at about 0.15.
    - It then moves under 50 units an update on both axes, so after 101 updates it unsticks (back 30,
      forward 30) and starts again.
  - The player, with its four probes, climbs the same ramp from rest to 0.5.
  - The gravity is not the cause (it only enters the fall). Not compared: the original's own update
    by update (its single ray and probes are taken to be the same).
- [ ] Robots trapped on wall-flagged slopes (surface `0x1000001`, throttle
  cut `0x4e82cd`): off `cc_t1`'s bend near (-60600, 12400) and `sh_t1`'s
  jump (22000, -19600). The human slide `0x46a5a3..`/`0x4721f0` is not
  ported, and does not apply to robots (`0x46c858` clears `+0x157`). With
  the player parked at the `cc_t1` bend the native line matches Wine's
  (x -60930 against -60849 at z 13000, no fall in 108 passes): the falls
  only come in the always-near test pack (first lap), not compared.
- [ ] Trams, remainders (the crossing lights' UV animation `0x4caf95` is
  ported, 2026-11-01, for renderer mode 1): the tram frame copy and the pi/2 turn axis
  (provisional, not seen under Wine); the other WSaw/WRotoHill/WGoldenGate
  states (no instance on Championship I). A
  native robot reaching the crossing at 0.817 (always-near test pack)
  flies over the dip before it and stops outside the zone, keeping state
  5; the original's robots arrived at 0.57..0.66.
- [ ] Other linked nodes: cameras and portals; whether a disabled WPowerUp
  is untouchable for robots (native: yes) is not checked under Wine.
- [ ] Hidden robots, remainders: their lap trigger (the visible cars now use
  the original's `0x49a2f0`) (native: where the
  run passes the start line's distance into section 0, provisional); the
  section-0 placement branch with `+0x93f` set (`0x4a2108`); a robot that
  goes far (not hidden) with `+0x1c7` = 5 outside a crossing's zone stays
  put until it leaves the player's rows (native: after the airborne trap
  below, with the player parked in dt_t1's section 2).
- [ ] Car contacts, remainders: robots in the far mode. (`+0x2d1` is only
  written, at `0x46a47e`/`0x46b3b1`/`0x46b55e`, with no direct reader; voice
  event 4 is ported; the ±6° turn's sign follows RL − RR, `0x46b662`,
  2026-11-01.)
- [ ] Vertical motion remainders: rays along the car's down axis (native
  rays are vertical; the second `sh_t1` ramp pitches 0.49 against 0.44), the
  robots' ×1.3 ray, the lift ray (`0x512a44`), the contact-point spreads
  `+0x15b`/`+0x15f`, the `0x800000` return (`0x4a6730`).
- [ ] Robot lines not compared with Wine (only lap times); the original's
  far-mode route lookup across sections (`+0x2c5`) and the 5% lap gap are
  not traced.
- [ ] Robots trapped against walls with the robots always near (seeds 0..5,
  3600 updates; worst robot laps): `sh_t1` seeds 0 (1 lap) and 1 (0),
  `cc_t1` seeds 2 (1) and 5 (0); `dt_t1` and `ct_t1` none. On `sh_t1`'s first
  hairpin (-6880, 734, -650) the robot cuts to goal 2, which lies behind the
  cliff (the near-and-fast rule of `0x4e5d..`), settles square to the wall
  (the wall turn converges there) at speed ~0, and each unstick returns it.
  Flipping the wall turn's sign moves the failures (`sh_t1` seeds 1, 3;
  `cc_t1` 2, 5 unchanged), so it is not the cause. `m_rWallCheckLen` (300)
  has no reader found yet (the one robot ray, `0x4e93fa`, queries the route
  layer). Under Wine (2026-10-30, single race, player parked at the hairpin)
  the original robots stall there too: robot 1 spent 13 s slow against the
  wall at (-6880, 730, -1800), pushing at speed ~0.16, reversed past z
  -2300 three times and got away on the fourth try after ~25 s; robots 0
  and 5 stalled 3-5 s. So the trap is original behaviour. A native trace
  (2026-10-30) shows the native robot at (-6880, 734, -650) also pushing at
  low speed (the corner case fired 2 times in 35 updates; no steep-ground or
  edge-guard stop), so the remaining difference is only that its unsticks
  never take it to a new line there (goals behind the cliff stay the same). `native_drive_test` asks one lap per
  robot on `sh_t1`.
- [ ] Power-up effects: the `WPU_*`/gadget actions of the template spawned by
  `0x4d2970`; pickup sound (`0x44c380`). (The item keys now come from the
  MenuPrefs table: '/' gadget, right Shift weapon.)
- [ ] Rest of the object query `0x4a5730`: the route-grid cell scan (native
  tests every course object; `WPrimCylinder` 0x482310, car contacts and
  the 0x10000/0x40000 hits are ported); compare pickup range and roulette
  outcomes with Wine.
- [ ] Pause remainders: page 243 per player (the strings 198/199 are drawn since
  2026-11-02, `MenuController::text_override`); the x shift of the list seen under Wine
  between the root and the option node; force-feedback output (`0x417180` on reload).
  - Player 2 has no pause key on the keyboard (2026-11-08): device bit `0x2000` (input `0x800`, the
    pause) is bound only to player 1's Escape (`0x40c87d`, `0x40c9ce`, list 1). Player 2's lists
    (`0x40ca65..0x40cf22`) bind the eight controls only, so its pause (`0x6e42b2`, `0x432b15`)
    would need a joystick binding.
- [ ] Drive the other championships' courses (`native_drive_test`, 2026-11-06,
  `analysis/lap-line-geyser-20261106.md`). All checks pass on `cc_t1`, `cc_t2`,
  `cc_t4`, `ct_t1`, `dr_t3`, `dt_t1`, `dt_t3`, `sh_t1`, `sh_t4`. Far robots finish
  on all 20. Open:
  - the test driver now finishes all 20 (2026-11-07, see the test-driver item above);
  - `dt_t2`: a traced native robot took 15 s across the fountain, the original's
    about 5 s;
  - +0x8bf compounding on surface 1 (`0x466df6`): compare a bombed car under Wine. On `cc_t2` (2026-11-08)
    a robot struck by the storm, in the air (surface 0) and held, had its speed double every update
    (-5.9 ... -188); its front wall probe, which stretches with the speed, overflowed the ground
    query. Natively a speed past 1e4 now returns the car to the road, like the position safety (the
    original's __ftol would not throw).
- [ ] Race audio, still open (the power-up voice events of the Flipper,
  BootKicker and BeeHive are ported); the exact Windows
  DirectSound3D pan/HRTF; no Wine capture of the 3D levels.
- [ ] Battle remainders: the robots' battle mode (`+0x34`, `0x4e9050`; only a
  one-player battle, which the menu never makes, has a robot), the winner
  text's pulsing colour, the route choice `+0x8c4`; not compared with Wine
  (`analysis/battle-20261102.md`).
- [ ] Menu remainders: right-aligned option labels sit 5-9 px (1024 wide) left
  of Wine's; the W image natively drawn over the keyboard on the
  controls page (absent under Wine). (The width routine's -4 branch `0x42dd99` needs the renderer
  `0x6f5958` = 0; Wine and the port run renderer 1, so -1 applies, 2026-11-09.)
  - RETOUR DE FORCE's availability (2026-11-08):
    - The controls page's class (vtable `0x513e48`, constructor `0x48e77a`) has slots `+0x38..+0x40`
      (`0x48e810`, `0x48e850`, `0x48e890`), which call `0x48eff0`.
    - With the player's device at −1 (keyboard), `0x48eff0` calls `+0x50` = `0x48cd80`(8), which
      clears row 8, RETOUR DE FORCE.
    - With a joystick whose `0x54190c[device] +0x36f` has bit `0x100` (force feedback), it calls
      `+0x60` = `0x48cd10`(2, 8) instead.
    - Natively `MenuController::availability` gives 0 for that row, which matches the keyboard case.
  - The W (2026-11-08) is not a model: it is page 560's second WMenuSprite (#79, GMAT `0x2ff`, at
    (110, 0)), beside the keyboard (#78, GMAT `0x2fe`, at (100, 10)). Their properties differ only in
    position, material and `0xa3`, which is a draw priority (1 and 0; 0 on 282 sprites, 1023/1024 on
    backgrounds). The image looks like a joystick, so the page probably shows one per player
    device.
  - `0x48eff0` branches on the device (`0x6f4580[player]`, −1 keyboard), but it sets the rows'
    availability (`0x48cec0`) and RETOUR DE FORCE's (below), not the sprites.
- [ ] Moving course objects: on `sh_t3`, WLogRide's logs move, are drawn and
  carry cars (`0x4cc450`, `0x4cc69e`), with the
  model mesh taken as their ground (`analysis/log-ride-20261102.md`).
  WLogJam, WLogModel and UVScroll are ported (`analysis/log-jam-20261106.md`,
  `analysis/dickstrick-uvscroll-rooms-20261107.md`). No Wine
  comparison. `cc_t2`'s WMisty bridge is ported
  (`analysis/misty-bridge-20261106.md`), as is WGeyser, with its model's own
  update `0x4c9850`, fade and sounds open (`analysis/lap-line-geyser-20261106.md`).
- [ ] Course object classes not ported, by course (scene census, 2026-11-06;
  they probably explain several driving failures, as WMisty did on `cc_t2`):
  `cc_t4` none (WChiselCrossover, WRolling and their WBlowUp ported,
  `analysis/rolling-chisel-blowup-20261106.md`; `ct_t2` and `dr_t4`'s WRolling
  only read from the code); `dr_t2` none (WBumSteer, WRotoHill, WVolcano and WRot ported,
  `analysis/roto-magnet-volcano-20261106.md`); `dr_t3` none (WMagnet, WConstructionSite, WJunkyard ported,
  `analysis/saw-crane-gate-road-20261107.md`; the crane's third-arm squash since 2026-11-08, not compared with Wine);
  `dr_t4` its WGeyser is ported, not driven; `dt_t2` none (WFountain ported, compared with Wine);
  `sh_t4` none (WGoldenGate, WShiftedRoad ported); `cc_t3`
  none (WDicksTrick2 ported, `analysis/dickstrick-uvscroll-rooms-20261107.md`); `sh_t2` none (WSaw ported; its
  WLogRollingB is ported and compared with Wine); `dt_t4` APortal, ARoom (read: visibility data only, not ported; to confirm with a capture). WPalAnim has no visible effect found.
- [ ] WRolling, WChiselCrossover remainders: not compared under Wine: a squash by
  the rock, a car's bounce off wall or rock, a projectile on the rock, `ct_t2`,
  `dr_t4`. Provisional: the section of the chisel's route node, the bricks'
  reduced `0x4a71e0`.
- [ ] Two-player remainders: the viewport projection is fitted to Wine since 2026-11-08 (the view at
  1/√2 of its full-screen size in each half; where the original halves the camera ratio is not
  traced), the standings and finish rule, the audio listener, two-player
  championships (start codes `0xe` + mode). (Player 2's roulette and power-up panel are ported,
  2026-11-02, `analysis/two-player-20261101.md`; the pause's strings 198/199 too.)
- [ ] Input: joystick devices. Since 2026-11-08 (`analysis/joystick-20261108.md`) a Linux joystick drives
  a race: `[input] joystick1/2` or `WPC_JOYSTICK1/2`, with the axes rule of `0x4399b0` and 0x4966f0's
  joystick buttons. Open:
  - (the menu's device cycle with left/right is ported since 2026-11-08, `0x48cab0` → `0x48a845`);
  - rebinding a joystick's buttons on the controls page (natively refused); the controls page's
    button names (native B1.., the original asks DirectInput);
  - joysticks in the menus (2026-11-08, provisional): player 1's stick gives the arrows, its
    accelerate button accepts and its brake button backs out; in the pause the stick and accept
    only. The original's menu bindings for a joystick (list 3) and its joystick pause key are not
    traced;
  - the `m_Controller1NotPresentText` check;
  - force-feedback output (`0x417180`);
  - not tested with a real joystick (none on this machine), only with an event file.
- [ ] The Indeo codec's own conversion routine (in ir50_32, not traced);
  native colours are fitted to a VfW reference decode.
- [ ] Verify representative complete races and game modes against the original.
- [ ] Keep the native asset, scene, menu, mesh and renderer checks passing;
  add regression coverage for each newly implemented race, menu or save path.
