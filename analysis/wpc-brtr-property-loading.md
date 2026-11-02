# BRTR property loading path

Static radare2 6.2.2 observations from the installed `wpc.exe`, SHA-256
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`.

The WGM dispatcher at `0x0041bf80` sends `BRTR` to `0x0041fe80`. The generic
parser resolves the object's class through the loaded class table, creates a
class object, and calls `0x00405cb3` (thunk to `0x0041bd20`) to load its `PRPS`
records. The `CLAS` chunk is separately passed through the class-table loader
at `0x00405a7e`.

The `PRPS` loader at `0x0041bd20` walks `PROP` records. It reads each property
ID, looks up its member descriptor in the registered property table, and calls
`0x004047f5` (thunk to `0x0041b6f0`) to resolve the field address from the
descriptor's offset and the object base. It then calls the member reader
through that field descriptor's first virtual method, passing the input
stream. Thus BRTR property IDs dispatch through registered class members; they
are not simply copied into an untyped property bag by this executable path.

The resolver at `0x0041b6f0` loads the field offset from descriptor `+0x18`;
when descriptor flag bit 1 is clear, it adds that offset to the object base.
The registration helper at `0x0045c300` stores its final numeric argument at
descriptor `+0x18`. This confirms the final numeric argument as the member
field offset for these registrations. The other numeric argument is stored at
descriptor `+0x14`; its meaning is not established by this path. For example,
the `m_TextID` registration stores zero there while menu objects serialize the
corresponding field under property ID `0x4a`. Therefore `+0x14` is not the
serialized property ID directly.

## Main-menu text property association

The executable registers a contiguous set of text-related member names and
field offsets:

| Member | Registration address | Field offset |
| --- | ---: | ---: |
| `m_TextID` | `0x0042b380` | `0x12c` |
| `m_Align` | `0x0042b3e0` | `0x134` |
| `m_Height` | `0x0042b440` | `0x13c` |
| `m_Space` | `0x0042b4a0` | `0x144` |
| `m_Narrow` | `0x0042b500` | `0x14c` |

In the installed `menu.WGM`, all eight main-menu `WMenuText` objects have
properties `0x4b`–`0x4e` set to 4, 13, 5, and 0 respectively. Property `0x4a`
varies by object, and its values resolve to the expected localized string
keys. The ordered correspondence between the five registered text members and
properties `0x4a`–`0x4e`, together with the varying string-key value at `0x4a`,
strongly supports this mapping:

| BRTR property | Registered member | Observed menu value |
| --- | --- | --- |
| `0x4a` | `m_TextID` | string key |
| `0x4b` | `m_Align` | 4 |
| `0x4c` | `m_Height` | 13 |
| `0x4d` | `m_Space` | 5 |
| `0x4e` | `m_Narrow` | 0 |

The registration/member order and data pattern make this a strong association,
though the class table's runtime descriptor contents have not been captured to
verify the mapping directly. The
member names provide semantic labels; detailed effects of alignment, height,
spacing, and narrowing still need to be confirmed in the draw path. This does
not identify the glyph-trim field at `+0x150`, scale field at `+0x152`, or the
second draw-scale input.

The relevant code is `0x0041c1c6` (`BRTR` dispatch), `0x0041fe80` (object
creation), `0x0041bd20` (`PRPS` loading), `0x0041b6f0` (field-address
resolution), and the five member registrations listed above. `analysis/
disassembly/wpc-menu-text-update.txt` and `wpc-menu-text-render.txt` document
the later placement and draw behavior.
