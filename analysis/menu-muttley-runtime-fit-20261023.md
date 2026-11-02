# Muttley main-menu animation fit

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

This note compares Muttley's menu animation with `FILQ 0x7cf` queue 15 and
records the current native timing fit. The queue selection and event trigger
are not serialized on the installed menu scene, so those parts remain
inferred.

## Wine capture observations

The source clip is
`menu-animation-times/wine-menu-muttley-sequence-20261023.mp4`,
captured at 30 fps on a private 640x480 Xvfb display. The main menu first
becomes visible around process time 38.87 seconds. A thresholded crop over
Muttley's mouth (x=500..549, y=324..329) finds two consecutive open-mouth
intervals when at least 90 pixels in the crop have all RGB channels below 55:

| Burst | Process time | Duration | Offset from menu appearance |
|---|---:|---:|---:|
| First | 46.400–47.933 s | 1.533 s | 7.533 s |
| Second | 49.067–50.200 s | 1.133 s | 10.200 s |

The timestamps are resolved to the 30 fps capture interval. The 13-second
clip contains no synchronized menu-start event; the visible menu transition
is the time reference.

## Queue timing match

The decoded queue has 109 frames. Queue 15 frames 16–37 and 54–69 contain the
two open-mouth poses. With the serialized speed 0.5 and a 35 ms animation
update, those groups predict:

- First burst: 22 frames × 2 updates per frame × 35 ms = 1.54 s.
- Second burst: 16 frames × 2 updates per frame × 35 ms = 1.12 s.
- Onset gap: 38 frames × 2 updates per frame × 35 ms = 2.66 s.
- First open-mouth pose: frame 16 appears 1.12 s after queue playback starts.

These durations and the onset gap match the Wine capture within one or two
30 fps samples. Subtracting 1.12 seconds from the first measured onset places
queue playback about 6.41 seconds after the menu first appears. This is a fit
to one runtime capture, not a recovered WPC timer or event trigger.

## Native implementation

The C port waits 6,410 ms after menu entry, then plays all 109 queue frames at
speed 0.5 with a 35 ms frame tick. A fresh native 30 fps capture showed bursts
at 7.467–9.000 and 10.100–11.267 seconds after menu appearance, closely
matching the initial Wine sample. The clip is
`menu-animation-times/native-menu-muttley-sequence-20261023.mp4`.

## Extended Wine capture and repeat behavior

The 100-second private Wine capture
`menu-animation-times/wine-mainmenu-long-startup-20261023.mp4`
reaches the main menu at process time about 42–44 seconds and remains there
until the attract demo begins around 71 seconds. During the menu, Muttley's
open-mouth poses recur twice. A mouth crop (x=500..549, y=324..329; RGB
maximum below 55 in at least 90 pixels) gives the following intervals:

| Occurrence | Process time | Duration |
|---|---:|---:|
| First burst, cycle 1 | 52.367–53.933 s | 1.567 s |
| Second burst, cycle 1 | 55.033–56.167 s | 1.133 s |
| First burst, cycle 2 | 60.300–61.867 s | 1.567 s |
| Second burst, cycle 2 | 63.000–64.100 s | 1.100 s |

The 30 fps intervals show the same ordering in both cycles: about 2.7 seconds
between burst onsets and about 7.9 seconds between repeated first bursts. This
supports replaying queue 15 with mode 2's wrap branch. The start offset differs
from the first capture, so 6,410 ms remains a fit rather than a recovered
trigger delay. Queue 15 is still selected by inference because serialized menu
object `0x21f` selects queue 13. The base 109-frame/35 ms loop is about 0.35
seconds short, so the native implementation adds a fitted static-pose pause.

The native renderer now loops queue 15 and restores the static queue-13 pose
for a 355 ms tail pause after each mode-2 pass. This pause fits the difference
between the predicted 7.595-second queue pass and the observed 7.95-second
cycle. A 25-second native capture
`menu-animation-times/native-menu-muttley-loop-refined-20261023.mp4`
measures repeated first-burst starts 7.966 seconds apart and repeated
second-burst starts 7.966 seconds apart. The Wine cycle measurements are
7.933 and 7.967 seconds. This confirms the repeat fit to within one 30 fps
frame. The initial trigger delay still differs between Wine launches and has
not been recovered from WPC state.
