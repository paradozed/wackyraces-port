# Native power-ups: pickups, charge, roulette and use

Recovered from the executable. The effects of using a gadget or weapon
(the `WPU_*` classes) are ported since, in `weapons-bomba-20261029.md` (this
note's first line said "not ported yet" until 2026-11-12).

## Track pickups (`WPowerUp`, update `0x4d3ec0`)

- **Placement:** each `WPowerUp` has a `GDModel` child. Its type is property
  `0x28b`: 0 gadget, 1 weapon, 2 refill. `cc_t1` has 6, 8 and 11 of them.
  Pickups float 100 units above the ground. The model's `m_VcRadius` (`0x6a`:
  110, 116 for refills) is not the collision radius.
- **State `+0x142`, counter `+0x146`, scale `+0x14a`:**

  | State | Behaviour |
  |---|---|
  | 0 | idle |
  | 1 | taken: hide the model (flag `0x100`), go to 2 |
  | 2 | wait 60 updates (1000 in battle mode), go to 3 |
  | 3 | when no car overlaps it (`0x4040e8`), show it at scale 0.2, go to 4 |
  | 4 | grow by 0.04 per update for 20 updates, then scale 1 and back to 0 |

- **Spin:** every model turns by one shared rotation (`0x2ab0f08`).
  `0x4d40c0` adds 0.2269 rad (`0x3e685696`) per manager update, wrapped to
  ±π.

## Taking one (car update `0x46aad5`)

The car's object query (`0x4a5730`, mask `0x79600`) reports a power-up with
flag `0x8000`. If its state is 0:

- it goes to state 1 and the car's charge `+0x399` rises by 1 (capped at 3
  in `0x4679d0`);
- a pickup sound plays: the actor's second GDSound (`+0x60` first child, `+0x14` next sibling; `0x46ab2c` → `0x44c380`), unless the brick's `+0xc0` is 1 (robot-driven; `0x49eb20` sets 1 for robots, 0 for the player, 2 past the finish). Ported 2026-11-02 for both players;
- the type acts on the slots: 0 starts the gadget roulette (`+0x3a0` = 1),
  1 the weapon roulette (`+0x39f` = 1), and 2 empties both slots, which then
  refill with the vehicle's own items.

### The collision (recovered)

- **Volumes:** objects carry their collision volumes in a `PRIM` chunk of
  their BRTR node, a serialized `GDPrimSphere` (class `0x339485b5`) or
  `WPrimCylinder` (`0x3bbfe94c`). Properties: `0x8c` centre (local), `0x8d`
  radius, `0x85` name, `0x86` flags, `0x87` `m_SensorGroup` (runtime `+0x24`),
  `0x88` `m_iObj`.
- **Every `WPowerUp`:** one sphere of radius 100 at its origin, sensor group
  `0x8000`. Every `WackyAuto` template: radius 100, sensor group `0x100`.
  Neither is scaled.
- **The query (`0x4a5730`):**
  - The car's sphere is placed in world space: centre through the car's
    transform, radius × the transform's X-axis length.
  - It scans the route-grid cells under that circle, and every object's
    primitives whose sensor group matches the mask (`0x79600`).
  - Spheres use `0x482190`, cylinders `0x482310`. A hit returns the
    primitive's sensor group, and the pickup code tests it for `0x8000`.
- **`0x482190`:** the centres must lie within r₁ + r₂ on each axis; then the
  integer-truncated squared distance must be at most `int(r₁+r₂)²`.

So a power-up is taken when the car's origin, at road level, comes within 200
units of the power-up's origin, 100 units above the road. That's about 173
units of horizontal reach over the road. The respawn check (`0x4040e8`) uses
the same overlap.

`node_prims` (`game_scene`) decodes `PRIM` chunks. `RaceTrack` keeps each
power-up's sphere and `RaceSession::spheres_touch` ports `0x482190`, box test
included. Without the box test, far-off pickups overflowed the integer
squares and all touched at once.

## Slots (`0x4679d0`, human player, race modes)

| | Gadget (`+0x3a0` state, `+0x38d` icon) | Weapon (`+0x39f` state, `+0x391` icon) |
|---|---|---|
| 0 | empty → icon `m_iVehicle`; state 3 | empty → icon `m_iVehicle+11` (vehicle 6: `rand()%9+17`); state 3 |
| 1 | icon `(rand()&3)+7`. At rank ≥ 4, with chance `(rank−4)×0.15+0.3`, icon 8 if `frand ≥ 0.7`, else 7 | icon `rand()%9+17`. At rank ≥ 4, with chance `(rank−4)×0.1+0.5`: `frand` < 0.25 → 0x18, < 0.5 → 0x19, else 0x15 |
| 2 | roulette: every 5 updates the icon steps (11 → 7), for 10 steps; the key stops it | the same (0x1a → 0x11) |
| 3 | key + charge > 0 + not drowning + no active effect (`+0x385`): charge −1, spawn the effect (`0x4d2970`), state 4 | icons 0x14/0x16 first spawn a held preview (`+0x395`); then the same with `+0x389` |
| 4 | when the effect has ended, state 0 | the same |

- **Random numbers:** MSVC `rand()` seeded `0x1312d6e` by the scene loader;
  `frand(x)` = `rand()·2⁻¹⁵·x` (`0x430fa0`, `0x430ec0`).
- **Item keys:** the gadget and weapon bits (`0x52aac8` = `0x0001`,
  `0x52aacc` = `0x0002`) are tested in `0x6e42b0`, which is the input word
  ANDed with the previous update's (`0x45c205`). Items therefore act on key
  presses. Natively the gadget is Ctrl and the weapon Space; the original
  default bindings are not traced.

## Effects (`0x4d2970`)

`0x4d2970` clones the template at `0x6f2c50[icon]` with `CreateHierarchy`
(`0x41fd00`). Its first action receives the car (`+0x28`), the "key held"
state (`0x4d24f0`, bit `0x0004` of `0x6e42b8`) and the sprite's `0xf8`/`0x100`
values. Race setup (`0x483a20`) fills all 27 entries from the object whose id
is at the `WFirstPowerUp` record's `+0x10`. The per-icon behaviour is
therefore in that template's actions (`WPU_Missile`, `WPU_OilSpill`,
`FlyPower`, …). Natively an effect ends at once, except `WPU_Landmine`
(icon 19). The icon → template order and the mine are in
`weapons-bomba-20261029.md`.

## Native

- `RaceTrack::power_ups` holds the static data.
- `RaceSession` runs the pickup states, the shared spin, pickups and the
  slots. It exposes `charge_level`, `gadget_icon`, `weapon_icon` and
  `take_item_events` (logged as "Power-up at update …").
- `RaceView` takes the pickup models out of the static world and draws them
  hidden, growing or spinning. The HUD icons and gauge follow the slots and
  charge.

## Verification

- `native_drive_test` on `cc_t1`:
  - 25 pickups; the Dingo starts with icons 3 and 14;
  - driving through `WPowerUp 56` adds a charge;
  - the roulette lands on 7..10, using the gadget spends the charge, and the
    slot returns to icon 3.
- In the game, placed before pickup 56:
  - the spinning models show;
  - the pickup makes the icons opaque and moves the gauge needle;
  - the gadget roulette runs, and Ctrl uses the gadget, after which the
    icons fade again.
- Not compared with Wine: roulette outcomes and sounds. The pickup range is
  now the recovered sphere test.

## Robots and a taken pickup (2026-11-12, static reading)

In the car update, the object query's power-up branch (`0x46aab3..`) reads the pickup's state `+0x142`
first and skips the take unless it is 0 (`0x46aad5..0x46aae0`). It is the same path for the player and
the robots: the robot flag (the brick's `+0xc0` = 1) only skips the pickup sound (`0x46ab05..0x46ab0d`).
So a hidden, waiting or growing pickup is untouchable for every car, as natively. Before that there is
another test: the hit object's `+0x132` must not exceed its `+0x13a` (`0x46aab8..0x46aac6`).

**That test, identified (2026-11-12).**
- WPowerUp is a WHotSpot (class record `0x2ab1118`, name `0x52dfc8`). Its registered members (record
  offset, value at +4): `m_idOnHit` `0x11e`, `m_iMaxCount` `0x126`, `m_iCounter` `0x12e`,
  `m_iUnlockCount` `0x136`. So the test is `m_iCounter` ≤ `m_iUnlockCount`.
- The hotspot's hit method (`0x4d3720`, reached by vtable only): with the counter at 0, it sends the
  `m_idOnHit` event (`0x401640`), reloads the counter from `m_iMaxCount`, and activates `+0x94`.
  Another vtable method (`0x4d3b10`) counts the counter down, and at 0 sets flag 1 on the object.
- Every pickup on the 25 course and arena files has `m_iCounter` 0, `m_iMaxCount` 0,
  `m_iUnlockCount` 1 and no `m_idOnHit`. So the counter never rises above 1 and the test always
  passes. The port's take without it is equivalent for the shipped game.
Not seen under Wine.

