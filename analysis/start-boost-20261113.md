# The start: the rev during PRET and the boosts at GO (2026-11-13)

## Found under Wine (`sh_t1`)

- **The run:** memory polled every 10 ms from the grid, Up held. Grip `+0x8bf` 1.0, `+0x8c3` 1.0 and surface
  `+0x8bb` 2 under every car; the speed rows `0x6f2990` are the port's (row[1] 0.014 / 0.017 / 0.019).
- **The robots:** the robot in grid slot 0 (vehicle 2 here) gains 0.101, 0.189, 0.266, 0.333 … a frame, which is
  (0.8 − speed) × 0.014 × 9.0. Slot 2's robot (vehicle 4) gains 0.041, 0.080, 0.116 …, the same with 0.017 × 3.0.
- **The player:** its `+0x3ef` reads 0.026, then 0.052 during the countdown while the car stays put, then 0 at
  GO. From there it gains about 0.017 a frame: 0.8 × 0.017 × (0.052 × 4 + 1) = 0.0164.

## The original

- **The countdown, state 1 (`0x4a2b57..0x4a2c1c`):** unless the false-start flag `0x6f5834` is set, player 1's
  accelerate key (held word `0x6e42b8`, binding `0x52aab8`):
  - **held:** raises `+0x3ef` by 0.026 (`0x514bbc`) while it is below row[0], with `+0x257` = 0. At row[0] or above
    it is a false start: `+0x3ef` = 0 and `0x6f5834` = 1, and nothing more happens until GO;
  - **released:** the rev falls by 0.03 (`0x512a34`) while it is at least 0.03.
  - The countdown ends when `+0xf34` passes its limit (state `+0xf30` = 2).
- **At GO (`0x4a2c22..0x4a2d69`):**
  - in one player (app `+0x1a64` = 0), each robot gets controller flag 1. The first three cars of the robot list
    `+0x60` (grid slots 0..2) get their vehicle's row[1] (`0x6f2994`) × k × 3 (`0x511894`), with k = 3, 2, 1, so ×9,
    ×6 and ×3, and `+0x3b3` = 100;
  - player 1: row[1] × (`+0x3ef` × 4 (`0x510434`) + 1 (`0x510214`)), `+0x3b3` = 100, `+0x3ef` = 0.
- **The restore:** the car update counts `+0x3b3` down; at 1, row[1] returns from `+0x3b7` (`0x46967c..0x4696a4`),
  which the car init saved (`0x46a275`). So the boost lasts 99 updates. A contact that ends a running TurboSpeed also
  ends it: on the ground a solid contact (`0x46a9b6..0x46a9ea`, speed 0) and a wall push whose unit y is below 0.92
  (`0x46c5c1..0x46c601`, speed × 0.4) set `+0x385` = −1 and `+0x3b3` = 0, and put row[1] back from `+0x3b7`.

## Native

- **`RaceSession::step`:** during Ready (not battle), the rev and the false start (`false_start_`) as above, from
  the accelerate input. The PRET update still runs `drive()` at speed 0 (contacts, ground), then the rev goes
  back onto the speed, so the car stands and the speedometer shows the rev. Whether the original runs the car's
  contacts in state 1 is not traced. Without that `drive()` a check failed: the mine check needs the PRET contact
  push.
- **PRET motion (2026-11-13, later):** the car update does run in state 1, but `0x46b97b..0x46b9f5` zeroes the update's
  motion when `+0xf30` is 1 or 0xc. That motion holds the forward step and the contact pushes summed into it. So
  contacts are computed, but neither the player nor a robot moves during PRET. Natively the player's and the robots'
  positions are now put back after their PRET update (a robot's in `update_opponents`). This answers the question
  the first version left open.
  - With it the mine check passes again: robot 0's mine waits while its dropper is the nearest car.
  - The detour-sign check, which hit the player during PRET, now runs after the countdown.
  - The autopilot check accepts a car that keeps its speed against a wall (mean speed above 0.1 over 300 updates)
    as well as one that travels 3000 units. With the new start, `dr_t3`'s race ended with the test driver against a
    wall, and the autopilot holds 0.195 there without getting out. Control: without the autopilot `sh_t3` coasts
    18 units, mean 0.000, and the check fails.
- **`start_boosts()`, at Ready → Go:** the three robots' `tuning.convergence` × 9/6/3 and the player's × (rev × 4 + 1);
  `Body::start_boost` = 100.
- **`count_start_boost()`:** at the start of each car update (`drive`, `drive_opponent`, before the far-mode
  returns) it counts down and restores `Body::base_convergence` at 1.

## Checks

- **Harness, `sh_t1`, accelerate for the last update before GO:** slot 0's robot 0.1008, 0.1889, 0.2659 and slot 2's
  0.0408, 0.0795, 0.1163, matching Wine to the third decimal. The player gets one rev step here (× 1.104); Wine's
  two give × 1.208.
- **`native_drive_test`:** a new check on all 20 courses. A two-step rev gives row[1] × 1.208, slot 0 × 9, slot 2 ×
  3, slot 3 unchanged, and everything restored after 100 updates. Control: with `start_boosts()` emptied it fails
  ("GO: row[1] x (rev x 4 + 1)"). All 20 courses pass; ctest 14/14.
- **Holding accelerate through all of PRET:** the rev passes the top after about 23 updates, a false start, and
  ends at 0. So the existing "PRET! blocks the throttle" check still reads 0.

- **`end_turbo()`** (2026-11-13): both TurboSpeed cancels now go through it. It ends the effect and, if a start
  boost is running, restores row[1] and clears the counter. The solid contact did not cancel TurboSpeed natively
  before; now it does, as `0x46a9b6` does. No dedicated check: it needs TurboSpeed driven into a solid or a wall
  within the start's 99 updates. All 20 courses pass.

## Open

- **Wine's grid.** Wine's `sh_t1` grid is the same in every run, slots 0..5 = vehicles 2, 0, 4, 1, 5, 6 (memory
  read 2026-11-13). Seed 140 of the port's shuffle gives it (also 163, 169, 420, 966); `WPC_GRID_SEED=140` sets it
  in the game, and the scratch harness takes it too.
- **The crash after GO is the grid's.** With the clock's grid natively, vehicle 0 in slot 0 cut across into vehicle
  6 in slot 1 within four updates of GO and both stopped. With Wine's grid there is no crash: slot 0's robot
  (vehicle 2) drifts z −698 → −965 over 27 updates while the front row runs on side by side.
- **Still differs:** that drift is a little faster natively: z −808 ten updates after GO, against Wine's −718 on that
  robot's tenth frame. The robots' other draws (`+0xb4`) are not matched by the seed. Not traced further.
- **The Wine timing, settled.** When the key script first sees the car on the grid, the countdown counter `+0xf34`
  is already at 55–57 of 60. And XTest keys take effect about 1.1 s late: Up pressed at about 0.3 s raised the speed
  only from 1.46 s, and it kept rising after the release at about 1.8 s. So the start observations made with key
  scripts are about a second off, including "the rev only two updates before GO" and "the robots left a second
  early". The robots and the player leave at state 2 in both builds.
- **The false start under Wine:** Up held from before the race loaded. `0x6f5834` was already 1 when the grid was
  first seen, the rev stayed at 0, and from state 2 the car gained 0.014, 0.027, 0.040 … (row[1] 0.017, not boosted),
  as natively.
- **No knock at a normal start (2026-11-13):** a native harness ran a race with accelerate held from GO for 150
  updates and measured the player's largest heading change, on five grids (seed 140, Wine's, and 0..3).
  - `sh_t1`: no change before update 125. The 0.1–1.3 rad that follows is the first hairpin's wall at x ≈ −7000,
    where Wine's car (Wine's grid, robots on it) also turns, at 7 s.
  - `cc_t1`: no change at all in the 150 updates.
  - Wine `sh_t1`: −0.025 throughout to the hairpin.
  - So in neither build do the robots knock the player aside at the start. The earlier native knock (cc_t1's
    drowning check) came before the start boosts, when native robots stayed level with the player.
- **The cc_t1 drowning check** drives straight into the river from the grid. It again runs with the robots
  enabled, and passes.
- **The drift is not the robots' parameters (2026-11-13).** Under Wine every robot's WCtrlRoboCar block
  (`[car+0x1c3]+0x18`) reads: m_rDirModifier `+0x24` 0.9 (0.4 on the one rank-boosted robot), `+0x34` 6.2832,
  `+0x3c` 0.7854, `+0x44` 2.0944, `+0x8c` 0.3491, `+0x94` 0.55, `+0x9c` 1.0, `+0xac` 900, `+0x84` 300. These are the
  port's `RoboParams`. Left as causes: the steering's random factor (rand % 30 + 80, its draws not aligned), the goal
  points at the grid, or the robots' yaw step at speed. A per-frame Wine log of slot 0's yaw and steer `+0x2ad` would
  separate them.

## The first steps after GO (2026-11-13)

- **Wine (`sh_t1`, slot 0's robot, polled every 10 ms from GO):**
  - steer `+0x2ad` and `+0x403` are exactly 0 and the heading −0.0245 for 18 frames, while the robot runs straight from
    (−17156, −698) to (−15256, −745). Then it steers (−0.079, −0.060, −0.039 …) and eases toward the middle: z −811 at
    x −14518.
  - its cursor: `+0x2c5` reads 0 for one frame at GO (0.152 s), then the grid polygon again with the next polygon
    `+0x2c9` filled (0.192 s). That is a module change, so its goals are set at once. The next change is at x −14267
    (`+0x2cd` = the old polygon), where the port's route also changes (polygon 207, x −14250).
- **Why it goes straight:** x −15256 lies about 1000 units from goal 1 (−14281, −1116). The decision's rule (ported in
  `RoboDriver::drive`): with the angles to goal 1 and goal 2 on opposite sides and the car not within 1000 of goal 1,
  it does not steer. So the straight line comes from the goals, not from missing ones.
- **Tried and reverted:** a first reading gave the robots no goals until their next polygon (`observe_module`). It
  matched the line but contradicts the cursor log above, and was removed.
- **The native difference:** for its first 8 updates natively the robot is in the avoidance branch, with its steer
  swinging between ±0.3 and ±0.5. Slot 1's robot, 7 units behind it along x, counts as "ahead": with slot 0's heading of
  −0.0245, the test dz·f_z + dx·f_x gives +0.35. It is within 300, heading the same way, and slower. Wine's robot, from
  the same positions, never steers. The original's exact test (its units, which positions it takes) is not traced.
- **The original's ahead test (static, 2026-11-13):** `0x4e69bd..0x4e6c70` matches `RoboDriver::avoid`. Cars on our
  module or the next (`+0x2c5`/`+0x2c9`). Positions from each car's world frame (`0x404def` → `0x4038f5`). The
  heading test only within 300 (`0x512610`, `0x4e6af5`): slot 1 is 300.08 away on the grid, so it does not apply.
  The ±120° window while `+0x1a7` < 20000 and the follow-mask bit `+0xc8` is set (`0x4e6bbf..0x4e6c4c`), otherwise
  dz·f_z + dx·f_x < 0 skips (`0x4e6c53..0x4e6c70`, in 1/4096 units). So the original also counts slot 1 as ahead
  (+0.35). `+0x2ed` is a per-update cache of each car's step: the octagonal length max + min/2 of (world-frame position −
  `+0x173`), recomputed when it reads −1.0 (`0x4e6c76..0x4e6da7` for ours, `0x4e6db0..` for the other car). That is
  the port's `moved(position, previous)`. So the test matches up to the step comparison. The divergence after GO
  lies in the step values on the first updates (what `+0x173` and the frame hold just after PRET) or later in the
  branch; not resolved.
- **Wine attempts at the first frames (2026-11-13):** reading the cars through the car table `0x6f2ce0` (by vehicle:
  slot 0 is vehicle 2, slot 1 vehicle 0) works, but the key script reached the race only after GO, and in one run it
  landed on the two-player selection screen (the menu keys slipped; the registry and the game files were unchanged).
  In the run that reached the race, `+0x2ed` read −1.0 between updates, which confirms it is reset every update. The
  first frames after GO were not captured. A reliable capture needs a launch path that does not depend on the menu
  key timing (for instance a demo or a save that starts the race directly).
- **Tooling, later the same day:** the reader got a `track` command that waits inside the reader for cars 2 and 0 in
  the car table and logs from that moment, with no FIFO round trips. But in two more runs the menu keys landed
  elsewhere: once on the vehicle selection, once in a two-player race (after extra Returns). The scratch prefix's
  game registry key still matched the original `~/.wine` one, and the first runs of the day had started one-player
  races from it, so this is Wine's timing drifting against the fixed key delays. Next: drive the menu from its
  state in memory (the menu node and cursor) rather than by timing.


## The GO update and the first decisions (2026-11-13, captured)

- **The capture.** The reader's new `track` command polls the car table `0x6f2ce0` inside the reader. The menu keys now
  wait 4 s each. The race was then caught at the countdown's first update. Slot 0's robot (vehicle 2) and slot 1's
  (vehicle 0), every 10 ms:
  - **GO update** (first frame with `+0xf30` = 2): neither robot has moved, speed 0, goals still (0, 0).
  - **next frame:** slot 0 has moved 20.6 (speed 0.101) and slot 1 13.7, `+0x173` holds the previous position. Goals
    are set: goal 1 (−14281, −1116), goal 2 (−12086, −1164), the port's exactly. Avoid side `+0xc4` 0, and it stays 0.
  - steer `+0x2ad` 0 until x −15256, as before.
- **Ported: the GO update itself drives nothing.** The port's Ready → Go switch happened at the top of the update, which
  then drove the cars. Now neither the robots nor the player get control in the first Go update
  (`!(phase == Go && phase_updates == 0)`), so the first move comes one update later, as under Wine. All 20 courses pass.
- **Still different:** the native robot steers −0.056 on its first driven update and is in avoidance from the second
  (slot 1 counted ahead once slot 0 has turned). Wine's steers 0. With the same goals, a = 0.120 and b = 0.067 are on
  the same side, so the decision steers natively.
- **The original's decision (`0x4e796b..0x4e7a22`):** same side when both angles have one sign, then
  s = (|x| < |y| or (within 1000 of goal 1 and speed > 0.4)) ? x : y, where y is `[esp+0x10]` and x the value on the FPU
  stack. Traced (2026-11-13): y = `[esp+0x10]` is a, the goal 1 angle, and x, left on the FPU stack, is b. That is the goal 2
  angle (`0x4e777d..0x4e7797`, from `+0x64`/`+0x6c` when there is a next link `+0x54`), or the edge aim on a dead end
  (`0x4e77fa..0x4e7856`, `+0x58`/`+0x70`). So s = (|b| < |a| or near-and-fast) ? b : a, the port's rule exactly.
  After the pickup turn (`0x40489a`, not in battle with a target) the steer is −(k × s × (80 + rand % 30) × 0.01),
  written whenever s ≠ 0 (`0x4e7a9e..0x4e7b2b`). So Wine's 0 means s was 0 after the pickup turn, or the race path
  (`0x4e64ad` on) left before the decision on those frames. That path, between the avoidance and `0x4e796b`, is not
  traced.
- **Tried and reverted:** running the robots' decision and power-up list during PRET. Under Wine the goals are (0, 0)
  until the first frame after GO, so the decision does not run in PRET.

## The robots' decision hold (2026-11-14)

- **Recovered:** WCtrlRoboCar `+0xb8` = 20 at the controller's init at GO (`0x4e48d2`). `0x4e4f70` counts it down at
  the start of every update of a single race or championship (`[0x5472b8]+0x1a60` 0 or 3, `0x4e4f5d..0x4e4f6a`),
  before the state dispatch. `0x4e53df` (WackyAutoBomba running, `[+0x36d]+0x2c` ≠ 0) jumps past the decision first;
  then `0x4e6962..0x4e697d` skips the whole decision to `0x4e7d60` while `+0xb8` is above 0 (states 2 and 3). The
  control keeps its default: throttle 1, steer 0. So the robots drive straight off the grid for 20 updates.
- **This explains the open difference above:** Wine's steer 0 and avoid side 0 just after GO are the hold. The port's
  avoidance on the second driven update came from deciding at once.
- **Ported:** `Opponent::start_hold` (set in `start_boosts`), decremented at the top of `drive_opponent`; a hit keeps
  the last control (checked first, as `0x4e53df`), then the hold gives `RoboControl{}`. Native `sh_t1` (grid seed 140):
  first steer at x −15259 with −0.0795, Wine −15256 with −0.0789.
- **Tests adapted** (they had been written against robots that decide from the first update):
  - far robot hit: the test picked a robot already hit or shielded on `dr_t4` (`start_bomb` refuses, guard 2), and
    one still in its hold on `ct_t1`. It now skips those. On `ct_t1` the chosen robot's last control was a brake, so it
    backs about 670 into a wall and comes back during the hit (93 from the start at the end). The check takes the
    farthest point.
  - autopilot after the end (`dr_t3`): the race now ends with the test driver against a wall; the autopilot waits
    there until the robot's stuck rule (over 100 slow updates, then state 0x10) backs it out, driving again by the
    end (830 units, final speed 0.176). A final speed above 0.1 with over 500 units now counts; a coasting car ends at
    0.
- **Not verified:** whether Wine's autopilot gets stuck at the same wall; the lap times with the hold against Wine.
- **Avoidance after GO (native, sh_t1, grid 140, 2026-11-14):** with the hold, slot 0's avoid flag stays 0 over the
  first 60 updates; the first steer comes at update 19 (x −15259), then −0.042, −0.023 and 0 by update 22. The 8
  avoidance updates seen before the hold are gone, which matches Wine's avoid side 0. Later frames were not compared.
