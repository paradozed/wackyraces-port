# Native startup and intro movies

## Recovered behavior

`0x4acad0` (thunk `0x40240f`, called during initialization at `0x4ad27f`):

1. Calls `0x4ac850`, which opens `legal.avi` from the install directory on
   player `0x53eff8` (`0x4026d0(name, 0)`), sets a 640×480 presentation area
   (`0x402e5f(0x280,0x1e0)`) and plays it.
2. Opens `<CD root>\Movies\intro.avi` with the paired audio file
   `Voice\iaudio.avi` (`0x4026d0(path, "Voice\iaudio.avi")`).
3. Presents `L2.bmp` and `L3.bmp` at 640×480, each followed by
   `FUN_00413bd0(2)` (two whole seconds of `timeGetTime`).
4. Sets the 320×240 movie area (`0x402e5f(0x140,0xf0)`) and plays the intro.

Both playback loops follow the same pattern:

- Each pass computes `frame = (timeGetTime() − start) × rate / 1000`
  (`0x53f389`) and draws that frame through `0x4043f9`.
- Playback stops when the frame reaches the stream length (`0x53f385`).
- An Escape press edge also stops it: DirectInput key index 1 goes from up in
  `0x541adc` to down in `0x5419dc`.

When audio is present (`0x53f4e7`), the clock is the audio start.

## Files

| File | Video | Audio | Frames |
|---|---|---|---|
| `legal.avi` (install) | IV50 640×480, 1000000/40000 | PCM mono 22050 Hz in file | 150 |
| `Movies/intro.avi` (disc) | IV50 320×208, 25/1 | none (`Voice/iaudio.avi`, stereo 22050 Hz, title `intro_fr.wav`) | 1965 |
| `Movies/outro.avi` (disc) | IV50 320×208, 25/1 | none (`Voice/oaudio.avi`) | 1381 |

All chunks are non-empty and stored in `movi` order.

## Presentation

The intro is centred in the 320×240 surface: rows 16..223, matching the Wine
frame where Xvfb kept the unscaled surface at the top-left. On a real display
the mode change makes that surface fill the screen. The native player
therefore scales the surface to the 4:3 game area with nearest sampling.

## Native implementation

- `game_avi`: bounded RIFF/AVI 1.0 reader.
- `movie_decoder`: Indeo 5 through optional libavcodec, converted to RGBA with
  BT.601 studio-range coefficients and each 4×4 chroma sample replicated. This
  differs from ffmpeg's interpolated RGB by about 2.5/255 on average. The
  original `ir50_32` conversion is not recovered.
- `movie_player`: time-indexed frames, sequential decoding, and per-frame
  texture updates (`wpc_vk_texture_update`).

`linux_main` follows the order above. A missing file or decoder skips that
movie with a log line, and the main menu's SEQUENCE D'INTRODUCTION replays the
intro. Audio is not played.

## Colour conversion fitted to ir50_32 (2026-11-01)

**Reference frames.** A small VfW program, `AVIStreamGetFrameOpen` with a 24-bit `BI_RGB` request, built with mingw and run under Wine in the scratch prefix, decoded `intro.avi` frames 300 and 800 through the installed `Ir50_32.dll`. Neither the program nor the frames are kept in the repository. The same frames' YUV 4:1:0 planes came from libavcodec.

**Fits.**
- A free linear fit per channel is close to BT.601 studio range with chroma interpolated bilinearly: R 1.1645 Y + 1.5905 V − 223.5, G 1.159 Y − 0.380 U − 0.808 V + 130.5, B 1.165 Y + 1.998 U − 278.0. Error 1.0/0.67/1.07, against 1.74/0.97/2.18 with replicated chroma.
- With the standard coefficients fixed, the best chroma sample position is the block centre (1.5 + 4k), and the median offsets are R −2.0, G −3.3, B −1.7.
- **Result.** The mean absolute error per channel against the codec is 0.91 (frame 300) and 1.39 (frame 800), against 3.08 and 3.55 for the earlier replicated, offset-free conversion.

**Native.** `yuv410_to_rgba` uses fixed point in eighths for the interpolation, edges clamped, and the offsets above. It is an empirical match. The codec's conversion routine is not traced, and the offsets may partly reflect luma differences between libavcodec's and the codec's decoders.
