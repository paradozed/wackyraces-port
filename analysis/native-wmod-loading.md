# Native WMOD loading and route joins

Target: original installed `WPC.EXE`, SHA-256
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`.
Evidence is the checked-in Ghidra export, focused executable disassembly and
actual WGM bytes. No original runtime execution was observed. The REA tools
were unavailable; no REA or client configuration was changed.

## API and supported format

`game_mode.h` exposes `decode_mode(ByteView)` and `load_mode(const Archive&)`.
`ModeData.definitions` preserves file definition order; `route_indices`
preserves the separate route table; `route(i)` follows that indirection.
`ModeRoute.rows` holds each definition's nested rows. `ModeRow.object_ids`
contains the three plain object IDs stored in runtime wrappers at
`+0x48/+0x50/+0x58`; `ground_object_id` names the third value.
`ModeRow.cells` retains each encoded list and its literal terminal word.
`object_links` reproduces the runtime flattened buffer, including nonempty
list terminators and excluding empty-cell terminal words.

`first_start_line_id(scene)` implements the normal first-route, first-row
scan. It returns the first linked kind-zero `WStartLine`, skipping absent
object IDs as the original does. It returns no marker when the route/row or
matching object is absent; it does not invent a battle or no-marker fallback.

The parser deliberately accepts only shipped version **0x01312ea1**. Earlier
version branches require compatibility conversion (`0x49c010`) and are not
silently interpreted as the modern layout. Unknown trailing chunks retain
their tag and payload after strict tag/length framing validation. Other
uninterpreted scalar/vector fields remain exposed under runtime-offset names.

## Serialized framing

The top-level WGM `WMOD` payload is read by **0x49b3b0**:

| Field | Bytes |
| --- | ---: |
| Version | 4 |
| Two vectors, then three words | 24 + 12 |
| Definition count | 4 |
| Definitions below | variable |
| Route count and definition indices | 4 + count * 4 |
| Transform count and affines | 4 + count * 48 |
| Optional framed child chunks | variable |

A definition is a literal 32-byte name, four words corresponding to runtime
`+0x20,+0x28,+0x30,+0x34`, row count, then rows. ID wrappers at runtime
`+0x24/+0x2c` serialize only their four-byte values: constructor **0x448ea0**
and reader **0x448ec0** establish this.

Each modern row is:

| Serialized field | Runtime destination |
| --- | --- |
| Four three-float vectors | `+04,+10,+1c,+28` |
| Four words | `+34,+38,+3c,+40` |
| Three plain object IDs | wrapper values `+48,+50,+58` |
| Count A, A vectors, A vectors | `+5c,+60,+1e0` |
| Count B, B vectors, B plain IDs | `+360,+364,+4e8` |
| One stored link-word-count word | loader temporary; preserved |
| Width * height cell lists | grid dimensions at `+3c,+40` |

The original fixed vector arrays have capacity 32; the flattened link buffer
has capacity 2048 words. The native reader checks these limits, all remaining
byte spans, count multiplications, finite floating components, and route
index references before consuming or allocating their content. It also caps
rows and total cells to avoid unbounded work on untrusted inputs.

**Ghidra's exported modern-cell branch is incomplete.** It labels the useful
block unreachable and appears to discard every link. Disassembly
**0x49b7c7..0x49b85f** proves the actual behavior:

```text
for every cell:
    word = read_u32()
    if (word & 0xfffffffc) == 0:
        cell = empty
    else:
        do:
            append word to this cell and the row's flattened buffer
            if (word & 0xfffffffc) == 0: break
            word = read_u32()
```

Thus words 0, 1, 2 and 3 all terminate lists. The low two bits of nonterminal
words encode link kind, while `word >> 2` carries the ID/packed payload.
Kind zero is the scene-object path. Kind three is resolved through action
lookup `0x41ea70` during `0x4a8450`, then rewritten at runtime as kind two;
the native file reader preserves the serialized kind-three word.

Affines are **48 serialized bytes**, not 52: **0x43ce40** calls
**0x43add0** for nine basis floats and **0x429910** for translation. The
reader inserts zero at runtime affine word nine (`+0x24`). `ModeTransform`
therefore contains basis columns X/Y/Z followed by position, matching
`affine_matrix` in the native transform helpers.

## Route selection and actual start markers

**0x49dc90** selects the first route-pointer entry, then row zero. At
**0x49e376..0x49e3ce**, it scans that row's flattened links, resolves kind-zero
IDs through `0x49b0e0 -> 0x41b660`, skips null lookup results, and matches
the `WStartLine` class getter `0x49a270`. This is not a scene traversal.

The file route table matters even when every definition has one row:

| Course | Definition indices in route order |
| --- | --- |
| `cc_t4` | 1, 0, 2 |
| `ct_t3` | 0, 2, 1, 3 |
| `dr_t2` | 1, 3, 0, 2 |
| `dr_t3` | 3, 0, 1, 2 |
| `sh_t2` | 2, 1, 3, 0 |
| `sh_t4` | 1, 2, 0 |

All other observed course route tables use sequential definition indices.
For `cc_t1`, route zero is `Snake`, row zero links marker **0x44**, and its
ground object is **WGndBrick 0x47**, whose property `0x174` links **WGNR
0x291 (657)**. All 20 race courses have a marker found through this join.
`sh_b` also has a linked marker, while the other four battle files do not;
this does not establish the battle bootstrap's route-selection policy.

## Active frames and ground-query coordinates

**0x4a8450 assigns group IDs, not matrices.** It assigns route-order index
to runtime object `+0x104`, including the row's ground object, kind-zero
linked objects and their descendants (`0x469ba0`). Definition order must not
be used for this field. Method **0x4a8680** enables linked objects and copies
row bounds through `0x4a89c0`; it does not transform the WMOD affines.

The relevant initialization copies and inverses in `0x49dc90` are:

| Operation | Exact call site |
| --- | --- |
| `transforms[route_count+1]` to application world root local affine `+2c` | `0x49dccf..0x49dcd6` |
| `transforms[0]` to first-route context `manager+8d4` | `0x49dd79..0x49dd80` |
| Last-route context from `transforms[route_count+1]` to `manager+840` | `0x49dde7..0x49ddee` |
| First ground inverse to `manager+908` | `0x49e2de..0x49e309` |

For that last operation, a nonzero row ground ID resolves the actual object's
active affine through **0x4200e0**, then **0x43c930** computes its inverse.
Only a null ground ID uses the context affine at `manager+8d4` instead.
`0x4a4860` uses the context inverse to transform the query endpoints before
calling the WGNR query. World-space endpoints cannot be passed directly to
the local WGNR query merely because `transforms[0]` happens to be identity.

There are two hierarchy caches: runtime object `+0x98` and `+0xcc`.
**0x432660** initializes the active-cache offset table at `0x5472d0` to
`0x98`. **0x432690** selects `0xcc` for wrap-related groups according to
globals `0x5472bc/0x5472c0`; **0x432970** resets the first global to zero.
**0x420110(0/1)** computes the two caches recursively. Crucially, its root
case **0x42016f..0x420193** sets root `+98` to identity and root `+cc` to
the root's local affine. Thus the normal unwrapped hierarchy excludes the
special root local transform. Children compose their local affine against
the corresponding parent cache, through **0x43c620**.

The lazy active helper **0x4201f0** also chooses a cache according to wrap
state; its separate dirty-root entry path copies local data differently.
Initialization order and the two-cache lifecycle should be retained when
porting the full runtime. Copying WMOD `transforms[route_index]` onto every
scene object would not reproduce these routines.

For `cc_t1`, composing serialized node affines below the world root gives:

```text
selected ground WGndBrick 0x47:
  basis = identity
  position = (-417.0634765625, 448.059326171875, 1057.2767333984375)

selected WStartLine 0x44:
  basis and position = those recorded in native-race-bootstrap.md
```

The primary one-player pre-query candidate is approximately
`(3630.990977,1012.059326,-6862.556219)` in the normal unwrapped frame.
Its WGNR-local endpoint is approximately
`(4048.054454,564,-7919.832952)`; the upper endpoint adds 150 to Y.
This coordinate conversion is static derivation, not a captured ground hit.
The `cc_t1` special root local source `transforms[4]` has translation
`(73386.4921875,-0.1295166015625,-31976.5234375)`, which demonstrates why
blindly prepending that root affine changes the unwrapped start location.

## Validation and limitations

Validation used the 33 disc archives under `build/original-disc/{cc,sh,dt,ct,dr,Win}`
plus installed `menu.WGM`, `SWMenu.WGM`, `general.wgm` and `swgeneral.wgm`.
All **37/37** WMOD payloads parse, with **86 definitions, 86 route entries,
86 rows, 15,052 cells, 7,021 flattened link words and 236 affines**. Every
observed definition has one row. The 25 course/battle files have nonempty
WMOD; the other 12 have the 52-byte empty form. No trailing extension chunks
occur in this corpus. No copyrighted fixture bytes were added to tests.

The test uses explicit throwing checks, including with `NDEBUG`: **395 checks**
cover every truncated prefix of a synthetic nonempty payload, route
indirection, typed terminal words, missing object IDs, original capacity
limits, count/dimension overflow, NaN/Infinity, matrix framing and malformed
or preserved extension chunks. ASan/UBSan passes the synthetic suite and
representative real files. Leak detection is disabled for that run because
LeakSanitizer reports that it cannot run under this environment's ptrace.

```sh
g++ -std=c++17 -Wall -Wextra -Werror -O2 -Isrc/port \
  tests/native_mode.cpp src/port/game_mode.cpp src/port/game_scene.cpp \
  src/port/game_assets.cpp -o build/native_mode
build/native_mode build/original-disc/cc/cc_t1.WGM
```

Pass additional WGM paths to repeat the corpus inspection. The parser makes
no claim about older WMOD versions, action execution, all wrap-state
transitions, battle/no-marker spawn fallback, or the full collision query.
The APIs expose the recovered joins without guessing those remaining rules.
