# Text-related reflection metadata

Static radare2 6.2.2 observations from the installed `wpc.exe`, SHA-256
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`.

The executable contains reflection strings `GDPsText`, `m_TextID`,
`m_Align`, `m_Height`, `m_Space`, `m_Narrow`, and `m_Scale`. Member
registration calls pass those names and numeric field-offset arguments to
helper `0x0040520e` (thunk to
`0x0045c300`). The helper stores six supplied values in a linked metadata
record. The observed registrations include:

| Member name | Registration site | Offset argument | Descriptor list |
| --- | ---: | ---: | ---: |
| `m_TextID` | `0x0042b380` | `0x12c` | `0x5469a0` |
| `m_Align` | `0x0042b3e0` | `0x134` | `0x546b08` |
| `m_Height` | `0x0042b440` | `0x13c` | `0x546a68` |
| `m_Space` | `0x0042b4a0` | `0x144` | `0x546a90` |
| `m_Narrow` | `0x0042b500` | `0x14c` | `0x546850` |
| `m_Scale` | `0x0042eca0` | `0xb4` | `0x5470b0` |

The scale registration also passes `0x14e` as its adjacent numeric argument.
`GDPsText` has a separate class descriptor registration at `0x0042b020`.
These observations show that the binary has named text-related reflection
metadata. The BRTR loader and its member-descriptor dispatch are now traced in
`analysis/wpc-brtr-property-loading.md`. In the main menu, the ordered property
IDs `0x4a`–`0x4e` and observed values strongly associate with the five text
member names above. The class table's runtime descriptor contents have not
been captured, so retain this as a strong static/data association. The
`m_Align` field offset does not match the runtime position fields; it must not
be treated as a position offset. The scale entry at `0x0042eca0` is in a
separate descriptor list and is not established as a `WMenuText` member.

The `WMenuText` class descriptor registered at `0x004871e0` also has a member
registration for `m_bMustPosition` at `0x00487430`. The registration call uses
the same helper and passes `0x15c` as its final numeric argument. The BRTR
member resolver reads that helper argument back from descriptor `+0x18` as the
field offset, so `+0x15c` is confirmed as the registered member's field
location. The preceding `0x1c6` value is stored at descriptor `+0x14`, whose
meaning is unresolved; the `m_TextID` registration stores zero there while
its installed BRTR property ID is `0x4a`, so this argument is not itself the
serialized property ID. The registration does not identify which BRTR
property selects `m_bMustPosition`. The update method reads a separate mode
field at `+0x1c2`; no equivalence between that mode and `m_bMustPosition` has
been shown. The focused call is appended to
`analysis/disassembly/wpc-menu-text-registration.txt`.

Constructor disassembly at `0x004876xx` calls thunk `0x00405236` with `this`
adjusted to `+0x15c`; that thunk jumps to `0x004488a0`, which writes vtable
pointer `0x00511ba8` at the adjusted address. The same initialization pattern
is used at `+0x154` and `+0x164`. Thus `+0x15c` is initialized as an embedded
polymorphic subobject, rather than a byte-sized boolean field in this
constructor. Its final registration argument is now confirmed as the field
offset; the other numeric argument's semantics and the serialized property
mapping remain unresolved. In the inspected update body (`0x00487950`
onward), `+0x1c2` is read as the mode and no access to `+0x15c` occurs. This
narrows the evidence; it does not prove that no other method reads or updates
the subobject.
