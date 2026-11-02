# `.snd` audio observations

## Installed file headers

The installed audio files use no recognized RIFF/WAVE container header. The
following uniform first-word pairs were observed directly in all files:

| Directory / subset | Count | First little-endian u16 | Second little-endian u16 |
| --- | ---: | ---: | ---: |
| `Music/*.snd` | 20 | `0x0101` | `44100` |
| `Voice/*.snd` (most) | 266 | `0x0100` | `22050` |
| `Voice/vo-N-900.snd` through `vo-N-903.snd` | 4 | `0x0100` | `44100` |

All 20 music files are about 7.6–8.9 MB (7,958,532–9,285,124 bytes). The
voice files range from 4,788 to 56,411 bytes. The executable identifies the fields: byte 0 stores channel count minus one
(0=mono, nonzero=stereo), byte 1 enables IMA ADPCM when nonzero, and bytes 2–3
are a little-endian sample rate. Thus the installed music files are stereo
IMA ADPCM at 44,100 Hz; the common voice files are mono IMA ADPCM at 22,050
Hz. `ffprobe` does not recognize `.snd` because it is a game-specific raw
stream, not a self-describing media container.

The voice examples contain long zero-filled regions after the four-byte
prefix, while music samples show varied nonzero data immediately after the
prefix. These are payload observations; framing and codec behavior are
established from the executable code below.

## Executable voice-name lookup

Static disassembly of `wpc.exe` around `0x00426e0c` shows a lookup call to
`0x00404a6b`, followed by formatting a path with the literal `Voice/%s`.
The lookup has explicit special cases returning `vo-N-900.snd` through
`vo-N-903.snd`; a later branch reads a 7-by-7 indexed table at
`0x00527d20` and formats a `vo-` prefix. This supports a runtime path from
some numeric/object inputs to voice filenames. The meaning of all lookup
indices and the association between those indices and game events are not
fully recovered. It does not by itself prove which files are used in a
particular scenario.

The recovered lookup table has 196 entries for 49 raw argument pairs, with four
table entries per pair. These yield 192 unique table basenames, 158 of which
exist under the constructed `Voice/vo-*.snd` paths; 34 unique table basenames
are absent. The four special `vo-N-900.snd` through `vo-N-903.snd` names are
outside the table and installed. Thus 162 unique statically named paths match
the directory. Raw keys and per-entry filenames are in
`analysis/voice-sound-map.tsv`; their event meanings remain unknown. This is a
static lookup/data inventory, not a complete runtime event map.

## Music stem selection

Static disassembly around VA `0x00431690` builds paths under the literal
`music\\` and probes suffixes `_t1`, `_t2`, `_t3`, `_t4`, and `_b`. The
function compares a requested music key against embedded groups including
`Cc`, `Ct`, `Dt`, and `Sh`, selects the corresponding embedded stem basename,
and adds a stem to its local list when the matching check succeeds. The installed
files contain those four five-file groups, consistent with the observed names.
The routine assembles one path buffer and calls the audio-object setup method
once, so these suffix checks do not start all five files together in one
invocation. Other calls could still create concurrent streams. This is static
evidence for name resolution, but does not establish the caller's state,
cross-call mixing, gain, synchronization, or transition behavior. The audio
stream's whole-file loop is established separately by the timed callback
described above.

An earlier music-file inventory found all 20 installed stems use marker
`0x0101` and rate 44,100, with four groups
(`Cc`, `Ct`, `Dt`, `Sh`) and stems `_t1` through `_t4` plus `_b`. Its duration
estimates range from about 180.5 to 210.5 seconds when interpreting marker
`0x0101` as stereo IMA data with one frame per payload byte. These are estimates
under the exploratory decoder assumption, not verified playback durations. The
inventory also recorded missing stems in incomplete groups.

## Executable decoder evidence

Static disassembly contains the 89-entry IMA ADPCM step table at file offset
`0x125030` (VA `0x00525030`) and the 16-entry index adjustment table at
`0x124ff0` (VA `0x00524ff0`). The decoder at VA `0x0040bdd0` reads the high
nibble first, clamps the signed 16-bit predictor and clamps the step index to
0–88.

The stream constructor at VA `0x00428790` reads the four-byte header in order:
channel-count-minus-one, ADPCM flag, then little-endian sample rate. It creates
signed 16-bit PCM output with the indicated channel count and rate. In the
compressed callback at `0x00428ca0`, mono bytes yield two consecutive samples
(high nibble then low); stereo bytes yield one interleaved frame (high nibble
channel 0, low nibble channel 1). Each channel has a three-byte predictor/index
state initialized to zero. The timed callback at `0x00428dc0` advances the
compressed-stream offset and wraps it to zero at the stored stream boundary;
the wrap path seeks back and clears the decoder state at `+0x44` and `+0x47`
before decoding from the beginning. This establishes whole-stream looping
with a predictor reset, rather than a repeated PCM tail. The audio is streamed
in buffers by a 500 ms multimedia timer. This settles the installed `.snd`
framing, channel mapping, nibble order, initial state, and loop boundary from
executable code rather than byte-pattern inference.

The native C port decodes `.snd` streams as mono or stereo IMA ADPCM into
bounded PCM memory. Its shared SDL mixer can combine the menu music with PCM
effects, resampling clips to 44,100 Hz stereo. The menu plays
`Music/Cc_t1.snd`, which the original runtime trace shows being read while the
menu is active. The port loops the decoded PCM; this matches the original
whole-stream wrap and zeroed predictor state at the loop boundary. On race
entry, the port selects the matching `Cc/Ct/Dt/Sh_tN` stream when installed
and restores `Cc_t1` on return. The first-race file trace
confirms the `cc_t1` basename pair; extending it to the other installed groups
and variants follows the executable track table. `FUN_00431690` also contains
the `Dr` fallback names `Cc_t1`, `Sh_t1`, `Dt_t1`, and `Ct_t1` corresponding
to suffixes t1 through t4; the port applies that mapping. Whether WPC uses
those aliases only for `Dr` fallback resolution is established for this
routine; separate invocations may still create concurrent or state-dependent
streams. The `_b` policy also remains unknown. The path construction and exact
fallback strings are recorded in
[`disassembly/wpc-music-path-selection.md`](disassembly/wpc-music-path-selection.md).
The menu sample bank `Menu.GSW` is a separate PCM format and is also loaded by
the port.

The two installed audio-only AVI references are PCM signed 16-bit little-endian,
22,050 Hz stereo. `Voice/iaudio.AVI` is 82.396 seconds and carries the title
`intro_fr.wav`; `Voice/oaudio.AVI` is 58.001 seconds and carries the title
`Sound Forge 4.0 Audio`. With the executable-confirmed header and decoder, special files
`vo-N-900.snd` through `vo-N-903.snd` decode to roughly 0.217–0.598 seconds at
44,100 Hz mono. Their duration and format differ from the full AVI tracks; this
rules out a one-file/one-track comparison but not a match to a short segment.
An exploratory sample comparison converted each AVI to 11,025 Hz mono signed
16-bit PCM with ffmpeg, and decoded the four special `.snd` files with the
executable-confirmed decode rules before applying similar resampling and
downmixing.
Normalized sliding cross-correlation found the strongest pair at
`vo-N-901.snd` against `oaudio.AVI`, at about 21.51 seconds with coefficient
0.545. The other seven pairings peaked between absolute coefficients 0.17 and
0.40. This is suggestive of a short audio relationship, but does not establish
that the decoded clip is present in the AVI: the comparison uses an assumed
decoder state/channel layout, and correlation can be elevated by similar
speech or music. Verification against the original runtime output or a clear
waveform/listening match remains open. Temporary conversions and the
correlation utility were kept under `/tmp`.

## Still unresolved

- Recover the music caller state and determine whether separate invocations
  create concurrent streams; measure their gain, synchronization, and stop
  behavior. `FUN_00431690` builds one path and configures one stream per call,
  but other engine calls may create more streams.
- Recover the exact menu sound trigger event. `menu.WGM` attaches GSMP `0x80000065` and `0x80000066` to menu items, but `WMenuSound` uses the base `GDSound` virtual table; current static evidence does not identify whether playback starts on focus, activation, or another dispatched event. The native menu reloads page-local links for text-backed pages, matches them by owning `WMenuItem` ID, and plays them on selection as a provisional trigger. Pages using static label tables are not yet mapped to object IDs. The disposable Wine sound-event capture failed before game startup (`wineserver: bind: Operation not permitted`), so the original event and timing remain uncaptured.
- Map lookup inputs to voice events and reconcile missing referenced assets.
- Verify the exploratory `vo-N-901.snd` / `oaudio.AVI` correlation against
  runtime playback or a stronger decoded waveform match.
