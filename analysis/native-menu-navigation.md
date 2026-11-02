# Native menu navigation: recovered evidence

Target: installed `WPC.EXE`, SHA-256
`e770a5ea14d19f8d9b3692645f22e6ea83468dc0e14565b7dc7e45cf3640850b`.
The findings below come from direct installed-file byte reads, the existing
Ghidra export, and focused radare2 disassembly. They are static observations,
not original-runtime event traces.

## ACTN and SCRT

`ACTN` is a serialized action object, not a per-item Enter command list. Its
payload is `class_token:u32, action_id:u32, PRPS`, where `PRPS` contains the
same typed `PROP` framing used for brick properties. The class token's low
12 bits index `CLAS/ACCM`, whereas brick and screen tokens index `CLAS/BRCM`.

The loader at **0x0041ac40** reads the action class token, resolves the action
descriptor through table `0x006efa94`, creates an object through
`0x0045cd10`, and calls the generic property loader **0x0041bd20**. The brick
loader **0x0041fe80** dispatches an `ACTN` to that loader and attaches its
result through **0x0041fa00**. Attachment appends the action ID to the brick's
action list at `+0x2c`, writes the owner brick ID to action `+0x0c`, and uses
action `+0x10` for the next action ID.

Direct traversal of installed `menu.WGM` finds:

| Action class | Count | Attachment |
| --- | ---: | --- |
| `WMenu` | 1 | Brick `0x13e`, action `0x13f` |
| `WMenuAnimation` | 1 | Brick `0x220`, action `0x221` |
| `WSpriteMove` | 7 | Common road sprites |
| `WRot` | 14 | Vehicle preview models |

The `WMenu` action stores highlight R/G/B as properties `0x1a5`, `0x1a6`,
`0x1a7`, with values 240, 240, 0. Property `0x1a4` is its maximum idle count
(800 in this archive). No main-menu item has its own serialized `ACTN`.

`SCRT` is a separate brick-class tree: root `GDScreen` ID `0x3` with two
`GDViewport` children IDs `0x25` and `0x3a`. It is not a script-command table.
The current parser exposes `Node.actions`, preserves raw ACTN chunks in
`extra_chunks`, and exposes `Scene.screen()`. `Scene.find()` searches both
trees and actions. All **1,017 IDs** are distinct across 991 BRTR bricks,
23 actions, and three SCRT nodes in the installed menu. Duplicate IDs across
these scopes are rejected. Decoding an action does not implement its behavior.

## Descriptor versus runtime instance

Several older notes conflate class/serialized descriptors with the larger
runtime objects. The binding helper **0x0041b180** creates a runtime object
through the descriptor's virtual allocator, then stores the descriptor at
runtime `+0x18`. It stores the class pointer at runtime `+0x04` and links the
new object to the descriptor through descriptor `+0x1c`.

For `WMenuNode`, descriptor construction at **0x0048bf30** installs vtable
`0x00513970` in an allocation of `0xac` bytes. Its allocator at
**0x0048c250** creates a **0x12e-byte runtime instance** using constructor
**0x0048c4f0**, which installs vtable **0x005139a4**. This explains the valid
runtime state fields `+0x122` and `+0x12a`; they do not exceed the runtime
allocation. Reads of `[runtime+0x18]+0x98` access the descriptor's start code.

`WMenuPage` constructor **0x0048b2e0** installs its actual runtime vtable at
**0x00513884** (instruction **0x0048b32a**), not `0x00513860`. The latter
address belongs to the adjacent descriptor/reflection table. Vtable slots
must be interpreted from the address actually installed in each object.

## Selection and transition behavior

The `WMenuNode` runtime vtable `0x005139a4` supplies:

| Slot | Body | Observed operation |
| --- | --- | --- |
| `+0x38` | `0x0048c700` | Remove current-item highlight when leaving |
| `+0x3c` | `0x0048c750` | Layout/render linked page and current-item highlight |
| `+0x40` | `0x0048c8b0` | Handle navigation, Back and Accept; return transition result |
| `+0x44` | `0x0048c030` | Write an indexed child descriptor's active state |
| `+0x48` | `0x0048c0a0` | Read an indexed child's active state; missing child defaults to 1 |
| `+0x54` | `0x0048d0a0` | Base Accept callback, a no-op |

**0x0048cab0** changes the selected row, wraps at the linked page's item
count, and skips rows whose indexed menu child has active state zero.
State two remains selectable. Left/Right invoke the selected visual item's
`+0x50/+0x54` hooks and the node's `+0x58/+0x5c` hooks rather than acting
like Accept; `m_bSelectLeftRight` changes which directions navigate rows.

**0x0048c8b0** invokes the selected visual item's `+0x4c` hook and the menu
node's `+0x54` callback before returning result two with the selected index.
Back returns one unless `m_bNoChangeUp` forbids it. Some start codes also
perform game/language side effects in this body; these are not generic page
transitions and must be recovered individually.

The manager **0x0048fac0** interprets the result:

```text
1: select the current menu node's menu-node parent, if present
2: select the indexed menu-node child, or first child if m_bOneWay
   reject the transition when the chosen child active state is 2
3: search from the menu root for the configured start code; fall back to root
```

On a change, it calls the old runtime node's `+0x38` hook, assigns the active
node, and calls the new node's `+0x3c` hook. It updates global current start
code `0x006f41bc` from the new descriptor's `+0x98` field. The recursive
start-code selector is **0x0048fe10**.

`WMenuNodeOnlyAction` and related subclasses have a runtime `+0x40` method
at **0x0048d2a0** that invokes their `+0x54` callback and returns two. The
manager can therefore traverse these intermediary nodes automatically.
Subclass callbacks perform game-specific operations. For example,
**0x0048d7b0** (`WMenuNodeLoadTrack` callback) selects the return start code,
handles controller state, chooses the original course resource name, and
requests its load. **0x0048d510** (`WMenuNodeLoadFile`) requests a different
load flow. Replacing these callbacks with ordinary page changes would omit
required game state and file operations.

## Confirmation and vehicle callbacks

`WMenuNodeYesNo` is registered at **0x004923e0**, descriptor metadata
`0x005143b8`; allocator **0x00492430** installs runtime vtable
**0x005143dc**. The superficially nearby vtable `0x00514288` instead belongs
to `WMenuNodeStartMenu`: its method at `0x00491890` writes the configured
start-code target and returns manager result three.

The YesNo runtime slots are `+0x38 -> 0x0048c700`,
`+0x3c -> 0x004928b0`, and `+0x40 -> 0x004928f0`. Entry sets selected index
to **one** before invoking the base page renderer, so confirmation starts on
the second/No row. Its input method first invokes the base input handler, then
acts only on result two (Accept). Ordinary confirmation behavior is:

```text
Accept index 1: return result 1, to the parent
Accept index 0:
    descriptor+0xd0 == 0: search start code descriptor+0xc8, return result 3
    descriptor+0xd0 != 0: return result 2, enter the indexed child
otherwise: return 0
```

Consequently, a Back result from the base handler is consumed by this wrapper;
mapping Escape directly to parent here is an additional native behavior, not
what this static routine implements. Start code `0x22` has a separate
championship-related branch: No searches start code one; Yes computes course
state and searches start code seven.

Vehicle acceptance is directly established by **0x004893c0**. The
`WMenuCarItem` runtime's `+0x4c` hook receives a player-selection flag. For
`0x800000` it writes the selected item's descriptor value at `+0x98` to
application `0x005472b8`'s `+0x1a68`; for `0x1000000` it writes the same
value to `+0x1a6c`. This is the value wrapped by the descriptor member at
`+0x94`, serialized as property `0x18c`, and therefore the vehicle enum rather
than the selected menu-row index. The installed menu order is
`[6, 3, 5, 1, 2, 4, 0]`.

The two-player entry method **0x0048e1c0** initializes two selection cursors,
calls **0x0048c110(2)** to configure the two-player viewport arrangement,
and clears two ready flags. Input handler **0x0048e320** waits for both ready
flags, invokes each selected car item's accept hook with its respective
player flag, then invokes the menu-node accept callback and returns result
two. Merely treating this page as a single selected row omits this readiness
protocol.

## Verification and remaining boundaries

`tests/native_scene.cpp` now performs **190 synthetic checks**, including
ACTN class-namespace separation, preserved action bytes, SCRT children, global
duplicate IDs, malformed ACTN/SCRT extents, and failed-reload retention. Its
optional installed-file arguments report the observed 991/23/3 tree counts
and all seven localized string tables. GCC warnings-as-errors and ASan/UBSan
passed; leak detection was disabled because LeakSanitizer cannot operate
under this host's ptrace environment.

The controller must still supply class-specific callbacks for profile/save
selection, game launch, car/track selection, controller configuration and
other special nodes. Static framing and base traversal do not prove these
callbacks or a playable race. Animation cadence, audio hooks and runtime
comparisons of the complete menu remain separate work.
