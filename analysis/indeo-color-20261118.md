# Indeo 5 colour conversion: Intel's ir50_32, recovered (2026-11-18)

Replaces the fitted conversion of `analysis/native-movies-20261027.md` ("Colour conversion fitted to ir50_32",
2026-11-01).

## Which codec

- The game opens its frames with `AVIStreamGetFrameOpen(stream, NULL)` (`0x4135ad`, IAT `0x2ad45c0`), so the
  codec picks the output. Then `DrawDibDraw` puts the frame on the screen.
- The scratch prefix holds Intel's codec (`syswow64/Ir50_32.dll`, 755200 bytes, 1999-05-17). Wine also ships a
  builtin `ir50_32`, and that is what loads by default: `+loaddll` shows `ir50_32.dll ... builtin` and
  `winegstreamer.dll`. The 2026-11-01 reference frames therefore came from GStreamer's decoder, not Intel's.
- `WINEDLLOVERRIDES=ir50_32=n` loads Intel's (`... native`). Its 24-bit output differs from the builtin's by 3.2..4.5
  per channel (mean |difference|, intro.avi frames 300 and 800). The port's fitted conversion was within about 1 of
  the builtin's and 3.2..4.5 off Intel's.
- A NULL format request gives the same 24-bit BI_RGB frame as an explicit 24-bit request (byte-identical, frame 300).

Tools (scratch, `build/wine-scratch/vfw/`, not kept in the repository): `vfwdec.exe` (VfW decode to raw BGR, or with
`VFW_FOURCC` to IYUV), `probe.exe` (a one-shot `int3` at every routine start of the DLL, recorded by a vectored
handler: which code runs for a 24-bit decode), `snap.exe`/`snap2.exe` (a re-arming breakpoint that dumps the
converter's stack frame), `dumpyuv` (libavcodec's YUV410 planes), `mmx5.py` (the model).

## What runs

- Intel's luma decode equals libavcodec's: the IYUV output's Y plane is identical, 100% of pixels, frames 300 and 800.
- The table converter at `0x100522d0` (tables built at `0x100520d0` from the constants at `0x10093038`: 16, 235,
  1.164 x 2^16, 1.37112, -0.69846, -0.33592, 1.73282 x 2^16; a 4x4 dither at `0x10093064`/`0x100930a4`) did not
  match. It is the C path; the probe shows that a 24-bit decode runs the MMX converter at `0x100641a0` instead.
- `0x100641a0`, for each 4-row band (one chroma row):
  - `0x10064210`: each chroma row is interpolated horizontally to one value per 2 pixels: c0, (c0 + c1) >> 1, c1, ...
    (`punpcklbw`, `paddusw`, `psrlw 1`), with the last sample repeated (`0x100641ec`).
  - The two rows of a band's lower half take the midpoint with the next chroma row, which is repeated at the bottom.
    This was found by emulation: rows 2..3 of every band were off before it.
  - Per pixel, in unsigned saturating 16-bit lanes, with the constants read from the converter's frame at the
    breakpoint (`ebp = esp + 0x100`):
    - `[esp]` 16: `(Y - 16)` (`psubusb`), times `[ebp-0x80]` 149;
    - R: `(y + 204 V - dR) x 2 >> 8` (`[ebp-0x78]` 204);
    - G: `(y + dG - min(50 U + 104 V, 32767)) x 2 >> 8` (`[ebp-0x70]` 50, 104 through `pmaddwd`, then `packssdw`);
    - B: `(min(258 U, 65535) + y - dB) x 2 >> 8` (`[ebp-0x68]` 129, then `paddusw` doubles it).
  - The offsets dR, dG, dB (`[esp+0x20..0x7f]`, the same in every band) form a 4x4 dither by (row % 4, column % 4):

    | row | R | G | B |
    | --- | --- | --- | --- |
    | 0 | 668c 6428 65c0 64f4 | 4da8 4c3e 4d72 4c74 | 80c0 7fbe 81c2 7ebc |
    | 1 | 64f4 65c0 6428 668c | 4d0a 4cdc 4c70 4d76 | 81c2 7ebc 80c0 7fbe |
    | 2 | 6428 668c 64f4 65c0 | 4c3e 4da8 4c74 4d72 | 7fbe 80c0 7ebc 81c2 |
    | 3 | 65c0 64f4 668c 6428 | 4cdc 4d0a 4d76 4c70 | 7ebc 81c2 7fbe 80c0 |

    They are about 128 x the chroma terms' bias (204 x 128 = 0x6600, 2 x 129 x 128 = 0x8100) plus the dither.

## Result

`mmx5.py`, and then the port (`yuv410_to_rgba`), on libavcodec's planes against Intel's 24-bit frames: **bit-exact**
(mean 0.0000 per channel, maximum 0, 100% of pixels):
- intro.avi frames 300 and 800 (320x208);
- outro.avi frames 300 and 900;
- legal.avi frame 50 (640x480).

The same offset tables served every file and size. ctest's `native-movie` checks hand-worked values (white
(253, 255, 254), the next row's dither (255, 255, 252), red (255, 0, 4), the midpoint chroma (255, 217, 215)).

## Not covered

- How the converter builds the offset tables (read at run time here; constant across the three movies and two sizes).
  Settings the codec could expose (brightness, contrast, saturation) were left at their defaults.
- `DrawDibDraw`'s step onto the game's surface. If the display surface is 16-bit, the frame is reduced to it there;
  not traced.
- legal.avi frame 150 did not come out of the VfW test program (not investigated).
