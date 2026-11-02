# Native Championship I: `cc_t1`, `sh_t1`, `dt_t1`, `ct_t1`

Follows [native race](native-race-cc-t1-20261027.md), which ported race 1.
Course images and captures stay in the session scratchpad; none are committed.

## Course order (recovered)

`DAT_006f41e0` holds the selected championship: `0x491140` copies it from the
menu node (`+0x12a`). The first race is set by `0x48de20` and `0x4928f0`.
For championships 0..3 they set course `DAT_006f41dc` = championship and
environment `DAT_006f41d8` = 0. For championship 4 they set environment 4
(`dr`) and course 0. After a race, `0x491cf7` case 3 advances:

- championships 0..3: environment + 1 while it is below 3, then state `0xd`
  (championship over);
- championship 4: course + 1 while it is below 3, then state 1.

Filenames come from `0x52bef0[course + environment*4]`. Championship I is
therefore `cc_t1`, `sh_t1`, `dt_t1`, `ct_t1`, and Championship *N* is the *N*th
variant of each environment in turn. The menu's Championship I track page
(`WMenuNodeSelectTrack 0x149`, page 518) has one `WMenuNodeLoadTrack` per race,
`0x14c..0x14f`. Only the first is serialized visible (render bit `0x2000`).

Native: `championship_course()` (`game_files`). `MenuController` enters the
current race's node. `championship_finished()` advances after a finished race
(Enter on the finish screen); after the fourth race the championship restarts.
Escape abandons a race without advancing. Results, points and unlocks are
not ported.

## Course data differences from `cc_t1`

| Course | Sections | Loop transform (WMOD) | Surface types (WGNR low byte) |
|---|---|---|---|
| `cc_t1` | 3 | translation (+73386.5, −0.1, −31976.5) | 0x1, 0x2, 0x4; deep water 0x800000 |
| `sh_t1` | 3 | half turn about +Y, (−8679.3, −2232.6, −46537.9) | 0x1, 0x2, 0x10; deep water 0x800000 |
| `dt_t1` | 4 | about a quarter turn about +Y (basis 0.01), (−46120.7, 0, −18374.2) | 0x1, 0x2, 0x10, 0x40 |
| `ct_t1` | 3 | translation (−15205.8, 2588.3, −87923.3) | 0x1, 0x2, 0x4, 0x20, 0x40, 0x80 |

The loop transform is chosen as the serialized WMOD matrix that maps the last
route's exit portal onto the first route's entry. Each course matches it to
within 50 units. The native port previously took only the translation, so
`sh_t1` and `dt_t1` failed to load. The car's position, heading, up vector,
recent safe points and the follow camera now go through the full matrix at
the seam. The world copy drawn beyond the seam uses the same matrix: on
`sh_t1` the start line appears past the last exit.

Every low-byte surface type is inside the `0x018000ff` ground mask. Their
gameplay meaning (friction, sounds) is not recovered.

## Course features met

- `sh_t1`: a stone tunnel with two hairpins (the S-bend after the forest
  meadow; its mouth is under an overhang), and the "piramid" route. That route
  is three wooden ramps jumping across deep water (road 2950 → ramp to ~3130
  → platform 3200 → ramp to ~3390 → platform 3450 → 3200 → 2963).
- `dt_t1`: a road around an ice-rink dome (surface types 0x10/0x40); a tram
  (`WTram`, not ported) on route 2.
- `ct_t1`: snow (0x80) and ice (0x20/0x40) surfaces.

## Provisional native rules (not recovered)

- **Edge guard.** A move that would leave the car's centre over no WGNR, here
  or through the loop transform, slides along the edge like a wall, grounded
  or airborne. The two leading probe sweeps miss corners swinging over the
  course edge while turning or reversing (`dt_t1`'s rink road). A bump at the
  rink edge also launched the car sideways off the course.
- **Ramp launch.** A grounded car keeps its climb rate (0..40 units/update) as
  vertical speed, so a ramp's lip throws it. At top speed in a straight line
  the car clears both `sh_t1` jumps under the provisional gravity
  (0.05 × 45). The original vertical response is not recovered.
- **Respawn.** Repeated failures within ten seconds return the car two more
  seconds further back each time, so it can regain take-off speed before a
  jump. History is kept for 12 s.
- **Lap line.** Crossings count within 5000 units of the start line's centre
  (was 2500): `cc_t1`'s shallow river beside the road crosses about 3000 out.
- **Progress field** (rank, wrong way, the test driver):
  - It holds the *remaining* distance to the exit portal, so a dead end far
    from the entry no longer scores high (`sh_t1`'s meadow).
  - Each 150-unit column keeps one node per surface layer (bridges, tunnels),
    joined to neighbours within 150 units of height.
  - Each wall crossed costs 4000. Thin walls fall between samples; a block
    would cut sections apart at kerbs.
  - Deep water joins at eight times the cost, bridging the jump gaps.

## Verification

`native_drive_test` takes the four course archives. For each, it checks the
seam transform, field connectivity, the grid and the PRET! hold. Its test-only
lookahead driver then runs the race:

| Course | Result | Respawns |
|---|---|---|
| `cc_t1` | 3 laps, 118.8 s | 0 |
| `sh_t1` | 1 lap, 207.9 s | 2 |
| `dt_t1` | 3 laps, 155.0 s | 0 |
| `ct_t1` | 3 laps, 122.2 s | 0 |

On `sh_t1` the driver is slow through the tunnel hairpins (about 300 s a lap),
so the test requires one lap. In a 15-minute run it completed two laps and was
in the third lap's jump section. `native_menu_test` follows Championship I
through its four race nodes. `wackyraces --race COURSE.wgm` and
`WPC_RACE_PLACE=x,z,yaw` were used for screenshots at the tunnel, the jumps
and the seams.

A same-input Wine comparison of races 2–4 followed; see
[Wine comparison](wine-comparison-championship-i-20261028.md). Lap times,
jump distances and the respawn point are still not compared.

The other 16 course archives were load-checked. All load except `cc_t4`,
`dr_t2` and `sh_t2`, where no WMOD transform maps the last exit onto the first
entry. They have not been driven.

## All championship courses (2026-11-01)

**Route order.** The route table is a permutation of the definitions. `route(k)` is `definitions[route_indices[k]]`, and the sections are `route(0..n-1)`: each exit meets the next entry at 50, and the last exit meets the first entry through the loop transform. `RaceTrack` iterated the table's values and applied the indirection a second time. That was harmless on the t1 courses, whose table is the identity. On `cc_t4` (table 1 0 2), `dr_t2` (1 3 0 2) and `sh_t2` (2 1 3 0), it produced an order that no WMOD transform closes. With the order fixed, all 20 `_t1.._t4` courses load.

**Progress field.** The field is native, not recovered. Section 1 of `sh_t2` (the log ride) and of `ct_t4` (a climb to y 1200) did not join their entry and exit. Two changes fix this:
- the entry node is the nearest node reached from the exit;
- if the entry is still apart, a second pass allows steep steps (4 samples high) and gaps of up to 3 columns, at an extra cost of 2000.

**Ground truth and test.** `ct_t2`'s `GDWorld` sets 9 laps. `native_drive_test` now accepts any lap count and any championship course. The rank boost is checked against the course's own row; the t1 range 1.2..1.25 applies only to cc..ct.

**Driving results (2026-11-01).** Only `dt_t4` passes every check. The failures are listed in TODO. Most are robots stalling on slopes and in dips once the slope filter is gone: on `cc_t2`, robots hover on their wall rays where a dip climbs out. None was compared with Wine.

## Post-race menus and championship completion (2026-11-01)

After the standings, the original reloads the menu at the start code `0x6f41cc`.

**Single race: code 2.** The node is `WMenuNodePostRace` `0x2ca` (page 703). Its rows are REVOIR LA COURSE, ENREGISTRER LA COURSE (`.WSR` save), SAUVEGARDER PARTIE, RECOMMENCER LA COURSE, AUTRE COURSE and QUITTER.
- Its enter callback (`0x491cc0`) unlocks with `0x496c50` (rank ≤ 3, one player, not battle).
- RECOMMENCER (case 3, `0x491d14`) reloads the course.
- AUTRE COURSE (case 4, `0x491d1e`) goes to start code mode + 4, or mode + 0xe with two players. In a single race that is the car page `0x14a`.

**Championship: code 3.** The node is `0x2d2` (page 715). Its enter callback calls `0x496b80` after the last race (`0x491d69`). CONTINUER (case 3, `0x491e10`) then branches on the rank:
- **Rank above 3**: `0x496d50` takes the race's points back off the totals (`+0x10d` −= `+0x106`), and the same race runs again.
- **Otherwise**: the next race. For championships 0..3, the environment advances, and the menu opens that environment's track page: start code 8 + environment, nodes `0x33e..0x352`, pages 518 (cc) .. 816 (dr), "PISTE SUIVANTE" and its four variants. Only the championship's variant is available there (`0x4913f0`).
- **After the last race**: code `0xd`, the championship page `WMenuNodeSelectCShip`. For championship 4, the course advances instead, and code 1 follows its last race.

The championship's first race also uses its environment's page: the car page's `WMenuNodeSelectTrack` child at index 0 (cc), or 4 (dr) for championship 4. The port previously took child n° championship, which was right only for Championship I.

**Native.** `MenuController::race_over` and `post_race_accept`. REVOIR (replays) and two-player championships are not ported. Covered by `native_menu_test`.

## Winner scenes and the outro (2026-11-01)

**Race exit `0x486fe0`.** It runs from `0x4a3533`, on a new press of the results key.
- In a championship it adds the race's points (`0x496d10`).
- It loads `win\win*.wgm` (`0x52bf54`, indexed by vehicle `+0x1a68`) when three conditions hold:
  - the player's vehicle is `0x496d90`'s points leader;
  - the rank is ≤ 3;
  - this was the last race: environment 3, or environment 4 with course 3.
- Otherwise it loads `MENU.WGM`.

**Movie selector.** `0x431c20(path, flag, movie)` stores `movie` in `0x5299ac`, which the loader `0x432bbf..0x432cf6` reads:
- 0 plays `intro.avi` with `Voice\iaudio.avi`;
- 1 plays `outro.avi` with `Voice\oaudio.avi`;
- -1 plays nothing.

**Winner scene music (2026-11-01).**
- `0x40c430`, the application's world load, calls `0x431690` for every file.
- `0x431690` builds `music\` plus the last path component as `.snd`. Names containing "dr" take the `_t1`..`_t4`/`_b` fallbacks (strings `0x5299d8..0x529a04`).
- The path goes to the sound manager's `+0x4c` (`0x006f5bcc`, flag 1).
- For `Win\WinMean.WGM` that is `music\WinMean.snd`. No such file is on the disc (Music holds the 20 course tracks only), so the scene is silent, as natively. The manager's handling of the missing file is not traced.

**End of the winner scene, `0x494b20`.** It is called from the scene object's update `0x494be0`, either on a skip or when the sequence ends.
- After ct_t4 it loads `win\DickOff.wgm`.
- After dr_t4 it loads `MENU.WGM` with movie 1 and start code `0x2c` (`0x6f41bc`), the credits.
- Otherwise it loads `MENU.WGM`.

So the outro plays only after Championship V is won. The random check before the selector is METEO's: in mode 2, rain when `rand() % 100 > 25`.

**Native.**
- `race_over` returns `PlayOutro` for a won Championship V.
- `linux_main` plays `Movies/outro.avi` with `Voice/oaudio.avi` (`Stage::Outro`, Escape skips), then `show_credits()`.
- The winner scenes and DickOff are not ported. Championships I–IV show the post-race page.

**Checks.**
- `native_menu_test` runs Championship V on an unlocked profile, dr_t1 to dr_t4, then `PlayOutro`, then the credits.
- `native_movie_test` decodes the disc's `outro.avi`: 1381 frames, 320×208, 25 fps.
- The full sequence was not run in the game, and not compared with Wine.

## WWinnerMovie ported (2026-11-01)

**Template.** The GDWorld's action, members `0x1b8..0x1bb`:
- m_nWinnerMovieType (1 for DickOff);
- m_nStartMoveIn;
- m_nStartOtherSpeak and m_nStartOtherSpeak2 (−10 when unused).
- Values in the corpus: WinTur and WinPen use 0/3/2/−10; WinMean uses 0/−10/−10/−10; DickOff uses 1/−10/−10/−10.

**Init `0x4949f0`.**
- `+0x24` = the world's second child (the WMenuItem of sounds), `+0x28` = its first child.
- Unless the type is 1, the first film of the first child (the winner) plays sequence 0 in mode 2.
- Satanas state 0 and Muttley 13 (`0x6f41f8`/`0x52bf74`).

**Update `0x494be0`**, once a menu update:
1. Unless the type is 1, a new press of `0x52aada`'s bits ends the scene (`0x494b20`).
2. While `+0x2c` is set, `0x494ab0(4)` moves the visual group's GDSprites (`+0x12a`) and GDFilm2s (`+0x1a2`) 4 to the left. After 40 updates it clears.
3. A speaking voice (`0x6f4200`) blocks the update. Its end calls `0x486dd0` and sets `0x6f41ff` (WMenuAnimation's speech end).
4. Without a next sound the scene ends.
5. Phase `+0x35` += 1:
   - at OtherSpeak + 1 and OtherSpeak2 + 1 the winner film returns to sequence 0;
   - at OtherSpeak it waits for the film's last frame (`0x493fa0`, phase − 1), then plays sequence 1;
   - at OtherSpeak2 it waits the same way, then plays sequence 2.
6. Progress `+0x31` += 1. At m_nStartMoveIn the slide starts and the film returns to sequence 0; no sound plays on that step.
7. Otherwise `0x486ef0(sound, 0)` speaks the sound, sets `0x6f41fe` and sets the actor `0x6f37dc` = m_nSpeakingActor. The actor then picks a reaction:
   - vehicle 6, not type 1 (`0x494ea4`): the winner film plays sequence 2/3/1/–/3/2;
   - otherwise (`0x494ebc`): `0x486d20` Satanas 3/0/0/–/0/2 and `0x486d50` Muttley d/e/d/–/10/d.
8. Next sibling. A row that is not a sound stands for the environments: the scene moves on to row + `0x6f41d8` (the last race's environment) and takes its first child.

**Exit `0x494b20`.**
- Type 1 goes back to the menu.
- Otherwise: environment 3 with course 3 loads `win\DickOff.wgm`; environment 4 with course 3 loads the menu with the outro and start code `0x2c`; anything else loads the menu.

**Native.** `WinnerScene` (`src/port/winner_scene.*`):
- The scene is loaded with `Scene::load(archive, &menu members)` and uses its `Win/*.GSW` as the speaking bank.
- WMenuAnimation is used when the scene has one.
- In `linux_main` it is `Stage::Winner` at 30 Hz. The accelerate key skips: `0x52aada` = 0x40.
- After it come DickOff, the outro and credits, or the post-race page (`after_winner`).
- `WPC_WINNER=v,e,c` opens a scene for debugging.

**Checks.**
- Xvfb captures of WinTur showed the winner with the trophy, the slide to Satanas and Muttley, then the post-race page.
- The WAV capture has voice through the first ~13 s.
- WinMean skipped with Enter loads DickOff.
- `native_menu_test` checks `PlayWinner` for Championships I and V.
- Not compared with Wine. The music during the scene is unknown: there is no `Music\Win*.snd`, so the native port is silent.
