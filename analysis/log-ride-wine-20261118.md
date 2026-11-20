# sh_t3's WLogRide against Wine (2026-11-18)

Follows `analysis/log-ride-20261102.md` ("Not compared with Wine").

## The logs' motion: matches

Wine (scratch prefix, Xvfb `:77`, `build/wine-scratch/logride_sh3.sh`, `wmem.py logride`). The WLogRide is found by its
vtable `0x515968` (record `0x515944` + 0x24; slot 2 the update `0x4cc450`, slot 4 the init `0x4cc3e0`). Logged on each
change of its distance `+0x246` (a float): the delay `+0x23a` and each WLogBase's local place `+0x54`, the logs
taken from `[+0x242]` along `+0x14`.

- The distance runs 400, 450, ... in steps of m_vel 50, reaches 6100 = (500 + 50) x 2 + 5000 exactly, then wraps to 50.
  That is the port's `offset > wrap()` rule.
- Native (`lrcar.cpp`, `lrcmp.cpp`): `path_frame` per log at Wine's distance, + 550, then + m_dist after every third
  log, taken back to the ride's local space (the ride's world matrix is a translation, (176278.2, 5049.1, -1769.2)).
  Over 889 updates the nine logs' places agree within 0.027 units (mean 0.012). First line, d 400, log 0: Wine
  (-1645.57, 1066.88, 4641.95), native (-1645.55, 1066.88, 4641.95).

## A car dropped onto a log: differs (not ported)

- **Wine** (`logride2_sh3.sh`). The car went along a robot's line to near the flume (the robots never come within
  1669 of it), then, as log 5 came within 300 of (6227, 192, 513) local, it was hopped to 150 above the log's centre:
  (182223.3, 5380.0, -1306.8) at d 4650. It was **not carried**:
  - its x and z stayed put;
  - y fell 0.2, 0.5, 0.7, 1.2 ... to about 5361, then sank by 0.1..4 an update for 30+ updates (5344 by d 500);
  - its last ground object `+0x89f` kept the value it had on the grid.
- **Native**, the same point and distance (robots off): the car stands on the log's mesh at once (`Body::log` 5) and
  follows it, x 182223 to 183604 in 29 updates.
- **The data.** Each WLogBase's PRIM (sensor group 2) is a **box**:
  - type word `0x20004003`, name "PrimBox";
  - key `0x8b` holds min (-143.8, -304.1, 0) and max (143.8, 306.7, 86.6) in the log's frame;
  - the log's z row is almost straight up ((-0.03, 0.999, -0.04) for log 5 then), so the box runs from the log's
    centre (y 5230.0) to about 5317;
  - the port's `node_prims` does not decode boxes (radius 0), so the port took the log's model mesh as its ground.
- The static ground under that point is 5232.6, the flume's floor (native ray). What the Wine car rested on at about
  5361, sinking slowly, is not known (water, presumably, and drowning). So the original's car does not take a log
  as ground the way the port's does, at least when it is dropped there. How `+0x89f` becomes a WLogBase (the
  sensor-group-2 box query) is not traced, and the port's mesh-as-ground rule stays provisional.

## Not done

WGeyser's model update `0x4c9850`, its fade and its sounds (`analysis/lap-line-geyser-20261106.md`).
