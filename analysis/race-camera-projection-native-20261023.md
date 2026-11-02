# Native race camera projection

The native software rasterizer now applies the projection inputs recovered
from `wpc.exe` and the mounted track WGM files. The installed executable's
`GDCamera` reflection stubs register `m_nFOV`, `m_nRatio`, `m_NearPlane`, and
`m_FarPlane` at `0x004206d0`, `0x00420730`, `0x00420790`, and `0x004207f0`.
`FUN_00435410` reads the corresponding float/int values four bytes past these
registered offsets and builds the perspective matrix:

```text
P00 = sqrt(m_nRatio * 0.0075) / tan(m_nFOV / 2)
P11 = 1 / (sqrt(m_nRatio * 0.0075) * tan(m_nFOV / 2))
P22 = m_FarPlane / (m_FarPlane - m_NearPlane)
P32 = -(m_FarPlane * m_NearPlane) / (m_FarPlane - m_NearPlane)
P23 = 1
```

The five track families' twenty time-of-day WGM files on the mounted ISO
contain 92 `WTrackCamera` nodes. A bounded BRTR scan found all four projection
properties on every node with these invariant values: FOV `1.04719758` radians,
ratio `100`, near plane `0.5`, and far plane `20000`. The WGM reader now
validates these values and retains them with the first serialized track
camera. For a 640x480 viewport, applying the matrix then the half-width and
half-height viewport scales produces equal horizontal and vertical pixel
factors, as expected for the executable's ratio value.

The renderer uses the recovered ratio formula for separate X/Y focal scales
and clips polygons to the camera's near/far interval before perspective
projection. `FUN_004354a0` also scales projected coordinates by
`sqrt((viewportWidth / viewportHeight) * 0.75)`. The native projection applies
that factor so the recovered matrix works for arbitrary viewport aspect ratios;
the 640x480 game viewport yields a factor of one. If a camera lacks a complete
valid projection tuple, it retains the previous one-unit-near-plane fallback.
The chase-camera transform and camera selection still use native fitted/runtime
assumptions. A frame-matched Wine comparison remains outstanding.
