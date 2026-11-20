# Course-object census follow-up: dt_t4's rooms, WPalAnim, dr_t4's geyser (2026-11-17)

Follows the census (2026-11-06) and `analysis/dickstrick-uvscroll-rooms-20261107.md`.

## dt_t4's APortal and ARoom: captures against the original

The records (`rooms.cpp`, `rooms2.cpp`). 20 ARoom nodes, each with a frame `0x1b` (48 bytes) and a box
`0x91..0x93`; 32 APortal nodes, each with a size `0x4`, `0x5` and two room ids `0x6`, `0x7`. Two clusters:
- rooms 1031..1035 (parent 1030) around (-11000, -460, 16000), the big ones (box up to 16221 x 7061 x 13747);
- rooms 989..1003 (parents 989 and 1028) around x -13000..-22000, z 38000..47000, y 352..2591: the car park's
  levels.

Two poses compared, the original on Xvfb `:77` (`build/wine-scratch/dt4_cap.sh`, `dt4_room.sh`) and the
port on `:78` (`--race dt/dt_t4.WGM`):
- The start grid, after GO (Wine 00:08, native 00:27; the camera behind the player, which stays on the grid).
  Every course surface matches: the street, the banner, the facades, the arrow sign, the barriers. The
  differences are moving things only: pickups and the robots.
- Inside the car park, room 995. Wine: the car hopped along a robot's line recorded natively (`line20`, 125
  points) to (-19088, 721, 44112), with its grid heading (yaw 0). Native: `WPC_PLACE=-19088,44112,0`. Both
  show the same concrete wall, band and floor. Native's camera sits a little lower (the band at y 150 of 384,
  Wine 180). That is probably Wine's chase camera after the hop: the start views match exactly.

So skipping the rooms and portals drew nothing missing or extra at these two poses. Their readers are still
not traced, and only two poses were compared (both looking at near geometry; neither looks through a portal
into a far room).

## WPalAnim: the vtable

Vtable `0x514f6c` has 14 slots. 14..16 are null, and slot 17 (`0x514fb0`) is already WBlowUp's class record.
Its own slots:
- 0: the destructor `0x4bf580`;
- 1: `0x4bf5d0`, a release through `0x4035bc`;
- 2: the update `0x4bf670`. From the third update it dispatches on the mode `[[+0x18]+0x24]` (`m_24`): 2 calls
  slot 11, 1 calls slot 13, 0 calls slot 12;
- 4: the init `0x4bf600`;
- 11: the step `0x4bf6b0`, `+0x28` from 0 to 11 (reset at 0xb);
- 12, 13: empty (`ret`).

The others are the base's. No slot returns `+0x28`, and only the constructor `0x4bf558` writes the vtable.
So no reader goes through the class. A reader using a raw offset on a generic action is not ruled out.

## dr_t4's WGeyser, driven

dr_t4 has one WGeyser: home (-77547, -625, -90134), width 200, m_Frames 40, linked to section 0. In a full
`RaceSession` the player is placed at the column's aim while it erupts (state 2): it rises 100 an update from
y -625 to 675 (1300, held 14 updates). It stays at the top until about update 25, falls from about 30, and is
back on the ground at update 84 (state 5 by then). `native_drive_test dr_t4` checks it: `geyser lifts the
placed player 1300, held 14 updates, back on the ground after 84`.

Not compared with Wine. A parked car makes the geyser aim `m_Distance` (1700) ahead of it (state 0), so the
original's hit needs a moving car or the geyser's state read; not set up.

## Still open from the census

(2026-11-18: all three compared under Wine, `analysis/crane-geyser-portal-20261118.md`: the crane's arm is now
solid, the geyser's car query has its range, and the portal view showed a far-plane difference, not a room one.)

- dr_t3's crane third-arm squash (2026-11-08), not compared with Wine;
- dr_t4's geyser under Wine;
- the rooms' readers, and a view through a portal.
