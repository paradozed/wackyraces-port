# Wine screen-capture attempt

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

On 2026-10-21, a 35-second X11 desktop capture was recorded at 5 fps while
launching the installed `wpc.exe` with Wine 11.20, the user-local GStreamer
`gst-libav` plugin, and software GL. The executable was started with its full
Windows path as documented in `README.md` and was bounded to 30 seconds.

The extracted frame at 20 seconds is entirely black. The video file is at
`/tmp/wpc-current-capture.mp4` and the inspected frame is at
`/tmp/wpc-current-frame.png`; both are temporary and outside the game
directory. This attempt did not provide a usable original menu frame and
therefore does not confirm the menu state or validate native C renderer placement.
Earlier notes of reaching the main menu under Wine remain separate evidence.

## 2026-10-22 follow-up

A fresh capture was attempted from an isolated copy of the Wine prefix. The
prefix originally had `D:` mapped to the absent mount path
`/run/media/nicolas/Wacky`, with its device link pointing to the absent
`/dev/loop0`. Launching a copied game directory therefore displayed the
French message asking for the Wacky Races CD-ROM.

Static disassembly confirms the startup check: routine `0x004abaf0` calls
`GetLogicalDriveStringsA`, filters roots using `GetDriveTypeA`, and retains
only drives returning 5 (`DRIVE_CDROM`). If none are found it displays the
localized error and exits. The selected CD-ROM root is retained for subsequent
startup work. The focused listing is in
`analysis/disassembly/wpc-cdrom-check.txt`.

For a second attempt, a temporary prefix copy was configured with Wine's
`D:` drive type set to `cdrom`, and that drive was pointed at the installed
game directory. The copied prefix's original drive links were restored after
the run; the installed prefix, executable, and game assets were not modified.
With the same Wine 11.20, GStreamer Indeo 5 plugin, and software GL settings,
the process stayed alive until the 35-second timeout but the captured display
was black. The capture video is temporary at
`/tmp/wpc-menu-capture/cdrom-configured.mkv`; the extracted black frame is
preserved as `analysis/wine-cdrom-configured-black-frame.png`. This shows the CD-ROM presence gate
was passed, but does not establish that the menu rendered or that media checks
passed beyond drive-type selection.

The original-frame comparison remains open. The prior note about a successful
Wine main-menu run is preserved as separate evidence; this capture did not
reproduce a visible menu.

## 2026-10-22 isolated runtime pass

### Reproduction setup

Wine 11.20 was used with a temporary default WoW64 prefix at
`/tmp/wpc-runtime-20261022/prefix`. This Wine build rejects
`WINEARCH=win32` (`WINEARCH is set to 'win32' but this is not supported in
wow64 mode`), so the fresh profile is WoW64. A copy of the installed game
files was placed under `/tmp/wpc-runtime-20261022/clean-install`; the
installed executable and assets were not changed. The profile's `D:` points
to the mounted ISO at `/run/media/nicolas/Wacky` and is marked as a CD-ROM.
The initial profile had no Wacky Races registry key and displayed the
captured `analysis/wine-setup-required-20261022.png` dialog, “Please run Wacky
setup”.

To proceed past that check, only the installed profile's
`HKLM\\Software\\Wow6432Node\\Infogrames\\Wacky Races\\1.00.000` values
were exported and imported into the temporary profile. Observed values were
`LANG=3` and binary `Prefs` (the latter includes the renderer/device text
`NVIDIA GeForce RTX 4070`). This means the successful menu run is an isolated
copy with the installed configuration, rather than a zero-configuration
profile. The clean-profile failure and configured run are separate results.

Run from `/tmp/wpc-runtime-20261022/clean-install`:

```sh
GST_PLUGIN_PATH=/tmp/wpc-menu-capture/gst-plugins-good/usr/lib/gstreamer-1.0:/tmp/wpc-menu-capture/gst-libav/usr/lib/gstreamer-1.0 \
GST_REGISTRY=/tmp/wpc-runtime-20261022/fresh-registry.bin \
LIBGL_ALWAYS_SOFTWARE=1 \
WINEDEBUG=+loaddll,+file \
WINEPREFIX=/tmp/wpc-runtime-20261022/prefix \
wine wpc.exe
```

The plugin archives were unpacked under `/tmp/wpc-menu-capture` for the run;
the plugin path supplies the AVI demuxer and Indeo 5 decoder. Wine's window
was 1920x1080 on the X11 desktop, while the game's 640x480 picture appeared
at the upper-left with the rest black. Captures were made from the WackyPC
window by X11 window ID because desktop-wide capture did not include its
DirectDraw surface. `analysis/wine-mainmenu-clean-20261022.png` is a direct
runtime capture; the logical 640x480 area visibly contains the animated
banner, characters, French menu text, highlighted selection, and keyboard
hints. The supplied `analysis/wpc-mainmenu.png` remains a separate reference.

### Observed runtime behavior

- The disc gate passes when the mounted image is exposed as CD-ROM `D:`.
  WPC reads `D:\\WID.ID` and `D:\\Movies\\intro.avi`.
- WPC also reads `legal.avi`, `Voice\\iaudio.avi`, `L2.bmp`, `L3.bmp`,
  `MENU.GSW`, `menu.WGM`, and `D:\\music\\Cc_t1.snd`. Note that the two menu
  file names were preserved with their exact on-disk capitalization in the
  Linux-backed temporary install; a prior incomplete copy lacked `menu.WGM`
  and did not reach the menu.
- The intro movie advances, and Escape skips onward to the French main menu.
  The menu shows `DEMARRER`, `OPTIONS`, `CHARGER PARTIE`,
  `SAUVEGARDER PARTIE`, `DEMO`, `SEQUENCE D’INTRODUCTION`, `CREDITS`, and
  `QUITTER`. The screen hints show arrow-key selection, Enter acceptance,
  and Escape return/skip behavior. Down plus Enter opened the options page.
- The options page visibly contains player, effects/music/voice, brightness,
  weather, and dust-related rows, but Wine's rendering leaves overlapping
  text trails. `analysis/wine-options-clean-20261022.png` records the state;
  exact values and adjustment timing cannot be read reliably from this frame.
- Entering the start flow displayed `CHOISIR SAUVEGARDE` and `LIBRE` options.
  A later transition left the game window stale and WPC busy at roughly one
  CPU core. This is a Wine/runtime rendering hang after menu navigation, not
  evidence of a successful race, pause, post-race, save, or load operation.
- `WINEDEBUG=+loaddll,+file` shows WPC loading Wine's `DDRAW`, `DINPUT`,
  `DSOUND`, `AVIFIL32`, `MSVFW32`, `winegstreamer`, and `ir50_32` modules;
  OpenGL/WineD3D modules also load. Logs include repeated D3D pixel-format
  fallback errors, DirectDraw Flip flag warnings, and the AVIFileExit stub.
  These are Wine diagnostics; the observed menu proves the run did make it
  beyond them.
- No newly created game save or other game data file was found in the
  temporary C: profile after this pass. The start flow was not completed, so
  this does not establish when a normal save is written. The only game
  configuration introduced was the imported registry key.

### Behavior matrix and limits

`analysis/runtime-behavior-matrix.md` links these observations to repeatable
scenarios and records what this Wine session could not establish. In
particular, the renderer option matrix, controller configuration and input
timing beyond the reported Escape pause, audible volume/playback controls,
pause-menu actions, save/load acceptance, and normal shutdown still require
further observation in the original runtime. Static menu structures and
resource names do not substitute for those observations.

## 2026-10-22 mounted-disc follow-up

The user mounted the game ISO at `/run/media/nicolas/Wacky/`; the installed
Wine prefix's `D:` drive now points to that mount, and the device link resolves
to `/dev/loop0`. This supplies the original disc files to the executable.

Wine 11.20 was launched on the real desktop with software GL and the host
GStreamer Indeo 5 decoder. The host did not have the matching AVI demuxer
plugin in the system plugin directory, so the `gst-plugins-good` archive was
downloaded and unpacked under `/tmp/wpc-menu-capture` for this run only. The
combined plugin paths provided `avidemux` and `avdec_indeo5`; a standalone
GStreamer pipeline decoded `Movies/intro.avi`, and Spectacle captures of the
active Wine window at 20 and 35 seconds showed frames from the game's intro.
This is runtime evidence that disc discovery and AVI playback proceed beyond
the earlier CD-ROM gate. The isolated plugin extraction did not change system
packages or game files.

A later 110-second attempt dismissed the license prompt and captured additional
frames, but the terminal regained foreground focus while it was running. The
later `--activewindow` samples therefore captured the terminal rather than
Wacky Races. The attempted run is not evidence that the game failed to reach
the menu. A main-menu screenshot at `analysis/wpc-mainmenu.png` was supplied by
the user separately; it shows the French labels resolving from language 3 and
matches the eight text nodes under page object `0x4`. It is a visual reference,
not a frame captured from this Wine session. The renderer's page composition,
texture decoding, and text placement remain unvalidated against an original
runtime frame.

## 2026-10-22 first race capture

The user confirmed that the active WackyPC session had entered the first race
and that they were playing. Passive Spectacle active-window captures were taken
without sending input. The preserved frame
`wine-first-race-20261022.png` is the cropped
640x480 game viewport; the full 1920x1080 window capture is
`wine-first-race-window-20261022.png`.
The frame shows a desert race scene with the HUD at lap `1/3`, position
`7 EME`, and timer `02:04`. Other visible HUD elements include two upper-right
icon panels, a lower-right colored gauge, and a dotted strip with racer icons
along the bottom. Their exact meanings are not established. Two additional
captures a few seconds apart show changes in scenery and racer positions,
consistent with an advancing race scene.

The user supplied a close-up of the HUD's lower strip at
`hud-race-progress-map-20261022.png`
(482x85 pixels). It shows a horizontal row of small dots, larger dots at both
ends, and seven distinct racer/car markers above the row. The icons are
distributed unevenly: several are grouped toward the left, with others farther
along the strip. This looks like a linear race-progress display, but the
interpretation of the endpoints, direction, spacing, and each marker's exact
semantics has not been confirmed. It depicts no course turns, terrain, or
branching route in this crop.

The accompanying [`first-race-file-trace.txt`](first-race-file-trace.txt)
preserves selected `+file` trace lines. WPC opened
`Voice\\loadingpc1024.bmp` and `D:\\cc\\cc_t1.wgm`, confirming that the `cc_t1`
world archive was loaded in the captured session. The screenshot and the
user's confirmation establish active gameplay, but do not yet establish the
identity of the course, input mapping, collision rules, pickup effects,
lap-crossing condition, or finish sequence.

## Water hazard frame

The user supplied `wine-first-race-water-20261022.png`
as another frame from the race. The HUD reads lap `2/3`, position `7 EME`, and
timer `04:42`. Water fills the foreground, and a bright circular ripple/swirl
is visible near the racer marker on the bottom HUD strip. The user reports
that the character drowns when going into the water. This frame records the
water scene but does not show the entry or the full drowning/reset sequence;
the consequence is therefore a user observation, not a sequence independently
verified from this still image. The user further reports that the player
returns automatically to the race map afterward. The exact timing, any time
penalty, and whether control is locked during the return have not been
captured.

## Player controls reported during the first race

The user reports this keyboard mapping during the race:

| Key | Reported effect |
|---|---|
| Up arrow | Accelerates |
| Down arrow | Decelerates |
| Left arrow | Turns the car left |
| Right arrow | Turns the car right |
| Right Shift | Triggers the power-up |

These are player observations, not yet matched to a recorded input/video
sequence. For the character whose super gadget is Wings, the user reports that
activating the power-up starts an animation and the vehicle flies. The flight
duration, any cooldown or stock limit, and the Dragoon weapon effect remain
unknown.

## Turn-direction cue

The user supplied `wine-first-race-turn-arrow-20261022.png`
as another race frame. It shows lap `2/3`, position `7 EME`, and timer `08:20`,
with a large arrow marker above the road ahead of the player's vehicle. The
user reports that this arrow appears as a turn approaches and indicates which
direction to take. The image supports the marker's placement in the forward
view; the exact distance before the turn, update timing, and steering control
mapping have not been recorded.

## Race finish and post-finish camera

The user reports that finishing the race brings up an ordered list of
participants. The supplied frame
`wine-race-finish-standings-20261022.png`
shows these entries in order:

| Place | On-screen entry |
|---:|---|
| 1 | `DEMONE DOUBLE-ZERO` |
| 2 | `TURBO TERRIFIQUE` |
| 3 | `CARAVERNE` |
| 4 | `COMPACT PUSSYCAT` |
| 5 | `DINGO-LIMOUSINE` |
| 6 | `TOCARD TANK` |
| 7 | `BOLIDE ECARLATE` |

The user identifies `DINGO-LIMOUSINE` as the player's vehicle and reports that
its entry blinks. They further report that the AI takes over afterward and the
camera switches to `DEMO` mode while following the same character/vehicle. The
frame itself shows the ordered list over the race scene; blinking and the
subsequent AI/demo-camera behavior are user-reported observations. The screen
instructs `Pour continuer presser entrée` (“Press Enter to continue”), and the
user confirms Enter proceeds. This opens a separate next-track screen. The
preserved capture `wine-next-track-20261022.png`
reads `PISTE SUIVANTE`, names the next course `LE GRANG CANYON 1`, and again
displays `APPUYER SUR ENTREE` (“Press Enter”). The user confirms Enter is
required to continue from this screen. In the following Grand Canyon race,
the user reports finishing in third place. After Enter, the game advanced to
another next-track screen. The supplied capture
`wine-next-track-happy-hills-20261022.png`
shows `PISTE SUIVANTE`, `LES COLLINES JOYEUSES 1`, and `APPUYER SUR ENTREE`.
This confirms advancement to the next named course after a third-place finish;
the next gameplay state has not yet been observed. Whether other keys advance
or results are saved remains unknown.

The user also reports that the Grand Canyon course appeared to restart with
different weather before this third-place finish. This supports that weather
can vary between observed course runs. Why the course appeared to restart is
still unconfirmed; the later advance to `LES COLLINES JOYEUSES 1` after third
place shows that winning was not necessary for this observed progression.

## In-race pause menu

The user reports pressing Escape during `LES COLLINES JOYEUSES 1` and says
this pauses the game. The supplied capture
`wine-race-pause-happy-hills-20261023.png`
shows the race HUD behind a menu. The HUD reads lap `1/3`, position `7 EME`,
and timer `00:00`. Visible menu entries are `CONTINUER`, `EFFETS`, `MUSIQUE`,
`VOIX`, `RETOUR DE FORCE ACTIVE`, `REDEMARRER CHAMPIONNAT`, and `QUITTER`.
Slider marks appear beside the effects, music, and voice rows. This capture
establishes the menu labels and visible state only: navigation, slider values,
the meaning of `RETOUR DE FORCE`, and the effects of restarting or quitting
have not been exercised. The `00:00` timer may mean the race was paused before
the start or the timer display had not advanced; its significance is unknown.

The native C port now opens a seven-row pause overlay on Escape, dims its text
with a black offset, and draws the three slider rows with WGM font glyphs.
Up/Down changes the selected row. Left/Right adjusts the session effects,
music, and voice values or the force-feedback toggle; effects/music gain and
race brightness update immediately. Enter resumes, restarts the currently
loaded course, or exits the native application on `QUITTER`. Those restart and
quit outcomes are implementation assumptions because the original actions
were not observed. Force-feedback output and race sound effects are not yet
connected. This is an implementation of the captured interface, not evidence
that the action semantics match WPC.

## Save-slot selection and naming

After the championship, the user reports opening the save flow through a menu.
It presents eight slots, each labeled `LIBRE`. The user chooses the first slot
and enters the save name `NICO`. The supplied image in this message is still
the `TOUS EN VILLE 1` next-track screen; it does not show the save menu, slot
list, or name entry, so those UI details are user-reported. The user later
identified `save/NICO.WSG`; the 126-byte file matches WPC's static `.WSG`
writer layout exactly. Its raw fields and correspondence with `wpc.exe` are
documented in [`save-format-and-logic.md`](save-format-and-logic.md). This
confirms a save file was written, but a load or re-save round trip has not been
observed. The earlier check that found no save predates this save attempt.

## `LES COLLINES JOYEUSES 1` race start and HUD

After continuing from the next-track prompt, the user reports that the race
began in the same manner as the previous race and that the weather was normal,
without rain. The supplied frame
`wine-happy-hills-race-20261023.png`
shows a bright course under a purple sky, lap `1/3`, position `6 EME`, and
timer `00:21`. The user identifies the lower-right gauge as speed and the
upper-right gauge as boost/turbo level. These gauge meanings are user-reported;
the frame records their appearance at this moment, not their scales, units, or
how gameplay inputs affect them. The user's report of “normal” weather is
relative to the earlier rainy race; weather settings and whether the course
uses dynamic conditions remain unverified.

The user later reports that the boost gauge increases only when collecting a
course power-up, and that they just collected an orange power-up. No capture
of the pickup or gauge change is available, and the orange color's specific
effect or item identity has not been established.

The user reports finishing `LES COLLINES JOYEUSES 1` in fourth place and says
the race restarted afterward. They infer that this race must be won to
advance. This restart/result has not been captured visually. The inference
applies as a current hypothesis for this course: it does not yet establish a
general championship rule, since the user earlier reported advancing from
Grand Canyon after finishing third. Whether courses have different
qualification thresholds or another progression condition is unknown.

The user then reports finishing in second place and advancing. The supplied
next-track capture
`wine-next-track-tous-en-ville-20261023.png`
shows `PISTE SUIVANTE`, `TOUS EN VILLE 1`, and `APPUYER SUR ENTREE`. Taken
together with the reported advance after third place and repeat after fourth,
the results support (but do not yet prove) a podium threshold for progression
in this championship. The exact result and course associated with this second-
place transition have not been independently captured; the image confirms the
next-course name.

The user reports arriving second again and advancing to the championship's
last course, `AU PAYS DES FRISSONS 1`. No screenshot of this transition is
available. This is another reported advancement after a second-place finish
and extends the observed course sequence; the user's identification of it as
the final course has not been independently verified from the championship
selector or a completion screen.

The user now reports finishing second on the championship's final course,
winning the championship with `22` points, and returning to the game menu at
the end. The supplied image is byte-for-byte the same next-track capture that
shows `TOUS EN VILLE 1`; it does not show the 22-point total or menu return.
Those completion details are therefore recorded as user-reported, not
visually verified. The point-award formula and exact completion-screen flow
remain unknown.

## Character-selection screen report

The user reports that the character-selection screen displays `MAX SPEED` and
`TRACTION` indications, the selected character's weapon (`Dragoon`), and
super gadget (`Wings`). The user also reports that each character has a
dedicated vehicle. Those earlier details were reported before a selection
screen capture was available. The user's later Championship II launch returned
to character selection; the frame is preserved as
`wine-character-select-caraverne-20261023.png`.
It names `CARAVERNE` / `LES FRERES TETES DURES` and displays `VITESSE MAXIMALE
MOYENNE` and `TRACTION EXCELLENTE`. It also shows icons under `UTILISER UNE
ARME` and `ACTIONNER LE SUPER GADGET`, but does not label their identities.
This confirms the displayed stat categories and values for Caraverne only;
the scale across the roster, weapon/gadget identities and effects, and exact
vehicle mapping remain unknown. The previously reported Wings effect is
recorded separately below.

A second captured profile
(`wine-character-select-turbo-terrific-20261023.png`)
shows `TURBO TERRIFIQUE` above `PIERRE DE BEAU-FIXE`. The user identifies
Turbo Terrifique as the character. The screen displays `VITESSE MAXIMALE
EXCELLENTE` and `TRACTION MOYENNE`. Weapon and super-gadget icons are visible
but unnamed; their identities and effects are not established. The paired
labels suggest a vehicle/character association, consistent with the user's
report that characters have dedicated vehicles, but their exact roles in this
screen's naming convention have not been independently confirmed.

A third profile capture
(`wine-character-select-tocard-tank-20261023.png`)
shows `TOCARD TANK` above `LE SERGENT GROSSE-POMME ET SOLDAT PETIT-POIS`.
The user identifies Tocard Tank as the character. The screen displays
`VITESSE MAXIMALE MOYENNE` and `TRACTION EXCELLENTE`. The weapon and
super-gadget icons are visible but unnamed, so their identities and effects
remain unknown. The exact roles of the paired title and subtitle are not
confirmed.

The user supplied the six character-selection entries listed in
[`race-gameplay-and-worlds.md`](race-gameplay-and-worlds.md): Max Le Rouge;
Les frères Têtes-dures; Sergent Grosse-Pomme et Soldat Petit-Pois; Pique et
Collégramme; Satanas et Diabolo; and Pierre de Beau-Fixe. The user clarifies
that Satanas et Diabolo are not currently playable and become unlockable later
after completing championship mode. The exact completion threshold and
whether all championships must be completed are not established. Other
vehicle/loadout pairings without profile captures remain open.

The user reports the pairing `MAX LE ROUGE` with vehicle `BOLIDE ECARLATE`.
A later profile capture
(`wine-character-select-bolide-ecarlate-20261023.png`)
visually confirms both names and displays `VITESSE MAXIMALE MOYENNE` and
`TRACTION BONNE`. The weapon and super-gadget icons are visible but unnamed;
their identities and effects remain unknown.

The user reports that vehicle `COMPACT PUSSYCAT` is driven by `PENELOPE JOLI
COEUR`. A later matching profile capture
(`wine-character-select-compact-pussycat-20261023.png`)
visually confirms the pair. It displays `VITESSE MAXIMALE LA + RAPIDE` and
`TRACTION BONNE`. The weapon and super-gadget icons are shown but unnamed, so
their identities and effects remain unknown.

The user reports that `DINGO-LIMOUSINE` is driven by `PIQUE ET COLLEGRAMME`.
A later matching profile capture
(`wine-character-select-dingo-limousine-20261023.png`)
visually confirms both names and displays `VITESSE MAXIMALE RAPIDE` and
`TRACTION BONNE`. The weapon and super-gadget icons are visible but unnamed;
their identities and effects remain unknown.

## Game modes reported from the menu

The user reports three one-player choices: course simple, championnat, and
entraînement. The two-player menu offers course and mode affrontement. These
labels are recorded from the user's observation; their rules, race format,
progression, and two-player presentation remain unknown.

The supplied image
`wine-championship-select-20261022.png`
shows the `CHOISIR CHAMPIONNAT` page with five horizontal thumbnails. The first
is highlighted and named `CHAMPIONNAT I`; the remaining four labels are not
visible in this frame. Footer hints show arrow-key selection, Enter acceptance,
and Escape return. The image accompanies the user's Wings description but
shows the championship-selection screen, not the vehicle-flight animation.

After reporting a win in Championship I, the user reports that Championship
II is now unlocked. A later selector capture
`wine-championship-select-2-20261023.png`
shows `CHAMPIONNAT 2` highlighted; the user identifies Championships III, IV,
and V as still locked. The capture confirms the second item is selectable and
the last three appear darkened, but does not establish the precise unlock
condition for Championship II or the unlock rules for III–V.

## 2026-10-23 native menu text comparison

The native menu was run with `SDL_VIDEODRIVER=x11` and `--skip-intro` to make
an X11 window capture possible. The desktop spans two 1920x1080 monitors;
capturing only the left monitor or the Wayland desktop root produced a black
image. Capturing the SDL window directly by its X11 window ID produced
`native-mainmenu-aligned-20261023.png`.
Compared with the user's Wine main-menu image, the eight text rows and footer
now align more closely. A live Wine/GDB capture recovered the main page's
viewport mapping as logical 320x240 to client 640x480, with zero transform
offset and zero text clip origin. WPC centers each label around page X=160
using WGM font advances; the native renderer measures those advances
dynamically. A live glyph descriptor and static quad construction recover
glyph placement as cursor plus the WGM bearing on both axes. The native code
uses the recovered cursor positions directly and applies the backend-specific
pixel adjustment established by mask comparison. A fresh equal-size capture
matched all eight text-row masks at zero translation; the current audit is
[`menu-user-reference-mask-audit-20261023.md`](menu-user-reference-mask-audit-20261023.md).
The footer cursor is Y=440, with its up/down controls drawn from WGM glyphs
`0x7f` and `0x7e`. This establishes placement, not exact raster equality; IoU
for the row masks ranges from 0.877 to 0.934.

## 2026-10-23 road-animation runtime retry

A fresh Wine run from a temporary installation path initially produced a
black capture and no game-asset file opens, so it did not establish a game
crash. A later run from the registered `C:\Program Files (x86)\Infogrames\Wacky
Races` path opened `D:\WID.ID` and `legal.avi`, then displayed WPC's modal
`Anim Play Error`. The video is a 640x480, 25-fps Indeo 5 AVI with a 22,050-Hz
mono PCM track. Static disassembly shows the native AVI path checks
`AVIFileOpenA`, video/audio `AVIFileGetStream`, `AVIStreamInfoA`,
`AVIStreamGetFrameOpen`, and `DrawDibOpen`; the observed dialog alone does not
identify which call failed.

The Wine log also reports `NtUserChangeDisplaySettings ... returned -2` on
Xvfb at both 1280x720x24 and 640x480x24; an additional 640x480x16 run still
showed the same modal. The exact relation between the rejected display-mode
request and the AVI error is unproven. The run did not reach `l2.bmp` or the
main menu and provides no new road positions or visibility samples. Host
GStreamer inspection confirms an `avdec_indeo5` plugin is installed in the
temporary plugin path, but this does not prove Wine's AVI/DrawDib path used it.
The earlier saved Wine captures remain the runtime evidence for changing car
sets and positions. The GDSprite update path and finite 40-tick callback are
documented in [`mainmenu-road-runtime-disassembly-20261023.md`](mainmenu-road-runtime-disassembly-20261023.md);
the callback's attachment to page 4 is still unproven.
