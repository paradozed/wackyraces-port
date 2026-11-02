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
- **State 2.** v.y += 4, and the cloud fades by 0.025 per update (`0x5169e0`).
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
- **Not ported:** `m_aEffectAngle`, `m_aMaxSteerMod` and `m_nTrapTime`; their reader is not found yet.

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
| 18 | `WPU_BeeHive` | `0x5167e4` | `0x4de9b0` (448) | `0x4de850` (90) | plus `WBeeHive` (`0x475040`, 680) |
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

  As every vertex moves by the same fraction, the colours are lerp(start, target, f), and the steps depend only on each channel's largest |target − start|. From white to (10, 10, 0): 30, 1, 11, 30, 1 updates, then 1 at the 74th call.
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
- The query range and angle at placement are provisional (10000, 60°).
- The fall animation and the camera shake are not ported.
- `native_drive_test` (practice): the wall lands 2000 ahead of the user, and the user drives through without being stopped.
- Not compared with Wine.

## WPU_BeeHive (icon 18, 2026-11-01)

**Template.** After `WPowerUpAction`: m_nUnlockTime 15, m_rGravity 50, m_nTrapTime 80 (`+0x4c`), m_aMaxSteerMod 0.524 (`+0x54`), m_rTargetSpeed 0.3 (`+0x5c`) and m_rTrapRange 200 (`+0x64`).

**Update `0x4de9b0`.**
- The fall, lock and arming are the mine's.
- **Armed** (`0x4dec73`): `0x403d69(no owner, range m_rTrapRange, 2π, flags 1)` takes any car. Then:
  - the hive leaves the object list (`0x404fe8`) and is hidden;
  - its sound plays and the sibling (the bees, WBeeHive `0x475040`) is activated;
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
