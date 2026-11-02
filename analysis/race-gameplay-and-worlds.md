# Race gameplay, worlds, and characters: evidence map

> Captures and image sheets named in this note were removed from the repository (no game content in git); the procedure described here regenerates them locally.

Historical investigation: references below to the earlier C/software port,
its inspectors and planar UV fallback do not describe the current C++/Vulkan
executable. Current verified behavior is recorded in
[native port progress](native-port-progress.md) and
[native world loading](native-world-loading.md). The `_b` files are battle
arenas, as established by the later mode trace, not course backdrops.

This note gathers the evidence relevant to understanding a race from the
installed Wacky Races files and the `wpc.exe` analysis. It is an investigation
map, not a recovered game design document. “Observed” below means visible in
a runtime capture or directly present in the installed files; class names and
control flow recovered only from static analysis are labeled accordingly.

## What a race needs to cross the menu boundary

The installed disc contains 33 world-related WGM archives: 25 files under
`cc/`, `ct/`, `dr/`, `dt/`, and `sh/` (each family has `_b`, `_t1` through
`_t4`), and eight files under `Win/`. The names suggest five families with
background/track variants and a victory family, but that role assignment is
inferred from filenames. The installed game directory itself contains only
`menu.WGM`, `general.wgm`, `SWMenu.WGM`, and `swgeneral.wgm`; the track worlds
are on the mounted disc.

All 37 checked disc and installed WGM files share the same outer `WPCG`
container framing. World/victory archives add resource families not present in
the installed menu-only examples: `GHMS` (65 resources), `MTNL` (47), `WGNR`
(80), and `GTXA` (9) across the checked corpus. `WGNR` is registered by the
executable as “Wacky Ground”; its packed bytes receive a pointer-relocation
pass. `GHMS` is registered as `GDHMesh`, and `MTNL` as `GDMotionLib`.
These names and loaders establish that world geometry, mesh-related data,
motion libraries, and texture-animation links exist in the asset model. They
do not yet tell us how a course is assembled, how its route is traversed, or
which resources belong to which gameplay object. See
[`resource-format-index.md`](resource-format-index.md) and
[`wgm-container.md`](wgm-container.md) for the byte-level limits.

## First-course BRTR class inventory

Running the bounded native inspector on the exact archive identified in the
first-race Wine file trace, `/run/media/nicolas/Wacky/cc/cc_t1.WGM`, produced
549 BRTR object instances. The current `--inspect-wgm` reports class
frequencies when the requested menu page is absent:

| Class | Instances |
|---|---:|
| `GDPath` | 1 |
| `WTrackCamera` | 5 |
| `GDModel` | 28 |
| `WGndModel` | 381 |
| `GDHModel` | 1 |
| `WGndSprite` | 88 |
| `Zartter` | 2 |
| `WPowerUp` | 25 |
| `WPortal` | 6 |
| `WBendSign` | 8 |
| `WStartLine` | 1 |
| `WGndBrick` | 3 |

These are parsed class-table names and instance counts from this one archive,
not proof of each class's runtime behavior. The 381 ground-model links and
their serialized transform arrays are now mapped to GMDL resources. The native
preview now appends all 88 type-0 WGndSprite quads to the combined mesh, using
their serialized center, scale, aspect, angle, material and composed transform.
The first course has no other sprite types. This adds sprite texture alpha and
depth testing through the software rasterizer, but does not yet match WPC's
render settings or prove exact scene ordering. `WGNR` is statically connected
to the collision/ground query path; its spatial grid, polygon spans, packed
planes, and vertex indices are mapped. The native race loop uses WGNR polygons
with sufficiently vertical normals for ground-height sampling and falls back
to visible scene meshes. Movement sweeps four rotated X/Z points against WGNR
polygon planes, stops at the earliest segment hit, and iteratively projects
remaining horizontal motion against the normals reported by all four sweeps.
The footprint uses the mode-2 layout from WPC's `FUN_00468280`: lateral
offsets of +/-52, longitudinal offsets of +/-102, and a speed-dependent
extension of the forward pair above normalized speed 0.4. The native query
also applies WPC's `0x01e000ff` record mask. It still uses a general polygon
intersection for the two matrix-indexed records that WPC dispatches through
`FUN_004834e0`; the other alternate records now use the recovered quad-edge,
directional and vertical checks. WGNR queries retain separate nearest hits
for ordinary records and records marked `0x80000000`, matching WPC's two
contact-record regions. The race controller projects both returned normals,
though its multi-contact solver remains an approximation of WPC's aggregation
and impact response. Generic `FUN_00482b30` intersections now apply WPC's
one-way denominator gate and `0x01000000` reverse-direction flag. The installed
mounted-disc collision records contain no query-eligible generic entries with
that reverse flag. Special quads marked `0x20000000` still use the native
plane-crossing approximation instead of WPC's per-edge clipper.
Runtime evidence is needed to establish how often WPC recalculates these points
and compare the native footprint in motion.
Gameplay draw/update paths also remain open. The inspector's
class-frequency output is an implementation aid, not a complete resource
schema.

### `WPortal` evidence boundary

The installed `wpc.exe` has a static class-registration stub at `0x0049a8a0`
that records the reflected class name `WPortal`, vtable pointer `0x00514a30`,
and type token `0x34598283`. This confirms that the six BRTR instances are of
an executable-registered class; it does not identify a checkpoint role or
their collision/update behavior. The recovered `WCtrlRoboCar` candidate scan
filters `WPowerUp` objects, so that path does not establish portal traversal.
The native lap approximation therefore continues to use the serialized
`WStartLine` plane; portal-based lap or checkpoint logic remains unimplemented
until a call path or runtime observation establishes what these nodes do.

The sole `GDPath` node in `cc_t1.WGM` is object `0x11`. Its `PRPS` block has
property IDs `0x7c` (28 value bytes), `0x7d` (4), `0x1a` (4), `0x1b` (48),
`0x1d` (4), and `0x1f` (4). Property `0x7c` starts with the u32 value `2` and
has six following float words, which could encode two XYZ points; this
interpretation is not confirmed. Property `0x7d` is zero. The executable
registers the reflected member name `m_idPath`, but its serialized property ID
and how it relates to `GDPathQ` are not yet established. This node alone does
not provide enough evidence to implement the full race route or lap progression.

The native scene reader now extracts the first `WStartLine` and first
`WTrackCamera` transforms from the same BRTR graph after composing their
parents. `--inspect-wgm cc_t1.WGM` reports the start marker at
`(3246.200, 1012.059, -6313.069)` and the first camera at
`(4494.234, 1348.828, -3675.033)`, with FOV `1.04720` radians (60 degrees).
These values are serialized data after the current parent/local matrix
composition; the composition convention and which of the five cameras is
used for a race remain unverified. The marker is a useful race-bootstrap
anchor, but it does not establish car spawn spacing or heading.

Static reflection in `wpc.exe` registers `WStartLine::m_rRadius` at instance
offset `+0x84` (registration stub `0x0049a2b0`). The installed track files carry
a four-byte float at serialized property `0x1df` on their `WStartLine` nodes;
observed values range from 1500 to 9300, with most at 5000. The C port parses
that property with finite/range checks and limits its approximate lap-plane
crossings to that lateral distance. The class/member and property pairing is
strongly supported by the reflection string and consistent track data, but
the serializer's property-ID mapping is not traced end to end. No matched
runtime lap crossing has yet validated the gate's semantics.

Each environment also has a companion `_b.WGM` archive on the mounted disc.
The bounded reader finds 223 supported model placements and 27 `WGndSprite`
nodes in `cc_b.WGM`. The executable's five-entry `*_b.wgm` table is selected
when application field `+0x1a60` equals 1. The two-player mode page in
`menu.WGM` assigns that value to `MODE AFFRONTEMENT`, and the menu callback
copies the selected value into `+0x1a60`; this identifies `_b` as the battle
mode resource family. In contrast, `cc_t1.WGM` contains a `WStartLine` and
`cc_b.WGM` does not. A native trial drew the `_b` scene behind the `_tN` course
and placed large objects across the course; that incorrect composition was
removed. The trial capture is
`native-race-backdrop-20261023.png`.
Battle arena behavior and the use of the other four `_b` files remain
unimplemented. See [`disassembly/wpc-game-mode-values.md`](disassembly/wpc-game-mode-values.md).

## First-course ground-model links and transforms

A bounded read of all 381 `WGndModel` instances in `cc_t1.WGM` found the same
serialized shape on every instance: property `0x67` is an eight-byte `GMDL`
resource link, and property `0x1b` is 48 bytes decoding as twelve finite
little-endian floats. The values fit a row-major 3x3 linear part and three
translations, matching the arrangement already used by the C mesh transform
helper. For example, object `0x48` links to `GMDL 0x184` and carries an
identity linear part with translation approximately `(6274.58, 340.88,
-3255.55)`. Its parent is `WGndBrick 0x47`. Nested ground models commonly have
ground-model parents, so their transforms must be composed through the scene
hierarchy before geometry is placed in world space.

The 381 links resolve to 378 distinct GMDL IDs in the same archive; every
referenced resource was present. Their vertex streams contain 18,678
positions. The complete MATS arrays refer to 81 distinct GMAT resources, whose
TEXP links resolve to 31 GTEX resources in this archive. Diagnostic
composition of parent and child matrices as `parent * local` gives approximate
world bounds X `[-76,398, 22,341]`, Y `[-16,668, 6,411]`, Z `[-15,218,
27,234]`. These bounds are derived, not an original-runtime observation: the
matrix storage shape and resource links are directly observed, while matrix
composition order still needs executable or matched-frame confirmation. Static
call sites connect WGNR to collision/ground lookups; its grid traversal,
polygon records, vertex and plane arrays, and selected record flags are
recovered, but the world transform and full collision response remain
unresolved. See
[`disassembly/wpc-wgnr-collision.md`](disassembly/wpc-wgnr-collision.md).

The executable's GMDL reader allocates 20 bytes per TSET row and reads five
32-bit fields. Across all 37 installed and mounted WGM archives, 7,010
GMDL/GHMS resources have TSET tables whose material indices and vertex ranges
stay within MATS/VERT, and whose triangle ranges cover TANG exactly without
overlap. This supports the inferred row fields: material index, vertex
start/count, and triangle start/count. The C mesh reader now preserves the
material ID on each triangle, resolves the Dingo selection model's GMAT/TEXP/
GTEX chain, and draws that model through per-material SDL surfaces. The same
material path is available for the course meshes, but their scene transforms,
WGNR dependencies, camera and runtime draw order still need reconstruction.

The C port now has a bounded `world_scene` reader for `WGndModel`, `GDModel`,
and `GDHModel` BRTR nodes. It follows each 48-byte transform through its parent
chain, collects the property `0x67` GMDL or GHMS links, assembles the meshes,
and resolves the referenced GMAT textures. The current composition order is
`parent * local`, still an inference. Running the existing native inspector on
`cc_t1.WGM` loads 410 placements: 381 `WGndModel`, 28 `GDModel`, and one
`GDHModel`. Twenty material IDs currently use flat-color fallback because their
GMAT `TEXP` records contain only a zero u32. These materials are intentionally
untextured in the serialized data; every non-null course texture link in this
scene resolves and decodes. `COLS` contains three RGB float triplets; static
disassembly of the material state updater at `0x00433d40` shows that WPC packs
the middle triplet into `grConstantColorValue`, confirming its use as the
constant material color. The updater also packs alpha from `1 - TRAN`. The
native loader and software rasterizer now preserve that constant alpha for
textured and untextured triangles as a provisional opacity rule. The renderer's
base `grAlphaCombine` outputs texture alpha, and the final interaction with
per-material blend flags has not been recovered. Texture-combine, lighting,
and full rendered-race parity remain open.
The focused static trace is documented in
[`disassembly/wpc-material-state.md`](disassembly/wpc-material-state.md).

The same archive contains 88 `WGndSprite` instances. Each has an eight-byte
`GMAT` link in property `0x9` and a 48-byte transform in property `0x1b`; those
links reference 23 distinct materials. All 88 type fields are zero. Properties
`0x170`–`0x173` are also four-byte values on every instance, but their roles
are not established. The native reader maps the ordered scalar properties
through render setting, resolves the linked materials, and appends the type-0
quads to the track mesh. Its center, scale, aspect, angle, and composed
transform drive the geometry. The software renderer applies decoded texture
alpha and the same depth buffer as the terrain; render-state parity and exact
ordering are not established. The bounded inspector confirms all links load
without missing-texture fallback. All 37 installed/disc WGM files pass
`--inspect-wgm`; all 26 course archives load their bounded model/material data.

The `WGndSprite` vtable's draw slot is traced statically to `0x00419b90`: it
resolves a material surface, constructs four 12-byte vertices, and submits the
geometry through the world renderer. The method falls back to surface H/W when
aspect is zero and applies scale, center and angle before submission. Exact
matrix, blend and depth behavior still need runtime comparison. See
[`disassembly/wpc-wgndsprite-draw.md`](disassembly/wpc-wgndsprite-draw.md).

The `sh_t3.WGM` course adds 187 `GD3dSprite` records. Static executable
reflection registers the same eight fields observed in the WGM records:
`m_MaterialID`, `m_SpriteType`, `m_CenterX`, `m_CenterY`, `m_Scale`,
`m_AspectR`, `m_Angle`, and `m_RenderSetting`. The native reader therefore
shares the bounded sprite-property parser across `WGndSprite` and `GD3dSprite`:
property `0x9` is a `GMAT` link, `0xa` the type, and `0xb` through `0x10` are
four-byte values in the field order above. The record also contains a
four-byte property `0x11`; its meaning is not established, and the loader
preserves no behavior for it. `--inspect-wgm` reports the combined record
count. The renderer currently appends only type-0 quads with a resolved texture;
the record count does not imply that all 187 are rendered. The shared draw
semantics and render-state behavior for `GD3dSprite` have not been independently
confirmed at runtime.

Across the checked corpus, five GMDLs are empty placeholders (zero MATS, VERT,
TANG, and TSET counts). Five other meshes in `dt_t4.WGM` have UVUV values
containing the exact `0xcdcdcdcd` uninitialized-heap pattern. The C mesh reader
recognizes those sentinel words and constructs a planar UV fallback by
projecting onto the two largest vertex axes. That projection has not yet been
compared with WPC's rendered texture orientation.

That load exposed two installed format cases. GMDL `0x1d5` has positions and
normals but no `UVUV` stream; meshes with this shape retain zero UVs until the
executable's alternate coordinate path is understood. GMDL `0x217` has
repeat UV values outside the normalized range, reaching roughly `-307..235`;
the software rasterizer now wraps those coordinates while leaving UVs in
`[0,1]` unchanged. Its material links to `GTEX 0x30`, whose indexed TGAF has a
495-byte TGA 2.0 extension area and a one-byte-short outer length field. The
decoder validates the extension offset, size, footer signature, and trailing
zero for this observed variant. These findings do not establish that every
world texture variant is covered.

The `WTrackCamera` instances in `cc_t1.WGM` have serialized transforms in
property `0x1b`; their translation rows place them at distinct points around
the course. Across all 92 cameras in the mounted disc's 20 track WGM files,
properties `0x2c`–`0x2f` are present and invariant: FOV `1.04719758` radians,
ratio `100`, near plane `0.5`, and far plane `20000`. Static reflection names
these fields `m_nFOV`, `m_nRatio`, `m_NearPlane`, and `m_FarPlane`; the
projection routine's accesses are four bytes past the registered offsets.
The native reader validates and retains the tuple, and the rasterizer applies
the recovered ratio-dependent X/Y projection and clips against the serialized
near/far planes. Camera selection and the chase-camera transform remain
unverified. See
[`race-camera-projection-native-20261023.md`](race-camera-projection-native-20261023.md).

Static startup code chooses between `menu.wgm` / `general.wgm` and
`swmenu.wgm` / `swgeneral.wgm` based on global state. The installed track
archives are not loaded by the observed menu path, and the state that selects
world archives has not been recovered. The menu has registered action classes
named `WMenuNodeSelectTrack`, `WMenuNodeLoadTrack`, and `WMenuNodePostRace`,
but these names alone do not prove which menu choice loads which archive or
what transition follows.

## Race and character class anchors

Executable reflection strings register the following names:

| Name | What static evidence supports | Still unknown |
|---|---|---|
| `WCtrlRoboCar` | A reflected class is registered at `0x004e39b0`; the name string is referenced at `0x004e39ce`. | Whether this is the player controller, opponent controller, or a shared controller; movement, input, AI, and update behavior. |
| `WackyAuto` | Registered at `0x00464390`; adjacent factory `0x004643e0` allocates `0x953` bytes and calls constructor `0x004660b0`, which installs vtable `0x0051295c`. Its reflected fields include `m_iVehicle` (`+0x84`), `m_inputchannel` (`+0x8c`), and `m_gravity` (`+0x94`). The per-frame routine at `0x0046a4a0` calls `0x00466db0` at `0x0046a719` and `0x00467640` through thunk `0x00404b1a` at `0x0046a720`. It adds three displacement components to actor fields `+0x288/+0x28c/+0x290` and three rotation components to angles `+0x267/+0x26b/+0x26f`. `+0x3ef` is a normalized forward/reverse speed state raised and lowered by two configured masks; `+0x403` is a bounded steering-angle state adjusted by two other masks. Both contribute to the actor transform. | Exact physical-key bindings, world units, collision response, and whether this actor is the serialized car or a vehicle subsystem remain open. |
| `WTrackCamera` | A reflected class name is referenced at `0x0047684e`. | Camera target, follow rules, smoothing, and player selection. |
| `WStartLine` | A reflected class name is referenced at `0x0049a03e`; the native port uses its serialized transform and radius for the approximate start/lap plane. | Its relation to WPC's 3-2-1 / `PARTEZ!` countdown is unproven; the countdown reads a separate `this+0x18` object's `+0x34` field. See [`race-countdown-state-20261023.md`](race-countdown-state-20261023.md). |
| `WPowerUp` | The class descriptor at `0x02ab05b0` is initialized with the reflected name at `0x004d3b8e`; the `WCtrlRoboCar` candidate scan filters against this descriptor. Native scene loading retains each instance ID, composed transform, and the observed four-byte `0x28b` value. Reflection registers `m_poweruptype` for this class; the `0x28b` to member link is strongly supported but not traced through the serializer. | The active flag, pickup radius, collection transition, effect mapping, and cooldown. |
| `m_nLapNum` | The registration stub at `0x0041d420` passes the member name `m_nLapNum` and runtime field offset `+0xa4` to the common property-registration helper (`0x0040520e`). | Owning class, unit/index convention, lap counting rules, and finish condition. The field name and offset alone do not establish a checkpoint algorithm. |

These are schema anchors from a stripped executable, not verified character
roster entries or proof of complete feature behavior. Further static tracing
of `WCtrlRoboCar` found a race-object scan that attaches its reflected instance
at `+0x1c3`, plus helpers that select participant/camera candidates and compare
rank/order fields. `WackyAuto` reads DirectInput and its per-frame routine
updates its position and orientation. `WCtrlRoboCar+0x18` points to an object
whose initialized fields overlap `WackyAuto`'s registered field offsets,
suggesting a link that still needs a runtime descriptor check. Full
keyboard/controller action mapping and vehicle dynamics remain incomplete;
details are in
[`disassembly/wpc-robocar-control.md`](disassembly/wpc-robocar-control.md).
The available menu
capture/reference shows seven small vehicle images and two tall character
cutouts in the menu scene. Their serialized nodes and texture links are mapped
in [`menu-scene-map.md`](menu-scene-map.md); that menu scene by itself does not
identify the playable roster.

The user reports that the character-selection screen has `MAX SPEED` and
`TRACTION` indications and shows the selected character's weapon (`Dragoon`)
and super gadget (`Wings`). They also report that every character has a
dedicated vehicle. This supports character-specific vehicle/loadout selection.
The displayed stat values and their scales, and the Dragoon weapon effect, are
not yet documented. The user reports that activating Wings starts an animation
and makes the vehicle fly. A later character-selection capture is preserved at
`wine-character-select-caraverne-20261023.png`.
It identifies Caraverne / Les frères Têtes-dures with `VITESSE MAXIMALE
MOYENNE` and `TRACTION EXCELLENTE`. Weapon and super-gadget icons are visible
but unnamed in the frame; their identities and effects remain to be
established. A second capture
(`wine-character-select-turbo-terrific-20261023.png`)
shows `TURBO TERRIFIQUE` above `PIERRE DE BEAU-FIXE`, with `VITESSE MAXIMALE
EXCELLENTE` and `TRACTION MOYENNE`. A third capture
(`wine-character-select-tocard-tank-20261023.png`)
shows `TOCARD TANK` above `LE SERGENT GROSSE-POMME ET SOLDAT PETIT-POIS`, with
`VITESSE MAXIMALE MOYENNE` and `TRACTION EXCELLENTE`. The user identifies
Turbo Terrifique and Tocard Tank as characters; whether the paired name is the
vehicle label is plausible but unconfirmed. All three captures show weapon
and super-gadget icons without readable names.

The user supplied this character-selection roster:

- Max Le Rouge
- Les frères Têtes-dures
- Sergent Grosse-Pomme et Soldat Petit-Pois
- Pique et Collégramme
- Satanas et Diabolo
- Pierre de Beau-Fixe

The list is recorded as provided. Captures now map several vehicle/character
pairs and stats; weapon and gadget assignments remain unidentified.

The user clarifies that Satanas et Diabolo are not currently playable and are
unlockable later after championship mode is completed. This is a user-reported
unlock condition; whether it requires completing every championship, and
whether the pair is the only locked character, remain unverified. Therefore,
their profile is not an omitted currently selectable character.

The user reports that `DINGO-LIMOUSINE` is driven by `PIQUE ET COLLEGRAMME`.
A matching profile capture
(`wine-character-select-dingo-limousine-20261023.png`)
confirms the pairing and shows `VITESSE MAXIMALE RAPIDE` with `TRACTION
BONNE`. Its weapon and super-gadget icons are visible but unnamed.

The user reports that Max Le Rouge is paired with vehicle `BOLIDE ECARLATE`.
A matching capture
(`wine-character-select-bolide-ecarlate-20261023.png`)
confirms the pairing and shows `VITESSE MAXIMALE MOYENNE` with `TRACTION
BONNE`. The weapon and super-gadget icons are visible but unnamed.
The user also reports that `COMPACT PUSSYCAT` is driven by `PENELOPE JOLI
COEUR`. A matching capture
(`wine-character-select-compact-pussycat-20261023.png`)
confirms the pairing and shows `VITESSE MAXIMALE LA + RAPIDE` with `TRACTION
BONNE`. Its weapon and super-gadget icons are visible but unnamed.

Selecting Championship II returned the user to character selection. Caraverne
was the first captured profile with visible stat values; the later profiles
do not establish whether championship selection restricts or changes the
available character.

## Race modes reported from the game menu

The user reports three modes under one-player play and two under two-player
play:

| Player mode | Menu choices |
|---|---|
| 1 joueur | Course simple; Championnat; Entraînement |
| 2 joueurs | Course; Mode affrontement |

These are menu options reported by the user. The rules, race format,
progression, and input/screen layout for each mode have not yet been observed.

The supplied championship-selection screen
(`wine-championship-select-20261022.png`)
shows `CHOISIR CHAMPIONNAT`, five horizontal championship thumbnails, and the
first item highlighted and labeled `CHAMPIONNAT I`. Its footer shows keyboard
hints to select with the arrow keys, accept with Enter, and return with
Escape. This establishes a five-item championship selection list, but not the
event sequence, unlock rules, or how championship progress is scored.

Following the reported Championship I victory with 22 points, the user says
Championship II became unlocked. The later selector image
(`wine-championship-select-2-20261023.png`)
shows `CHAMPIONNAT 2` selected while Championships III, IV, and V remain
locked, according to the user. This is consistent with sequential unlocking,
but one transition does not establish the exact unlock requirements for the
remaining championships.

## Race flow and current evidence boundary

Runtime evidence reaches the French main menu, the options page, and the start
flow labels `CHOISIR SAUVEGARDE` and `LIBRE`. A later active Wine session
reached the first race. The preserved screenshot shows a desert scene, lap
`1/3`, `7 EME`, and timer `02:04`; captures seconds apart show an advancing
scene. The user confirmed they were playing this race. WPC's file trace records
`D:\\cc\\cc_t1.wgm`, tying this session to that loaded world archive. See
[`runtime-capture.md`](runtime-capture.md) for the screenshot and trace.

The first-race frames do not establish the course's display name, car handling,
collision behavior, pickup effects, lap-crossing rule, pause, or save behavior.
A later finish capture adds the ordered participant list and a reported
post-finish demo-camera state, described below. The earlier start transition
hang and capture limits remain recorded in
[`runtime-behavior-matrix.md`](runtime-behavior-matrix.md) and
[`runtime-capture.md`](runtime-capture.md).

The native C port now connects track selection to an early race loop. It uses
the parsed start-line translation, the selected car's serialized `GMDL`/`GHMS`
group from `menu.WGM`, and the course mesh in one depth-buffered render. The
car material IDs are remapped into a separate synthetic range because WGM
resource IDs are archive-local. Input and per-frame position/heading updates
use provisional native dynamics; the original actor's exact update constants,
spawn pose, camera follow, and vehicle scale are not recovered. This is native
integration evidence, not a claim of matching WPC race behavior. The current
implementation details and remaining race work are in
[`../c-port/TODO.md`](../c-port/TODO.md).

The native loop now advances its provisional vehicle state in fixed
`1/60`-second steps. This cadence is grounded in the `WackyAuto` constant
decoded from `wpc.exe` at `0x005129c4`; acceleration, steering, speed limits,
and the 50 ms frame-time admission cap remain port choices. The accumulator is
cleared on pause, resume, and race restart, so pause time does not enter the
race timer and render cadence does not alter an individual simulation step.
This aligns update frequency only; it does not establish WPC's world-unit
conversion or full control equations.

`FUN_00466db0` recovers WPC's brake/reverse sequence in `WackyAuto+0x257`:
state 0 subtracts the `+0x933` step (0.023) until speed crosses below zero,
then clamps to zero and enters state 1; continued braking in state 1 holds at
zero. Releasing the brake changes state 1 to state 2 while coasting reduces
speed by `0x005129d4` (0.0096) per update. Braking in state 2 subtracts the
vehicle's speed convergence factor until reaching the reverse target
`0x005129d0` (-0.4). The native race loop now models these transitions and the
`0x005129c8` (0.015) stop threshold. This is static-equation matching; a
synchronized runtime comparison of reversing remains outstanding.

The same update routine uses a two-stage positive acceleration curve. When
current speed is below the vehicle's configured top speed, it converges toward
`max(top_speed, 0x005127ec)` using that vehicle's response coefficient. The
constant at `0x005127ec` decodes to 0.8. Once the speed reaches the configured
top speed, WPC subtracts 0.0096 per update and clamps at that per-vehicle
limit. The native acceleration branch now follows those thresholds and
targets; runtime speed traces are still needed to confirm the associated
vehicle ID and world-motion scale.

Steering now has a provisional angular state. Static analysis shows
`FUN_00467640` bounds `WackyAuto+0x403` at ±40 degrees, attenuates it by 0.8
per update, and receives a vehicle-specific increment initialized in
`FUN_004660b0` at `+0x937`. The native port applies those seven increments to
the seven menu car indices and maps SDL Left/Right to their signs. The car-index
association and keyboard-to-`+0x403` relation remain inferred; native conversion
from that angle to world yaw is still provisional. See
[`disassembly/wpc-directinput-state-20261023.md`](disassembly/wpc-directinput-state-20261023.md).

An isolated Xvfb run of the native build selected the Dingo-Limousine, entered
Grand Canyon, and accelerated for roughly 1.8 seconds. The captured car is
recognizable from behind and occupies about the same vertical portion of the
640x480 view as the original first-race capture. Its view was not synchronized
to the Wine frame, so this only validates approximate size and placement. The
native capture is `native-race-dingo-20261023.png`;
the original runtime frame is `wine-first-race-20261022.png`.
The native sky remains a color gradient, the large grey horizon surfaces do
not match the original desert skyline even with all three static model classes
loaded. The native race now reads the nine observed `TTXA` resources in the
mounted track WGM corpus and animates their indexed frames using the linked
TGAF palettes. The cloud texture in `cc_t1.WGM` is used by 21 ground-model
instances. Its native 100 ms cadence is provisional; the original clock and
remaining skyline/material behavior are unresolved. The HUD still lacks position,
gadget icons, the speedometer art, route cues, and lap progression.

The next championship course is identified as `LES COLLINES JOYEUSES 1` by
its transition screen. A race frame at lap `1/3`, `6 EME`, timer `00:21`, is
preserved in `wine-happy-hills-race-20261023.png`.
The user reports that this race begins like the previous one and has normal,
non-rainy weather. They identify the lower-right gauge as speed and the
upper-right display as boost/turbo level. Gauge meanings and weather condition
are user-reported; no gauge scales, behavior, or weather-selection rules have
been established.

The user subsequently reports that the boost gauge rises only when collecting
a course power-up and that they collected an orange one. The orange item's
identity and effect have not been established, and no image of the pickup or
gauge change is available.

The user reports finishing `LES COLLINES JOYEUSES 1` in fourth place, after
which the race restarted. They infer that winning is required to advance from
this course. This result/restart is not visually captured. It remains a
result-specific hypothesis rather than an established rule: the user
previously reported advancing from Grand Canyon after third place. The user
now also reports that finishing second advanced to `TOUS EN VILLE 1`; the
preserved next-track screen
(`wine-next-track-tous-en-ville-20261023.png`)
confirms that course name. Together, third-place advancement, a fourth-place
repeat, and second-place advancement support a podium cutoff for this
championship, though the second-place result screen and fourth-place restart
are user-reported and the rule has not been verified across every course.
The user now reports another second-place finish followed by advancement to
`AU PAYS DES FRISSONS 1`, which they identify as the championship's final
course. They report finishing second on that final course, winning the
championship with 22 points, and returning to the game menu. No capture of the
final result or menu return is available; the supplied image is the earlier
`TOUS EN VILLE 1` next-track frame. The point formula and exact championship
completion flow remain unknown.

Afterward, the user reports opening the save flow from a menu, seeing eight
slots labeled `LIBRE`, choosing the first, and entering `NICO` as the save
name. The attached image still shows the earlier next-track screen, so the UI
details are user-reported. Save acceptance, persistence, and loadability have
not been confirmed.

The visible HUD includes lap/position/time at upper left, two icon panels at
upper right, a colored gauge at lower right, and a dotted bottom strip with
racer icons. A user-supplied close-up of the bottom strip
(`hud-race-progress-map-20261022.png`)
shows a horizontal dotted line bounded by larger endpoint dots and seven
distinct racer/car markers in uneven positions. This is consistent with a
linearized race-progress display. The crop does not reveal what the endpoints
mean, which direction is forward, whether spacing is proportional to distance,
or how markers map to racers. It also contains no track bends or branching
geometry, so it is not enough to reconstruct the course shape. The images
establish placement and appearance; state semantics remain inferred.

The later water frame (`wine-first-race-water-20261022.png`)
shows the same race HUD at lap `2/3`, position `7 EME`, and `04:42`, with a
large water surface in the foreground. The user reports that the character
drowns on entering water and returns automatically to the race map. The still
frame does not show the entry or resulting reset; the return behavior comes
from the user's observation. The collision boundary, time penalty, control
lockout, and exact return timing remain unknown.

The user reports that Up accelerates, Down decelerates, Left turns the car
left, Right turns it right, and Right Shift triggers the power-up. For the
character whose super gadget is Wings, activation starts an animation and
makes the vehicle fly. These are user-reported controls and effect; flight
duration, cooldown/stock limits, the Dragoon weapon effect, and controller
mappings remain unknown.

The race speedometer resources have now been mapped back to the installed
`general.wgm` scene: `WHudBrick 0x1a4` owns dial sprite `0x1a5` (GMAT `0x10193`)
and needle sprite `0x1a7` (GMAT `0x10194`). Their serialized screen-like
coordinates and dimensions do not directly match the captured lower-right
position, and the HUD root carries a distant world transform. Static analysis
of `WHudBrick::Draw` recovers the speed needle equation and its upper angle
cap; the native renderer now uses it with the selected vehicle's normalized
speed target as an inferred maximum. The port still draws the correct decoded
materials using manual screen rectangles and a fitted needle pivot. The
remaining viewport, pivot, and maximum-speed field mapping are documented in
[`race-hud-resource-map-20261023.md`](race-hud-resource-map-20261023.md).

A later frame (`wine-first-race-turn-arrow-20261022.png`)
shows a large directional arrow above the road ahead at lap `2/3`. The user
reports that it appears when a turn approaches and points the player in the
turn's direction. Left/Right are reported as the steering keys. This is
evidence for an in-world turn cue; it does not establish how early it appears
or whether it changes with the route.

## Race finish and post-finish state

The user reports that finishing a race produces an ordered participant list.
The preserved frame
(`wine-race-finish-standings-20261022.png`)
shows seven entries, from `DEMONE DOUBLE-ZERO` through `BOLIDE ECARLATE`, with
`DINGO-LIMOUSINE` fifth. The user identifies that as the player's vehicle and
reports that its row blinks. Afterward, the AI takes over and the camera enters
`DEMO` mode while following the same character/vehicle. The list and order are
visible in the image; blinking, handoff, and camera behavior are user-reported.
The screen prompts `Pour continuer presser entrée` (“Press Enter to continue”);
the user confirms Enter proceeds. This opens a next-track screen, captured in
`wine-next-track-20261022.png`, which says
`PISTE SUIVANTE`, names `LE GRANG CANYON 1`, and prompts `APPUYER SUR ENTREE`.
The user confirms Enter is required again to continue. In the subsequent Grand
Canyon race, the user reports finishing third. Enter advanced to another
next-track screen, captured in
`wine-next-track-happy-hills-20261022.png`,
which names `LES COLLINES JOYEUSES 1` and again prompts `APPUYER SUR ENTREE`.
This is evidence that the championship advanced after a third-place finish;
the next gameplay state has not yet been observed. How results are stored,
how long demo mode lasts, and whether other keys advance remain unknown. The
listed vehicle names have not been mapped to the six character selection
entries.

The user reports that the Grand Canyon course appeared to start again with
different weather before the third-place finish. No image of the changed
weather is available yet, and the cause of the apparent restart remains
unknown. Since the game then advanced to `LES COLLINES JOYEUSES 1` after third
place, winning was not necessary for that observed championship progression.

Static analysis establishes a Windows message-driven outer loop and an update
call reached from it, but the update path has not been tied to per-race object
updates. No ordering has been established between vehicle control, world
collision, AI, lap state, camera, and rendering. In particular, the reflected
lap field and `WStartLine` name are not enough to conclude how lap completion
is detected. The “race condition” in the title of this investigation should
therefore be read as the gameplay state/update flow; no concurrency race has
been demonstrated.

## Native renderer near-plane clipping

The perspective software renderer previously discarded an entire triangle
when any one of its vertices was within one world unit of or behind the
camera. That removed visible track and scenery portions where a triangle
crossed the camera plane. The renderer now clips camera-space triangles at
depth `1.0`, interpolates camera-plane coordinates and UVs at each crossing,
projects the clipped polygon, and rasterizes its triangle fan. This follows the
existing camera-space convention; the one-unit depth threshold is a renderer
choice, not a recovered WPC near-plane value.

An isolated `demo1.pc` replay on a private 800x600 Xvfb display produced
`native-race-near-plane-clip-20261023.png`.
More of the road surface remains visible at the near edge than in the previous
native race capture. The frame is a renderer check, not a synchronized Wine
comparison; the gray horizon, sky, scene ordering, and vehicle materials still
differ from the original. The saved rerun used SDL's dummy audio driver, so it
does not establish race-audio playback.

## Next evidence that would close the gaps

1. During the active race, record the player's inputs against visible car
   responses, then capture lap transitions and the finish/results sequence.
   Check file reads and save-directory changes after the race.
2. Inventory each track WGM's resource IDs and links, then compare repeated
   `_t1`–`_t4` variants within each filename family. Keep filename-based role
   labels provisional until runtime selection or internal references confirm
   them.
3. Trace the reflected class registrations and virtual methods for
   `WCtrlRoboCar`, `WTrackCamera`, `WStartLine`, and `WPowerUp`, then connect
   their serialized BRTR objects and update calls. Ghidra names/types are
   analysis results and must be checked against machine-code call sites.
4. Resolve character identity from texture/resource links and runtime car
   selection, rather than treating menu illustrations as proof of the roster.

## Evidence references

- [`wpc-static-map.md`](wpc-static-map.md): address anchors and current
  race/world limits.
- [`resource-format-index.md`](resource-format-index.md): checked archive
  inventory and still-opaque world payloads.
- [`runtime-behavior-matrix.md`](runtime-behavior-matrix.md): observed menu
  boundary and unobserved race states.
- [`menu-scene-map.md`](menu-scene-map.md): menu vehicle/character imagery.
