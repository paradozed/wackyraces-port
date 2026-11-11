# WackyAutoBomba, the weapon icons and WPU_Landmine

Recovered from the executable and `general.wgm`. Native code: `src/port/race_weapons.*`, `RaceSession::hit`, `drop_mine`, `update_mines`, `bomb_step`, and the mine draw in `RaceView`.

## WackyAutoBomba (the car's hit reaction)

- **Class.** Registered at `0x47c900` (name `0x52b388`, class id `0x78946bcf`), factory `0x47c950` (size `0x8e`), vtable `0x512fd8`, update `0x47e220`. Every `WackyAuto` template in `general.wgm` lists it after `WackyAutoInit` and `WackyAutoNormal`. The car keeps it at `+0x36d`.
- **Members.** `m_TankGunHit` `+0x20`, `m_PaintBallHit` `+0x28`, `m_Particle` `+0x30`, `m_BoomerangHit` `+0x38`, `m_smokeparticle` `+0x40`, `m_bluepool` `+0x48`: the visuals of each hit.
- **Hits.** A weapon writes the hit type into the action's `+0x2c` only while it is 0, then activates the action (`0x404edf` → `0x41ba60`). A car with a hit running therefore cannot be hit again. The writers:

  | Type | Writer |
  |---|---|
  | 4 | `WPaintShot` (`0x499537`) |
  | `0x40` | `WPU_Tnt` (`0x4dd49b`) |
  | `0x80` | `WPU_Landmine` (`0x4df966`) |
  | `0x200` | course hazards: `WTram` `0x4cb1f7`, `WShiftedRoad` `0x4cbd0e`, `WJunkyard` `0x4ce676`, `WBumSteer` `0x4cf003`, and `0x4c442e`, `0x4c513c`, `0x4cebef`, `0x4d1c93` |
  | `0x400` | `WPU_Storm` (`0x4e1c82`, written unconditionally) |
  | `0x1000` and others | `WPU_Detour` (`0x4e37e3`) |
  | the object's `+0x12a` | `DragonBreath` (`0x476336`) |
  | the object's `+0x64` | `WPU_Flipper` (`0x4d9f84`) |

- **Readers.** `WackyAutoNormal` (`0x46a68e`, `0x46b3f4`: type `0x200` skips a block) and `WCtrlRoboCar` (`0x4e53df`, `0x4e690b`: no driving decision while a hit runs).
- **What every type does to the car.** While the counter `+0x82` is below `0x19` (25 updates), the speed `+0x3ef` is set to 0 and the motion counter `+0x3eb` to −5. On the 25th, `+0x8db` = 20 (the acceleration doubling, see `car-effects-20261028.md`). After that, `0x47f710` gives the car icon 10 (`WPU_Shield`) in battle mode only (`app+0x1a60` = 1). The counter increments at the end of each update (`0x47f1e8`).
- **The motion counter `+0x3eb`.** At `0x46a70d` the car update adds 1 while it is ≤ 0, and the motion block `0x46ab98..0x46ac7c` (movement probes and forward step) runs only while it is > 0 (`0x46ab8a`). It is 1 at construction (`0x466153`), and `0x4de107` sets it to −59. After a hold the car therefore stays in place 5 more updates while its speed builds up with the doubled acceleration. The yaw, the pushes and the ground contact still run.
- **The visuals by type** (not ported):
  - `0x40`/`0x80`: smoke particles every 5 updates, and the vertex colours fade toward (10, 10, 0) (`0x47d890`: 1/13 of the gap per update, done when every step is below 2/255 (`0x51300c`), about 30 updates from white);
  - 4: a paint sprite that grows and shrinks;
  - `0x200`: a vertical squash;
  - `0x400`: tinted vertices;
  - `0x10`/`0x100`: a spinning model;
  - 2: a long sequence that hides the car and its wheels.

  When its sequence is over, the action sets `+0x36`, and its next update deactivates it (`0x41ba90`); `0x47f690` clears `+0x2c`.

## Weapon icons → templates (`0x483a20`; recovered, see "The icon chain")

- **The chain.** `0x483a20` fills the 27 entries of `0x6f2c50` by following a chain from `WFirstPowerUp`. Each object's `+0x10` holds the next object's id, which `0x433070` resolves through `0x403558`. `0x4d2970` clones `0x6f2c50[icon]` with `CreateHierarchy`.
- **The order.** In `general.wgm`, the 27 world children after `WFirstPowerUp` are:

  | Icons | Templates |
  |---|---|
  | 0..6 | `FlyPower` ×7 (each vehicle's own gadget) |
  | 7..10 | `MonsterTires`, `TurboSpeed`, `WPU_FireSiren`, `WPU_Shield` |
  | 11..16 | `WPU_BootKicker`, `WPU_Flipper`, `WPU_Paintball`, `WPU_Breath`, `WPU_Boomerang`, `WPU_TankGun` (the vehicles' own weapons) |
  | 17..25 | `WPU_OilSpill`, `WPU_BeeHive`, `WPU_Landmine`, `WPU_FallingTree`, `WPU_Storm`, `WPU_BrickWall`, `WPU_Detour`, `WPU_Tnt`, `WPU_Missile` |
  | 26 | a `GDModel` (`0x389`) |

- **Why it is taken as the chain.** The fields at `+0x10` are not decoded from the file, so the chain is inferred from the world order. It fits the code:
  - the random gadgets are 7..10;
  - the battle-mode protection after a hit is icon 10 = `WPU_Shield`;
  - the trailing racers' weapons 0x15/0x18/0x19 are `WPU_Storm`, `WPU_Tnt` and `WPU_Missile`;
  - the two weapons with a held preview, 0x14/0x16, are `WPU_FallingTree` and `WPU_BrickWall`.

## WPU_Landmine (icon 19)

- **Registration.** Method table `0x516828`, factory `0x4df170`. The constructor `0x4df1e0` sets the instance vtable `0x516884`.
- **Members** (constructor defaults, the same as in `general.wgm`):

  | Member | Template offset | Value |
  |---|---|---|
  | `m_nLifeTime` | `+0x24` | 300 |
  | `m_nScaleNum` | `+0x2c` | 5 |
  | `m_scale` | `+0x34` | |
  | `m_nUnlockTime` | `+0x3c` | 15 |
  | `m_rGravity` | `+0x44` | 50 |
  | `m_idExplosion` | `+0x4c` | `WExplosion` `0x3ac` |
  | `m_rTrapRange` | `+0x54` | 200 |

  The model is `GDModel` `0x372`, with two sounds.
- **Init (`0x4df5b0`).** The mine takes the matrix of the car that used it (`+0x28`), moves (0, 10, 0) in it, then sets state `+0x34` = 0, life `+0x38` = `m_nLifeTime`, and `+0x48` to its sound.
- **Update (`0x4df670`).** The life counts down; at 0 the state becomes `0xffff`.
  - **State 0:** a ray down `m_rGravity` from the mine (`0x402199`, mask `0x8000fe`). A hit aligns the mine to the ground and registers it (`0x4050f1` into `+0x64`; a failure explodes it). The state becomes 2 if the life is below `m_nLifeTime` − `m_nUnlockTime`, else 1. Without a hit, the mine drops `m_rGravity`.
  - **State 1:** waits for that unlock.
  - **State 2:** the object query `0x403d69` at the mine with range `m_rTrapRange`. A `WackyAuto` other than the dropper gets hit type `0x80` if none is running; the mine goes to `0xffff` and notifies the car (vtable `+0x34`).
  - **State `0xffff`** (the next update): clones `m_idExplosion` at the mine, unregisters it, stops its sound, and destroys the object and the action.

## Native

- **`update_bomb`.** It holds the car (speed 0, motion −5) for counters 0..24 and sets `+0x8db` = 20 at 25. The end follows each type (see "Hit ends, the tint and the squash").
- **`RaceSession::bomb_step`.** It runs after each physical car's move. `move` applies the motion counter. A hit robot keeps its last control (`0x4e53df`).
- **Far robots.** A far robot slides at top speed regardless of `+0x3ef`, so a hold does not stop it (provisional).
- **Using the weapon.** Weapon 19 drops a mine owned by the player. `update_mines` runs after the player's update. The trap is `0x4a71e0`'s answer (see "The traps' query" below): the nearest idle car within 200, unless that is the dropper.
- **Drawing.** `RaceView` draws mines with model `0x372`, on the ground normal.
- **Not drawn:** the explosion, the smoke and the tint.
- **Robots.** Robots do not use items natively, so only robots can be caught by mines (the player's own mines skip the player).

## Verification

- **`native_drive_test`, synthetic:** 25 held updates, the boost on the 26th, the hit cleared afterwards; a dropped mine starts 10 above the car.
- **`native_drive_test`, on `cc_t1`:**
  - A robot hit with `0x80`: its speed stays 0 for 24 updates, and it does not move for 29 updates after the hit's own update. It then drives off, and the hit has ended. A second hit while one runs is refused.
  - A mine dropped by robot 0 with the player placed on it: it lies on the ground, stays locked 15 updates, hits the player (type `0x80`) on update 17, and is removed the update after.
- **Four Championship I courses:** results unchanged.
- **Not verified:** the mine drawing, which was not seen in a frame. No Wine comparison of a hit or a mine.

## Gadgets: TurboSpeed and WPU_Shield (20261029)

- **`+0x385`** is the car's running gadget effect: −1 none, 0 `MonsterTires`, 8 `TurboSpeed`, 10 `WPU_Shield`. The gadget slot is used only while it is −1, and slot state 4 returns to 0 once it is back to −1.
- **TurboSpeed** (icon 8; method table `0x5162c8`, init `0x4d6cb0`, update `0x4d71e0`):
  - **Init:** saves rows 0..2 of the car's `0x6f2990` row, multiplies row[0] (the speed limit) by 2.2 (`0x5162fc`) and row[1] (the convergence) by 10 (`0x510254`), and sets `+0x385` = 8. The visuals depend on the vehicle.
  - **Update:** a counter from 0; when it reaches 61 (`0x3d`), the rows are restored and `+0x385` goes back to −1.
  - The yaw still clamps against the car's own `+0x21b`, which stays unscaled.
- **WPU_Shield** (icon 10; init `0x4d7740`, update `0x4d7870`):
  - **Init:** `+0x385` = 10; WackyAutoBomba's guard `+0x86` = 2; timer `+0x64` = `m_shieldtime` (template `+0x3c`, 200 in `general.wgm`).
  - **Update:**
    - When the timer reaches 0, or the guard is back to 1 (a hit was absorbed), the shield goes to state 1.
    - State 1 sets the guard to 4 and the timer to 6.
    - State 2 counts down until the timer is below 2; the model then hides.
    - State 3 sets the guard to 1 and `+0x385` to −1, and removes the shield.
    - A guard of 8 cuts the timer to 30.
  - **The absorption is in WackyAutoBomba's activation** (`0x47d450`), in a race:
    - with guard 8, the hit is cancelled;
    - with guard 2, it is cancelled and the guard becomes 1, or 8 for paint (type 4);
    - the activation also ends the car's own running item effect (`+0x943` → state `0xffff`; not ported).
  - In battle mode a shielded car is not protected this way.
- **Native.** `start_bomb`, `start_shield`, `update_shield` (`race_weapons`); `RaceSession::use_gadget`, `update_gadgets` (after the player's update, and also while the car drowns); `Body::effect`.
- **Tests:**
  - **Synthetic:** the shield absorbs a hit and ends 7 updates later; paint leaves guard 8, which still absorbs and cuts the time to 30.
  - **On `cc_t1`:** the turbo takes the Dingo past its normal limit (top 0.58 × 2.2), stays on 60 updates and is off after the 61st; the shield absorbs a hit and is gone within 8 updates.

## WBreath projectiles: missile, breath, tank gun, boomerang (20261029)

- **The classes.**
  - `WBreath` is an action on `DragonBreath` objects: registration table `0x512c38`, constructor `0x475cc0` (size `0x70`), action vtable `0x512c00`, init `0x474e30`, update `0x475040`.
  - `DragonBreath`'s own update `0x4762c0` waits for its `WBreath` (`+0x11e`) to end. If the action struck a `WackyAuto` (`+0x40`) whose WackyAutoBomba is idle, it writes its type `+0x12a` there and activates it. It then clones `m_idExplosion` and hides its models.
- **Members** (template `+0x24`..`+0x6c`, constructor defaults):

  | Member | Default |
  |---|---|
  | `range` | 6000 |
  | `angle` | 1.047 |
  | `ABSspd` | 200 (300 in the file) |
  | `maneuverspd` | 5 (8) |
  | `FWDscan` | 2000 |
  | `H` | 200 |
  | `aimontarget` | 1000 |
  | `travel_dist` | 20000 |
  | `climb` | 100 |
  | `loweringspeed` | 30 |

- **Init (`0x474e30`).** The velocity `+0x2c` is the object's forward axis × `ABSspd`.
- **Update (`0x475040`):**
  1. **Target.** `0x474c80` clears `+0x28`, then looks for a target every update with `0x4a71e0`: owner `+0x24`, the projectile's frame (which follows the velocity), its section `+0x104`, `angle`, and `range` (an integer in the file: `0x1770`). The result goes into `+0x28`. With no racer in view the projectile loses its target.
  2. **Steering.** With a target and `maneuverspd` ≠ 0, the steer is d̂ × `ABSspd` − v. It stays horizontal unless the target is within `aimontarget`, and is applied normalized × `maneuverspd`.
  3. **Altitude.** Away from the target, two rays (`0x402199`, mask `0x1e000ff`) look down for the ground: below the projectile, and `FWDscan` ahead along its horizontal heading. The vertical speed then aims at `H` above that ground over the time it takes to get there, at most `climb`, lowering by `loweringspeed` / `H`. My reading of the stack offsets here is provisional.
  4. **Speed cap.** The vertical speed is set aside while the horizontal one is capped at `ABSspd`, then restored (`0x475507`).
  5. **Ground.** The move ray (mask `0x18000ff`) stops the projectile on the ground.
  6. **Cars.** The move is split into `ABSspd` / the sphere radius sub-steps. Each tests the cars' spheres (`0x4040e8`) except the owner's; a hit car goes into `+0x40`.
  7. **End.** `+0x38` adds `ABSspd`. A ground hit, a car hit or reaching `travel_dist` ends the action (`0x405a6a`).
- **The weapons:**

  | Weapon (icon) | Projectile | Hit type | Notes |
  |---|---|---|---|
  | `WPU_Missile` (25) | `0x38c` | 8 | `+0x389` = `0x19`; fired at once, 100 ahead of the car (`0x43b140`); 40 updates |
  | `WPU_Breath` (14) | `0x39a` | 8 | `+0x389` = `0xe`; 33 updates, fired when 28 are left; `maneuverspd` 20 |
  | `WPU_TankGun` (16) | `0x3a8` | 2 | `+0x389` = `0x10`; fired on the first update, aimed at the target at once (`0x474c80`, `0x474d50`); ends 20 updates later |
  | `WPU_Boomerang` (15) | `0x3a1` | `0x10` | `+0x389` = `0xf`; aimed at once and ends at once; `maneuverspd` 50, `angle` 2.1, `range` and `travel_dist` 10000 |

  The `DragonBreath` spheres have radius 150 (151 for the breath).
- **The weapon slot.** It is used only while `+0x389` is −1, and state 4 waits for it to come back to −1.
- **Native:**
  - `race_weapons`: `BreathParams`, `Projectile`, `breath_weapon`, `aim_projectile`, `target_distance`, `steer_projectile`; `RaceSession::find_target` (`0x4a71e0`);
  - `RaceSession`: `use_weapon`, `fire`, `update_weapons` (after the player's update, and while the car drowns);
  - `RaceView` draws the `GDModel` of `0x38c`, `0x3a8` and `0x3a1` along the velocity. The breath's `GD3DFilm`, the rocket tail and the explosions are not drawn.
- **Tests:**
  - **Synthetic:** the icon table; the cone (range 6000, 60°); aiming; a turn of at most `maneuverspd`; holding `H`; lowering and climbing; the horizontal cap.
  - **On `cc_t1`:** from 1500 behind robot 0, both the missile and the tank gun hit a robot, the projectile goes, and `+0x389` ends.

## The target query `0x4a71e0` (20261029)

- **Signature.** A `__thiscall` on the race manager (`0x6f2d00`): (owner, frame, section, half-angle, range as an integer, angle out, 0, flags).
- **Heading.** The frame's forward axis gives atan2(−fx, fz), the same yaw convention as the port. The `+0xfa8` flag reverses it (not traced further).
- **The racer list** (`+0x48`, 12-byte entries, `+0x40` + 2 of them). A racer is a candidate when:
  - it exists and has a WackyAutoBomba (`+0x36d`);
  - it is one of the first two entries, or its robot state `+0x2b1` is non-zero;
  - it is not the owner, and its object is active (flag 2);
  - its hit type `+0x2c` is 0, so **a car with a hit running is never targeted**.
- **Its offset** from the frame. When the shooter's section is 0 and the racer's is the last one, the racer's position first goes through the loop transform (`0x43b880`), and the other way round in the opposite case.
- **Tests:**
  - every axis strictly within the range (a box);
  - the truncated integer squared distance at most the best so far and at most range²;
  - atan2(−dx, dz) minus the heading, wrapped to ±π (`0x496f90`), strictly within ±angle.
- **The best.** The best squared distance and racer are updated only when the cone test passes, so the result is the nearest racer in the cone. The angle is written to the out pointer (the tank gun's turret, `+0x3ab`; not ported).
- **Second list.** Unless flag 1 is set, the list at `+0x64c` (`+0x648` entries: other targetable objects) is searched the same way, without the hit condition. Not ported.
- **Callers:**

  | Caller | Angle | Range | Flags |
  |---|---|---|---|
  | `WBreath` `0x474c80` | template `angle` | template `range` | 0 |
  | `WPU_Tnt` init | 1.047 | 5000 | 0 |
  | `WPU_Storm` | π | 700 | 1 |
  | `WPU_Breath` and `WPU_Paintball` inits | | | |

- **Native.**
  - `target_distance` (`race_weapons`) is the per-racer test.
  - `RaceSession::find_target` walks the player and the robots, skipping the owner and cars with a hit running, and maps positions across the seam with `RaceTrack::wrap`/`unwrap`. A projectile's section is the route's under it, or else its owner's at launch.
  - **Provisional:** robot state `+0x2b1` is taken as non-zero; the second list is left out.
- **Tests.**
  - **Synthetic:** the integer squared distance; the cone behind; the strict box; the box passing but range² failing; truncation; wrapping across ±π; the strict half-angle.
  - **On `cc_t1`:** robot 0 straight ahead is found, but not while it has a hit running, nor when it is the owner.
- **`0x403d69`** is a thunk to `0x4a71e0`; see "The traps' query" (2026-10-29).

## WackyAutoBomba's activation kicks (20261029)

When a hit takes, `0x47d66f..` adds to the car's `+0x48b` slots (`0x4726c0`):

| Types | Effect |
|---|---|
| `& 0xda` (2, 8, `0x10`, `0x40`, `0x80`) | a hop: force (0, 50, 0), flags 1, decay 0.9 |
| `& 0x3fc` | a 4π spin (flags `0x10`, value at `+0x24`) |
| `0x1000` | a −π turn |
| `0x800` | a +π turn |

- **The slot rotation.** The fields `+0x3e8`/`+0x3ec`/`+0x3f0` of the slot block hold the remaining turn R, the turn done D and the step S.
  - **Request (`0x4726c0`):** R = V − D, set only when V widens the turn.
  - **Each update (`0x4729ad`):** S = R / 9 (`0x512a98`), or all of R once |R| < 3° (`0x512a24`); then R −= S and D += S.
  - **Reset (`0x472680`):** all three are cleared once R is 0.
  - **Yaw:** `0x4727b0` adds S to the next update's yaw.
- **Which types hold.** WackyAutoBomba's update knows types 2, 4, 8, `0x10`, `0x40`, `0x80`, `0x100`, `0x200` and `0x400`; any other type ends at once (`0x47e264` → `+0x36`). So `0x800`/`0x1000` (the detour) only turn the car round, with no hold.
- **Native.** `bomb_kick`, `Spin`, `request_spin`, `decay_spin`; `RaceSession::hit` adds the hop and the spin, and `move` applies the step.

## WPU_Tnt (icon 24)

- **Classes.** Vtable `0x51670c`, init `0x4dcac0`, update `0x4dce10`.
- **Template:**

  | Member | Template offset | Value |
  |---|---|---|
  | `m_idExplosion` | `+0x3c` | |
  | `m_idBounceFilm` | `+0x44` | |
  | `m_rForwardSpeed` | `+0x4c` | 300 |
  | `m_rRiseSpeed` | `+0x54` | 25 |
  | `m_rGravity` | `+0x5c` | 4 |
  | `m_rSpeedReducer` | `+0x64` | 0.8 |
  | `m_aRotX`, `m_aRotZ` | `+0x6c`, `+0x74` | |

  The sphere has radius 150.
- **Init.** The TNT takes the car's frame and is lifted by 100. `0x4a71e0` looks for a target from the TNT's frame (the thrower excluded), with 60° and 5000.
  - With one, the velocity is the unit direction × 300, plus 25 up.
  - Without, it is the car's (0, 25, 300).
- **Update:**
  1. **Flight.** v.y −= 4 each update. The move is split into speed / radius sub-steps, each testing the cars except the thrower's (`0x4040e8`). A car hit explodes the TNT on that car, which takes type `0x40` if idle.
  2. **Bounce.** The move ray (mask `0x1e000ff`) detects the ground and clones the bounce film. On the third bounce the TNT explodes, hitting no one. Otherwise:
     - it sits 10 along the normal, and its speed is × 0.8;
     - a car in view (`0x403d69`, 60°, 5000, the thrower included) gets the velocity aimed at it, plus 25 up;
     - otherwise the velocity is reflected on the normal, and X and Z are × 0.8.
- **Native.** `throw_tnt`, `bounce_tnt`, `RaceSession::update_tnts`. `RaceView` draws `GDModel` `0x382`.
- **Provisional:**
  - the move ray checks only the ground under the new position;
  - an `m_nLifeTime` of 300 is kept as a safety; the update does not read it.

## WPU_Storm (icon 21)

- **Classes.** Vtable `0x516998`, init `0x4e15f0`, update `0x4e1890`. Template: `m_nLifeTime` 150 (`+0x24`), `spd` 250 (`+0x3c`).
- **Init.** The cloud starts 400 above the car (`0x5169dc`), with velocity = the car's forward axis × `spd`. The vehicle mask `+0x90` starts with the thrower's bit.
- **State 0 (roaming):**
  - **Route.** `0x4e1410` queries the route layer under the cloud. On a new polygon it heads for that polygon's first forward link, or its first backward link when the key was held (`+0x5c`, not ported). The point is taken 400 up; that height is provisional.
  - **Life.** The counter `+0x38` counts only in this state. Past `m_nLifeTime` the cloud goes to state 2 for 40 updates.
  - **Strike.** `0x4a71e0` (angle π, range 700, flag 1: racers only) returns the nearest idle racer other than the thrower. If its vehicle bit is already set, nobody is struck this update. Otherwise that car is struck: its bit is set, and type `0x400` is written unconditionally and activated. The cloud then goes to state 1 for 30 updates, or 2 when the car's guard is not 1.
- **State 1.** The cloud moves over the car (400 up) at no more than `spd` per update.
- **State 2.** v.y += 4, and the sprite's `+0x152` = timer × 0.025 (`0x4e196a`, `0x5169e0`).
  - The cloud is a GD3dSprite (`0x379`). Its `+0x152` is property `0x0f`, 8 bytes before
    `m_RenderSetting` `+0x15a`. The sprite draw passes it to `0x43b690` (`0x419eef`), so it turns the
    quad about the view axis, in radians. So the cloud turns from about 1 radian back to 0 as it rises;
    it does not fade (2026-11-08).
  - The port turns the billboard's offsets. The world sprites' property `0x0f` is now applied the same
    way (none of the 20 courses has a nonzero one). Not compared with Wine.
- **Native.** `Storm`, `RaceSession::update_storms`. The cloud (`GD3dSprite` `0x379`) is not drawn. A hit already running keeps its own type; in WPC it would be overwritten (provisional).

## WPU_Detour (icon 23)

- **Classes.** Vtable `0x516a78`, init `0x4e3290`, update `0x4e33c0`.
- **Template:**

  | Member | Value |
  |---|---|
  | `m_nLifeTime` | 150 |
  | `m_nUnlockTime` | 15 |
  | `m_rGravity` | 100 |
  | `m_rFromGround` | 50 |
  | `m_aEffectAngle` | 0.524 |
  | `m_aMaxSteerMod` | 0.524 |
  | `m_nTrapTime` | 5000 |

- **Init.** The sign is placed 270 behind the car, along its horizontal forward axis (`0x512a8c`).
- **Update.** It uses the landmine's states.
  - **Landing.** `rand() % 100` < 50 picks `+0x68` = 1 (type `0x1000`); otherwise 2 (`0x800`, with the sign turned round).
  - **Trap.** Armed, the sign queries for a car with `0x403d69` (= `0x4a71e0`, owner excluded). If that car is idle and within 150 (`0x51493c`), it takes the type, and the sign goes.
  - **Effect.** The type only turns the car ±π, through the activation kick.
- **Native.** `drop_detour`, through `update_mines`. `RaceView` draws `GDModel` `0x37f`.
- **The three other members (2026-11-08).** They are registered for the class `0x2ab0ab8` at
  `0x50` (`m_aEffectAngle`, `0x4e3130`), `0x58` (`m_nTrapTime`, `0x4e31f0`) and `0x60`
  (`m_aMaxSteerMod`, `0x4e3250`). Each value sits 4 past its registered offset, as the update's
  reads show (`m_nUnlockTime` 0x38 → `+0x3c`, `m_rGravity` 0x40 → `+0x44`, `m_rFromGround` 0x48 →
  `+0x4c`, all through `[action+0x18]`). So the values are at `+0x54`, `+0x5c` and `+0x64`.
  - The init overwrites `m_nTrapTime` with 5 (`0x4e32a4`: `[action+0x18]+0x5c` = 5).
  - No reader was found: neither the init nor the update reads `+0x54`, `+0x5c` or `+0x64`. A scan
    of every load through `+0x18` followed by one of those offsets finds only `0x4d5510` and
    `0x4e4f10` (other classes, `+0x54`) and the write above.
  - So they look unused in the shipped game (a static scan only; a read through another pointer
    would escape it; no Wine watchpoint was run).

## The remaining effects (survey, 20261029)

Action vtables share `+4` = `0x41b3f0`, `+0xc` = `0x4115b0`, `+0x18` = `0x41b5e0`; `+8` is the update and `+0x10` the init. Line counts are of the disassembly.

| Icon | Class | Vtable | Update (lines) | Init (lines) | What it needs |
|---|---|---|---|---|---|
| 0..6 | `FlyPower` | `0x5161e4` | `0x4d5510` (539) | `0x4d4d10` (458) | flight physics per vehicle |
| 7 | `MonsterTires` | `0x516258` | `0x4d6560` (282) | `0x4d6380` (128) | the monster-tire probes (template `+0x200..`), `+0x15b` from `0x52b800`, wheel contacts `0x18` |
| 9 | `WPU_FireSiren` | (other layout) | | | |
| 11 | `WPU_BootKicker` | `0x516404` | `0x4d8aa0` (504) | `0x4d8830` (154) | |
| 12 | `WPU_Flipper` | `0x51647c` | `0x4d9c90` (335) | `0x4d9a20` (143) | hit type from `+0x64` |
| 13 | `WPU_Paintball` | `0x5165c0` | `0x4db320` (223) | `0x4db280` (43) | the `WPaintShot` projectile `0x396` (`0x4991c0`, 397; type 4) |
| 14 | `WPU_Breath` | `0x516554` | `0x4dac70` (162) | `0x4dab40` (89) | a `DragonBreath` + `WBreath` projectile |
| 15 | `WPU_Boomerang` | `0x5166a0` | `0x4dc180` (97) | `0x4dc110` (35) | |
| 16 | `WPU_TankGun` | `0x516634` | `0x4dba50` (204) | `0x4db980` (51) | clones `DragonBreath` `0x3a8` |
| 17 | `WPU_OilSpill` | `0x516778` | `0x4ddcf0` (355) | `0x4ddb90` (89) | |
| 18 | `WPU_BeeHive` | `0x5167e4` | `0x4de9b0` (448) | `0x4de850` (90) | plus `WBeeHive` (vtable `0x512bac`; `0x475040` was a misreading: it is WBreath's update) |
| 20 | `WPU_FallingTree` | `0x516928` | `0x4e09e0` (424) | `0x4e0710` (184) | held preview |
| 21 | `WPU_Storm` | (other layout) | `0x4e1890` | | type `0x400` |
| 22 | `WPU_BrickWall` | `0x516a0c` | `0x4e2650` (429) | `0x4e2380` (184) | held preview |
| 23 | `WPU_Detour` | `0x516a78` | `0x4e33c0` (340) | `0x4e3290` (85) | type `0x1000` |
| 24 | `WPU_Tnt` | `0x51670c` | `0x4dce10` (522) | `0x4dcac0` (183) | a bouncing throw (type `0x40`) |
| 25 | `WPU_Missile` | `0x5164e8` | `0x4da5a0` (169) | `0x4da500` (54) | clones `DragonBreath` `0x38c`, a homing `WBreath` |

- **Projectiles.** `DragonBreath` objects (`0x38c`, `0x39a`, `0x3a1`, `0x3a8`) carry the hit type in `+0x12a` (`0x476336`) and a `WBreath` action, often with `WRocketTail`. The `WBreath` members for the missile `0x38c`:

  | Member | Value |
  |---|---|
  | `range` | 6000 |
  | `angle` | 1.047 |
  | `ABSspd` | 300 |
  | `maneuverspd` | 8 |
  | `FWDscan` | 2000 |
  | `H` | 200 |
  | `aimontarget` | 1000 |
  | `travel_dist` | 20000 |
  | `climb` | 100 |
  | `loweringspeed` | 30 |

  Porting `WBreath` covers the missile, the tank gun and the breath.
- **Course hazards** (type `0x200`): `WTram`, `WShiftedRoad`, `WJunkyard`, `WBumSteer` and four unnamed writers. They are moving course objects, which the port does not run yet.

## Not ported

- `WPU_Tnt` (`0x4dce10`, 522 lines: a throw at `m_rForwardSpeed` 300, `m_rRiseSpeed` 25, `m_rGravity` 4, bounces `m_rSpeedReducer` 0.8).
- The other `WPU_*` and gadget effects.
- `WPaintShot`, `DragonBreath` and the course hazards.
- The visuals of every hit type.
- How the weapon slot (state 4) waits for its effect.
- Robots using items (`0x4e4a10`).

## The traps' query `0x403d69` (20261029)

`0x403d69` is `jmp 0x4a71e0`, the target query above (owner, frame, section, half-angle, integer range, angle out, 0, flags). Its callers, from the pushes:

| Caller | Owner | Frame, section | Angle | Range | Flags | Then |
|---|---|---|---|---|---|---|
| `WPU_Tnt` init `0x4dcb7f` | the thrower | the TNT's, the thrower's section | 1.047 | 5000 | 0 | aim (above) |
| `WPU_Tnt` bounce `0x4dd1e9` | the TNT object (so nobody is skipped) | a copy at the new position along the incoming velocity, the TNT's section | 1.047 | 5000 | 0 | aim with 25 up |
| `WPU_Landmine` `0x4df93a` | 0 | the mine's | 2π | `m_rTrapRange` (template `+0x54`), truncated | 1 | a result equal to the dropper `+0x28` is dropped; a `WackyAuto` (`+0x4` against `0x4054b1`) becomes `+0x24` |
| `WPU_Detour` `0x4e373a` | the owner `+0x28` | the sign's | 2π | action `+0x2c`, truncated | 1 | hit type 0, and the 3D distance strictly below 150 (`0x51493c`, `0x4e37d0`) |

Angle 2π means any direction. Flag 1 skips the second object list. As for every caller, a car with a hit running is not a candidate. So the mine waits while its dropper is the nearest car, even with another car within range.

**Native.** `update_mines` and `update_tnts` call `RaceSession::find_target`; `projectile_sees` and `car_in_view` are gone. A TNT keeps a section: the thrower's at the throw, then the route's under it at each bounce. Mines take the dropper's section.

**Provisional:**
- the detour's range `+0x2c`: its writer is not found, so 150 (`m_rTrapRange`) is used; the 150 distance test follows anyway;
- the mine and sign object sections, taken as the dropper's.

**Tests.**
- Synthetic: a 2π query sees behind and aside.
- On `cc_t1`: robot 0's mine with the player 100 away stays armed; the player's mine left on robot 0 hits robot 0 the update after it arms (update 17). The detour and TNT scenarios still pass.

Not compared with Wine.

## Hit ends, the tint and the squash (20261029)

From `0x47e220`'s Ghidra decompile and the disassembly; not compared with Wine.

- **Action flags.** `0x41ba60` (thunk `0x404edf`) activates an object: without flag `0x1000` it sets `0x1002` and calls vtable `+0x10` (WackyAutoBomba: `0x47d450`). `0x41ba90` (thunk `0x405a6a`) deactivates: with `0x1000` it calls vtable `+0x14` (`0x47f690`) and clears `0x1002`. Every bomba update ends by activating the car's `+0x369` (`0x47f1f1`) and incrementing `+0x82`.
- **The end.** A type's sequence sets `+0x36`; the next update deactivates the action (`0x47e23d`), so `0x47f690` clears `+0x2c`. A hit lasts D + 2 updates:

  | Type | D (`+0x82` when `+0x36` is set) |
  |---|---|
  | `0x200` | 25: stage 1 counts `+0x28` to 0x17, and the end waits for 25 |
  | `0x400` | 27: stage 1 waits for `+0x82` > 0x19, stage 2 restores the colours, stage 3 ends |
  | `0x10`, `0x100` | 0x2e |
  | 8, `0x40`, `0x80` | the first ≥ 25 where `0x47d890(10, 10, 0, 10)` returns 1: 73 from white |
  | 4 | stage 5 (0 → 1, 1 until `+0x82` > 16, 2 for 51, 3 for 8, 4) at 78, with `0x47d890(20, 20, 255, 10)` done |
  | 2 | stage 8 from 50 (stages: 5, 36, 1, 1, 6 updates), when `0x47d890(100, 100, 0, 10)` returns 1: 94 from white |
  | others | 0 (`+0x36` at once, no hold) |

  Without the paint object (`+0x52`) type 4 never reaches stage 5; without its model (`+0x5a`) type 2 ends at once. Neither path is ported.
- **`0x47d890(r, g, b, n)`.** Over the car model's vertices (`+0x3e`, count `+0x42`; renderer `0x6f5958`: 1 packed bytes, 2 floats; 0 skips), by mode `+0x4a`:
  0. each channel steps (target − c) × 1/13 (`0x513010`); once every step is below 2/255 (`0x51300c`; 2 in bytes) the mode advances;
  1. the target is set, `+0x46` = 0;
  2. `+0x46` += 1, and past n the mode advances (n + 1 updates);
  3. the colours step back toward the saved copy `+0x56` the same way;
  4. the saved colours are restored;
  5. it returns 1.

  As every vertex moves by the same fraction, the colours are lerp(start, target, f), and the steps depend only on each channel's largest |target − start|. From white to (10, 10, 0): 30, 1, 11, 30, 1 updates, then 1 at the 74th call. (This is the float path, renderer 2; see the next section for renderer 1, which Wine uses.)

- **Renderer 1 (2026-11-08).** Wine reads `0x6f5958` = 1, so `0x47d890` takes the packed-byte path
  (`0x47d906`, table `0x47dff8`).
  - A step is (target − c) / 13 truncated toward zero (the `0x4ec4ec4f` division). A mode ends once
    every channel's largest |step| is below 2.
  - The fades thus take 33 updates from white to (10, 10, 0) and back, and stop short at
    (33, 33, 23) and 232. The next mode's write then sets the target or the saved colours.
  - Logged under Wine (`cc_t1`, robot 0, far, hit with 0x80; bomba `+0x4a` against `+0x82`): mode 1
    after 33 updates, 2 after 34, 3 after 45, 4 after 78, 5 after 79, `+0x36` after 80.
  - The model's colours (`[+0x3e] +0x30`, 753 vertices) read r 0.231..1, g 0.047..1, b 0..1, the
    native vehicle 0's range.
  - **Native:** `step_tint` follows the byte path on each channel's two extreme vertices (the
    truncating step keeps the vertices' order). `tint_colour` replays the steps per vertex for the
    drawing: modes 1 and 4 show the last fade until their write.
  - Every transition matches the Wine log. The 0x80 hit now lasts 81 updates (`+0x36` at 79,
    counted from 0); before, with the float path, it was 6 updates short.
  - **Effects on other types** (white colours): type 4 ends after 78, type 2 after 97; `0x200`,
    `0x400` and `0x100` are unchanged.
- **Colour writes.** `0x400`'s stage 0 writes (10, 10, 0) and stage 2 copies `+0x56` back. Type 2's stage 4 writes (100, 100, 0).
- **Squash (`0x200`).** `+0x66` = 0.1, target `+0x6a` = 1, `+0x62` = 0.2, then `0x43b190(1, +0x66, 1)` every update of stage 1. Past `+0x28` = 20: v = (1 − s) / ((66 − `+0x28`) × 0.05 + 0.3) (`0x513024`, `0x513020`, `0x512a04`), w = (v + w) × 0.87 (`0x51301c`), s += w. Three spring updates, then stage 2 at `+0x28` = 24.

**Native.**
- `step_tint` ports the modes.
- `update_bomb` ports the ends per type: it sets `done`, and the next update clears the hit.
- `BombHit` carries the tint, `scale_y` and the model colour range.
- `RaceSession::set_model_colours` takes the range from `RaceView::model_colours`. All seven vehicle models are white natively, so their 0x80 hit lasts 75 updates.
- `RaceView::submit_car` draws the opaque batches with lerped colours, and the model with its up axis × `scale_y`.

**Provisional:**
- the squash is applied only during stage 1, on the guess that the car's frame is rebuilt every update;
- translucent batches keep their colours;
- the tinted vertices are the whole vehicle model group.

**Not drawn:** the smoke clones (8/`0x40`/`0x80`, every 5 updates), the paint splat (`+0x52`), the spinning model (`0x10`/`0x100`, `+0x4e`), the tank gun's model (`+0x5a`) and its hiding of the car and wheels (flag `0x100` on `+0x273..+0x27f`), and `WExplosion`.

## The icon chain (`0x483a20`, recovered 20261029)

`0x483a20` starts from `WFirstPowerUp`'s data record (`[obj+0x18]`). For each of the 27 slots, `0x433070` resolves the record's `+0x10` id through the application's id table (`0x403558` → `0x431660`, `[app + id×4 + 0x30]`), and the next slot starts from the record it returned.

`0x41b470` creates each world object's data record. It writes:
- `+0x8`: the object's id `+0x1c`;
- `+0xc`: its parent's id;
- `+0x10`: **its next sibling's id** (`[obj+0x14]+0x1c`, `0x41b4bd`).

So icon i is `WFirstPowerUp`'s (i+1)th following sibling in `GDWorld`: the order in the table above, now recovered rather than provisional.

## WPU_OilSpill (icon 17, 2026-11-01)

**Template.** `WPowerUpAction`'s m_nLifeTime 300 (`+0x24`), m_nScaleNum 10 (`+0x2c`) and m_scale 1. Its own m_nUnlockTime 15 (`+0x3c`), m_rGravity 50 (`+0x44`) and m_rTrapRange 200 (`+0x4c`). The model is GDModel `0x36a`.

**Init (`0x4ddb90`).** The spot takes the car's frame moved (0, 10, 0). State 0, life = m_nLifeTime.

**Update (`0x4ddcf0`).** The life counts down. At 0, the state becomes `0xffff` and `+0x3c` = m_nScaleNum.
- **States 0 to 2** are the mine's: the fall and lock, then armed below life − unlock.
- **Armed** (`0x4ddfd1`): the object query `0x403d69` (range m_rTrapRange, angle 2π, no owner) takes any car, its dropper too. The car (`0x4de015..`):
  - gets a `+0x48b` slot (`0x40400c`, type 3): the force is (`+0x40b` − `+0x423`) × `+0x3ef`, decaying by 0.96 (`+0x18`); the turn is 1.31 rad (`+0x10`), decaying by 0.897 (`+0x1c`);
  - takes speed 0 and motion counter `+0x3eb` = −59;
  - triggers voice event 5 (`+0x2c(vehicle, 5, 0xff)`);
  - plays its `+0x3cb` sound (`0x405164`), the fourth GDSound.

  The spot then takes state `0xffff` with `+0x3c` = m_nScaleNum.
- **`0xffff`** (`0x4de16b`): `+0x3c` counts down, then the spot goes.

**Native.**
- `drop_oil` and `RaceSession::slip`. `Body::Push::yaw_decay` carries the slot's own rotation decay; the other slots keep their single decay.
- The force uses the horizontal forward × the probe span × the speed. The original's probe vector also carries the pitch, so this is provisional.
- The robots drop it too (icon 17 is in their `rand() % 9 + 0x11`).
- `native_drive_test`: the spot springs under its dropper once armed, the car turns more than 0.5 rad in 5 updates, the sound plays once, and the spot is gone 12 updates later.
- Not compared with Wine.

## WPU_BootKicker and WPU_Flipper (icons 11, 12; 2026-11-01)

**Template.** After `WPowerUpAction`'s three members: range 2000 (`+0x3c`), spd 200 (`+0x44`) and angle 1.047 (`+0x4c`), for both classes.

**Init** (`0x4d8830` kicker, `0x4d9a20` flipper).
- The user's `+0x389` becomes `0xb`.
- The arm model is placed on the car (film sequence `0xb`).
- The target is `0x403d69(user, frame, section, range + 1000 (0x512aa8), angle, 0, 0, 2)`, kept in `+0x74`.
- The hit type `+0x64` is `0x100` (kicker) or `0x200` (flipper).

**Update.** When the arm reaches the target (`0x4d9f4a..`; the distance test against the arm's travel is not fully traced) and `+0x3c` is clear:
- a target with an idle WackyAutoBomba (`+0x2c` = 0) takes type `+0x64` (`0x404edf`);
- voice event 6 goes to the user's vehicle and 5 to the target's.

**Native (provisional timing).**
- `use_weapon` 11/12 picks the target at use (range 3000, 60°) and strikes after ⌈distance / 200⌉ updates (`RaceSession::Strike`, `update_strikes`).
- The user's weapon effect `0xb` runs for that time + 2.
- The arm models are not drawn.
- `native_drive_test`: from the back of the grid, the flipper strikes a robot ahead.
- Not compared with Wine.

## WPU_FireSiren (gadget 9, 2026-11-01)

**Classes.** Record `0x516894` (`0x4dfb90`), factory `0x4dfbe0`, instance vtable `0x5168bc`: init `0x4dff20`, update `0x4e0130`, sprite placement `0x4e00a0`.

**Template.** m_nLifeTime 150, m_nScaleNum 0, m_rTargetSpeed 0.2 (`+0x3c`) and m_rSpeedMod 0.9 (`+0x44`).

**Init.**
- The user's `+0x385` becomes 9, state 0, life = m_nLifeTime.
- The siren's GDSound plays.
- The light sprite is placed for the viewport (`+0x12a`/`+0x132`: (160, 74), (160, 32) or (273, 128)).

**Update.**
- The siren follows the car, and the life drops by one.
- Every 5 updates the light toggles.
- In state 0, past the life the state becomes `0xffff`. Otherwise `0x405051` (= `0x4a7e70`) runs with angle 2π, flags 7 and range = the action's float `+0x2c`. Each car in the returned list gets `+0x3ef` × m_rSpeedMod.
  - `0x4a7e70` skips the distance test for a range ≤ 0 (`0x4a7eca`) but keeps the section test (`+0x104`) and leaves out the user.
  - `+0x2c` is read as 0 here, from m_nScaleNum's slot (inferred).
- In `0xffff`, the sound stops, `+0x385` = −1 and `+0x943` = 0.

**Native.**
- `use_gadget(9)` sets `Body::siren` = 150. `update_gadgets` multiplies the speed of every other visible car in the user's section by 0.9 each update, then ends the effect.
- The flashing sprite is not drawn.
- `native_drive_test`: with the same seed, the robots in the player's section are more than 20% slower 10 updates after the siren, and the effect ends after its life.
- Not compared with Wine.

## WPU_FallingTree and WPU_BrickWall (icons 20, 22; 2026-11-01)

**Template.** m_nLifeTime 150 and m_nScaleNum 5 (tree) or 3 (wall); m_rStartHeight 1000 (`+0x3c`), m_rForwardDist 2000 (`+0x44`) and m_rGravity 100.

**Init** (`0x4e0710` tree, `0x4e2380` wall).
- The user's held preview `+0x395` is cleared.
- The target is `0x403d69(user, frame, section, range = the action's float +0x2c, angle +0x30, flags 1)`, or the user when none.
- The object takes the target's position + its horizontal forward × m_rForwardDist + up × m_rStartHeight (`0x4e250b..`).
- The vehicle mask `+0x70` starts with the user's bit.

**Update** (`0x4e09e0`, `0x4e2650`).
- The object falls m_rGravity to the ground.
- Then each update `0x405051(range 300, angle 2π, flags 7)` sets `+0x3ef` = 0 on each listed car whose bit is not in `+0x70`, and adds the bit.
- The camera shakes by `rand() % 5` (`+0x20`/`+0x24`) for `+0x68` updates.
- After the life, it fades m_nScaleNum updates.

**Native.**
- `Barrier`, `drop_barrier` and `RaceSession::update_barriers`; `RaceView` draws each template's model.
- The query range and angle at placement are the icons' WPowerUpSprite `m_range`/`m_visangle` (5000,
  60°) since 2026-11-08 (the actions' `+0x2c`/`+0x30`, written by the use `0x4d2970`).
- The camera shake is ported since 2026-11-09 (below). There is no fall animation beyond the drop of
  m_rGravity an update (already ported).
- `native_drive_test` (practice): the wall lands 2000 ahead of the user, and the user drives through without being stopped.
- Not compared with Wine.

## WPU_BeeHive (icon 18, 2026-11-01)

**Template.** After `WPowerUpAction`: m_nUnlockTime 15, m_rGravity 50, m_nTrapTime 80 (`+0x4c`), m_aMaxSteerMod 0.524 (`+0x54`), m_rTargetSpeed 0.3 (`+0x5c`) and m_rTrapRange 200 (`+0x64`).

**Update `0x4de9b0`.**
- The fall, lock and arming are the mine's.
- **Armed** (`0x4dec73`): `0x403d69(no owner, range m_rTrapRange, 2π, flags 1)` takes any car. Then:
  - the hive leaves the object list (`0x404fe8`) and is hidden;
  - its sound plays and the sibling (the bees, WBeeHive, see "The bees" below) is activated;
  - state 3 starts with `+0x3c` = m_nTrapTime;
  - voice event 5 goes to the car.
- **State 3** (`0x4ded9e`): the bees follow the car.
  - Every 15 updates, `+0x68` = (`rand(1000)` × 0.001 + 0.2) × m_aMaxSteerMod, with the opposite sign of the previous value.
  - Each update it is added to the car's steer, `+0x2ad` (robots, m_isrobotauto) or `+0x403` (player), clamped to ±0.698 (`0x5129d8`/`0x5129e0`).
  - The speed is capped at m_rTargetSpeed.
  - At the trap's end: `0xffff`, and removal.

**Native.**
- `drop_hive` and `Landmine::hive` with its state 3 in `update_mines`. The robot's kick goes into its control steer (`+0x2ad`).
- The bees' model is not drawn; the hive is.
- `native_drive_test` (practice): the hive springs under its dropper, the speed stays at most 0.3 while accelerating, and the bees leave after 80 updates.
- Not compared with Wine.

## WPU_Paintball (icon 13, 2026-11-01, provisional)

**Original.** Init `0x4db280`: the user's `+0x389` becomes `0xd` and the action plays a car film. Update `0x4db320`: it clones `m_idClonable` `0x396` (a GDModel carrying `WPaintShot`, class `0x4991c0`, 397 lines, m_rTargetSpeed 0.3), with m_nWaitShot 5. The paint hit is type 4.

**Native.** `breath_weapon(13)` fires one WBreath-like projectile, object `0x396`, type 4 (`+0x389` = `0xd` for 20 updates). `RaceView` draws `0x396`. `native_drive_test`: from the back of the grid, a robot takes a type-4 hit.

**Not traced:** WPaintShot's own motion and targeting, the number of shots, and the paint splat model (`+0x52`).

## FlyPower and MonsterTires (2026-11-01)

**FlyPower** (each vehicle's own gadget, icons 0..6).
- **Init `0x4d4d10`.** A `+0x48b` slot of (0, 62, 0) with decay 0.92 (`0x3f6b851f`). The template gravity `+0x98` is kept in `+0x6c` and × 0.17 (`0x516228`); the vehicle's top-speed row × 1.2 (`0x512a74`). Then the `+0x3df` sound, airtime `+0x8a7` = 4, `+0x385` = 1, and a per-vehicle branch (`0x4d5354`).
- **Update `0x4d5510`.** The counter `+0x64` increments; at 20 the gravity doubles. Each vehicle's branch (`0x4d5b8c`) animates its model. The end comes past 95 updates with airtime < 3: the gravity comes back from `+0x6c`, and the per-vehicle end (`0x4d5ba8`) runs.
- **Native.** The common part, in `use_gadget` (0..6) and `update_gadgets`; the top speed is restored with the gravity. `native_drive_test` (practice): the car rises more than 200, and the effect ends with the gravity restored. The per-vehicle branches and models are not ported. The robots' decision to use their own gadget is not ported either.

**MonsterTires** (7).
- **Init `0x4d6380`.** Four tyre models on the wheel frames, the car's wheels hidden (`+0x273..+0x27f`), `+0x385` = 7, and `+0x15b` = `0x52b800[vehicle]`.
- **Update `0x4d6560`.** The tyres grow (m_tomaxframenum 10, +0.2), settle (3, −0.1), stay (m_innormframenum 300) and shrink (5, −0.4). Then `+0x15b` comes back and `+0x385` = −1.
- **Native.** Only the 318-update effect, which also frees the robots' gadget slot. The contact spread `+0x15b` (not used by the native vertical motion) and the models are not ported.

## WExplosion drawn (2026-11-01, provisional)

`m_idExplosion` `0x3ac` is a GD3dSprite (material `0x10164`, m_AspectR 0.32) carrying `WExplosion` `0x3ad`:
- m_Mode 1, m_isRing 0, m_Resolution 10;
- m_GrowTime 2, m_FadeOutTime 20;
- m_Size 500, m_FinalSize 0;
- colour (255, 64, 0) fading to (0, 0, 0).

The mine's end (`0x4df9b1`) and the TNT's clone it.

**Native.** `RaceSession::blasts` records them at a landmine's or the TNT's end. `RaceView` draws a camera-facing quad of the sprite, growing to 500 over 2 updates and fading over 20, tinted (255, 64, 0). The class's own update (ring, resolution, colour fade) is not traced, and no capture checked the look. `native_drive_test` checks that a thrown TNT leaves an explosion.

## Using a weapon backward (2026-11-08)

### Recovered

- **The flag.** The use routine `0x4d2970` writes the action's `+0x5c` from `0x4d24f0(car)`
  (`0x4d29cc`).
  - `0x4d24f0` returns 0 when the car's brick `+0xc0` (the robot driver) is set.
  - Otherwise it returns whether the player's input word `0x6e42b8[player]` holds the mask
    `0x52aad4[player]` = `0x4`: look-back (control 5, '.', the follow camera's rear view).
  - So a human holding look-back when the item fires uses it backward; robots never do.
- **The readers.** The frame turns by π about y (`0x403279` with `0x40490fda`) in:
  - the missile (`0x4da605`), the tank gun (`0x4dbbaa`) and the boomerang (`0x4dc213`): the clone's
    frame, before WBreath's init moves it 100 along its axis;
  - the breath (`0x4dacea`): the head's frame;
  - the TNT (`0x4dcaff`): the thrown frame.
- **Other readers:**
  - the tank gun sets the car's `+0x3ab` to π or 0 (`0x4dbc5e`): presumably the turret's angle;
  - the kicker and the flipper keep `+0x5c` in `+0xfa8` (`0x4d893a`, `0x4d9b31`), pick their arm's
    offset by it (`0x2ab10b0`/`0x2ab11e8` + 12 × flag, `0x4d876b`, `0x4d98f3`), load π or 0
    (`0x4d8aaf`, `0x4d9ca7`) and flip the swing (`0x4d8b69`, `0x4d9d46`);
  - the storm's route step `0x4e1410`: on a new route polygon it takes the first backward link
    (index = the forward count, `+0x16` bits 4..7) when `+0x5c` is set. With no backward link
    (bits 0..3 zero) it keeps its waypoint; the polygon is still recorded (`+0x8c`, before the
    test).

- **No gadget reads it.** A scan of `0x5c(` accesses in `0x4d2000..0x4e5000` finds only the weapons
  above. MonsterTires' `0x4d6641` compares a counter with its template's `+0x5c` (a phase limit), and
  `0x4d7c21` is a constructor's default.

### Native

- `RaceSession::use_backward_` takes `DriveInput::look_back` around the human's `use_weapon`.
  `WeaponEffect::backward` keeps it for the breath's later shot.
- `fire()` turns the yaw by π for the projectiles (missile, tank gun, boomerang, paintball,
  breath). The tank gun's and the boomerang's target search uses the turned heading.
- The TNT is thrown along the turned heading, and its target is looked for that way.
- `Storm::backward` makes the route step take `node.backward.front()`.
- The kicker and the flipper look for their target behind. This is **provisional**: the update's
  query is not traced.
- The breath head is drawn turned by π (`RaceView::submit_breath_head`).
- Not ported: the tank gun's `+0x3ab`, and the kicker/flipper arm offsets (their arms are not
  drawn).

### Checks

- `native_drive_test`, every course: a backward missile starts 100 behind and leaves backward. A
  backward TNT is thrown backward.
- Forward and backward storms from the grid, over 60 updates, counted as route-polygon steps along
  the last polygon's forward or backward links:
  - forward 5..17 steps forward and none backward;
  - backward 1..17 steps backward and none forward;
  - all 20 courses pass.
- Control: with the backward link choice disabled, `cc_t1`'s backward storm makes 7 forward steps
  and the check fails.
- Not compared with Wine.

## FlyPower per vehicle (2026-11-08)

### Recovered

- **Jump tables by vehicle** (`[template] +0x88`): init `0x4d5354`, update `0x4d5b8c`, end
  `0x4d5ba8`.

  | Vehicle | Init | Update | End |
  |---|---|---|---|
  | 0 | `0x4d4f26` | `0x4d59ed` | `0x4d5a75` |
  | 1 | `0x4d520f` | `0x4d59ed` | `0x4d5b38` |
  | 2 | `0x4d5309` | `0x4d59ff` | `0x4d5b20` |
  | 3 | `0x4d4e19` | `0x4d596f` | `0x4d5b13` |
  | 4 | `0x4d5172` | `0x4d58a1` | `0x4d5b13` |
  | 5 | `0x4d504b` | `0x4d5555` | `0x4d5aae` |
  | 6 | `0x4d523f` | `0x4d56da` | `0x4d5b13` |

- **Gravity.** Vehicles 3, 4 and 6 halve the template gravity `+0x98` again (× 0.5 after the
  common × 0.17).
- **Models.** The template's members `0x290..0x293` (offsets `+0x3c..+0x54`) are general.wgm nodes
  450 (GDHModel), 451 (a brick with WPU_Propeller and GDHModel 453 with WRot), 457 (a brick with
  GDDoubleHModel 459) and 456. Vehicle 4 clones `+0x3c`, 6 `+0x44` and 3 `+0x4c`. Vehicles 0, 1 and
  2 animate the body (clip 0, `0x40275c` overrides).
- **Vehicle 3.**
  - Init: the clone's child's local matrix is the identity (`0x43b090`) × 2.5 (`0x402027`), turned
    30° about x (`0x43b2b0`), at (−30, 150, −10) (`0x40150a`). Clip 0 plays (`0x405e1b(0, 0, 1, −1)`).
  - Update (`0x4d596f`): the clone takes the car's local matrix `+0x2c` and its group.

### Wine

`cc_t1`, the player given a charge (`car+0x399` = 3), then '/'. Two symmetric green wings flap
mid-tower: GDDoubleHModel draws a mirrored second copy.

### Native

- `RaceView::submit_fly` draws GDDoubleHModel 0x1cb posed by its MTNL (clip 0), and its copy
  mirrored across the car's x axis (x at +30).
- A native capture shows the same pair of flapping wings.
- All 20 courses pass with the halved gravity.
- Not compared: the timing of the flaps; the flight's height with the new gravity.

### Vehicles 4 and 6 (later the same day)

- **Vehicle 4** (`0x4d5172`, update `0x4d58a1`):
  - it clones member `0x290`, GDHModel 0x1c2 itself, and plays its clip 0 (MTNL 0x10355);
  - each update, its local matrix is the car's `+0x2c` with the translation 160 (`0x512a90`)
    higher.
  - Native: `submit_fly` draws it. A capture shows a pterodactyl carrying the stone car, wings
    beating. Not compared with Wine.
- **Vehicle 6** (`0x4d523f`, update `0x4d56da`):
  - it clones brick 0x1c3 (WPU_Propeller);
  - the brick keeps its own rotation, at the car's position (`+0x288`) + 100 (`0x510414`) up;
  - its GDHModel child 0x1c5 (with WRot) plays clip 0 and a second controller
    (`0x4024eb(0, 2, −1)`); `0x40360c` overrides run before update 20.
  - Ported 2026-11-09, see "Vehicle 6's rotor" below.

### Vehicle 6's rotor (2026-11-09)

Recovered (static reading of `0x4d523f` and `0x4d56da`; thunks `0x40360c` → `0x437f70`,
`0x405e1b` → `0x440970`, `0x4024eb` → `0x4409f0`, `0x401b95` → `0x440290`, `0x40526d` → `0x440900`):

- **Template.** FlyPower's `0x291` (`+0x44`) is general.wgm brick 0x1c3: identity axes, action
  WPU_Propeller 0x1c4 (`m_flytimetohigh` 1200, `m_flytime` 3000, `m_upy` 20, `m_tospeed` 0.52,
  `m_turnykonst` 0.13, `m_turnzkonst` −0.3, `m_minspeed` 0.2, `m_ishelicopter` 1, `m_StepRotDegY`
  −0.6458), children GDHModel 0x1c5 (GHMS `0x102d6`, 4 segments, MTNL `0x10357`) with WRot 0x1c6
  (type 2, `m_StepRotDegY` −0.6458) and GDSound 0x1c7 (GSMP `0x1033b`).
- **MTNL `0x10357`.** Four clips, all on controller 0 (so controller index 1, `0x440900`): 0 (40,
  next 1), 1 (−10, next 3), 2 (50, next 1), 3 (20, next 3).
- **Init `0x4d523f`.** Gravity × 0.5; the clone `+0x68` of the brick; on its first child (the
  GDHModel): vtable `+0x4c`; `0x437f70(1, 0, 1, 0, 1)` locks segment 1's scale to (0, 1, 0);
  `0x440970(0, 0, 1, −1)` plays clip 0; `0x4409f0(0, 2, −1)` sends command 2 (freeze at the end) to
  clip 0's controller; the clone is added to the scene (`0x41b3c0`, list `0x6f2c40`).
- **Update `0x4d56da`** (after `+0x64` is incremented):
  - the clone's local matrix keeps its axes; its translation is the car's `+0x288` + 100
    (`0x510414`) in y (`0x40150a`); `0x41eeb0(0)`; the car's group `+0x104` is copied to the clone
    and its children;
  - `+0x64` < 20 and `[[GDHModel +0x166] + 4] +0x28` (controller 1's flags) has bit 4 (frozen):
    bit 0 cleared (the controller stops), segment 1 to (1, 1, 1), segments 2 and 3 to (0, 0, 0)
    (all with flag 1);
  - `+0x64` = 70: bit 0 set, segment 1 to (0, 1, 0), segments 2 and 3 to (1, 1, 1), clip 2 played
    and frozen at its end the same way.
- The end (`0x4d5b13`) is shared with vehicles 3 and 4.

Native (`RaceView::submit_fly`; `MotionController::frozen`/`set_active`):

- The same stages, keyed on the session's FlyPower counter (the native counter starts at 1 at the
  use, as for the gravity's doubling at 20); a stage change shows from the next pose.
- The WRot spin accumulates `m_StepRotDeg*` per race update, wrapped to [0, 2π), on the GDHModel's
  local frame (as the course WRot, `analysis/roto-magnet-volcano-20261106.md`).
- A native capture (`cc_t1`, `--vehicle 6`, `WPC_WEAPON=11` for a charge, '/' after the start):
  a folded cone over the car opens into two blades within about four frames, the flat rotor spins
  on its shaft above the car, then at the fold the blades drop into a spinning cone that stays until
  the gadget ends.
- **WPU_Propeller** has no behaviour: its class record (`0x4d7ac0`, string `0x52e208`) builds a
  0x74-byte object with vtable `0x516398`, whose init (slot 4, `0x4d80f0`) only clears `+0x48` and
  `+0x4c` and whose update (slot 2, `0x4d8110`) is a bare `ret`. Its members (`m_flytime`, `m_upy`,
  `m_ishelicopter`, ...) have no reader in its own methods; no other reader was searched for.
- The GDHModel's vtable `+0x4c` call at the init builds its controllers (the car init `0x469bf0`
  makes the same call before a body's controllers exist, `analysis/mtnl-motion-20261028.md`).
- The GDSound 0x1c7 is never played: GDSound (registration `0x44b7b0`, constructor `0x44c6d0`,
  vtable `0x511f38`) overrides only its destructor (`0x44c810`), so it has no init, update or draw;
  it sounds only through the play `0x44c380` (thunk `0x405164`), whose 31 callers include one in
  FlyPower, `0x4d4dd6`, the car's `+0x3df` sound. Nothing to port.
- Not compared with Wine.

### The per-vehicle rules and vehicle 5's parachute (2026-11-09)

Static reading of the update `0x4d5510` (table `0x4d5b8c`) and the inits (table `0x4d5354`):

- **Gravity at the init.** Vehicles 1 (`0x4d520f`) and 2 (`0x4d5309`) end at `0x4d5337`: `+0x98`
  × 0.3 (`0x512a04`). Vehicle 0 keeps × 0.17; 3, 4 and 6 take × 0.5 (`0x51026c`).
- **The end test** (after `+0x64` is incremented and the gravity doubled at 20):
  - vehicles 0 and 1 (`0x4d59ed`): airtime `+0x8a7` < 3 ends at once (no counter test);
  - vehicle 2 (`0x4d59ff`) after its segment-5 turn, 3, 4 and 6 (`0x4d5a2e`): airtime < 3 and
    `+0x64` > 95 (`0x5f`);
  - vehicle 5 (`0x4d5555`): airtime < 3 and `+0x64` > 17 (`0x4d56d1`, `0x11`).
- **The end** (`0x4d5a56`): `+0x98` = the saved `+0x6c`; the per-vehicle end (table `0x4d5ba8`); the
  top-speed row × 0.8333 (`0x51622c`, 1 / 1.2); `+0x385` = −1; the sound stopped (`+0x4c` vtable
  `+0x14`); the action ends (`0x405a6a`).
- **Vehicle 5** (`0x4d5555`):
  - at the init, body segment 6's x rotation is locked at 35° (`0x4383b0(6, 35, 0, 0, 1)`; the
    routine locks the rotation slots 3..5 with a = −1 and b = its arguments 2..4, flag 0 zeroes
    them, −1 clears `+0x17a` bit 0) and both WFires are shown (`analysis/wfire-20261109.md`);
  - at `+0x64` = 10 (`0x4d5602`): the template's `+0x54` (member `0x293`, GDHModel 0x1c8, GHMS
    `0x102d3`, MTNL `0x10354`) is cloned, its vtable `+0x4c` called, clip 0 played and frozen at its
    end (`0x440970(0, 0, 1, −1)`, `0x4409f0(0, 2, −1)`), the clone added to the scene; then the
    gravity × 0.5 and the airtime = 4 (`0x4d56a2`). A failed clone ends the action;
  - each update the clone keeps its axes at the car's `+0x288` + 100 (`0x510414`), with the car's
    group;
  - the end (`0x4d5aae`): `0x4383b0(6, 0, 0, 0, 1)` then `(6, 0, 0, 0, −1)`, the clone's `+0xc` |= 1,
    the two WFires hidden.
- **Vehicle 2**'s update turns body segment 5 about z by 153 × `+0x64` degrees (`0x4383b0(5, 0, 0,
  153 n, 1)`); its end clears the lock (`0x4d5b20`). Not ported.

Native: `RaceSession`'s FlyPower step follows the gravity and end rules (the native counter is one
ahead of `+0x64`, as for the doubling at 20); `RaceView::submit_fly` draws the parachute from
counter 10. `native_drive_test` checks vehicles 0, 2 and 5 on every course (dt_t1: vehicle 0 ended
after 40 updates; gravity at update 10 × 0.17 × 0.3 for 2 and × 0.17 × 0.5 for 5; with the rule
disabled the vehicle-0 check fails, 95 updates). A native capture (`cc_t1`, `--vehicle 5`,
`WPC_GADGET=5`) shows the green parachute open over the tank for about 1.5 s, gone on landing. Not
compared with Wine.

## MonsterTires drawn (2026-11-09)

Static reading of the init `0x4d6380` and the update `0x4d6560`; native captures; no Wine comparison.

- **Template** (general.wgm): GDStdBrick `0x2c0` with the action (`m_tireradius` 50 at `+0x3c`,
  `m_tomaxframenum` 10, `m_tonormframenum` 3, `m_innormframenum` 300, `m_tominframenum` 5,
  `m_tomaxscalediff` 0.2, `m_tonormscalediff` −0.1, `m_tominscalediff` −0.4) and children: four tyre
  GDModels `0x2ca`, `0x2db`, `0x2fa`, `0x2fb`, and GDSound `0x2fc`.
- **Init:** the brick is added to the scene with the car's frame (`0x404def`); each tyre's local
  matrix is the car template's `m_monstertires11/12/21/22` (`+0x200`, `+0x238`, `+0x270`, `+0x2a8`);
  `+0x6c` = 1, `+0x68` = `+0x64` = 0; the car's `+0x385` = 0; the brick's fifth child (the GDSound)
  plays with its `+0xc` | 4; the wheels `+0x273..+0x27f` (the model root's first four children,
  `0x469ebc`) get flag `0x100`; `+0x15b` = `0x52b800[vehicle × 16]`; the four `+0x44b` records = 24.
- **Update:** the counter `+0x64` increments; by stage `+0x68` (table `0x4d6918`): 0 adds 0.2 until
  10, 1 adds −0.1 until 3, 2 waits until `m_innormframenum` − 30 = 270 (`0x4d661e`), 3 adds −0.4
  until 5; each limit resets the counter and moves on. Stage 4 (`0x4d6659`): the action ends, the
  brick is flagged, `+0x385` = −1, the wheels shown, `+0x15b` = `0x52b7f8[v]`, the records =
  ftol(`0x52b804[v]`). So the effect lasts 10 + 3 + 270 + 5 + 1 = 289 updates (natively 318
  before).
- **Each update,** each tyre's local matrix is the scale `+0x6c` (`0x402027`) turned about x by the
  car's wheel spin `+0x2a4` × 2.5 (`0x51629c`, `0x405e8e`), at its own translation; the brick takes
  the car's frame; the car's model height `+0x92f` = (s − 1) × `m_tireradius` × 0.5 + 20 (`0x51026c`,
  `0x51152c`), at least 20.
- **Per-vehicle table** (`0x52b7f8`, 16 bytes): `+0x15b` after, unknown, `+0x15b` during, the
  records after: 0 (1.2, 0.8, 1.6, 24), 1 (0.9, 0.3, 1.6, 8), 2 (0.34, 0.35, 0.9, 6), 3 (0.6, 0.6,
  1.3, 6), 4 (0.65, 0.1, 1.4, 7), 5 (1.0, 0.4, 1.9, 16), 6 (0.7, 0.5, 1.3, 8).

Native: `RaceSession` runs the stages (`monster_scale`); `RaceView` hides the wheels' batches, draws
the tyres at `vehicle_wheels` (the `m_monstertires` translations) scaled by s in the car's frame,
and raises the model. Not ported: the spin (the port has no wheel spin), the physics fields, the
GDSound. `native_drive_test` (dt_t1): "MonsterTires ran 289 updates, tyre scale up to 3"; with the
wait at 300 it fails (319). Captures (`cc_t1`, vehicles 0 and 3, `WPC_GADGET=7`): the wheels give
way to four large treaded tyres that grow, and the car rides higher.

## The tank's turret `+0x3ab` (2026-11-09)

Static reading; a `native_drive_test` check and a native capture; no Wine comparison.

- **The shot** (`0x4dbbb6..0x4dbc76`): after the clone's inner action picks and aims at its target
  (`0x402a4a`, `0x401893`), with a target (`[action +0x28]`), the car's `+0x3ab` = `0x496f90`(atan2(−x,
  z) of the action's `+0x2c..+0x34`, atan2(−x, z) of the clone frame's z axis `0x43bf30`), i.e. the
  difference reduced modulo 2π (`0x514780`) and brought into (−π, π]. Without a target: π
  (`0x510b54`) when the action's `+0x5c` (backward) is set, else 0.
- **Each update** (`0x469a1c..0x469a99`, vehicle 5 only, `[template +0x88]` = 5): with `+0x389` =
  −1 (no weapon running) the turret steps toward 0 by 3° (`0x512a24`, 0.05236), and is set to 0
  inside ±3°. Then body segment 2 is locked to the y rotation `+0x3ab` × 57.2958 (`0x512a20`) degrees
  (`0x4383b0(2, 0, a, 0, 1)`).

Native: `RaceSession::fire` sets `Body::turret` from the aimed velocity (`aim_projectile`) less the
shot's heading; the update steps it back while no weapon effect runs; `RaceView::body_gadgets`
locks segment 2. `native_drive_test` (dt_t1, practice, a backward shot with no target): "tank turret
3.142 after the backward shot, back to 0 after 82 updates" (21 of the weapon, then 61 steps); with
the write disabled the check fails. Capture (`cc_t1`, vehicle 5, `WPC_WEAPON=16`, look-back held):
the turret faces the camera after the shot and swings back forward.

## The BootKicker's arm (2026-11-09)

Static reading of the init `0x4d8830`, the update `0x4d8aa0` and the placement `0x4d8710`; a
`native_drive_test` check and a native capture; no Wine comparison.

- **Template:** brick `0x32b` (m_range 2000 `+0x3c`, m_spd 200 `+0x44`, m_angle 1.047 `+0x4c`), its
  GDHModel `0x32d` (a sphere primitive of radius 100) and GDSound `0x32e`.
- **Init:** the offsets `0x2ab10b0` = (0, 20, 190) and (0, 20, −160); the user's `+0x389` = `0xb`;
  the GDHModel's local matrix reset to the identity; the brick takes the car model's frame; the
  user's `+0xfa8` = the backward flag `+0x5c`; the target `+0x80` = `0x403d69`(user, frame, section,
  ftol(range + 1000), angle, 0, 0, 2); segment 1's scale locked at 1 (`0x437f70`); the extension
  `+0x68` = 0, the velocity `+0x74` = (0, 0, spd), the state `+0x84` = 0, the type `+0x64` = 0x100.
- **Each update,** first the aim: the target's position in the brick's frame (`0x43bb00`), a =
  atan2(−x, z), within ±m_angle of 0 (or, backward, of π); no target: 0 or π. It is kept in `+0x88`
  in state 0 only.
- **State 0 (out):** the velocity's z × 1.1 (`0x516450`); n = ftol(max(1, v.z / the sphere's radius
  `[prim +0x54]`)) substeps of v / n, each moving the sphere's centre (`[prim +0x44]`) to the
  extension and querying the cars (`0x4040e8`). A car other than the user (with `[car +0x369] +0xc`
  bit 2): a `+0x48b` slot of (d.x × 80, 40, d.z × 80) (d the unit brick → car, `0x512a3c`), decay 0.8,
  unless its bomba's `+0x86` is 2; the type `+0x64` if its bomba is idle; voice 6 (the user's
  vehicle, `0xff`); then the extension = (0, 0, |car − brick| − 100), v = (0, 0, −100), state 1,
  counter 10. Past m_range the same without a hit (z = m_range).
- **State 1 (bounce):** the sound at counter 10; the extension += v; v.z × −0.8 (`0x51644c`); after
  10, state 2 with v = (0, 10, spd × −0.7) (`0x516448`).
- **State 2 (home):** the extension += v; v.z × 1.1; below z = 0 the action ends (`+0x389` = −1);
  otherwise y is held at most z.
- **Placement** (`0x4d8710`, every update): the brick takes the car model's frame, at the offset by the
  backward flag (`0x43b880`); the GDHModel's axes are turned about y by `+0x88` (`0x43c380`); its
  segment 1's translation is locked at the extension (`0x4381a0(1, ext, 1)`, slots 0..2).

Native: `RaceSession::Kick`/`update_kicks` replace the flipper's timed strike for the kicker (the
query: the port's sphere test, 100 + 100; the bomba `+0x86` and `[car +0x369]` tests are left out);
`RaceView::submit_kicks` draws the boot with the translation lock (`AnimatedInstance::moves`).
`native_drive_test` (dt_t1, vehicle 0 at the back of the grid): "boot kicker struck at update 0,
reach 90, home after 12 updates" (the robot ahead is within the first step). Capture (`cc_t1`,
vehicle 0, `WPC_WEAPON=11`): the scissor arm stretches out ahead with the boot at its end, holds,
and folds back.

## The Flipper's arm (2026-11-09)

Static reading of the init `0x4d9a20`, the update `0x4d9c90` and the placement `0x4d9890`; a
`native_drive_test` check and a native capture; no Wine comparison.

- **Template:** brick `0x32f` (m_range 2000, m_spd 200, m_angle 1.047), its GDHModel `0x337` (GHMS
  `0x102f3`, MTNL `0x10359`: clip 0 of 190, clip 1 of 30 looping) and GDSound `0x35a`.
- **Init:** the offsets `0x2ab11e8` = (0, 20, 100) and (0, 20, −100); `+0x389` = `0xb`; the
  GDHModel's local matrix reset and its controllers built (vtable `+0x4c`); the brick on the car
  model's frame; the target `+0x74` as the kicker's; the reach vector `+0x68` = 0; the state `+0x78`
  = 1; the type `+0x64` = 0x200; `+0x7c` = `+0x80` = 0; clip 0 played (`0x440970(0, 0, 1, −1)`) and
  its controller's time `+0x2c` set to 11 (`0x4d9bff`).
- **Each update:** the aim a (raw and within ±m_angle of 0, or of π backward, as the kicker's); d =
  |the target in the brick's frame| − 350 (`0x5129b0`), or m_range − 350 without a target, 1 when
  negative; `+0x80` = reach × 3 (`0x511894`) / d.
- **State 1:** the controller's time = 51 (`0x4da02e`), frozen by command 3 (`0x440290`: flag 4
  toggled) if it is not; `+0x7c` = a; the reach += spd × 1.7 (`0x512dcc`); at d it holds d, command 3
  again (the clip runs on), state 2, the sound.
- **State 2:** `+0x7c` = a; the reach = d while m_range > d; the hit once (`+0x3c`) when the time ≥ 40,
  a target, the reach ≥ d and a equal to the raw aim (the target within the angle): the type if its
  bomba is idle, voice 6 to the user's vehicle and 5 to the target's; when the controller goes
  inactive (the clip's end), state 3.
- **State 3:** the reach −= spd; at 0 the action ends (`+0x389` = −1).
- **Placement** (`0x4d9890`): the brick at the offset in the car model's frame; the GDHModel's axes
  turned about y by `+0x7c`; segment 1's translation locked at the reach vector (0, 0, reach)
  (`0x4381a0`); segment 2's scale at min(1, `+0x80`), segment 4's at `+0x80` (`0x437f70`).
- `0x4d96d0` (thunk `0x404764`, after states 1 and 2) casts a ray (`0x402199`, mask `0x1e000ff`)
  from the brick to the arm's end; a hit sets state 3 and scales the reach vector by the hit fraction.
  Ported (2026-11-09) with `RaceTrack::contact` (the same mask, any slope); the fraction is read from
  the result's `+0x44` (provisional). dt_t1's check then reads "arm home after 15 updates" (16
  before); no case was built that sends the arm into a wall.

Native: `RaceSession::Flip`/`update_flips` replace the timed strike for the flipper (the clip's time
steps 10 a race update, provisional, and its end is the clip's 190); `RaceView::submit_flips` drives
the GDHModel's controller (`MotionController::set_time`, `toggle_freeze`) and the locks.
`native_drive_test` (dt_t1, vehicle 3 at the back of the grid): "flipper struck at update 1, arm
home after 16 updates"; with the hit disabled the check fails. Capture (`cc_t1`, vehicle 1,
`WPC_WEAPON=12`): a telescopic pole with a mallet head reaches far ahead, holds, and draws back.

## The Paintball and WPaintShot (2026-11-09)

Static reading; a `native_drive_test` check and a native capture; no Wine comparison.

- **WPU_Paintball** (template brick `0x35b`: m_idClonable `0x396` at `+0x3c`, m_nWaitShot 5 at
  `+0x44`; its first child is a GDSound, hidden at the init):
  - init `0x4db280`: `+0x389` = `0xd`; the brick added to the scene; the user's body plays clip 2
    (`0x440970(2, 0, 1, −1)`); `+0x3c` = 0, state `+0x34` = 1;
  - update `0x4db320`, each update the brick takes the car model's frame; state 1 counts `+0x3c`
    down (from 0: at once) to state 2 with m_nWaitShot × 6 = 30; state 2 counts down, and at 0
    sets 20 and state `0xffff`; on each multiple of m_nWaitShot (25, 20, 15, 10, 5) it clones
    `0x396` at the first child's frame, its z axis moved by rand(100) × 0.0004 − 0.02 (`0x516608`,
    `0x516604`, `0x430f60`) in x and in y, then 100 along it (`0x43b140`); the shot's action gets
    `+0x24` = the user and its init; state `0xffff` ends it (`+0x389` = −1, command 2 on the body's
    clip 2).
- **WPaintShot** (registration `0x498a90`, string `0x52ccf8`, factory `0x498ae0`, vtable
  `0x514890`; GDModel `0x396`, GMDL `0x102e5`, sphere 50): members m_idPaintFx 0 (`+0x24`),
  m_idExplosion `0x3af` (`+0x2c`), m_nFrameLifeTime 180 (`+0x34`), m_nUnlockCount 10 (`+0x3c`),
  m_rSpeed 350 (`+0x44`), m_nSpherePhaseNum 3 (`+0x4c`), m_nTrapTime 120 (`+0x54`),
  m_aMaxSteerMod 0.5236 (`+0x5c`), m_rTargetSpeed 0.3 (`+0x64`); the last two have no reader in the
  update.
  - init `0x4990f0`: a tracker slot (`0x402117`, none: the shot ends); life `+0x28` = 180;
    `+0x38` = 1 / 3; the velocity `+0x2c` = the frame's z axis × m_rSpeed; state `+0x40` = 0.
  - update `0x4991c0`, state 0: the life counts down (at 0, state 10); the next point is the
    position + the velocity; while the life is at least m_nUnlockCount the sphere takes 3 phases of
    velocity / 3 from the position, querying the cars (`0x4040e8`); a car but the owner, with
    `[car +0xc]` bit 2, gets type 4 if its bomba is idle, `+0x3a7` = `0xd` (and, for a human's
    car, the HUD paint), and the shot's flag 1 (it also sets state 1 with m_nTrapTime, overwritten
    by state 10 below). Without a car, a ray to the next point (mask `0x1e000ff`) stops it at the
    hit; without a wall, the solid objects (`0x402b26`, 0x10000) give flag 2. It moves to the next
    point; with a flag, state 10, hidden, and m_idExplosion cloned at its frame.
  - state 10: the tracker removed; the action ends once its sound is done; the car's `+0x3a7` = −1.
- **`0x3af`:** a GD3dSprite (GMAT `0x10159`) with WExplosion m_Size 200, (11, 7, 208).

Native: `RaceSession::PaintGun`/`PaintShot`/`update_paint` (the scatter from the session's random
stream, provisional), `Blast` with its size and colour (`paint` for `0x3af`'s sprite);
`RaceView` draws the shots and plays the body's clip 2. The objects' query is a touch of
`CourseObjects::solid_spheres` (the shot's sphere 50; the objects' handler `+0x34` taken as a hit,
provisional; 2026-11-09): `native_drive_test` sends a shot at each course's first solid sphere from
its radius + 140 ("cc_t4: r 580, burst at update 0", "dr_t2: r 250, burst at update 1", "sh_t4: r 60,
burst at update 2"); with the query disabled it fails. The brick-flag velocity change
(`0x4991ef..0x499243`) is the loop seam: with the frame's flag 0x100000 or 0x200000 (the seam
crossing marks of `0x4a23c2`/`0x4a25b5`), the velocity is turned (`0x43b980`, a 3 × 3 product) by
the matrix of the table `[0x5472b8] +0x18` (index `+0x1c` + 3 or + 5, wrapped at `+0x40`); ported as
`carry_across_seam` with the velocity, as the other projectiles. `native_drive_test` sends a shot
along the last section's exit on every course ("cc_t1: across the seam 1, 34 updates after"; on
dr_t3 it bursts on the grid past the seam, counted as across); without the call it fails on cc_t1.
Not ported: the HUD paint. `native_drive_test` (dt_t1, vehicle 3
at the back of the grid): "paintball fired 5+ shots, painted 1, blue blast 1, gun ended at update
31"; with the shots disabled it fails. Capture (`cc_t1`, vehicle 2, `WPC_WEAPON=13`): a line of
small blue paint balls leaves the biplane.

### The paint on the hit player's screen (2026-11-09)

- WPaintShot's hit on a human's car (`0x499585..0x499609`) walks the list `[[0x5472b8] +0xc] +0x94`
  for an object of class `0x402559` → `0x4aab50` = record `0x6f5900`, **WPaintFx** (string
  `0x52d250`, factory `0x4aa8a0`, vtable `0x514c14`: init `0x4aac30`, draw `0x4aae40`); found, it
  sets its `+0x24` (1 for the second player in two-player mode, else 0) and, when the car's bomba
  `+0x86` is 1, calls its init.
- No WPaintFx exists at run time: its factory's thunk `0x402252` has no caller, and no node or action
  of the class is in general.wgm, swgeneral.wgm or the courses (its name is only in their member
  tables). So the walk finds nothing, and the shipped game shows no paint on screen.
- The car's `+0x3a7` (set to `0xd` by the hit, to −1 at the shot's end and the car init `0x469c51`)
  has no reader.
- Nothing to port. Not checked under Wine.

## The FireSiren's light (2026-11-09)

Static reading of the init `0x4dff20`, the per-viewport placement `0x4e00a0` and the update
`0x4e0130`; a native capture; no Wine comparison.

- **Template:** brick `0x311` (m_nLifeTime 150, m_rTargetSpeed 0.2, m_rSpeedMod 0.9), children
  GDSound `0x322` (played at the init) and GDSprite `0x12e`: GMAT `0x10222`, m_CenterX/Y 16, white,
  render setting `0x180042`. A GDSprite is a screen sprite: `+0x12a`/`+0x132` are m_PosX/m_PosY.
- **Init:** the sprite's local frame at the origin; its position (273, 128) for a robot's car
  (`[template +0xc0]` = 1), else (160, 74), or (160, 32) in two-player mode; `+0x3c` = 1.
- **Each viewport** (`0x4e00a0`, by the viewport index `[[[0x5472b8] +0x1838] +0xbc] +0x13a`): (160,
  30) when the siren's car is that viewport's player's (`[0x6f2d00] +0x38`, `+0x3c`), else (276, 110).
- **Each update:** the brick takes the car's frame; the life `+0x38` counts down; each time it is a
  multiple of 5 the brick toggles: shown again (`+0xc` &= ~0x100, `+0x3c` = 1) when `+0x3c` > 1,
  else hidden (`+0x3c` = 2). So it starts shown and flips every 5 updates.

Native: `RaceView::siren_lights` draws the GDSprite in each view's HUD for every car whose siren
runs (`RaceSession::siren_life`), at 2 (x, y) − its centre in the 640 × 480 frame, by the toggle
count. Capture (`cc_t1`, `WPC_GADGET=9`): a red siren light at the top centre of the HUD, flashing.
Not compared with Wine; another car's light at (276, 110) was not captured.

## The bees (WBeeHive, 2026-11-09)

Static reading; a native capture; no Wine comparison.

- **The class:** registration `0x474130` (string `0x52b0d8`), record `0x6f1b38`, factory `0x474180`,
  vtable `0x512bac`: init (slot 4) `0x474580`, update (slot 2) `0x474600`, draw (slot 3)
  `0x474650`. It is an action on the hive brick `0x36c` (WPU_BeeHive's template): m_BeeNum 32
  (`+0x24`), m_Radius 150 (`+0x2c`), m_Speed 60 (`+0x34`).
- **Init:** `+0x24` = 260 (a draw budget); for each bee (`+0x5c` = m_BeeNum, 16 bytes from
  `+0x60`) two phases rand() & 0xfff and two steps m_Speed.
- **Update:** each phase += its step, & 0xfff.
- **Draw** (`0x474650`, a frame argument): while inactive (`+0x20` = 0) it only keeps the frame
  (`+0x28`); active, it takes the frame ticks `0x5299b4` (the timer step, clamped to 2..4,
  `0x4ad58b`) from `+0x24` and draws while that stays non-negative. Renderer 1 (`0x474850`): per
  bee, with the phases a, b used as radians (fild, then fsin/fcos), the centre p = R (sin a cos b,
  −sin b, cos a cos b), and two triangles: (p, p + (5, 5, 0), p + (5, −5, 0)) with the corners in
  (13, 0, 0, 173), and (p, p + (−5, 5, 0), p + (−5, −5, 0)) in (255, 255, 0, 173), the centre in
  (0, 0, 1, 255) (`0x512bf0` = 5); `0x404269(8, …, 2 × m_BeeNum)` in the kept frame. Renderer 0
  (`0x4746bf`) plots single pixels in a buffer instead.

Native: `RaceView::submit_bees` keeps the phases by trapped car, steps them by race update and
draws the triangles in the trapped car's frame (provisional: the hive clone follows the car, its own
frame is not traced) while the hive's state 3 runs (the 260-tick budget is not ported), with the
WFire draw's state (mode 8: culled, no depth write). Capture (`cc_t1`, vehicle 3, `WPC_WEAPON=18`,
dropped while standing): small yellow-and-dark triangles swarm about the car once the hive springs.

## The wall's and the tree's camera shake (2026-11-09)

- **Landing** (`0x4e286f` wall, `0x4e0bf0` tree): the counter `+0x68` = 30, and the landing sound.
- **Each later update** (`0x4e2b2b..0x4e2b7f`; the tree's `0x4e0eb8..0x4e0eff`): the graphics object
  (`[0x5472b8] +0x1838`) copies its `+0xb4` into `+0xb8`, and that object's `+0x20` and `+0x24` are
  set to `rand() % 5` each (`0x430f80`) while `+0x68` (counted down) was positive, else 0; past the
  life (`+0x38`), 0 as well.
- **Reading:** `+0xb4` is taken to be the active viewport and `+0x20`/`+0x24` its origin in x and y
  (provisional: their reader in the renderer is not traced).
- **Native:** `Barrier::shake`, `RaceSession::camera_shake` (the session's MSVC `rand()` stream, as
  the original's shared one), and `RaceView::draw_view` moving the projection by that many 640 × 480
  pixels in x and y. `native_drive_test` (dt_t1, practice): "brick wall landed at update 9, viewport
  shaken 29 updates, last at 39"; with the counter left at 0 it fails. No capture (a 0..4 pixel shake
  does not show on stills); not compared with Wine.

## DragonBreath's rotangle (2026-11-09)

- DragonBreath's update `0x4762c0` first adds its template's `rotangle` (member `0x138`, `+0x90`) to
  the angle `+0x14a` of its first visual child (`+0x126`; for a film, the turn of its corners about
  the view axis, see "the film" in `analysis/hit-smoke-20261107.md`).
- general.wgm: 0.0872665 (5°) for the breath `0x39a` (its GD3DFilm `0x39d`), 0 for the missile
  `0x38c`, the boomerang `0x3a1` and the tank shell `0x3a8`. So the boomerang's model (GDModel
  `0x3a4`, already drawn) does not spin.
- Native: `Projectile::rotangle`/`spin`, added each update, and the breath's film drawn at that
  angle. A synthetic check in `native_drive_test` reads the values; no capture of the turning film.

## WExplosion (2026-11-09)

Static reading; a `native_drive_test` check; no capture, no Wine comparison.

- **The class:** registration `0x47fa10` (string `0x52b470`), factory `0x47fa60`, vtable
  `0x513050`: init `0x4803c0`, update `0x480580`, draw `0x4806f0`. Members (template): m_Resolution
  `+0x24`, m_GrowTime `+0x2c`, m_FadeOutTime `+0x34`, m_Size `+0x3c`, m_FinalSize `+0x44`,
  m_Red/m_Green/m_Blue `+0x4c/+0x54/+0x5c`, m_isRing `+0x94`; `0x3ac`: 10, 2, 20, 500, 0,
  (255, 64, 0), 0; `0x3af`: the same with 200 and (11, 7, 208).
- **Init:** the record `+0x3c` = (m_Red, m_Green, m_Blue, 255), radius word `+0x40` = 0, segments
  `+0x42` = m_Resolution, then three ring records (`+0x43`, `+0x51`, `+0x5f`) used only with
  m_isRing; the step `+0x28` = m_Size / m_GrowTime; the state `+0x24` = 0; an object-list slot
  (`0x4050f1`) and its GDSound.
- **Update** (`0x480580`, table `0x480690`): state 0 adds the step to the radius until the counter
  passes m_GrowTime (three updates: 250, 500, 750 for `0x3ac`); state 1 sets the step to (m_Size −
  m_FinalSize) / m_FadeOutTime; state 2 takes it off the radius and sets the alpha `+0x3f` = ftol(255
  × (m_FadeOutTime − counter) / m_FadeOutTime) until the counter passes m_FadeOutTime (21 updates);
  state 3 waits for the sound, leaves the object list and ends.
- **Draw** (`0x4806f0`): while its tick budget `+0x70` (150, less `0x5299b4` a draw) lasts, the
  disc of the record `+0x3c` through `0x4b44e0` (renderer 1; the rocket glow's routine: a fan of the
  segments facing the camera, the centre in the record's colour, the rim transparent) at the object's
  position moved a fifth of the way to the camera (`0x510ef0` = 0.2); with m_isRing, the three rings
  (`0x40402a`, `0x405088`). Its GD3dSprite node carries flag 0x100, so the textured sprite is not
  drawn.
- **Native:** `RaceSession::Blast` runs the states (`update_blasts`), `RaceView::effects` draws the
  fan (no depth write, both faces); the earlier textured quad of the sprite (provisional since
  2026-11-01) is gone. Not ported: the rings, the tick budget. `native_drive_test` (dt_t1): "TNT
  explosion radius up to 750, 25 updates more, last alpha 0".

