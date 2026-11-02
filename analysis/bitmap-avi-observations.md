# Bitmap and AVI observations

These facts come from the installed files' headers and `ffprobe` output. They
describe container and stream metadata; they do not establish which runtime
scenarios display each file or how WPC's original renderer presents them.

## Bitmaps

`l2.bmp` and `l3.bmp` are Windows 3.x BMP files with 40-byte
`BITMAPINFOHEADER`s, a 54-byte pixel-data offset, 640x480 dimensions, one plane,
24 bits per pixel, and `BI_RGB` compression (value 0). With positive height,
the stored rows are bottom-up. Each row is 1,920 bytes, so no row padding is
needed. For this standard 24-bit BMP layout, pixels are stored as B, G, R byte
triples.

| File | File/declaration size | X/Y resolution fields | Pixel-array framing |
| --- | ---: | ---: | --- |
| `l2.bmp` | 921,656 bytes | 2,834 x 2,834 pixels/meter | 921,600 bytes of rows plus two trailing bytes |
| `l3.bmp` | 921,654 bytes | both zero | exactly 921,600 bytes of rows |

Both files declare a zero `biSizeImage`, which is permitted for uncompressed
`BI_RGB`. The two trailing bytes in `l2.bmp` are observed but their purpose is
unknown. The zero resolution metadata in `l3.bmp` differs from `l2.bmp`; it
does not change the pixel dimensions.

## AVI files

All listed AVIs are RIFF/AVI files. `ffprobe` identifies the following streams:

| File | Video | Audio | Duration / frame count |
| --- | --- | --- | --- |
| `legal.avi` | Indeo 5 (`IV50`), 640x480, 25 fps | PCM signed 16-bit little-endian, mono, 22,050 Hz | 6.000 s / 150 frames |
| `movies/intro.avi` | Indeo 5 (`IV50`), 320x208, 25 fps | none reported | 78.600 s / 1,965 frames |
| `movies/outro.avi` | Indeo 5 (`IV50`), 320x208, 25 fps | none reported | 55.240 s / 1,381 frames |
| `movies/redmax.AVI` | Cinepak (`cvid`), 320x208, 25 fps | none reported | 3.720 s / 93 frames |
| `Voice/iaudio.AVI` | none reported | PCM signed 16-bit little-endian, stereo, 22,050 Hz | 82.396 s |
| `Voice/oaudio.AVI` | none reported | PCM signed 16-bit little-endian, stereo, 22,050 Hz | 58.001 s |

FFmpeg reports `yuv410p` decoded video for the Indeo 5 files and `rgb24` for
the Cinepak file. The voice AVIs are audio-only despite being AVI containers;
their video dimensions are therefore not applicable. `iaudio.AVI` carries the
title `intro_fr.wav`; `oaudio.AVI` carries `Sound Forge 4.0 Audio`.

These metadata observations support the current `play` command's use of
`ffplay` for standalone playback, subject to the host FFmpeg build containing
the needed codec. They do not reconstruct WPC's AVIFile/DrawDib window
integration, audio synchronization, or movie-selection policy. Those paths
remain separate runtime-analysis work. No source assets were modified.

## Startup card timing and order

Static disassembly of `FUN_004acad0` (`analysis/decompilation/wpc/functions-036.c`)
calls the bitmap loader for `L2.bmp`, waits with `FUN_00413bd0(2)`, then loads
`L3.bmp` and waits the same way. `FUN_00413bd0` measures `timeGetTime()` until
the elapsed whole seconds reach its argument, so each card is presented for
two seconds. Both BMP sources are 640x480, and WPC sets its bitmap presentation
area to 640x480 before drawing them. After the second wait, this function
switches the movie presentation area to 320x240 before the intro path. That
value configures the offscreen DirectDraw surface. In the windowed path,
`FUN_004126e0` blits that surface to the screen rectangle initialized from
`GetClientRect`/`ClientToScreen` in `FUN_004123d0`. The observed WPC client area
is 640x480. With the intro's 320x208 source, WPC centers the frame in the
320x240 offscreen surface, then scales the surface to that client rectangle.
The native intro player now requests a 640x480 display area, matching this
windowed presentation geometry. The exact resampling/filtering and matched
Wine frame comparison remain open. The native card order and two-second
timing match the executable.

## Unresolved rendering formats

Menu `GMDL` geometry, selected `TGAF` indexed texture cases, and menu film
placement records are documented in the linked notes in `TODO.md`. The 32-bit
TGAF variant and parts of indexed-film decoding are still unresolved. The
available evidence here does not identify a separate track-file encoding;
race/world loading and `.pc` demo/save semantics remain open in
`analysis/pc-formats.md` and the TODO list.
