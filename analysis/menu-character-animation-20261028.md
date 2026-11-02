# Main-menu character animation (`WMenuAnimation`)

Replaces the earlier fitted timings (a 6.41 s Muttley delay, a 1,064 ms
Satanas gesture), which came from a C port no longer in the tree, and the
`src/port` loop of each film's serialized range. Recovered from the executable;
checked against a same-session Wine capture.

## Objects

The common group `GDStdBrick 27` holds three `GDFilm2` objects on
`FILQ 0x7cf` (17 sequences: frame counts 1, 1, 4, 26, 10, 20, 31, 40, 50, 60,
80, 90, 19, 1, 48, 109, 4). In order:

| Node | Role | Serialized sequence / mode / range | Active |
|---|---|---|---|
| `0x220` (544) | Satanas body | 0 / 2 / 0..0 | yes |
| `0x21f` (543) | Muttley | 13 / 2 / 0..0 | yes |
| `0x21b` (539) | Satanas talking head | 3 / 2 / 0..24 | no (flag 0x2 of 0x1a clear) |

`0x220` carries the action `WMenuAnimation 545`. Its class record is at
`0x6f3f90`, its constructor at `0x494210` (vtable `0x5145d8`) and its update
at `0x4942b0`. The update reaches the film it is attached to and the next two
objects through the `+0x14` sibling links.

## Film player (`GDFilm2`)

- **Switch (`0x493f10(film, sequence, mode)`)**: sequence `+0x15a`, first 0,
  last = frames − 1, frame 0, mode `+0x16a`. The accumulator `+0x196` is kept.
- **Finished test (`0x493fa0`)**: frame == last.
- **Activate (`0x41ba60` → vtable `+0x10` = `0x4300d0`)**: sets flags `0x1002`
  and, when the mode is nonzero, accumulator = first and completion cleared.
  Deactivate (`0x41ba90`) clears them.
- **Update (`0x430110`)**: `acc += speed` (m_Speed 0.5), frame = truncated
  `acc`.
  - Mode 1: completes and holds at last.
  - Mode 2: when `acc > last`, `acc` = first, so the last frame shows for one
    update.
  - Mode 3: past last, `acc` = last and the speed is negated; below 0, the
    speed is negated and `acc` = 0.
- **Draw order**: a frame's `FRMQ` lists pieces front to back. Satanas's rest
  frame is head, torso, coat and shadow, and the gesture arm comes first. They
  are drawn in reverse, so the shadow lies under the feet.

## Controller (`0x4942b0`, once per menu update)

States: Satanas `0x6f41f8`, Muttley `0x52bf74` (0 and 13 after `0x48fa26`).
They are set by `0x486d20`/`0x486d50`, which raise the requests `0x6f41fc` and
`0x6f41fd`.

1. **Satanas's claw**: state 12 at the last frame → 0. In state 0, a
   `rand() % 1000 < 5` draw starts 12.
2. **Muttley's laugh**: state 15 at the last frame → 13. In state 13, a
   `rand() % 1000 < 5` draw starts 15.
3. **Voice start** (`0x6f41fe`), by the speaking actor `0x6f37dc`: 0 → Satanas
   3; 1 → Muttley 14; 4 → Muttley 16.
4. **Voice end** (`0x6f41ff`): for actor 0, the head is deactivated and
   Satanas set to 0, playing sequence 0 in mode 2. Actors 1 and 4 return
   Muttley to 13.
5. **End of a talk gesture**: when Satanas plays sequence 4..11 and reaches
   its last frame, he goes to state 1.
6. **Satanas request**:
   - In state 3, the head is activated and the body plays a talk gesture once
     (mode 1). It is chosen from `v` = voice length / 4: `v` ≤ 10 → 4;
     `v` > 85 → 11; `v` ≥ 70 → (v+5)/10 + 2; otherwise (v+5)/10 + 3.
     Sequence *k* (10..90 frames) then lasts about the length that selects it.
   - In any other state, the body plays that state's sequence in mode 2.
7. **Muttley request**: plays its state's sequence in mode 2.

`rand()` is MSVC's (`0x430e40`), seeded with `0x1312d6e` by the scene loader
(`0x431fc3`). Other code also draws from it during the menu, so the native
build reproduces the distribution, not the exact gesture times.

## Voices (`0x486da0`, `0x486e20`)

- **Queueing**: selecting an item (`0x488ba0`) queues its `WMenuSound` with a
  delay of 20 menu updates; another caller (`0x48ba7f`) uses 15.
- **Start**: when the delay runs out, the voice plays. Its length in 1/60 s
  (voice vtable `+0x20` = `0x427c00`: bytes ÷ (rate × bytes per frame) × 60.0
  at `0x510c10`) goes to `0x52bf78`. The sound's `m_nSpeakingActor`
  (registered at `+0xd4`, serialized as property `0x1bc`) goes to `0x6f37dc`,
  and `0x6f41fe` is set.
- **End**: when the voice ends, `0x6f41ff` is set.
- **Actors**: 54 of the menu's sounds have actor 0 (Satanas) and one has
  actor 2, which animates nobody.

## Tick

The talk-gesture lengths require 30 updates per second: sequence 5 (20
frames) fits `v` ≈ 20 (1.33 s) only at 0.5 frame per 33 ms update. This
matches the 33 ms frame limiter `0x4ad540`.

## Native port

- `menu_animation.{h,cpp}`: `FilmState`, `MsvcRandom` and `MenuAnimation`,
  ported from the routines above.
- `MenuView::update` runs them at 30 Hz. A restart of the menu clock reloads
  the initial states, as reloading `MENU.WGM` does.
- `GameAudio::queue_menu_item`/`menu_voice_update` give the 20-update voice
  delay and report a voice's start (actor, length) and end.

`native_menu_test` checks the three modes, the `rand()` sequence, the
length-to-gesture table, the talk/hold/rest cycle and Muttley's speaking
sequence.

## Wine comparison (1024×768, French)

| | Wine | Native |
|---|---|---|
| Idle, 12 s | Muttley animates throughout; Satanas stays at rest | Muttley animates; Satanas makes random claw gestures |
| After Down (OPTIONS, 56/60 s) | about 1.0 s later, mouth open and open hand for about 0.5 s, then rest | about 0.75 s later, the same gesture, then the talking head until the voice ends |
| Feet | standing on the shadows | standing on the shadows (were hidden before the reverse draw order) |

The idle difference is within the random draw: a 12 s window without a
Satanas gesture has about a 16% chance at 0.5% per update. The voice onset is
a frame or two earlier natively. Wine's update interval is about 35 ms, and
XTest key latency adds to it.

Not compared: the actor 1/4 (Muttley speaking) paths, which no menu sound
uses, and the attract-demo hand-off after about 28 s idle.
