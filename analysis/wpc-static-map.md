# `wpc.exe` static entry and subsystem map

Static evidence from `wpc.exe` (SHA-256 recorded in `TODO.md` and
`analysis/manifest.tsv`), inspected with radare2 6.2.2 `aaa` and focused
cross-reference/disassembly queries. These are disassembler addresses and
interpretations, not runtime observations or recovered source names.

PE metadata from GNU objdump: PE32 i386, GUI subsystem, image base
`0x00400000`, entry RVA `0x000f18a0`, COFF linker version 6.0, no exports,
and imports from AVIFIL32, MSVFW32, KERNEL32, USER32, GDI32, ADVAPI32,
WINMM, DDRAW, DINPUT, and DSOUND. The timestamp field corresponds to
2000-11-15 14:11:43 UTC. Ghidra 12.1.2 DEV's saved `WPC` project was reopened
in read-only headless mode with analysis disabled; it is usable. Radare2 is
6.2.2. No imported CRT DLL is present.

## Startup and message loop

- PE entry point: VA `0x004f18a0` (`entry0` in radare2); PE32 i386 GUI.
  The CRT-like startup code queries `GetVersion`, obtains the command line and
  startup info, performs runtime initialization, then calls a wrapper at
  `0x00402185`, which calls the function radare2 labels `main` at
  `0x004ada60`. This supports a CRT-to-application-entry path, though exact
  compiler/runtime attribution is still open.
- `main` (`0x004ada60`) requests a 32-byte named file mapping using the
  embedded name `WackyMap`. If the call returns a handle and
  `GetLastError() == ERROR_ALREADY_EXISTS` (183), it displays
  `Wacky is running already.`, closes the handle, and returns. A null mapping
  handle or another last-error value continues into startup. This is the
  static single-instance check.
- Main calls `0x00404d04`, a jump thunk to a large routine at `0x004ad1f0`.
  That routine reads the command line, derives the executable directory, and
  calls `SetCurrentDirectoryA` with that directory before calling several
  initialization routines. Static code later chooses between `swmenu.wgm` and
  `menu.wgm`, attempts resource-loading calls for those names, and references
  `swgeneral.wgm`. The exact meanings of the state fields controlling this
  choice are not recovered. Calls to `0x00503600` at `0x004ad346` and
  `0x004ad360` are case-insensitive string comparisons (their helper handles
  case conversion/byte comparison); they are not evidence of WGM loading. The
  actual resource-open path still needs to be traced.
- Startup routine `0x004abaf0` calls `GetLogicalDriveStringsA`, then tests each
  root with `GetDriveTypeA` and retains only drives returning 5
  (`DRIVE_CDROM`). With no such root it displays the localized insert-disc
  message and exits. It stores a selected CD-ROM root for later startup use.
  This behavior was confirmed against the French dialog from the installed
  run; the original Wine prefix's `D:` path currently points to a missing
  mount. A copied prefix configured with a `cdrom` drive type passed this
  drive-type gate but produced a black capture. See
  `analysis/disassembly/wpc-cdrom-check.txt` and
  `analysis/runtime-capture.md`.
- After initialization, `main` uses `PeekMessageA`, `TranslateMessage`, and
  `DispatchMessageA`, with `WaitMessage` on the idle path. A function reached
  from the loop is `0x004ad540`; its full state/update semantics are not yet
  identified. This establishes a Windows message-driven outer loop, not the
  game's complete per-frame behavior. `WM_QUIT` (`0x0012`) leaves the loop.
  Both startup failure and normal quit call `0x00405ba5`, a thunk to
  `0x004ad650`, which invokes two application-object virtual methods, destroys
  the window at global `0x006f5ba0` if present, and calls cleanup helpers
  `0x00405ff6`, `0x004038c8`, `0x004047f0`, and `0x0040119f`. `main` then
  closes the named mapping. The exact cleanup responsibilities of those
  helpers remain unknown. Full call order and bounded error-path evidence are
  in `analysis/disassembly/wpc-main-lifecycle.txt`.

## Subsystem address anchors

| Area | Static evidence | Limit |
| --- | --- | --- |
| DirectDraw renderer | Imported `DirectDrawCreateEx`; calls via thunk `0x004ebb90` at `0x004127b2`, `0x004b6b29`, and `0x004bc415`. At `0x004b6b10`, WPC queries an interface using IID bytes at `0x00518610`, reads capabilities through vtable slot `+0x2c`, and sets cooperative level through slot `+0x50`. Setup at `0x004b14b0` performs three virtual pixel-format enumeration passes with a callback at `0x004b13c0`; the callback filters bit depth, flags, and channel masks, including 16-bit alpha-mask checks for `0x8000` and `0xf000`. Texture upload at `0x004b0050` retrieves a surface description, creates and locks a destination surface, and repacks source channels using destination pixel masks into 16-bit output. | COM interface identities and exact selected format are not recovered; alpha blending remains unknown. See the DirectDraw format-selection and upload dumps. |
| Glide backend | Routine at `0x004331b0` calls `LoadLibraryA("glide2x.dll")`; a null handle returns immediately. A loaded module is checked through `0x00401cf3`; a false check also returns. The successful path resolves Glide drawing, buffer, window, hardware-query, screen-size, and alpha functions with `GetProcAddress`. | No fallback branch is visible in this loader, but indirect caller/backend-selection policy and cleanup after a failed module check remain unresolved. Static evidence cannot establish which backend is selected at runtime. |
| DirectInput | `0x004174f0` creates DirectInput 7; `0x00417470` enumerates eligible devices; `0x00417230` creates/acquires `GUID_SysKeyboard` with a 256-byte state format. The manager at `0x00541908` owns current/previous keyboard snapshots; `0x00417650` refreshes them, and `0x00417050` polls enumerated-device state and reacquires after input loss. On capable devices, `0x004169a0` creates constant-force and sine effects; `0x00416dd0` updates their parameters every third call. `0x004267e0` routes held keys through a linked command/listener list. | The command/listener owner, race action bindings, and force-feedback input values remain unresolved. See `analysis/disassembly/wpc-directinput-state-20261023.md`. |
| Menu | Startup chooses `menu.wgm`/`general.wgm` or `swmenu.wgm`/`swgeneral.wgm` using global state, then calls an object virtual method. Menu classes include WMenu, WMenuPage, WMenuText, WMenuSprite, WMenuSound, and concrete WMenuNode action/load/selection classes. Manager/page routines in `0x0048bxxx`–`0x00490xxx` are mapped in the focused menu notes. | Active object context binding, complete page transitions, draw ordering, and runtime feature behavior remain unresolved. See `analysis/disassembly/wpc-menu-page-*.txt` and `analysis/menu-text-properties.md`. |
| Race/world classes | Reflected class-name evidence includes `WCtrlRoboCar` (`0x004e39b0`), `WackyAuto` (`0x00464390`), `WTrackCamera` (`0x0047684e`), `WStartLine` (`0x0049a03e`), and `WPowerUp` (`0x004d3b8e`); `m_nLapNum` is registered at `0x0041d420`. `WackyAuto` fields include `m_iVehicle` and `m_inputchannel`; its object is initialized by `0x004660b0`. Per-frame update `0x0046a4a0` invokes input updates at `0x0046a719`/`0x0046a720`, then applies displacement to actor position (`+0x288/+0x28c/+0x290`) and rotation to angles (`+0x267/+0x26b/+0x26f`). The `+0x3ef` state tracks normalized forward/reverse speed; `+0x403` tracks bounded steering angle. Exact physical-key bindings, world units, collision response, and the `WCtrlRoboCar+0x18` ownership relation remain unresolved. See `analysis/race-gameplay-and-worlds.md` and `analysis/disassembly/wpc-robocar-control.md`. |
| Configuration | Registry helpers read `Prefs`, `MenuPrefs`, and `LANG`; `0x00460407` requests `LANG` into global `0x006f46e8`. A menu preference cleanup path deletes `MenuPrefs` at `0x00460380`; another routine serializes menu preference data near `0x00460522`. | Individual preference fields, defaults, write timing, and relationship to installer settings are incomplete. See `analysis/wpc-strings.md` and `analysis/wpcsetup.md`. |
| Music stem selection | Routine around `0x00431690` builds paths below `music\\`, probes `_t1` through `_t4` and `_b`, and matches embedded stem groups such as `Cc`, `Ct`, `Dt`, and `Sh` before handing available paths to audio-object setup. | Stem names and key matching are visible in static code; simultaneous playback, synchronization, gains, and looping are not established. See `analysis/snd-observations.md`. |
| SBPC menu sound bank | Filename comparisons at `0x00431d69`/`0x00431d7b` call `0x00503600`; on a menu filename match, code opens MENU.GSW and selects its directory entry using LANG at `0x006f46e8`. Selector `0x0044c510` reads the counted directory. Loader `0x00426bd0` reads table/body bytes after the eight-byte length prefix and sets the body pointer. Lookup `0x0044c640` splits GSMP references into an 8-bit group and 24-bit row ID. Binding `0x0044b000` stores bank body base plus row offset in sample object `+0x2c`. Audio setup `0x00427dc0` consumes flag bit 0 as stereo and bit 1 as 16-bit; `0x004279c0` builds PCM WAVEFORMATEX and `0x00427ce0` copies bytes directly to the DirectSound buffer. | Installed banks and references were previously parsed and resolved during format analysis. The C port does not yet implement bank parsing or playback. Nonzero sample extension fields and menu event/playback timing remain unresolved. See `analysis/sbpc.md`. |
| WGM `SBNK` sample data | Parser `0x0041bf80` dispatches `SBNK` through a resource-manager virtual method at `0x0041c05a`. Installed WGM tables contain 1 menu sample or 46 general samples, and every ID matches a `GSMP` resource in its file. | `--inspect-wgm` validates SBNK row framing and scene GSMP references. Playback, field meanings, gaps, and trailing bytes remain unresolved. See `analysis/wgm-container.md`. |
| WGM metadata chunks | Chunk reader `0x0041bf80` dispatches `GNRL`, `WDIM`, and `RSPR`. `GNRL` checks first dword `0x0130e02b` and assigns `Invalid general chunk version.` on mismatch. `WDIM` and `RSPR` use virtual handlers on the global object. | The second `GNRL` dword and delegated `WDIM`/`RSPR` semantics remain unknown. See `analysis/wgm-container.md` and `analysis/disassembly/wpc-wgm-metadata.txt`. |
| `GDSound` fields | Registration routines `0x0044b9a0`–`0x0044bd00` expose ten named members from `m_SampleID` through `m_VoiceOver`; `GDPlaySound` and `GDStopSound` names are registered nearby. | Member names map to serialized properties, but the methods, event triggers, and runtime field effects are not traced. See `analysis/general-gdsound-map.md` and `analysis/disassembly/wpc-gdsound-registration.txt`. |
| DirectSound / IMA ADPCM | Encoder `0x0040ba80`, decoder `0x0040bdd0`; decoder uses tables at VAs `0x00524ff0` (index adjustment) and `0x00525030` (step sizes), reads/writes three-byte predictor/index records, and is called through `0x00402518` during buffer processing around `0x00428d08` with adjacent state slots at object `+0x44` and `+0x47`. `DirectSoundCreate` thunk `0x004ed656` is called at `0x0041313f` in `0x0040207c` and at `0x004271e8` (not assigned a containing function by this radare2 pass). Voice method `0x00426df0` calls lookup `0x00426f20`, formats `Voice/%s`, and conditionally creates a voice-resource object through `0x00403b25`/`0x00429110`; argument/event meanings remain unknown. The 196-row filename table and installed-file matches are in `analysis/voice-sound-map.md`. | The decoder algorithm and persistent state layout are identified; `.snd` channel mapping, buffer framing, initialization, and runtime event meanings remain unresolved. The exploratory AVI correlation is suggestive but unverified. See `analysis/snd-observations.md` and `analysis/disassembly/wpc-voice-lookup.txt`. |
| Menu film resources | Constructors at `0x0042b810`, `0x0042b950`, `0x0042f5c0`, `0x0042f700`, and `0x0042f840` register `FILM`, `REEL`, `FILQ`, `REEQ`, and `FRMQ` with the corresponding GDFilm/GDReel/GDFrame resource classes. | The registration confirms tag/class associations; timing and draw behavior remain unresolved. See `analysis/film-resources.md`. |
| Texture resource loader | `GDTexture` and `GTEX` registration fields are initialized at `0x0044dc24`; the `TGAF` branch at `0x0044e3b7` loads five reflected fields, allocates the source buffer, and calls resource-state logic. Validator `0x0044dd10` accepts indexed 8-bit and truecolor 24/32-bit uncompressed TGA forms. Converter `0x0044e950` resamples input, expands indexed palette entries to 32-bit, copies truecolor pixels at their stored byte depth, and assigns indexed alpha-like bytes. The threshold source is a linked runtime object selected through descriptor fields `+0x38`/`+0x3c`; see the detailed notes and converter dump. Glide uploader `0x00434520` packs BGR pixels into RGB332/RGB565/ARGB1555/ARGB4444 and uses the source high byte for alpha-bearing formats. DirectDraw uploader `0x004b0050` derives channel shifts/scales from destination pixel masks and writes converted 16-bit pixels to a locked surface. | Wrapper-to-TGA mapping, threshold meaning, DirectDraw alpha compositing, and runtime output remain unresolved. See `analysis/tgaf-menu-film.md` and the focused upload dumps. |
| AVI | `0x004133f0` initializes AVIFile, opens a primary and optional second AVI, obtains `vids` and `auds` streams, reads stream info, opens a frame reader and DrawDib context, and allocates/clears an audio buffer from the audio format. `0x00413760` obtains frames with `AVIStreamGetFrame` and displays them with `DrawDibDraw`. | This is a call-site reconstruction from Ghidra pseudocode; inferred parameter types, object fields, audio synchronization, window integration, and frame timing still need runtime validation. The Linux tool currently delegates AVI decode/playback to ffplay instead of reproducing these Windows APIs. See `analysis/decompilation-audit.md`. |
| Strings | `STRINGS.DAT` loader at `0x0044d950` rebases indexed offsets and XORs text bytes with `0x7f` while preserving `0x7f`; the product key's `LANG` value is queried at `0x00460407` into `0x006f46e8` and matched against table IDs at `0x0044db50`. | Setup-written/default language value and non-ASCII character encoding are unknown; see `analysis/wpc-strings.md`. |
| Saves | `0x004b7b40`/`0x004b7cd0` write/read five `.WSG` blocks totaling 126 bytes; `0x004b7920`/`0x004b7a30` write/read a `0x382f9`-byte `.WSR` replay buffer. `0x004b7690` enumerates up to eight names per extension. WSR and demo `.pc` files use the same fixed buffer and playback path; recorder code stores per-frame state and compressed input events. `save/NICO.WSG` matches the WSG layout and contains a 5-by-4 timing/record table. | Several `.WSG` fields, record units/metadata meaning, and full replay-buffer field/offset schema remain unknown. See `analysis/save-format-and-logic.md` and `analysis/disassembly/wpc-save-load.txt`. |

The map is an address index, not a complete decompilation. Routine labels are
autogenerated, indirect calls and broad call chains remain unresolved, and
race/world type names are schema anchors rather than evidence of complete
gameplay semantics. Runtime observations are kept separately in
`analysis/runtime-capture.md`.
