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
    - (The crossing test is the original's portal test `0x4a1a20` since 2026-11-15, section-culling note.)
    - The skid marks (`0x47a7c0`) follow their car's group since 2026-11-12 (`0x4a8e60` on the parent
      node `+0x10`, the car); not compared with Wine.
    - (A dropped weapon outside every slot box keeps group 0xffff: since 2026-11-15 the view draws such a mine
      or barrier always, unwrapped (`filed`); a mine's section is then -1, which the target search treats as
      the original's 0xffff: `0x4a7329..0x4a734a` wraps the cars only for the querier's section 0 or last.)
    - A fired or thrown object (missile, TNT, storm, paint shot) that no slot box holds: `0x4a6b30` returns -1
      before writing anything (`0x4a6bc8`), so the object is not in the manager's list (never stepped or
      wrapped) and keeps the cloned template's group (not traced). Natively it takes the firer's section and is
      tracked by the route queries. It needs a robot within 5000 of the player (near mode, items) but outside
      the player's three rows; not ported (2026-11-15).
    - With two players, both enabled sets are drawn: traced 2026-11-12, as the original (the enabled flag
      is global, both trackers set it; the seam's camera group is reset per view), section-culling note.
    - Robots and weapons across the seam were not compared with Wine.
- [ ] Runners/gates on `dr_t2` (2026-11-07): the scene matches Wine (sky and fog fixed:
  `analysis/on-screen-checks-20261107.md`). The gates' timing matches (2026-11-12, Wine memory reads with
  the profile's tracks unlocked in memory: the cycle starts at the scene's clock reset and lasts about
  468 updates, as natively, `analysis/roto-magnet-volcano-20261106.md`). The runner's squash is the
  original's rule (`0x4cefb7..0x4cf01d`: in state 0 each update gives the cars touching a runner, mask
  `0x100`, type `0x200` when idle) and `native_drive_test` now checks it on `dr_t2` (the player parked
  on the runner's path is squashed at update 204); not seen under Wine.
- [ ] `sh_t3` logs on screen (2026-11-07): the resting logs, the hand-off and the struck log match Wine
  (frames and the six WLogModels' world matrices `+0xc0`, update for update). Once, at 02:00 with
  robots past, Wine showed a seventh log-like object on the left road edge for at least 12 s. It is
  not identified (six WLogModels only; perhaps a robot's item). (2026-11-12: `sh_t3` is reachable under Wine with
  the profile unlocked in memory, and the car can be moved by shifting its fifteen position copies, but a long
  jump leaves the section culling behind, `analysis/runtime-capture.md`. Later the same day: the car hopped
  there along a robot's line recorded natively, 233 steps of all fifteen copies, then held at (232800, −4600);
  the view catches up by race time 02:04, and from 02:04 to 02:49 no log-like object showed on the left road
  edge. The car kept its own heading, not the earlier yaw −1.107, so the view differs from the sighting's;
  not identified.)
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
- [ ] Slides and skid marks not compared with Wine. (The item-effect case, `+0x385` = 0 during
  MonsterTires forcing the slide's x to 0, `0x46ae73`, is ported since 2026-11-11.)
  - Tried 2026-11-12 on `dt_t1`, both builds driven by the same timed XTest keys, about 25 s after the start.
    - A hard brake from about 0.4, then on, viewed with the rear view: no mark showed on the cobbles in
      either build.
    - A sharp left at full speed: Wine showed no mark either; the port's car was hit by a robot's item first.
    - Not conclusive: whether `dt_t1`'s cobbles take marks (the surface table) was not checked, and no
      road surface known to mark was tried.
    - By the way: the port's drift at the green light came from contact with the robots still on the
      grid; after idling, a straight run stays at yaw −1.57.
  - Later the same day, with the rear view held during the brake, plus Wine memory reads:
    - the original sets the skid flags (`+0x25b`/`+0x25f` = 1) and all four wheels are on surface 2, so
      it builds the same shade-`0x5c` segments as the port;
    - the port draws them as two grey strips (a control run with the drawing off removes them), and
      Wine's renderer 1 shows none;
    - renderers 1 and 2 share the vertex list `0x47b069..`, then go to different backends (`0x26fae78`
      against the Direct3D device's `+0xf0`), and the port follows renderer 2;
    - so whether the marks show needs the original on a real Direct3D device, not Wine
      (`analysis/car-effects-20261028.md`).
    - 2026-11-13: the renderer comes from the registry's `Prefs` record (first dword, then the device name
      written by `wpcsetup.exe`): with it set to 2 in a scratch prefix, `0x6f5958` reads 2, but WPC.EXE dies
      before the menu (page fault calling address 0) under Wine's builtin ddraw on Xvfb. Renderer 2 cannot be
      compared here; the device record may need `wpcsetup.exe`'s own choice (not tried).
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
    The shadow (2026-11-11, static): `+0x32` is the body model; the shadow `+0x2e0` is flagged only
    by the far mode (`0x47c1bb`), so it stays drawn during the hit as natively (not re-checked under Wine).
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
    propeller is not seen from the follow camera. Not compared with Wine. TurboSpeed's `+0x70`
    = 1.0 for vehicle 1 (`0x4d7015`) has no reader (2026-11-11: none in the 11 slots of `0x5162c8`); the body steps once per update (the original per draw);
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
  shows thin flames). The draw's rand() is the second stream `0x5472c4`, shared with the car effects and the
  bees and seeded with the race seed (ported 2026-11-14, `analysis/wfire-20261109.md`). (TurboSpeed's body clips
  are ported, see the FlyPower item.)
- [ ] Wheel spin (2026-11-09, `analysis/wheel-spin-20261109.md`): ported; the wheels roll and the front
  pair steers. Open: the sign test reads the speed copy `[esp+0x44]` (written at `0x46ae50` from
  `+0x3ef`, provisional), the step's length is the native forward step (the probes' pitch left out);
  not compared with Wine; far robots (no motion block) keep their angle.
- [x] `WPU_Detour`'s `m_aEffectAngle`/`m_aMaxSteerMod`/`m_nTrapTime`: no reader (2026-11-14, a scan of
  every `[r+0x18]` load followed by `[r+0x54/0x5c/0x64]`: 14 matches; the readers are WExplosion's init
  `0x4803c0` (vtable `0x513050`), FlyPower `0x4d560c`, a stack slot at `0x4e619d` and the `+0xd37` object at
  `0x4ea96e`; the writers the menu objects' inits and Detour's own `0x4e32a4`, `m_nTrapTime` = 5). Nothing
  to port; a Wine read watchpoint is not available in this setup (no debugger attach).
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
  the tyre models, the hidden wheels and the raised model `+0x92f` are ported; the wheel records
  `+0x44b` = 24, the tyre tracks' half width (end: ftol(`0x52b804`)), since 2026-11-11; the contact spread
  `+0x15b` = `0x52b800[vehicle]` (end: `0x52b7f8`; read 2026-11-11 as midpoint + (probe − midpoint) ×
  spread, `0x46ca33`, `analysis/vertical-motion-20261030.md`, waiting on the probe solve) and the original's `+0x385` = 0 are not); every weapon icon 11..25 now has an effect (Tnt, Storm,
  Detour, Flipper, BootKicker, Breath, Missile, TankGun, Boomerang,
  Landmine, OilSpill, BeeHive, FallingTree, BrickWall, Paintball), and the
  robots use them.
- [ ] Weather remainders: (the flake texture is none, 2026-11-09: renderer `+0x23969` is never
  written, so the flakes are plain white squares, `analysis/car-effects-20261028.md`), D3D line width (native ~1 px strips), the
  per-frame stepping (native per race update; its rand() is the second stream `0x5472c4`, shared since
  2026-11-15, so the original's draw count there follows the frame rate).
  - The Zartter shelter cut (`0x4565e0`/`0x456180`) is ported (`RaceTrack::sheltered`, 2026-11-02; its
    section rule is provisional).
  - The rain's lighting is ported (2026-11-08, `analysis/car-effects-20261028.md`) and compared with
    Wine on `cc_t1`: textures at half intensity (`0x4b08b1`), the clear/fog colour (`0x4ae364`), the
    dome's vertex colours (`0x4b4347`).
  - The rain streaks and the snow were not compared with Wine.
- [ ] (WKBumm compared with Wine 2026-11-15: the car hopped next to `cc_t1`'s cactus 25 in both builds, the same
  burst of pieces around a standing car for about a second, the cactus gone after; `analysis/car-effects-20261028.md`.)
  The shadow sits further from the airborne native car than in Wine, and the
  native car still drowns on the bank.
  - The drowning splash was recorded in both builds (2026-11-12, `analysis/car-effects-20261028.md`). The
    column, foam and timing are close. Wine's WalkOnWater ripples merge into one swirl as the car sinks;
    the port's stay as two ring sets at the frozen front wheels. That waits on the original's drowning
    motion (`+0x947`, the car rolling in and sinking).
- [ ] Winner scenes, remainders: WinCrep compared with Wine on screen (2026-11-16, reached by memory writes from a
  cc_t1 race, `analysis/native-championship-i-20261028.md`: the same frames, sequence and length); the other six scenes, DickOff,
  the timing and the sound not compared; the GDCamera, the WMenuItem's own placement.
  (The music is settled, 2026-11-12, `analysis/native-championship-i-20261028.md`: the sound manager's play
  `+0x4c` (`0x4273a0`) first destroys the running stream (`+0x44` = `0x427430`, `+0xbc`), then opens
  `music\WinMean.snd`, which the disc lacks. So the scene is silent after the course music stops, as natively.)
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
- [x] Steering from rest (2026-11-12, `analysis/steer-creep-20261112.md`): a steering key at a speed in
  [0, 0.1) sets 0.05 and `+0x257` = 1 (`0x4677da`), so a stopped car turns on a small circle; compared
  with Wine on the `cc_t1` grid, same circle. The release's coast to 0 and the 0.0815 rad a frame match Wine
  exactly. The joystick's analog branch skips it (`axis_steer`). (The `+0x3ff` lock is never set: its only other
  writer is the car init, 1.) The analog branch and the keys' order are ported too (2026-11-12).
- [ ] Recover WackyAuto wall/contact response (`0x46a4a0` after the probe
  queries), the gravity scale, the vertical/ramp response, out-of-bounds and
  respawn rules and `0x478070`; replace the native edge guard, ramp launch
  and stepped respawn. Wine evidence: the original slides past `sh_t1`'s
  tunnel post but stops dead at `dt_t1`'s first bend, and slides down
  `ct_t1`'s snow slopes (`analysis/wine-comparison-championship-i-20261028.md`).
- [ ] Vertex colours, remainders: the Direct3D colour of untextured parts
  (no DIFFUSE without VCOL, ALPHAOP disabled; native keeps COLS[1] and
  1 - TRAN from Glide `0x433d40`); (out-of-range VCOL: the cache's ftol and OR packing are ported,
  2026-11-15, ct_t1's GMDL 361 not compared on screen); the splash and
  WalkOnWater (additive, TRAN x VCOL) and the 2D icons' "transparency 0.4"
- [ ] Route cursor remainders: how the original updates `+0x2c5` (native:
  the player's route query each update, the robots' driver module), the `0x4a394e` fallback
  without a polygon, the start-line test `0x4a3f28` (forward-linked polygon
  and `+0x2b9`); the race distance uses the rows' `+0x34` (see the rank item).
  - Read 2026-11-13 (static, not ported): the ranks' progress is not `0x4a3937`'s. The loop `0x4a3d4d..0x4a3f1a`
    (over the rank table `+0x7f0`) writes `+0x1a3` (distance into the lap) and `+0x1a7` = (`+0x187` − 1) × the lap
    `+0xf38` + `+0x1a3`; a car with lap `+0x19f` ≤ 0 gets `+0x1a3` = 0 and `+0x1a7` = count − rank − 10000.
    `+0x1a3` sums `0x4a3c30` over the route rows `0..+0x1b7` and their polygons up to `+0x1bf` (the cursor's row
    and polygon; row table at manager `+0x2c` → `+0x20`, count `+0x1c`; polygons at row `+0x3c`, stride 0xcf0, count
    `+0x38`), with a start-line branch when `+0x1b7` = `+0x1bf` = 0 and `+0x1bb` > 0. Hidden robots add `+0x2b5`
    (`0x4a3ec4`). At `0x4a3f28`, when the cursor polygon `+0x2c5` has flags `+0x16 & 0xf0` and the row polygon's
    `0x40647e` equals `+0x2b9`, it adds the distance from that polygon's vertex (`+0x1c`/`+0x20` shorts) to the car.
    Native ranks use `race_distance()` (row lengths × the `0x4a3937` fraction) instead.
    Then `0x4a3c30`(car, route, brick) and `0x4a3a10`: each passed brick gives its route length `+0x34`; the car's
    own brick gives `+0x34` less what is left in it (`0x4a3a10`: 0 when the brick's `0x40647e` is not the car's
    `+0x2b9` and the cursor polygon `+0x2c5` has `+0x11` flags, else the remainder); brick 0 of route 0 also takes
    off the manager's `+0x34` (the start line's place in it) and, with `+0x1bb` > 0 (behind the line), counts its
    length twice. So the ranks' progress is the port's row lengths + fraction, up to an offset the same for every
    car. The `0x4a3f28` term (`+0x16 & 0xf0` is the forward link count, so nearly every polygon) is ported
    (2026-11-13, `analysis/rank-progress-20261113.md`): native − Wine on `sh_t1` is 3952 ± 4 at 75 of 79 samples.
    The cursor: the shadow update `0x47c180` writes it, and a miss clears it (`0x47c331`); the rank loop then keeps
    the car's last progress (`0x4a3d91`). Ported for the player (2026-11-13). The share is the route flags' byte
    `+0x11` (`0x4a3a10`), not `+0x17`; `0x4a3937` and its fallback `0x4a394e` are in the unreferenced `0x4a38d0`
    (switched and checked against Wine, `analysis/rank-progress-20261113.md`). Before the first crossing the
    original ranks by the rank-table index (`0x4a3d66`), ported in `race_distance`. The HUD shows `+0x1af` (`0x485d8d`), so
    `rank()` is now `race_rank(-1)` (2026-11-13). The robots' cursor is the same query
    (`0x4e82be` only clears with it; far mode queries the route layer alone and still hits, Wine 2026-11-13). Its
    miss rule is not applied to the robots: tried, it held native robots' progress off the route and broke
    `ct_t3`'s avoidance check (81% on the route). Open: that, once the robots stay on the route layer as in the
    original; routes past the first (`+0x1b7` > 0) not seen.
- [ ] Rank scaling remainders (2026-11-08):
  - The race distance and the HUD strip now use the rows' route lengths `+0x34`, as the original's
    lap length does (`0x4a3ba0`, manager `+0xf38`) and its distance in a row (`0x4a3a10`; `0x4a3937` is in the unreferenced `0x4a38d0`). Before,
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
  (the section-cell object list is the section row's, ported 2026-11-15, `analysis/robocar-ai-20261028.md`). (FlyPower: a robot's slot gets 7..10 from `(rand & 3) + 7`, its own gadget
  0..6 only with `+0x1c7` = 2 (`0x4e59a4`), which no instruction writes; 1 (WGoldenGate) has no reader and
  `0xb` (WRotoHill) is the MonsterTires use, ported; so the robots never fly, as natively, 2026-11-15.) (The robots' battle mode, `+0x34`,
  `0x4e9050`, is unreachable: battle is two players only and the menu never makes a robot opponent,
  `analysis/battle-20261102.md`; the native one-player battle exists for tests only.)
- [ ] Avoidance remainders: (compared with Wine on `cc_t1` and `sh_t1` 2026-11-14 with hooks in the loop:
  the step `+0x2ed` and edges `+0x35d` are per-car caches filled on the first request and cleared at the end
  of the car's own turn, so a robot's own step is its last update's move; ported as `AvoidCache`, v0's
  avoidance on `cc_t1` now starts at update 23 as Wine's, `analysis/race-stream-20261114.md`; the caller of the
  clearing `0x469550` is not traced) the target-speed angle while avoiding; stale `+0x315/+0x321` with fewer
  than two shared vertices; edges across sections. (The app gate `[0x5472b8]+0x1a60` == 1 (`0x4e699a`, `ebx` = 1
  from `0x4e68ce`) is the battle: ported 2026-11-14, `native_drive_test` checks the arena robot never avoids;
  without the gate cc_b's avoids 6 updates. The `0x6f2d00+0xf30` 2/3 test at `0x4e6962` skips the decision
  while `+0xb8` > 0: that is the start hold, ported as `Opponent::start_hold`.)
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
  jump (22000, -19600). The human slide `0x46a5a3..`/`0x4721f0` is ported
  (`RaceSession`'s glide, `player_glides`) and does not apply to robots (`0x46c858` clears `+0x157`). With
  the player parked at the `cc_t1` bend the native line matches Wine's
  (x -60930 against -60849 at z 13000, no fall in 108 passes): the falls
  only come in the always-near test pack (first lap), not compared.
  Re-measured 2026-11-14 with the start hold (seeds 0..5, 3600 updates, always near; worst robot laps):
  `sh_t1` all ≥ 2, `dt_t1` ≥ 1, `cc_t1` seeds 4 and 5 at 0, `ct_t1` seed 1 at 0. `cc_t1` seed 4's robot 1
  spins in the pack on the first climb (yaw −0.86 → 1.93 → −2.40 over updates 210..220), comes to rest at
  (-5634, 1171, 6530) on surface 1 and stays there to the end: throttle 0 in every state, the unsticks
  included, since `0x4e82d0..0x4e82dc` clears `+0x2ac` after the whole decision (the port's order). Its
  height sinks 0.14 an update with no xz motion: it lies on the wall face itself (pitch −1.12, roll 0.57, up (0.26, 0.42, 0.87)), and the robots' one ray along the car's up axis (`0x46c7f2..0x46c97e`) keeps meeting that face, so it stays grounded on it. `cc_t1` seed 5's robot 2 sits at (-60530, 1772, 10688), the
  bend above; `ct_t1` seed 1's robot 5 shuttles at (-23900, -1700, 60900) unsticking. Not compared with Wine
  (the always-near pack has no Wine equivalent). With the original's seed 0x1312d6e (2026-11-14) a `dr_t1` robot (vehicle 6) crosses the loop seam at
  update ~1190 at speed 0.95, turns round (state 4), rolls onto flat ground flagged 1 at (-128422, -3870, 16060), off the
  route layer, and stays there to the end (the throttle cut). `native_drive_test`'s edge check now counts only cars
  on the route layer.
- [ ] Trams, remainders (the crossing lights' UV animation `0x4caf95` is
  ported, 2026-11-01, for renderer mode 1): the tram frame copy and the pi/2 turn axis
  (provisional; seen under Wine 2026-11-15 with the car hopped along a recorded line to dt_t1's crossing:
  the red two-car tram upright and side-on across the road in both builds, `analysis/runtime-capture.md`); the other WSaw/WRotoHill/WGoldenGate
  states (no instance on Championship I). A
  native robot reaching the crossing at 0.817 (always-near test pack)
  flies over the dip before it and stops outside the zone, keeping state
  5; the original's robots arrived at 0.57..0.66.
- [ ] Other linked nodes: cameras and portals. (A pickup not in state 0 is untouchable for every car,
  robots included, as natively: the car update tests `+0x142` before anything else, `0x46aad5..0x46aae0`,
  and the robot flag `+0xc0` only skips the sound, `0x46ab05`; static reading 2026-11-12,
  `analysis/native-powerups-20261028.md`; not seen under Wine.)
- [ ] Hidden robots, remainders: (their lap trigger is ported since 2026-11-12: the lap pass `0x4a43f4`
  counts a hidden car in section 0 coming from a later one, `+0x1bb` > 0, past the start line's
  distance into row 0, the manager's `+0x34` from `0x49e552`; `+0x1bb` is the previous section, kept by
  the trackers `0x4a13ae`/`0x4a234d`/`0x4a201d` and cleared by a forward crossing; and a robot shown
  into a row is no longer hidden at once on the previous section's ground, which on `dr_t2` held
  every robot hidden in section 0 with the player two sections ahead, provisional grace of 60
  updates (dt_t1 against Wine, 2026-11-15: every hidden span 83 updates in both builds, no re-hide soon after a show in either); `analysis/robocar-ai-20261028.md`); (the section-0 placement with `+0x93f` set, `0x4a2108`,
  is ported since 2026-11-12: a robot shown in section 0 right after its lap stands at
  `m_idStartLineModel`'s frame, 611–709 from it on the 11 four-section courses, against 1100–4900 from
  row 0's portal without the branch; the mark's clearing off section 0 is provisional; not compared with
  Wine); a robot that
  goes far (not hidden) with `+0x1c7` = 5 outside a crossing's zone stays
  put until it leaves the player's rows (native: after the airborne trap
  below, with the player parked in dt_t1's section 2).
- [ ] Car contacts, remainders: robots in the far mode. (`+0x2d1` is only
  written, at `0x46a47e`/`0x46b3b1`/`0x46b55e`, with no direct reader; voice
  event 4 is ported; the ±6° turn's sign follows RL − RR, `0x46b662`,
  2026-11-01.)
- [ ] Vertical motion remainders: (the second `sh_t1` ramp's pitch matches: per frame under Wine it reaches
  0.493, the port's 0.491; the old 0.437 was a sampling gap, 2026-11-12, `analysis/vertical-motion-20261030.md`), the
  ceiling ray (`0x46c6cf..0x46c7c4`, ported 2026-11-16 and compared with Wine on dt_t1: in the air, a one-sided ray of mask
  `0x1e000ff` from 26 under the car to 200 above that point lowers the car by B - H under a ceiling; not a lift;
  `analysis/vertical-motion-20261030.md`; the 26 is measured, provisional), the contact-point spreads `+0x15b`/`+0x15f`. (The robots' ×1.3 ray `0x512a40` and the
  `0x800000` return `0x4a6730`, frame + 200 along + 50 up, are ported: `RaceSession::settle`, `respawn`.)
- [ ] Robot lines (compared with Wine on `cc_t1`, 2026-11-12: on the same branch within ~20–30 units; the
  fork's branch is the random `0x4e9610` draw, and from the grid start too; `dt_t1` too, medians 22–44, one robot returned
  to the track under Wine at the tram crossing; the 18 other courses too: medians 4–17 from some native
  robot's line, `analysis/robocar-ai-20261028.md`; `dt_t2`'s fountain (explained and ported 2026-11-14: the hold rebuilds a far robot's frame from its frozen
  yaw `+0x26b`, `0x4ca647` → `0x468620`; Wine's v6 stays trapped for good, natively three robots; which ones depends on
  the start pack's yaws, not compared update by update; `analysis/lap-line-geyser-20261106.md`) held one far robot under Wine for 13–33 s,
  reproduced, where the native far step carries it through; traced in `analysis/lap-line-geyser-20261106.md`: the goals, the
  link draw and the far step agree with the port; it is a far-mode loop between route polygons 132 and 270 that
  only a wide approach plus the eastern link produces, never seen natively; no rule difference found); the original's
  far-mode route lookup across sections (`+0x2c5`) is not traced. The 5% lap gap is closed (2026-11-14,
  `analysis/far-step-laps-20261114.md`): Wine's seconds were wall time at ~26.7 updates/s; counted in updates the
  native far robots were 6.5% slower, because the far step `0x4e54ef` is top × `+0x163` × WCtrlRoboCar `+0x4c`
  (203 under Wine, the probe-point distance at the init `0x4e4869`) × 0.56/0.48 with no frame factor. Ported; on
  `sh_t1` each robot's lap ends are now within 10 updates of Wine's (vehicle 6: 34 behind, from the start). Vehicle 6's
  34 updates were its `+0xb4` draw; the race load reseeds the stream with 0x1312d6e (`0x431fc3`), so the
  grid is draws 1..6 and `+0xb4` draws 7..12, now ported (default seed 0x1312d6e, no clock): `sh_t1` and `cc_t1`
  lap ends within 1% of Wine's. `ct_t1` too (within 0.6%, 2026-11-14), `dr_t1` within 1.7% (vehicle 6 79 updates early from lap 1,
  the start). `ct_t2` and `dr_t2` within 2%, `cc_t2` within
  1.5% but v5 359 updates late on lap 2 natively (native v5 is hidden 286 updates every lap, (16860, 10102) to the
  portal at (34828, 20712); Wine's v5 lap 2 is 315 updates shorter than its own lap 3, not traced), `sh_t2`: Wine's v6 held ~3300 updates on lap 2, frozen far at (40570.8, 1167.5, 89821.4)
  riding a WLogRide log (`+0x89f`), speed 0, no hit; lead: the far step starts from the stored route contact
  `+0x2d5` (shadow query `0x47c180`), so a contact not refreshed plus the ride's heading reset gives the same point
  every update; cause found: a storm cloud strike (type 0x400) on the far robot at update ~1470 set it
  down 1420 west in the pocket (40570, 1167, 89821), and after the strike the far step never moved it again; the
  move is the car update running the struck far robot along its frozen yaw `+0x26b` (frame rebuilt from the angles),
  ported 2026-11-14; it stays because it drowned there and a far robot's drowning counter
  `+0x8e7` stops at 1 (the return below 1 is not reached while far), ported 2026-11-14 too,
  `dt_t2`: Wine's v0 held ~1270 updates at the fountain on lap 2 and then freed (how not traced). Cards 3 and 4 compared too (2026-11-14,
  table in the note): `sh_t3`, `ct_t3` within 3%, `dr_t4` 5.7%, the others with single-lap events. `cc_t3`'s systematic gain was
  the WGeyser holding each far robot about 60 updates a lap under Wine, which natively never hit anyone: m_Distance
  (`0x241`) is a float read as an integer (fixed 2026-11-14); the native robots still slip past it (their line); the events on `sh_t4` (Wine's v5/v6 from lap 2),
  `ct_t4` (native v0/v5 late), `dr_t3` (native v6 lap 3); championship mode.
  Robots move at `+0x163` = 1.0 (`0x469550`: `[car+0x18]+0xc0` set on robots; the player 0.9333), and the step
  stretches the whole 204 above speed 0.4 (fitted to Wine): the `sh_t1` start pack now matches Wine within 3 units
  for 19 updates. Open: the first steer comes one update earlier natively and smaller (order of the decision and
  the car update; a plain one-update steer lag fixes vehicle 0 but breaks vehicle 1, reverted); the one-update shift was
  the car update's order (it moves along the last heading and turns after, `0x46ba57`), ported, and the hold is
  back to the recovered 20; the far test's timing is fixed (the switch update also runs the far step). The race
  stream's consumers are traced (`analysis/race-stream-20261114.md`, a rand hook under Wine): the GO-update draws,
  the link draws during the hold and the engine jitter `0x47058a` are ported, and the native draw counts follow
  Wine's. The cars update in their `general.wgm` template order (4, 0, 6, 5, 1, 2, 3; Wine's caller registers),
  each robot's car update before its controller: ported, and with the stream offset by hand all six first
  decisions on `sh_t1` equal Wine's (v0's first turn included). The `Kosza` creatures' draws are ported (the
  stream equals Wine's on `sh_t1` and through update 20 on `cc_t1`). `cc_t1`'s v0 avoiding v2 at update 21
  natively was the avoidance's step cache (a robot's own step is its last update's move), ported: the packs
  follow Wine within 10 units to updates 157..242 on `cc_t1` (v0 185) and 162..210 on `sh_t1` (v0 163, was
  33), except far robots, which switch from goal 2 to goal 1 a few updates apart (rounding of a settled angle,
  not a rule). The player's engine draw and the car contact's dropped draw `0x46b822` are ported (2026-11-14,
  rand hook with Up held). The player's place in the order for every vehicle is ported (Wine, vehicle 5).
  See also `analysis/far-step-laps-20261114.md`.
- [ ] The start (2026-11-13, `analysis/start-boost-20261113.md`): the countdown rev and false start (`0x4a2b57`), the
  GO boosts (robot slots 0..2 row[1] × 9/6/3, the player × (rev × 4 + 1), `0x4a2c22`) and their 99-update restore
  (`0x46967c`) are ported and match Wine's robot speeds. With Wine's grid (`WPC_GRID_SEED=140`) the native front row
  runs on after GO; the crash seen before was another grid's (vehicle 0 beside vehicle 6). Under Wine the false start
  is confirmed (Up held through the load: `0x6f5834` = 1, no rev, a plain start). Key scripts reach the grid at the
  countdown's update 55–57 of 60 and their keys act about 1.1 s late, so start timings measured with them are a
  second off. Robots do not knock the player aside at a normal start in either build (native 5 grids, `sh_t1`/`cc_t1`; Wine `sh_t1`). After GO the robots go straight because of the goal rule (not within 1000 of goal 1 with the goals on opposite sides), Wine 2026-11-13. Open: natively the slot 0 robot enters the avoidance branch for 8 updates, because slot 1 beside it counts as barely ahead (dot +0.35); Wine's does not (the original's ahead test `0x4e69bd..0x4e6c70` matches the port's and also counts it ahead; the cached steps `+0x2ed` are the port's `moved()` too; next: Wine's slot 0 and 1 `+0x173`, `+0x2ed` and `+0xc4` on the first frames after GO. Captured 2026-11-13 with longer menu delays and the reader's `track` command: the GO update drives nothing (ported), the goals match, Wine's avoid side stays 0 and its steer 0. Found 2026-11-13: WCtrlRoboCar `+0xb8` = 20 at its init (GO, `0x4e48d2`), counted down each update (`0x4e4f70`); `0x4e6962..0x4e697d` skips the whole decision while it is above 0 (states 2 and 3), so robots drive straight for 20 updates (Wine: first steer and list count about 22 frames after GO). Ported as `Opponent::start_hold` (2026-11-14, `analysis/start-boost-20261113.md`), which matches Wine's first steer (x −15259, −0.0795, against −15256, −0.0789); the hit check `0x4e53df` comes before it, and three checks were adapted to it (far robot hit: skip hit, shielded and holding robots, farthest point; dr_t3 autopilot: drives again after the stuck rule). With the hold the native slot 0 robot no longer enters avoidance after GO (sh_t1, grid 140: avoid 0 over the first 60 updates, first steer at update 19), so the 8 avoidance updates noted above are gone, as under Wine. Wine lap times with the hold: player idle, every robot within 1.4% of Wine over three laps on cc_t1, 0.4% on ct_t1, 2.2% on dt_t1 (single laps up to 90 updates apart: no robot waits at the tram in either build; v1's lap-1 loss is a fork taken the other way after the stream split; the split at update 24 was the item slots' place in a car's turn, ported 2026-11-15, then the power-up list's count during the start hold (`0x4e7321` after the hold skip) and the beehive's and oil's drop 270 behind (`0x4de850`, `0x4ddb90`), ported 2026-11-15: the stream follows Wine's to update 47 and the first laps of four robots within 8 updates; robot 6 under the bees too (the hive's `+0x68` holds 6257.7 until the first draw, read under Wine, ported provisionally), so the order of finish is Wine's and the stream stays within a few draws to about update 750; each robot's `+0x4c` is the float-truncated probe span at GO (203 or 204 by course, `analysis/far-step-laps-20261114.md`), ported; dt_t1 robot 1 follows Wine within 0.1 unit to update 52; from 53 its steer differs in the 4th decimal near a goal (float against x87 in the car update), which grows into the later splits (no rule difference found, `analysis/far-step-laps-20261114.md`); `analysis/race-stream-20261114.md`), 2026-11-15, `analysis/far-step-laps-20261114.md`); (PRET: the car update runs but its motion, pushes included, is zeroed, `0x46b97b`;
  ported.) (The TurboSpeed cancels `0x46a9b6`/`0x46c5c1` end a running start boost too, ported,
  and the solid contact now cancels TurboSpeed natively.)
- [ ] Robots trapped against walls with the robots always near (seeds 0..5,
  3600 updates; worst robot laps; 2026-11-14 with the start hold: `sh_t1` all ≥ 2 laps, the others under the
  wall-flagged slopes item): `sh_t1` seeds 0 (1 lap) and 1 (0),
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
- [x] Power-up effects (closed 2026-11-12): every gadget 0..10 and weapon 11..25 action of the template
  spawned by `0x4d2970` has its effect (`analysis/weapons-bomba-20261029.md`); the pickup sound
  (`0x44c380`) is ported since 2026-11-02 (`analysis/native-powerups-20261028.md`). Their remainders are
  the MonsterTires, FlyPower, weapon-visual and backward-use items. (The item keys come from the
  MenuPrefs table: '/' gadget, right Shift weapon.)
- [x] The pickup take's `+0x132 <= +0x13a` test (`0x46aab8`) is WHotSpot's `m_iCounter <= m_iUnlockCount`,
  always true on the 25 shipped files (0, max 0, unlock 1), so equivalent natively (2026-11-12,
  `analysis/native-powerups-20261028.md`).
- [ ] Rest of the object query `0x4a5730`: the route-grid cell scan (native
  tests every course object; `WPrimCylinder` 0x482310, car contacts and
  the 0x10000/0x40000 hits are ported); the pickup range matches Wine (2026-11-16, cc_t1: taken up to 174
  from the pickup over the road, not from 175, in both builds, `analysis/native-powerups-20261028.md`);
  the roulette outcomes are not compared with Wine.
- [ ] Pause remainders: (page 243 is not shown in two players either, Wine 2026-11-12: the option list at
  once, `analysis/pause-menu-20261030.md`; its list sits about 20 px higher than the port's there: static, not a slide, the page data and the layout `0x48bab0` are the same in both modes, source not found, 2026-11-12); the x shift of the list seen under Wine
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
    about 5 s (2026-11-14: far robots now circle the column for good, as under Wine, see the robot lines item);
  - +0x8bf compounding on surface 1 (`0x466df6`): compare a bombed car under Wine. On `cc_t2` (2026-11-08)
    a robot struck by the storm, in the air (surface 0) and held, had its speed double every update
    (-5.9 ... -188); its front wall probe, which stretches with the speed, overflowed the ground
    query. Natively a speed past 1e4 now returns the car to the road, like the position safety (the
    original's __ftol would not throw).
- [ ] Race audio, still open (the power-up voice events of the Flipper,
  BootKicker and BeeHive are ported; the engine pitch's jitter is the car update's race-stream draw since
  2026-11-14, `analysis/race-stream-20261114.md`); the exact Windows
  DirectSound3D pan/HRTF; no Wine capture of the 3D levels (2026-11-12: Wine runs silent with an ALSA file
  tap, `analysis/runtime-capture.md`, but the capture's sample format is not settled).
- [x] Battle remainders (closed 2026-11-12; the end screen, which needs five hits, was not compared
  under Wine): the robots' battle mode (`+0x34`, `0x4e9050`; only a
  one-player battle, which the menu never makes, has a robot), (the winner
  text's pulsing colour is ported since 2026-11-11, `0x4aa497`; the route `+0x8c4` is always 0, also
  2026-11-11: its three writers in `0x49dc90` store a zeroed `esi`, so route 0's first row, as natively).
  The robots' battle mode is unreachable from the menu (`menu.WGM` has one battle node, start code 15,
  two players). Compared with Wine 2026-11-12 (`cc_b`, two players, `analysis/battle-20261102.md`):
  - (fixed 2026-11-12: the radar; `0x4aa120` puts the opponent's dot in the manager's `+0xf94..+0xfa0`
    and the manager film's battle branch `0x499b96` draws frame 0, the disc, at (40, 40) or (32, 32) per
    viewport and frame 1, the dot, there; `m_nRadarRadius` 20 and `m_nRadarRange` 10000 at run time;
    `RaceView::radar`, matched to Wine's discs and dots within 2 px);
  - (fixed 2026-11-12: Wine shows one hit count per half, child 14, the opponent's hits received;
    child 13, the own count, shows only in one player, `0x486105`; the port now draws the right one only);
  - (fixed 2026-11-12: the top half's camera; Wine memory reads put player 1's camera at view 0, 800
    back and 100 up, player 2's at view 1, 900 and 350, and one Return switches player 1 to 900/350:
    the Return that starts a battle, which has no countdown, reaches player 1's CAMERA key; the port
    now gives a menu-started battle's first update a view press, provisional mechanism).
- [ ] Menu remainders: (the right-aligned option labels' 5-9 px shift is fixed since 2026-11-11: the
  original lays text out in screen pixels, spaces 10 px unscaled and each glyph's step truncated,
  `0x42dc50`/`0x42de80`; the options page's labels match Wine to the pixel but VOIX (2 px) and
  LUMINOSITE (1 px); the key-help GDPsText line takes a provisional fitted narrow of 1, path not traced,
  `analysis/menu-profiles-20261101.md`).
  - Wine walk 2026-11-11 (main menu → 1 JOUEUR → COURSE SIMPLE → car → environment → variant → race,
    `analysis/menu-profiles-20261101.md`): the car page's texts match; the closed dr banner is now
    greyed (page children 2r/2r+1 by row, `0x490d60`/`0x4910a0`/`0x4913f0`); the track pages' three
    mask-0 icons of brick `0x23d` are hidden (provisional). The pause's dim is ported since 2026-11-11 (the
    renderer's screen wipe, slots `+0xd0`/`+0xcc`/`+0xc8` = `0x4ae6f0`/`0x4ae6b0`/`0x4ae730`,
    `analysis/pause-menu-20261030.md`). Open: the port's menu is ~3.6% brighter than Wine's (pixel ratio 0.964:
    Wine's 239/230/222 against 248/240/232; the region means had said 7%). Wine's frames use only
    R5G6B5 levels (32 red, 64 green values) and the original sets 16 bpp (`ddraw7_SetDisplayMode … bpp
    16`), so the original draws into a 16-bit surface; how 248 becomes 5-bit 29 (texture conversion or
    Wine's) is not traced, and the port keeps 8 bits per channel; behind the pause Wine keeps the power-up gauge and the speedometer undimmed (1.0; the port draws them after the wipe since 2026-11-11, provisional order) and dims the item icons to 0.64 (explained 2026-11-12: compared on the same picture, the uncharged icons' opacity is 0.36 = 0.6², provisional, not 0.6; the port now gives 0.638/0.630, `analysis/native-hud-20261028.md`). The opening wipe at each scene load (`0x431690` → renderer slot `+0x1c`
    → `+0xcc`(0)) is ported too and matches a Wine recording of a race load (timing within 0.1–0.2 s;
    the native start is the load's end, the original's before the load). (The W image drawn over the keyboard on the controls page is hidden since 2026-11-11:
  `0x48eff0` shows the page's child 0, the keyboard, or child 1, the joystick image, by the player's
  device, `analysis/menu-profiles-20261101.md`.) (The width routine's -4 branch `0x42dd99` needs the renderer
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
  - `0x48eff0` branches on the device (`0x6f4580[player]`, −1 keyboard): its two `0x48cec0` calls
    set or clear the selection mask on the page's children 0 and 1, the keyboard (#78) and joystick
    (#79) sprites (2026-11-11, ported), then RETOUR DE FORCE's availability (below).
- [ ] Moving course objects: on `sh_t3`, WLogRide's logs move, are drawn and
  carry cars (`0x4cc450`, `0x4cc69e`), with the
  model mesh taken as their ground (`analysis/log-ride-20261102.md`).
  WLogJam, WLogModel and UVScroll are ported (`analysis/log-jam-20261106.md`,
  `analysis/dickstrick-uvscroll-rooms-20261107.md`). No Wine
  comparison. `cc_t2`'s WMisty bridge is ported
  (`analysis/misty-bridge-20261106.md`), as is WGeyser, with its model's own
  update `0x4c9850`, fade and sounds open (`analysis/lap-line-geyser-20261106.md`).
- [ ] `Kosza` creatures (2026-11-14, `analysis/race-stream-20261114.md`, `analysis/kosza-motion-20261114.md`):
  states, waypoints, rates, race-stream draws, the moves (Hermite `0x458230`, chord, circle and Lissajous
  `0x458be0`, GDPath `0x457f80`), the frame along the motion (`m_bRotate`), the hidden waiting state (flag
  0x100), the clip (speed `0x52a990`, start frame, freeze) and the car hit (`0x459311`/`0x45905b`) are ported;
  sh_t1's birds and cc_t1's two creatures match Wine's node frames (birds until the stream splits at t 0.5).
  The hit was compared with Wine on cc_t1 (2026-11-15): the car parked on its line stops it unsquashed and squashes it once it moves, as natively, now that the query's radius is the PRIM's own, unscaled (`0x4a6198`; it stopped one step early before). A moving car's squash and the restart (frand() < 0.01, counters patched into the original) match Wine too (2026-11-15). Open: mode 2's walk `0x456af0` (unused); a reversing car's touch not compared with Wine (mode 4 on dt_t3 is compared, 2026-11-14); the object query's car order is Wine's (the manager's list in grid order, the player first, read on cc_t1 2026-11-16; two players not checked); the clip's pace matches Wine (2026-11-16: the bird's controller, `[node + 0x166]` element 2, steps 20 an update = dt 10 x speed 2, as the view does).
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
- [x] Two-player remainders (closed 2026-11-12, `analysis/two-player-20261101.md`):
  - the viewport projection: the camera ratio is not halved (Wine memory read: both follow cameras
    keep 100); the Direct3D projection `0x4aec30` takes s = √(viewport height / width × ratio × 0.01),
    which gives the earlier 1/√2 fit exactly; `RaceView::draw_view` derives it from the two aspects;
  - the finish: the first player to finish ends the race (`0x4a43b9`/`0x4a2dcf`), ported; so does the last robot to finish in a single-player race (`0x4a8a10`, 2026-11-13);
  - the standings: state 4 → 5 as in one player with both players in the rank table, drawn in
    viewport 0 only (`0x499c7d`), as natively (not compared on screen);
  - the audio listener: viewport 0's camera, player 1's (`0x427620`), as natively;
  - two-player championships do not exist: `menu.WGM` has start codes 14 (race, `0x147`) and 15
    (battle, `0x146`) only, and AUTRE COURSE's `0xe` + mode returns to them (`0x491d1e`, ported).
  (Player 2's roulette and power-up panel are ported, 2026-11-02; the pause's strings 198/199 too.)
- [ ] Input: joystick devices. Since 2026-11-08 (`analysis/joystick-20261108.md`) a Linux joystick drives
  a race: `[input] joystick1/2` or `WPC_JOYSTICK1/2`, with the axes rule of `0x4399b0` and 0x4966f0's
  joystick buttons. Open:
  - (the x axis steers through the original's analog branch since 2026-11-12, proportional inside ±85,
    then a degree an update past it, `0x46769a..0x467798`, `analysis/steer-creep-20261112.md`;
    the y axis accelerates below 0 and brakes above 0 as the speed code reads it, `0x466f9e..0x466fe8`;
    open: the device value's scaling into the byte, DirectInput's range and dead zone);
  - (the menu's device cycle with left/right is ported since 2026-11-08, `0x48cab0` → `0x48a845`);
  - rebinding a joystick's buttons on the controls page (natively refused); the controls page's
    button names (native B1.., the original asks DirectInput);
  - joysticks in the menus (2026-11-08, provisional): player 1's stick gives the arrows, its
    accelerate button accepts and its brake button backs out; in the pause the stick and accept
    only. The original's menu bindings for a joystick (list 3) and its joystick pause key are not
    traced;
  - the `m_Controller1NotPresentText` check (2026-11-12: a WMenu member, deactivated by WMenu's update
    `0x48fac0`, the per-frame slot `+0x8`, every frame; nothing else reads the member, so it would need
    another object's reference to show; none found, `analysis/joystick-20261108.md`);
  - force-feedback output (`0x417180`);
  - not tested with a real joystick (none on this machine), only with an event file.
- [ ] The Indeo codec's own conversion routine (in ir50_32, not traced);
  native colours are fitted to a VfW reference decode.
- [ ] Verify representative complete races and game modes against the original. (2026-11-11: a 200 s
  `cc_t1` run with the player idle, recorded in both builds: the robots' pace on the strip agrees
  within the 5 s sampling; the player's head sat 11% into the lap before the first crossing, fixed
  (`0x4a3d5c`, `analysis/native-hud-20261028.md`). 2026-11-12: the robots'
  laps over a whole `cc_t1` race against Wine memory reads: the lap paces match within about 3 % per vehicle, the
  first lap of vehicles 1 and 2 differs because the random grid order (`0x49d580`) put them in other slots,
  `analysis/robocar-ai-20261028.md`. 2026-11-12, `analysis/driven-race-20261112.md`: one
  closed-loop driver run in both builds on `cc_t1`.
  - Native drives three laps: 1611, 1564 and 1523 updates.
  - The original's lap 1 takes 1558 frames, counted at `0x214c3a8`. Its wall time is 57.8 s because
    the frame limiter `0x4ad540` holds Wine's ~37 ms frames to 2/60 s of motion. The motion per frame
    matches.
  - On `sh_t1` (2026-11-12) lap 2 is counted in the original too: 2029 frames, against the port's clean laps
    of 2184–2374 updates. With the port deciding at Wine's rate (every 1.35 updates) its lap 2 is 2046,
    within 0.8 % of Wine's 2029: the earlier 3–7 % was the driver's timing, not the car. Lap 3 in the original
    is 1559 frames (the finish), with stretches above top speed (124.6 a frame; a boost or push, not logged), while
    its lap 2 has a hold-up; the port's laps lie between. The repeat (2026-11-13) shows a TurboSpeed (`+0x385` =
    8) started for the player without the gadget key, at the lap-3 refill. Explained (2026-11-13): every robot had
    finished, so the rank update `0x4a8a10` ended the race (`0x4a2dbd`) and put the player on robot control
    (`[+0x18]+0xc0` = 2, `0x4a2df8`), whose item code used the TurboSpeed. The race end is ported; the
    autopilot is provisional (item below). (A second attempt, 2026-11-12, with a route recorded
    natively: native laps 1 in 1496–1531 updates, then both builds' driver ends in a pocket off the outside
    of the left-hander near (−21000, 19800) and cannot get out; it needs a better line or speed control
    there, `analysis/driven-race-20261112.md`.) Its car twice stayed for tens of seconds against
    the outer wall of the left-hander near (−57500, 4570), where native recovers in 2 s; the wall
    response or the driver, not separated. Then the standings of a driven race.
  - [ ] The player's autopilot once the race has ended (`0x49eb20` writes 2 into `[+0x18]+0xc0`): ported
    provisionally (2026-11-13, `analysis/driven-race-20261112.md`: a `RoboDriver` with the robot speed and steering
    branches `0x466e0b`/`0x467886`). The controller is the player car's own WCtrlRoboCar (`+0x1c3`, found under Wine
    2026-11-13) and its item use (state 5, `0x467c9c`/`0x46803b`) is ported. Its init `0x4e45e0` (doubled
    gravity, dir modifier 0.25, item wait) runs at the end, ported and confirmed under Wine (2026-11-13: `+0x24`
    attached, gravity 0.05 → 0.1). Finished cars drive on in the original (no parking), now in the port too.
    The rank scaling and item code run for finished cars and the autopilot (no finish test in `0x4e4f10`),
    ported. Compared with Wine on `sh_t1` (2026-11-13): the same route and pace after a stall-counter fix,
    and pickups after the end (ported). Open: the `sh_t2`
    pocket near (42700, 92500) where it sticks (see below). Pocket diagnosis (2026-11-13, debug
    print of the autopilot): heading straight (yaw 0.02) at 0.57 the player's car drifts about 16 units per update to
    the right from z 87100 to 91200 (x 41650 → 42430), 400 units right of the robots' line (goal x 42005), and enters
    route polygon 1293, whose goal (42416, 92280) is soon behind it; the module has flag 1, so the driver turns
    round (state 4), brakes before the drop at z 93300, falls (surface 0x800000) and respawns at (41975, 82859).
    The test driver stuck at the same spot. The drift's cause (found 2026-11-13 by splitting each update's motion):
    the forward step moves the car along its yaw (−1.7, +90); the +22 x an update comes from `update_objects`, the
    WLogRollingB placement (`0x4c2547`, `CourseObjects::rolled()`): the log at z 85148..93778 turns the offset of
    the player cars only (`0x4a7e70` flags 1, `analysis/lap-line-geyser-20261106.md`) by 0.0349 an update. So it is the
    course's hazard, not a port error; the robots are never turned. The robot driver (autopilot) does not steer
    against it enough to stay on the robots' line. Whether the original's autopilot falls off there too is not
    checked (it would be turned, being a player car).
  - The finish standings screen matches with a forced last lap (Wine `+0x19f` = 3, native `WPC_LAST_LAP=1`), `analysis/native-hud-20261028.md`.)
- [ ] Keep the native asset, scene, menu, mesh and renderer checks passing;
  add regression coverage for each newly implemented race, menu or save path.
