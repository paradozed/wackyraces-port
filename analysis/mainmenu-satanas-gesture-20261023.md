# Satanas main-menu gesture

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

This note separates the serialized animation data from the inference used by
the native port. The source is the installed `menu.WGM`; the comparison target
is the supplied `analysis/wpc-mainmenu.png`.

## Resource observations

- `FILQ 0x7cf` has 17 sequence links. Index 12 links to `REEQ 0x79d`, which
  has 19 frames (`FRMQ 0x6d9` through `0x6eb`).
- These frames reuse Satanas's static torso, coat/legs, and shadow placements
  (`GMAT 0x3e1`, `0x3e2`, `0x3e3`) and add changing head and arm textures.
  The arm art uses materials `0x3ea`, `0x3ed`–`0x3f7`; the decoded frame sheet
  is `analysis/menu-satanas-gesture-20261023.png`.
- At 500 ms, the native preview's 16 ms tick approximation and speed 0.5
  select frame index 15 within the gesture's clamped frame range. The original
  engine tick duration is unknown, so this time-to-frame mapping is only a
  native preview result.
- The object anchor is `(66,60)`. Mapping it through the observed 2x menu
  viewport and adding each FRMQ coordinate places the frame-12 arm at screen
  `(101,218)`, with no extra fit or screenshot-derived artwork.

## Port behavior and inference

The serialized common scene contains three `GDFilm2` nodes selecting sequence
indices 0, 13, and 3. The 26-frame node explicitly selects index 3 and changes
Satanas's head. No serialized node explicitly selects index 12. The C port
decodes sequence 12 as an inferred startup gesture at the selected head node's
anchor. The supplied still reference shows the raised arm, while fresh Wine
captures show it lowered after the first menu snapshot. Six successive menu
captures, spaced by an Up/Down navigation pair, show the raised arm in the
first image and the static lowered pose in the following five. The sequence
sheet also ends in lowered poses. This supports playing sequence 12 once and
then returning to the explicitly serialized static composition; it still
does not recover the original trigger or exact start time.

The native renderer now clamps sequence 12 instead of looping and removes the
overlay after an estimated 1,064 ms: 19 frames at the serialized 0.5 speed and
a 28 ms gesture tick. A 30 fps private Wine capture shows the raised arm in
the first visible menu transition around 38.9 seconds and lowered by 39.97
seconds, bounding the visible pose to about 1.0-1.1 seconds. The tick is fitted
to that observation; WPC's timer origin and update interval remain unknown.
The captures and current native settled-pose comparison are
`analysis/menu-animation-times/wine-menu-contact.png`,
`analysis/menu-animation-times/wine-satanas-contact.png`, and
`analysis/menu-satanas-idle-comparison-20261023.png`. Road-car motion and
visibility remain unresolved.

The timed source capture is
`analysis/menu-animation-times/wine-mainmenu-entry-20261023.mp4`; it is a
six-second excerpt of a private 640x480 Xvfb recording at 30 fps. The launch
first displayed an `Anim Play Error` dialog, which was dismissed before
playback continued. The excerpt includes the intro movie and main-menu
transition. It is runtime evidence for the visible pose interval, not for the
internal WPC animation clock.

The rebuilt native binary was captured on a private 640x480 Xvfb display at
30 fps in `analysis/menu-animation-times/native-satanas-entry-20261023.mp4`.
Its first visible frame has the raised arm, and the sample about one second
later has the lowered pose. This confirms the fitted one-shot duration in the
native build. The Wine and native recordings begin at different process times,
so they do not establish a frame-for-frame shared clock.
