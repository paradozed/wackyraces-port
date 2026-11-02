# Native audio

## Output

`AudioMixer` mixes 44.1 kHz interleaved stereo s16. Voices have linear
resampling, pitch, volume, attenuation-only pan and looping, and belong to
four buses: effects, music, voice and movie. `AudioOutput` feeds it in 20 ms
periods. Sinks, in order of preference:

1. `WPC_AUDIO_CAPTURE=file.wav`, paced in real time; the header is refreshed
   every second.
2. libpulse-simple (PipeWire's pulse server here; ALSA's `default` device
   failed with "dmix: unable to open slave").
3. A silent sink. `WPC_AUDIO=silent` forces it, and the automated scripts set
   it.

The original uses DirectSound buffers (`0x427dc0`) and a 500 ms stream timer
(`0x428dc0`). Its volume law is reproduced (see "Buses and volume law",
2026-10-29); its pan curve is not.

## Formats

- **`.snd`** (`0x428790`): byte 0 is channels − 1, byte 1 is the IMA flag,
  then a u16 rate. Decoding uses the recovered `wpc_ima_decode` (`0x40bdd0`):
  high nibble first, stereo interleaved per byte. Music is decoded whole and
  looped, which equals the original wrap with a reset predictor.
- **Banks:** SBPC directory (`0x44c510`, exact language ID) and WGM `SBNK`
  share one bank layout (`analysis/sbpc.md`). Row flags: bit 0 stereo, bit 1
  16-bit; all installed rows are 16-bit. Rates vary per row
  (8000/11025/22050). Lookup matches the group in the high byte and the
  low 24 bits (`0x44c640`).
- **AVI PCM:** the audio stream's chunks are concatenated.

## Movies

`legal.avi` plays its in-file track. The intro pairs `Movies\intro.avi` with
`Voice\iaudio.avi` (`0x4026d0`'s second argument). With audio present
(`0x53f4e7`) the original times frames from the soundtrack start; the native
player uses the voice's mixed position as the clock, then wall time if the
track ends first. Skipping or finishing a movie stops its sound.

## Music (`0x431690`)

The path is `music\` plus the course's last path component with `.snd`.
`dr` courses use the fallbacks Cc_t1/Sh_t1/Dt_t1/Ct_t1/Cc_b, and the default
(menu) is `Cc_t1`. The disc is searched first, matching the traced
`D:\music\Cc_t1.snd`. Music stops during movies and changes between menu and
race.

## Menu

`Menu.GSW` loads the bank for the `LANG` selection (`0x431d87..0x431db1`).
Page entry selects items with flag 1 (`0x48c700`/`0x48c750`). Cursor movement
(`0x48cd80`) selects with flag 0, and `0x488ba0` then plays the item's
`WMenuSound` child (class `0x6f3ce0`) through the menu manager (`0x486f60`).
The native build plays it on user cursor moves, one menu voice at a time.

`WMenuSound` property `0x1bc` is `m_nSpeakingActor`. `WMenuSetVolumeItem`
`m_nSoundType` (`0x193`) is 0 music, 1 effects (EFFETS), 2 voice (VOIX), and
3 a non-sound slider (string 152). `m_rVolume` starts at 0.5 of
`m_rMaxVolume` 1, and gain is applied linearly.

## Voice-overs (`0x426df0`, sound-manager vtable `0x510b80` slot `+0x2c`)

The call is `voice(character, event, chance)`:

- Characters 0..6 are skipped while another voice plays.
- Chance 255 always plays; otherwise the voice plays when
  `chance > 3·(rand % 255)`.
- Filenames come from `0x426f20`:
  - Events 100..103 return `vo-N-900..903.snd` only in order, using the
    sequence byte `0x546331`, which resets after 903.
  - Otherwise the table at `0x527d20` gives `[vehicle][event][4 variants]`
    (generated into `voice_table.cpp` from `voice-sound-map.tsv`). The native
    build cycles the variants and skips the 34 missing files.

Call sites found:

| Site | Arguments |
|---|---|
| `0x46a7c6` | (vehicle, 5) on the `0x10000` contact |
| `0x46b429` | (vehicle, 4) on the mask-`0x100` contact |
| `0x4a8bac` | (new leader's vehicle, 0) on a leader change |
| `0x4a9018` | (7, `0x67`−n) during PRET!, with n = max(1, 3 − counter·3/60) |
| `0x4a90e7` | (7, `0x67`) on PARTEZ! |
| `0x4d8fdf`, `0x4d9fcf`, `0x4d9fee`, `0x4de126`, `0x4ded33` | events 5/6 from power-up code |

## Race sounds

WackyAuto setup (`0x469bf0`) takes the actor's children in order:

- the first `GDSound` → `+0x3bf` (engine loop; Dingo #231);
- the third → `+0x3c3` (created paused);
- the next seven → `+0x3cb..+0x3e3`.

Engine (`0x4705ca`):

- ratio = |speed/top|, plus (rand%20 − 10)·0.03 when top − speed < 0.15;
- target = `m_minpitch` (`0xf6`) + (`m_maxpitch` `0xf7` − min)·ratio;
- `+0x3bb` += (target − `+0x3bb`)·`m_pitchmul` (`0xf8`).

Other sounds:

- **Wall-response branch** (`0x46c64b`): `+0x3d7` (#238) unless `+0x3e7`
  (30 updates) is running. Other contacts use `+0x3cf`/`+0x3d3`.
- **Drowning:** `WSplash`'s `GDSound` (`0x1033d`).

Native coverage: the engine, narrator, wall hits (all native wall contacts
use `+0x3d7`), the splash and the leader voice; since 2026-11-01 also the
skid loop `+0x3c3`, voice events 4 and 5, the robots' engines and skid
loops, and 3D distance and pan (see below).

## Verification

`tests/native_audio.cpp` covers the mixer, decoding, banks, names and AVI
PCM, plus corpus checks with data/disc paths. A WAV-captured startup showed
`legal.avi` sound over 0–6 s, silence over the cards, the intro soundtrack,
then menu music. A captured race logged narrator 3/2/1/GO at updates
1/20/40/62, the engine loop, the leader voice and a splash on each drowning.
No listening comparison with the original has been made.

## Menu cursor sound and volume law (2026-10-28)

- **Cursor click.** Every cursor move (`0x488ba0`, flag 0) queues the item's
  voice (`0x486da0`, 20 updates) and then calls `0x4041fb(0)` → `0x486ef0`.
  That plays the manager's cursor sound in the speaking slot `0x6f4200` at
  once, stopping the speaking voice; a nonzero argument plays only when the
  slot is idle. The sound is `0x6f420c`, set at `0x48f9ab` from `WMenu 319`
  property `0x1a8` = `WMenuSound 1009`. It is `GSMP 0x7f7` in `menu.WGM`'s own
  SBNK: mono, 22,050 Hz, 0.129 s. When the voice's delay runs out, `0x486e20`
  stops the click if it is still playing.
- **Bus.** Item voices have GDSound `0x9d` = 1 and the click 0; the voice
  factory (`0x44c2f4`) turns that field into flag `0x10`. Native puts sounds
  with it on the VOIX bus and the click on EFFETS. Wine levels support this:
  with EFFETS 10/10 and VOIX 8/10, the click is 2.15× the OPTIONS voice, while
  the raw ratio is 0.68. (Traced on 2026-10-29, below: the bus reads the
  GDSound field itself, not the flag.)
- **Volume law (`0x427f90`).** IDirectSoundBuffer SetVolume(min(v,1) × 4000 −
  4000), in hundredths of a dB: 1 → 0 dB, 0.5 → −20 dB, 0 → −40 dB.
  (Superseded on 2026-10-29: the slider caps the volume rather than
  multiplying it, and race sounds follow the same law.)
- **Wine check.** Recorded through a temporary PulseAudio null sink with
  MUSIQUE at 0. A cursor move gives the click about 0.1 s after the key, then
  the OPTIONS voice 0.73 s later (0.64 s natively; Wine's update interval is
  about 36 ms).

## Buses and volume law (2026-10-29)

Read from the disassembly (`objdump` of the PE); not compared with Wine
captures beyond the menu ratio below.

- **Bus.** The sample voice's play (`0x427ed2`) and update (`0x427f90`) read
  GDSound `+0x16a` (property `0x9d`) through the voice's `+0x8`: nonzero takes
  the slider level `0x545320`, zero `0x545258`. The voice factory's flag
  `0x10` (`0x44c2f4`, voice `+0xc`) has no reader in the sound code; the
  `+0xc & 0x10` tests at `0x4a8680`/`0x4a8850` are on world objects.
- **Law.** `0x427f90`: SetVolume(min(volume `+0x14`, level) × 4000 − 4000);
  `0x427ed2` does the same at play. Frequency is the buffer's rate × `+0x1c`;
  pan `+0x18` × 4000 unless the voice is 3D (flag 2). Streams (`0x428dc0`,
  every timer tick) take the level itself through `0x428c50`
  (SetVolume(level × 4000 − 4000)): `0x545324` when the stream's byte `+0x5a`
  is set, else `0x545320`.
- **Sliders.** `0x426c70` (sound manager vtable `0x510b80` + `0x34`, called as
  (`0x6f4570`, `0x6f4574`, `0x6f4578`, `0x6f457c`)) stores `0x545324` = music
  `0x6f4570`, `0x545258` = effects `0x6f4574`, `0x545320` = voice `0x6f4578`;
  the fourth argument is unused there. The slider page (`0x489aa0`, `0x489b60`
  down, `0x489cc0` up) shows and writes the global for its `m_nSoundType`
  (0 music, 1 effects, 2 voice, 3 the 0..100 `0x6f4588`, applied through
  manager `+0x1838` vtable `+0xc0` with (v − 50) × 800); the effects slider
  plays the cursor sound (`0x4041fb(1)`) as it moves. Every change reapplies
  all four.
- **Defaults.** `0x496590` (reached from `0x4964b0` → `0x4964d0` with
  `this` = `0x6f4568`; provisional: a static initializer) sets music 1.0,
  effects 1.0, voice 0.75, the `0x6f457c` toggle 1 and `0x6f4588` 50. At
  startup `0x4603c0` replaces the 0x3b words at `0x6f4568` with the
  `MenuPrefs` registry value when its size field is 0x178; otherwise it
  deletes the value (`0x460380`). `0x4604f0` saves the block. The user's
  prefix holds the same 1, 1, 0.75 (VOIX shows 8/10), so the Wine levels were
  the defaults. `wpcsetup.exe` does not write `MenuPrefs`.
- **Menu check.** Every menu and course GDSound/WMenuSound has `0x97` = 1;
  race and shared sounds have `0x9d` = 0 (EFFETS). With the law, the click
  plays at 0 dB and a menu voice at min(1, 0.75) = −10 dB: 0.68 × 3.16 = 2.15,
  the ratio measured under Wine.

Native: `AudioMixer` gives each voice `directsound_gain(min(volume, bus
level))`; bus levels are the slider values (`apply_volume_item`), music and
movies play at volume 1 (the level itself). `sound_bus` picks the bus for
menu and race sounds; race sounds pass their GDSound `0x97`, the splash and
WKBumm included. `MenuController::volume` starts each slider at the
`0x496590` default for its type instead of `m_rVolume` (0.5 in `menu.WGM`).
Not ported: `MenuPrefs`, the voice-over player's own volume path
(`0x426df0` plays on VOIX at volume 1, provisional) and movie volume.

## Skid loop, contact voices, robots' engines and 3D (2026-11-01)

Read from the disassembly.

- **`+0x3c3` is the tyre skid loop.** It is the third `GDSound`, #233. At every update's end, `0x4705f6..0x470644` sets `+0x25b` = `+0x25f` (brake skid) || `+0x263` (slide), the flag the skid marks use.
  - While the flag is set, the voice plays: `IsPlaying` (vtable `+0x18`), then `Play` (`+0x10`) if it is idle.
  - Otherwise it stops (`+0x14`).
  - This runs for every car. Native: `RaceAudio::update_car` plays the third sound of each car while `skidding()` / `opponent_skidding(i)`.
- **Voice event 4** (`0x46b429`). In the car-to-car contact (query mask `0x100`), unless the other car's hit type is `0x200`:
  - the toucher speaks event 4 with chance 255, every update in contact;
  - then `+0x3cf` plays on the free 30-update timer `+0x3e7` (`+0x3c7` holds its handle).
- **Voice event 5** (`0x46a7c6`). On an `0x10000` primitive (a rock), the toucher speaks event 5 with chance 255, every update in contact.
- **Voice events from power-ups.** These are in weapons the port lacks:
  - `WPU_Flipper` `0x4d9fcf`/`0x4d9fee`: the user speaks 6, the victim 5;
  - `WPU_BootKicker` `0x4d8fdf`: 6;
  - `WPU_OilSpill` `0x4de126` and `WPU_BeeHive` `0x4ded33`: 5.
- **Native voice events.** `RaceSession::voice_events()` lists the requests (vehicle, event) of the last update. `RaceAudio` passes them to the voice player, which skips characters 0..6 while another voice speaks.
- **Robots' engines.** `0x469bf0` and `0x4705ca` run for every WackyAuto. Native: each robot has its actor's engine loop and its own pitch `+0x3bb`; it is silent while the robot is hidden or drowning.
- **3D.**
  - A voice with `m_3D` (`0x9b`, set on 64 of the 70 WackyAuto sounds) is an `IDirectSound3DBuffer`. `0x427c50` sets `SetMinDistance(m_MinRange)` and `SetMaxDistance(m_MaxRange)`; a max not above the min is raised.
  - Every voice update (`0x42804e..0x42808c`) passes the GDSound node's frame position to `SetPosition` (`+0x4c`, immediate). The voice's own pan (`+0x18` × 4000, `+0x40`) applies only to non-3D voices.
  - The listener is set up at `0x4272f6..`: `SetDistanceFactor(1.0)` (`+0x2c`), `SetRolloffFactor(5.0)` (`+0x3c`, `0x40a00000`). It then follows the camera node every update (`SetPosition` `+0x38` and `SetOrientation` `+0x34` at `0x4276ce`/`0x4276fa`).
- **Native 3D law** (`directsound3d`, provisional). It follows DirectSound's documented model:
  - the gain is (min / d)^rolloff, with d held within [min, max]. With 1000/10000, that is full volume within 1000, −30 dB at 2000, and silence at 10000;
  - the pan comes from the azimuth in the follow camera's frame. ±90° maps to ±10000 mB on the far side, the mapping Wine's dsound uses;
  - the listener is the follow camera (`initial_follow_camera`);
  - the player's sounds sit within 1000 of it. Wine's dsound ignores the rolloff factor, so a capture under Wine would fall off more slowly than this law.
- **Check.**
  - `native_audio_test` checks the law and the mixer's spatial gain and pan.
  - A cc_t1 race log shows voice event 4 from the robots touching after the start.
  - A run with the player idle gives the robots' engines gains of 0.15..0.87 on the grid, ~0 once they leave, and 1 when one passes.
  - No listening comparison or Wine capture.

## The robots' one-shot sounds (2026-11-01)

The robots are WackyAuto cars and run the same contact code as the player.
Each has its own 30-update timer `+0x3e7`. Sounds by contact:
- wall: `+0x3d7` (`0x46c64b`);
- rock: `+0x3d3` (`0x46aa11`);
- cactus and car contacts: `+0x3cf` (`0x46aa86`, `0x46c6..`);
- a cactus break: the WKBumm clone's sound (`0x46aa58`).

Natively, `RaceSession::opponent_sounds(i)` gives the robot's counters. `RaceAudio` plays the robot's own GDSound slot on each new event, placed by DirectSound3D at the robot's position. Hidden (far-mode) robots stay silent.

A `cc_t1` run of 4000 frames with `WPC_AUDIO_CAPTURE` logged robot car hits at the start (updates 73–114) and one robot wall hit. The 3D levels were not compared with Wine.

## The hit sound and the remaining slots (2026-11-01)

- **The hit sound `+0x3e3`.** The tenth GDSound. `0x47d768`, at the end of WackyAutoBomba's activation, checks for that sound with the handle slot `+0x3c7` free. It then sets the 30-update timer `+0x3e7` and plays it (`0x405164`), keeping the handle in `+0x3c7`. Natively, `RaceSession::hit` counts it on a free `+0x3e7` timer, and `RaceAudio` plays slot 9, the player's directly and the robots' through DirectSound3D. A dt_t1 run of 6000 frames logged two.
- **The other slots.** `+0x3cb` is read by a weapon (`0x4de12c`) and `+0x3df` by FlyPower (`0x4d4dcf`). None of the seven slots depends on the ground surface.
