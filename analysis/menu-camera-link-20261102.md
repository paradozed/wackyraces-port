# Vehicle selection camera link trace

This trace narrows the missing camera binding for the vehicle-selection page.
It uses the installed `menu.WGM` and static disassembly of the installed
`wpc.exe`; it does not claim runtime observation.

## Serialized scene

The bounded BRTR walk finds two root `GDCamera` nodes, objects `0x2` and
`0x3c`, and no `GDViewport` instances. The two cameras have nearly identical
FOV, projection values, and transforms. They therefore remain camera
candidates, but the WGM does not provide a serialized viewport-to-camera link.

Three `WMenuNodeCarSelection` nodes occur at IDs `0x14a`, `0x1a0`, and
`0x381`; the first two are children of object `0x144`, and the third is a child
of `0x380`. Their three consecutive properties serialize as `(28, 1, 0)`.
The class registration order and field offsets identify these as
`m_MenuPageBrickId` at `+0xac`, `m_bSelectLeftRight` at `+0xb4`, and
`m_MenuCamera` at `+0x8c`, respectively. Thus the page link names page object
`0x1c`, horizontal selection is enabled, and the optional `m_MenuCamera`
reference is null in all three instances. Directly treating property `0x1a0`
as a camera ID was incorrect; it is the page-brick link.

The relevant registration sites are `0x0048c3f0` (`m_MenuPageBrickId`),
`0x0048c450` (`m_bSelectLeftRight`), and `0x004900f0` (`m_MenuCamera`). The
class vtable routes its entry/exit callbacks through thunks at `0x004039a4`
and `0x00402504` to `0x004902b0` and `0x004904c0`. Both callbacks inspect the
object pointer at `[this+0x18]` and its field at `+0x88`; when that field is
non-null they change global menu/render state. This points to a parent-owned
preview context, but the field's type and how it binds a viewport are not
established by this static trace.

A second bounded BRTR walk resolved the serialized parents of those three
nodes. `WMenuNodeCarSelection 0x14a` and `0x1a0` are direct children of
`WMenuNode 0x144`; `WMenuNodeCarSelection 0x381` is a child of
`WMenuNodeSelectCShip 0x380`, which is itself a child of `WMenuNode 0x144`.
`WMenuNode 0x144` is under `WMenuNode 0x141`. This makes `WMenuNode 0x144` the
common serialized ancestor of all three camera-selection nodes and a focused
runtime candidate for the context lookup. The serialized parent relation does
not prove that the callback's runtime `[this+0x18]` points at object `0x144`;
WPC's object factory or page activation may bind another runtime context.

Focused radare2 disassembly of both callbacks confirms the entry/exit path
touches the manager objects at global `0x005472b8` offsets `0x1838`, `0x183c`,
and `0x1840`, toggles render/menu flags, and reaches the `MENU.WGM` resource
loader. The callbacks read `[this+0x18]+0x88` but do not expose a concrete
`GDViewport` pointer or directly call the viewport binder. This narrows the
next trace target to the manager-owned objects and the page draw/update route
that uses them; it does not establish that the callbacks themselves perform
the car preview draw. The exported Ghidra function list omits bodies at
`0x004902b0` and `0x004904c0`, so these details come from direct x86
disassembly, not pseudocode.

The object at manager offset `+0x1838` is selected from the executable's
render-system registry. `FUN_004311c0` begins at `DAT_006aa9e4`, follows each
object's `+0xa8` link, and selects by its virtual query at `+0x0c` when a type
is requested. Base initializer `FUN_00449b20` installs vtable `0x00511c50`,
links the object into that registry, and sets default output dimensions to
`0x280 x 0x1e0` (640x480). `FUN_0044a1f0` stores a supplied viewport at render
system `+0xbc`, reads the viewport's active camera at `+0x186`, and updates the
camera-dependent backend state. This identifies the manager slot's subsystem
and the renderer field that holds the active viewport, but not which viewport
the car-selection draw supplies.

`FUN_004b8550` is a separate frame setup method reached through a renderer
vtable. It accepts a viewport-like object, reads its active camera at `+0x186`,
computes the viewport rectangle and projection scale, initializes depth/color
buffers, and starts `FUN_004baa30`. That makes it a candidate 3D scene-render
entry, but the call path from page `0x1c` to this method is not established.

## Port implication

The native renderer must not pass camera `0x2` or `0x3c` directly based on the
`m_MenuPageBrickId` value. The optional camera reference is null, and the WGM
contains no viewport node. WPC appears to obtain its preview context through
the menu-node parent/runtime state, then binds an engine-created viewport and
camera through `FUN_0044a1f0`; the projection routine reads that active
viewport in `FUN_004354a0`. Directly projecting the selected model with either
root camera omitted this parent/runtime transform and produced a tiny,
off-center car. The fixed-fit native preview therefore remains in use until
the parent context and runtime viewport binding are recovered.

The next source-level question is whether the common serialized parent
`WMenuNode 0x144` is also the runtime object at `[WMenuNodeCarSelection+0x18]`
during page entry and what that object's `+0x88` field controls. A Wine runtime
capture of those fields and the viewport's `+0x186` camera pointer would settle
the association before further camera changes.

## Projection builder details recovered from code

The projection math itself is now explicit in `FUN_00435410`. It zeroes a
4x4 matrix, computes `tan(FOV * 0.5)` from camera field `+0x156`, and writes:

```text
P[0]  = sqrt(camera[+0x15e] * 0.0075) / tan(FOV / 2)
P[5]  = 1 / (sqrt(camera[+0x15e] * 0.0075) * tan(FOV / 2))
P[10] = far / (far - near)
P[14] = -(far * near) / (far - near)
P[11] = 1
```

The adjacent GDCamera reflection stubs resolve all four values by name:
`m_nFOV` is registered at `+0x152`, `m_nRatio` at `+0x15a`, `m_NearPlane` at
`+0x162`, and `m_FarPlane` at `+0x16a` (stubs `0x004206d0`, `0x00420730`,
`0x00420790`, and `0x004207f0`). The projection routine reads these four
values at `param_2 + 0x156`, `+0x15e`, `+0x166`, and `+0x16e`, respectively:
each read is four bytes after its reflected offset. The consistent difference
indicates an adjusted pointer or property-wrapper base in this call path; the
precise pointer adjustment is not yet identified. The serialized GDCamera
properties `0x2c`–`0x2f` are `1.04719758`, `100`, `2`, and `20000`, which align
in order with FOV, ratio, near plane, and far plane. Therefore the scale input
is `m_nRatio`, while its `0.0075` normalization is an executable constant.

This recovers the named projection inputs and matrix formula, but it does not
identify which viewport/camera transform is active on the car-selection page.
Keep the local fitted camera until that runtime binding is recovered.

## Renderer dispatch follow-up

The `GDModel` constructor at `0x0043f1d0` installs vtable `0x00511478`, and
stores class ID `0x80062` at object offset `+0x12a`. Its virtual slot `+0x40`
lands at `FUN_0043f820`, which only returns the value at `+0x122`; this is a
class/property accessor, not a model draw routine. Treating that slot as the
missing render callback would therefore be incorrect. The class registration
sets the serialized type name `GDModel` and FourCC `GMDL` separately from that
runtime ID.

The primary renderer vtable at `0x00514c74` has its `+0xb0` slot routed through
thunk `0x00402720` to `FUN_004ae060`. That method takes a viewport, binds it
through `FUN_0044a1f0`, reads the active camera, and updates projection state.
The viewport-taking frame setup `FUN_004b8550` invokes this slot indirectly,
then reads viewport dimensions and camera fields and calls `FUN_004baa30`.
Its entry thunk `0x00404cfa` appears as slot zero at `0x00514e60`, a secondary
renderer/interface table embedded in the larger renderer vtable layout. This
locates the handoff between frame setup and viewport binding more precisely,
but still does not identify the caller that submits page-28's scene or model
objects. No page-to-`FUN_004b8550` edge is established by the static xrefs.

The beginning of `FUN_004baa30` resolves the object ID stored at
`viewport +0x172` through the global object table (`FUN_0041b660`). If present,
it configures the resolved object's render-list flags and calls the renderer's
vtable slot `+0x40` with that object and a render-context buffer. This is
consistent with `+0x172` being a scene/root-object reference used during a 3D
pass. It does not prove which WGM object fills that field for the vehicle page;
the page has no serialized `GDViewport`, so that reference must be assigned by
runtime setup or a different viewport instance.

The `GDViewport` constructor at `0x00453ac0` independently initializes
`+0x172` (scene object ID), `+0x152` (serialized camera reference), and
`+0x186` (resolved camera pointer) to zero. `FUN_00453e90` handles the camera
reference: it stores the incoming handle at `+0x152`, resolves the camera via
`FUN_00433090`, and stores the pointer at `+0x186`. `FUN_004540b0` invokes
that setter when the serialized camera-reference field is nonzero. This
confirms that viewport camera assignment is a two-stage handle-to-pointer
binding; it does not reveal who assigns the separate scene ID at `+0x172` on
the vehicle page.

The renderer object's constructor chain gives a more precise table boundary.
`FUN_004addf0` calls `FUN_00449b20`, which installs base vtable
`0x00511c50`, links the render system into the global registry, and sets the
default 640x480 dimensions. The derived constructor then installs primary
vtable `0x00514c74`. The table at `0x00514e60` starts `0x1ec` bytes after that
primary table and its first entry is thunk `0x00404cfa` to
`FUN_004b8550`. This is consistent with an adjacent secondary interface table,
but the constructor does not directly store `0x00514e60` in the exported
pseudocode. A byte search finds no absolute store/reference to that table, and
the only direct code edge found to `FUN_004b8550` is the thunk. This still does
not identify the runtime object or page-render dispatch that reaches it.

The viewport scene handoff also should not be conflated with `GDModel`'s own
vtable slot `+0x40`: the former is a renderer method; the latter is an accessor
that returns a class/property value. Their identical slot offset belongs to
different vtables and different object types.

The next trace should follow the caller of the interface slot at
`0x00514e60`, identify who populates the runtime viewport's `+0x172` scene
reference, and determine whether the page root, selected `WMenuCarItem`, or
both are submitted. Until that dispatch is connected, the native fitted
preview remains preferable to guessing which serialized camera or page-level
model belongs to the preview.
