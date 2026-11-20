# Race audio against Wine: a paced capture of a cc_t1 start (2026-11-18)

Follows `analysis/runtime-capture.md` (the capture's sample format was not settled) and
`analysis/native-audio-20261028.md`.

## Method

- **Wine:** the scratch prefix with the ALSA driver into a PulseAudio null sink (`alsa_pulse.conf`), recorded with
  `parec` as s16le, 44100 Hz, stereo (`build/wine-scratch/snd_cc1.sh`). The capture follows the server's clock, so
  its length is real time: 115.9 s for the run.
  - At the main menu the music and voice volumes (`0x6f4570`, `0x6f4578`) were written to 0.0; the effects volume
    `0x6f4574` stayed 1.0.
  - Then a single cc_t1 race, the player idle on the grid.
- **Native:** `--race cc/cc_t1.WGM` with `WPC_AUDIO_CAPTURE`. The music and voice buses were muted by a temporary
  patch, since reverted.
- Both mixes compared in 0.5 s blocks: RMS per channel, then L - R in dB.

## Results

- **The capture.** Real time, and silent except for sound (0 RMS until the game plays):
  - legal.avi's soundtrack at 24..30 s, mono (L = R);
  - the intro at 34..56 s;
  - the menu with the music at 0 (about 80 RMS, a menu sound every 4.3 s);
  - the race from 81.5 s.
- **Steady part** (from about 3 s after the start, with the robots gone, the player's own idle engine): both L = R.
  Wine about 3810 RMS, native about 3385, so **native is 1.0 dB quieter**. Not traced: the player's engine volume
  in the port, or Wine's mixer and resampling (48 kHz float through PipeWire).
- **The start** (the robots' engines leaving the grid, 3D):

  | 0.5 s block | 1 | 2 | 3 | 4 | 5 |
  | --- | --- | --- | --- | --- | --- |
  | Wine RMS L/R | 6108/5301 | 3982/4037 | 5252/5947 | 8154/8420 | 7055/6488 |
  | Wine L - R | +1.2 dB | -0.1 | -1.1 | -0.3 | +0.7 |
  | native RMS L/R | 6214/5938 | 3504/1928 | 1217/2384 | 4200/5687 | 4723/3960 |
  | native L - R | +0.4 dB | +5.2 | -5.9 | -2.6 | +1.5 |

  The port pans the passing robots much harder (up to about 6 dB) than Wine (about 1.2 dB), with the same signs at
  first. Wine is also louder at the start's peak (8420 against 6214). The blocks are not aligned exactly, since the
  two runs start the race at different points of a block.

## What this does and does not show

- It measures the port against **Wine's** DirectSound 3D, which is Wine's own implementation (its pan and distance
  law), not Windows'. So it does not give the original's pan or HRTF on Windows, and no port change was made from it.
- What remains open: the exact Windows DirectSound3D pan and attenuation (Windows itself is needed, or a reading of
  the game's `IDirectSound3DBuffer` parameters together with Microsoft's documented software 3D algorithm); the 1.0
  dB difference in the player's engine level.
