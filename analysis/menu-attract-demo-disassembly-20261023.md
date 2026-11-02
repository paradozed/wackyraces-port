# Main-menu idle attract demo path

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

This trace follows the original executable's automatic demo path after menu
inactivity. Function behavior is from the installed `wpc.exe` decompilation;
the observed delay comes from the preserved 100-second Wine capture.

## Static executable evidence

- The executable's class metadata contains `WMenu` and the property name
  `m_nMaxIdleTime`. Its registration at `0x0048f450` associates the field with
  runtime offset `+0x24` after the four-byte object base adjustment.
- The WMenu factory `FUN_0048f2d0` allocates `0x70` bytes and calls constructor
  `FUN_0048f7f0`. That constructor writes `300` at `this + 0x24`; the following
  registered highlight-color defaults are `240, 240, 0`, matching the same
  object's members. This ties the 300 value to `m_nMaxIdleTime` rather than
  inferring it from the Wine delay.
- `FUN_0048fac0` is the menu update path. It reads a context pointer from
  `this + 0x18`, checks the current page type at
  `current_page + 0x18 + 0x98`, and only advances its idle counter when that
  type is `0` or `0x10` and global flag `DAT_005299b0` is nonzero. The counter
  is at `this + 0x28`; after it passes the WMenu threshold `300` at `+0x24`,
  the routine starts the next demo from the `demo1.pc` / `demo2.pc` / `demo3.pc`
  table and advances the table index modulo three. The counter increases once
  per `FUN_0048fac0` invocation; the caller cadence is not statically resolved.
- The same routine clears the idle counter on its input/change path. The
  decompilation does not establish the caller cadence or the meaning and
  initialization of the global gate.
- `FUN_0048a3d0` also starts and advances the three-demo table when a menu
  action node has type 10. That is the explicit Demo command and is distinct
  from the inactivity-triggered path in `FUN_0048fac0`.

## Runtime observation

The 100-second Wine capture
`menu-animation-times/wine-mainmenu-long-startup-20261023.mp4`
was sampled at one frame per second. At second 44 the full main menu is stable;
it remains stable through second 70. At second 71 the screen is black during a
transition, and second 72 shows the attract race. This bounds the visible idle
period to about 27–28 seconds in that run. Dividing by the recovered threshold
of 300 callbacks gives an observed average near 90–93 ms per callback, if the
counter starts when the menu becomes visible. The counter may start earlier,
during menu entry, so this is consistent with a roughly 100 ms engine update
but does not prove its exact cadence or when the timer starts. The capture also
does not establish whether every launch uses the same timing.

An isolated `winedbg --gdb` run with a breakpoint set at `0x0048fac0` did not
reach that function. Continuing from the initial loader stop produced a Wine
access violation at `0x004ad247` while writing to address `0x1`, before the
menu loaded. The debugger attempt was stopped and its temporary Wine server
was shut down. Thus the runtime video bounds the user-visible delay, while the
exact update cadence remains unmeasured.

## Native port status

The C port launches the three installed demo replays from the explicit Demo
menu row. It starts the next attract demo after 30 seconds without keyboard or
controller input on the main page and resets that timer after a demo returns.
The 300-callback threshold is directly recovered from the WMenu constructor;
the native 100 ms interval is an approximation consistent with the observed
27–28-second visible delay, not an exact recovered engine cadence. Runtime
instrumentation of the callback caller and a second idle capture would be
needed to resolve the timer's start point and exact cadence.
