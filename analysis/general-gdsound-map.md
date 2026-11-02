# General WGM `GDSound` attachments

This is a static map of the 103 `GDSound` nodes in the installed
`general.wgm` BRTR tree. Each node has one property `0x94` whose eight-byte
payload begins with `GSMP` and contains a group/sample reference. The low 24
bits identify a local `SBNK` row; all 103 observed references use group 0.
The 46 distinct referenced IDs cover every local general-bank row
(`0x1031f`–`0x1034c`). This establishes the scene-to-bank resource links, not
which runtime event triggers each sound or how the audio is played.

Executable class registration at `0x0044b9a0`–`0x0044bd00` provides member
names and offsets for the ten `GDSound` fields. Comparing registration order
with the ten serialized properties `0x94`–`0x9d` yields this mapping:

| Property | Registered member | Object offset | Installed values in `general.wgm` |
| ---: | --- | ---: | --- |
| `0x94` | `m_SampleID` | `0x84` | `GSMP` reference |
| `0x95` | `m_MinRange` | `0x8c` | f32 `1000.0` |
| `0x96` | `m_MaxRange` | `0x94` | f32 `10000.0` |
| `0x97` | `m_Volume` | `0x9c` | f32 `0.0` or `1.0` |
| `0x98` | `m_Pitch` | `0xa4` | f32 `1.0` |
| `0x99` | `m_Pan` | `0xac` | f32 `0.0` |
| `0x9a` | `m_Loop` | `0xb4` | u32 `0` or `1` |
| `0x9b` | `m_3D` | `0xbc` | u32 `0` or `1` |
| `0x9c` | `m_AutoDestroy` | `0xc4` | u32 `0` or `1` |
| `0x9d` | `m_VoiceOver` | `0xcc` | u32 `0` in all 103 nodes |

The property-name association is supported by the registration strings and
the matching serialized values. The names suggest range, volume, pitch, pan,
looping, 3D, auto-destroy, and voice-over controls, but runtime effects of
those fields have not been observed. All installed nodes use pitch 1, pan 0,
min/max ranges 1000/10000, and voice-over 0. Volume is 1 except where its
serialized value is 0.

Immediate parent classes among these nodes are:

| Parent class | `GDSound` nodes |
| --- | ---: |
| `WackyAuto` | 70 |
| `GDStdBrick` | 13 |
| `GDModel` | 8 |
| `DragonBreath` | 6 |
| `GD3dSprite` | 4 |
| `WSplash` | 1 |
| `WKBumm` | 1 |

The full object ID, parent object ID/class, raw group/sample ID, and decoded
field values are in [`general-gdsound-map.tsv`](general-gdsound-map.tsv).
Parent class is the direct BRTR parent, not a confirmed gameplay event name.

This WGM/GSMP route is distinct from the executable's `Voice/%s` lookup and
the installed `Voice/*.snd` files. Although `m_VoiceOver` is a registered
GDSound field, all these installed nodes set it to zero; this does not resolve
the numeric voice lookup arguments, the 34 absent voice basenames, or the
`.snd` channel/state assumptions.
