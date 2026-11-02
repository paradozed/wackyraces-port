# Main-menu text alignment audit

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

This audit follows the user's report that letters appeared alternately high
and low in a tiled Wine/native screenshot.

## Cropped user screenshot

The user's Wine image `/tmp/codex-clipboard-0scEIS.png` is 641x506 and includes
the Wine Desktop title bar. Removing 30 rows leaves a 640x476 game image, so it
cannot serve as an exact one-pixel reference. Compared against the then-saved
native 640x480 frame, all eight labels shared one best translation (move the
native mask 2 pixels left and 1 pixel down). There was no alternating
per-row displacement. The visual crop is `menu-user-vs-native-crop.png`.

## Controlled 640x480 comparison

To remove the title-bar and width ambiguity, the current native build was
launched in a private 640x480 Xvfb display and captured at
`native-mainmenu-current-20261023.png`. It was compared with the existing
640x480 Wine capture `wine-mainmenu-20261022.png`; `DEMARRER` is selected in
both. The first current-build capture showed one common offset: its text was
one pixel left and two pixels above the Wine text. The menu renderer's
centered X origin was shifted by one pixel and its former two-pixel vertical
adjustment was removed.

After rebuilding and recapturing, thresholded masks for all eight labels have
their best overlap at `(dx=0, dy=0)`. IoU ranges from 0.877 to 0.934. The
side-by-side result is `menu-current-capture-comparison-20261023.png`. This
fixes the shared text placement difference and shows no reason to change
individual glyph bearings. The earlier connected-component audit found
zero-pixel vertical shifts for all matched components in seven rows and 27 of
28 components in the long introduction row.

The road and character animation phases differ between the two images, so the
comparison only validates label placement. See `menu-text-properties.md` for
the recovered cursor positions, glyph bearings, and the previous comparison.

## Follow-up on the tiled desktop screenshot

The later side-by-side desktop image has a title bar, unequal client heights,
and different selected rows (`DEMARRER` in Wine and `QUITTER` in native). Those
conditions make apparent glyph-height differences unreliable. The equal-size
controlled pair above is the placement reference: all eight labels match at
zero translation, so no per-row or per-glyph Y offsets are justified by the
tiled image.

## Latest Wine/native desktop comparison

The later user capture `codex-clipboard-MmupwN.png` has both menu clients
visible at the same vertical origin. The native window is clipped by the
right edge of the screenshot (638 visible client pixels versus 640 for Wine),
and the selected rows differ, so its mask comparison is approximate. The
seven unobstructed common labels show a small shared residual, but this tiled
image is approximate because the native client is clipped and selection
states differ. It is superseded by the controlled same-size comparison and
does not justify changing individual glyph bearings. The current renderer
uses that comparison's placement: centered X plus one pixel and captured
cursor Y directly.

A thresholded grayscale-mask search over the eight row bands in this tiled
capture puts the native labels roughly 1–3 pixels to the right and 2–3 pixels
lower than Wine. The per-row optima vary by at most two pixels, and the masks
have low overlap because the image has different selected rows, antialiasing,
and animated-character overlap. This is consistent with a small viewport or
capture-origin difference; it is not evidence for per-letter baseline
corrections. The controlled 640x480 pair remains the source for renderer
coordinates.

A rebuild check on 2026-10-23 found the current sources already up to date. A
fresh native capture could not be started because another `wpc-native`
instance held the program's per-user single-instance lock. That instance was
left running. At that point, the saved equal-size capture remained the only
verification; the isolated current-source capture below supersedes that
limitation.

## Fresh capture of the current renderer

The active desktop's native window was confirmed to be an older executable
instance by comparing its mapped executable inode and start time with the
current `c-port/wpc-native`. I left it running. To verify the current renderer,
I built a temporary copy of the current C sources with only its lock filename
changed, then launched that copy in a private 640x480 Xvfb display. The fresh
frame is `native-mainmenu-current-isolated-20261023.png`; the raised Satanas
hand is visible in this build.

I compared the seven unobstructed label rows against the 640x480 Wine capture
`wine-mainmenu-20261022.png`. After grayscale thresholding at 65%, each row's
best translation over a +/-3-pixel search was `(0,0)`: `DEMARRER` (155 differing
pixels), `OPTIONS` (107), `CHARGER PARTIE` (206), `SAUVEGARDER PARTIE` (249),
`DEMO` (55), `CREDITS` (131), and `QUITTER` (108). The varying residual pixels
are shape/antialias differences; they do not improve at any nonzero shift.
The long `SEQUENCE D'INTRODUCTION` row was omitted from this fresh metric
because the character overlaps its left edge. Thus the current renderer's
global text placement is verified; road timing/visibility and the long-row
overlap still need a synchronized original capture.

## Fresh review capture

After the latest tiled comparison, I rebuilt with `make -C c-port` and
launched the current executable in a private 640x480 Xvfb display with
`--skip-intro`. The new frame is `native-mainmenu-review-20261023.png`; the
side-by-side Wine/native image is `menu-current-review-comparison-20261023.png`.
Both have `DEMARRER` selected, so the comparison is not affected by the yellow
selection color. A fresh threshold-mask search over each label row, allowing
translations from -3 to +3 pixels, again prefers `(dx=0,dy=0)` for all eight
labels. Row IoU ranges from 0.734 to 0.921; the lower values reflect raster
shape/color differences and do not improve at a shifted position. The long
introduction row overlaps the characters in both images. The dash seen before
`DEMO` in the earlier tiled screenshot is absent from this current-build
capture. No per-letter or per-row Y correction is supported by this capture.

The two existing desktop instances were left running. The isolated run used a
separate runtime lock and X server. Menu audio was unavailable because the
machine has no ALSA default device; that does not affect this graphics review.

## Recheck of the latest supplied desktop image

I cropped the user's `codex-clipboard-MmupwN.png` into its visible Wine and
native clients. It is 1278x509 overall; after the 30-pixel title bars, the
Wine client is 640x479 and only 638 pixels of the native client are present.
The selected rows differ (`DEMARRER` versus `QUITTER`), so the screenshot still
cannot establish per-glyph vertical offsets. The WGM inspector independently
resolves all eight page-4 strings; the `DEMO` entry is exactly `DEMO`, with no
leading dash. Thus the dash visible before it in this desktop image is a
separate rendered element, not a text-string or glyph-bearing error. The
current-source controlled 640x480 comparison remains the reliable text
placement check; no coordinate change is justified by this cropped image.

## Fresh matched-state capture

I relaunched Wine and the current native executable on separate private
640x480 Xvfb displays. Both frames have `DEMARRER` selected and are captured at
the full client size, without desktop title bars or clipping. The frames are
`wine-mainmenu-followup-20261023.png` and
`native-mainmenu-followup-20261023.png`; the side-by-side image is
`menu-followup-comparison-20261023.png`.

A bright-font mask search over each of the eight row bands, allowing shifts
from -3 to +3 pixels, again has its best score at `(0,0)` for every row. The
long introduction row and footer contain animated character/background
content, so their mask overlap is not a clean font-shape metric. The controlled
frames confirm that page-4 text anchors and vertical bearings need no further
coordinate adjustment. Road positions and character animation phases still
differ between these unsynchronized single frames. A separate six-frame Wine
sample shows Satanas's extended arm in the first captured menu frame and the
lowered pose in the next five. A new private Wine capture at 30 fps shows the
arm raised through about 39.93 seconds and lowered by 39.97 seconds, around
one second after the menu first appears. The excerpt is preserved in
`menu-animation-times/wine-mainmenu-entry-20261023.mp4`. A rebuilt native
capture in `menu-animation-times/native-satanas-entry-20261023.mp4` shows the
raised pose on its first visible frame and the settled pose about one second
later. The native one-shot now lasts an estimated 1,064 ms using a fitted 28 ms
gesture tick. Its trigger and timer origin remain unknown. The isolated Wine
launch used the prepared GStreamer plugins and reached the menu; the native
launch used a private runtime lock and left the two existing desktop instances
running.

## Current-source isolated render

On 2026-10-23, the current `c-port/wpc-native` binary was launched with
`--skip-intro` on a private 640x480 Xvfb display. The capture is
`native-mainmenu-current-session.png`. It shows `DEMARRER` selected and the
current WGM glyph rendering at the recovered page-4 cursor positions. SDL
could not open the host's default ALSA device, so menu audio was disabled for
this graphics capture. This confirms the current build's layout but does not
provide new matched-state Wine evidence for changing the coordinates.

## Current-binary check

The current executable was launched in a private 640x480 Xvfb display with
`--skip-intro` and captured as `native-mainmenu-current-check-20261023.png`.
`DEMARRER` is selected, and the extra dash before `DEMO` seen in the user's
tiled screenshot is absent. At capture time, the two native desktop processes
(PIDs 172635 and 199174) mapped older executable inodes than the current
`c-port/wpc-native`; neither was stopped. The supplied tiled view therefore
does not represent the current binary. The fresh frame uses the same recovered
page-4 anchors and glyph bearings. Existing matched-state mask comparisons
still put all eight rows at zero translation, so there is no evidence for a
per-row or per-letter coordinate patch.

## Recheck of the current source binary

On 2026-10-23, I rebuilt with `make -C c-port` (already up to date) and
launched the current `c-port/wpc-native` in a private 640x480 Xvfb display
using a separate `XDG_RUNTIME_DIR`. The fresh frame is
`native-mainmenu-verified-20261023.png`. It matches the same-size Wine frame
`wine-mainmenu-followup-20261023.png` in client dimensions and selected row
(`DEMARRER`). The side-by-side image is
`menu-verified-comparison-20261023.png`. Visual inspection shows the current
native text using the same row anchors and glyph bearings, with no new shared
baseline shift. The older desktop processes were left untouched. This recheck
does not justify changing page-4 coordinates. For a new visual discrepancy,
capture both programs at 640x480 with the same selected row and compare the
client frames before changing the renderer.

## Isolated current-source launch

I rebuilt with `make -C c-port` (no compilation was needed) and launched the
current binary on a private 640x480 Xvfb display with `--skip-intro` and a
private runtime lock. The new capture,
`native-mainmenu-live-20261023.png`, has
`DEMARRER` selected. All eight labels render from the current source, and
there is no extra dash before `DEMO`. Their row cursors and apparent glyph
heights match the already measured layout; no source-coordinate correction is
supported. A new GDB-instrumented Wine run did not reach a rendered menu after
the `Anim Play Error` dialog, so it adds no runtime evidence and is not used
for this conclusion. The existing same-size, same-selection Wine/native pair
remains the valid placement comparison. Continue menu work with sprite/color
and animation differences, not a speculative text baseline patch.

## Live desktop recheck

On 2026-10-23, the two native desktop processes were still mapped to deleted
executables from 06:38 and 10:46; the current `c-port/wpc-native` was built at
16:44. I launched that current binary in a third window with `--skip-intro`
and captured its 640x480 client as
`native-mainmenu-live-current-20261023.png`.
The current build selects `DEMARRER`, shows no stray mark before `DEMO`, and
uses the measured row positions and font bearings. This explains the latest
tiled screenshot's extra dash and selected `QUITTER`: it came from an older
native process, so it is not a valid review of the current sources. The two
older windows were left running. The current launch has no ALSA default audio
device, so menu audio is disabled in this session.

## Current-turn exact-size recheck

After the latest request to act on the reported letter offsets, I rebuilt with
`make -C c-port` (the current build was already up to date) and launched that
binary in a fresh 640x480 Xvfb display with `--skip-intro`. The capture is
`native-mainmenu-current-turn.png`. It starts with `DEMARRER` selected, matching
`wine-mainmenu-followup-20261023.png`; all eight label rows visually retain the
same cursor anchors and baseline placement as the reference. This confirms the
earlier mask audit for the current binary. In the fresh frame the visible
differences are in road-car phase and character pose, not a shared or
alternating menu-text offset. No text-coordinate or per-glyph correction is
supported. The attempted new Wine launch did not produce a usable frame because
Wine's prefix startup timed out with RPC and graphics initialization errors,
so the existing controlled Wine frame remains the reference.

## Runtime background-color correction

The Wine runtime samples at `(20,220)` in `wine-mainmenu-followup-20261023.png`
and all twelve frames of `menu-animation-times/wine-road-6s.mkv` measure
`RGB(25,53,90)`. This differs from the user-provided reference image's
`RGB(16,52,90)`, which the old native renderer had used. I updated the main
page clear to the runtime value, rebuilt, and captured
`native-mainmenu-background-verified.png`; its pixel at `(20,220)` is exactly
`RGB(25,53,90)`. The supplied image remains a visual reference only.

## Selection-color sampling

I sampled the selected `DEMARRER` row in the six-second Wine road capture at
its twelve 500 ms frames. Every sample has the same 1,356 yellow-mask pixels
in the row and an average selected-pixel RGB of `(217,216,0)`. This shows no
slow highlight pulse in that capture. It does not measure selection-entry
timing or rule out color changes faster than the half-second sample interval;
the native renderer's fixed selected color therefore remains provisional.
