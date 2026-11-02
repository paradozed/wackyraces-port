# Race countdown state and rendering

This note follows the countdown display through the installed `wpc.exe`
decompilation. It distinguishes the visible countdown from the unresolved
source of its duration.

## Observed executable path

`FUN_004a2b30` (`analysis/decompilation/wpc/functions-035.c`) owns the race
state at `this+0xf30` and its update counter at `this+0xf34`. In state 1, each
update calls the participant progress/order helpers and increments the counter.
When the counter passes the integer read from
`*(int *)(*(int *)(this+0x18)+0x34)`, the controller resets the counter and
enters state 2. The constructor evidence below identifies the default as 60;
the strict comparison makes this 61 state-1 updates from a zero counter. State
2 continues those helpers, advances the counter, and enters state 3 when the
old counter exceeds 10, which takes 12 state-2 updates from zero. State 3
waits for a participant progress/order result before entering state 4.

The companion renderer `FUN_004a3d20` reads the same `this+0x18` object field
at `+0x34`. In state 1 it fetches localized string ID 3 and formats the
integer `max(1, 3 - counter * 3 / duration)`, producing the 3-to-1 countdown.
In state 2 it fetches localized string ID 4. `STRINGS.DAT` language 3 decodes
these IDs as `PRET!` and `PARTEZ!`, respectively. State 3 draws neither
countdown label. The labels and constructor default are data-backed; original
update cadence and overlay placement still need runtime comparison.

When state 3 advances to state 4, the controller sets the player's race clock
state to 2 and captures `DAT_006ab2d0` as the start timestamp. This is a better
static anchor for the race timer than starting it when the native window opens.
The exact timer object and whether other race-start offsets are included still
need runtime comparison.

## Duration property owner and remaining gap

The registration owner is now identified. The setup at `0x0049cae0` labels the
class descriptor at `0x006f56e0` `WModManager`; `FUN_0049ccb0` returns this same
descriptor. `FUN_0049cdb0` registers `m_nStartWait` against that descriptor and
passes member offset `0x30` to `FUN_0045c300`. The matching small-object
allocation path is `FUN_0049cbb0` -> `FUN_0049d3d0`, which allocates `0xa0`
bytes and initializes the value at `+0x34` to `0x3c` (60). The separate
`FUN_0049cb30` -> `FUN_0049d2d0` path allocates the large race-state object at
`DAT_006f2d00`; that object owns state fields including `+0xf30` and `+0xf34`.

The countdown state machine reads `+0x34` through its `this+0x18` context. The
registered offset is `+0x30`; the four-byte difference matches the executable's
adjusted object pointer in the member-registration path. Together with the
`WModManager` constructor's `+0x34 = 60` initialization, this strongly supports
60 as the countdown duration default. `cc_t1.WGM` contains no `WModManager`
instance among its 117 BRTR classes; its `WStartLine` node has properties
`0x1a`, `0x1b`,
`0x1d`, `0x1f`, and `0x1df`. However, its variable-length `WMOD` chunk has an
unparsed tail beginning with a count of 3 and the text `Snake`. That tail has
not been ruled out as a source of the runtime value. See `wgm-container.md`
for the current byte observations.

The state-1 code increments its counter before comparing it with the 60-tick
duration and transitions only when the counter is greater than the duration;
from zero this takes 61 updates. State 2 checks the old counter before
incrementing and transitions when that old value exceeds 10; starting at zero
this takes 12 updates. The native manual-race path now displays localized IDs
3 and 4 (`PRET!` / `PARTEZ!` in French), blocks motion during those 61 + 12
updates, and starts its race clock after the overlay. Replays and demos skip
the count-in. State 3 waits for participant progress/order activity before
entering state 4; the native implementation releases after the GO display
instead of reproducing that handshake. Original update cadence and overlay
placement still need a matched runtime capture. The variable course `WMOD`
tail has not been linked to `m_nStartWait`.
