# Settings and modding

Status: **Proposed** (2026-11-12). Reusable components: FFCCEOT-Recomp's `game/src/settings.{hpp,cpp}` (INI file,
tested by `tests/test_settings.cpp`), its RmlUi settings/controls windows with controller navigation, and its presets
(Original / Enhanced). RmlUi 6.3 has renderers for GL3 (incl. Android GLES), Vulkan, DX12 and SDL_GPU, so the UI
follows whichever present backend a platform uses (ADR-0007). Patterns studied: libultraship CVars + ImGui menus (Ship of Harkinian, 2S2H), Wind
Waker HD Recomp's settings overlay and mod manager.

## 1. Settings architecture

Three parts, each replaceable without touching the others:

| Part | Responsibility | Must not |
|---|---|---|
| **Schema registry** (`app/settings`) | Declares every setting: `id` (`screens.pip.size`), type, default, range/enum, category, title, description, platform defaults, flags `experimental`, `changes_gameplay`, `needs_restart`, `per_game_version`, owner (`core`, `game/st`, `mod:<id>`) | Know about UI widgets or files |
| **Storage** | Versioned text file (`settings.ini` today; TOML considered) with a `schema_version` and ordered migrations; atomic writes; unknown keys preserved; corrupted file → backup + defaults + notice | Hold logic beyond parse/validate/migrate |
| **Frontend** (`app/ui`) | Renders the schema: categories, search over titles/descriptions, tooltips, "experimental" and "changes gameplay" badges, per-setting reset, controller/touch navigation | Contain defaults or validation rules |

Observers subscribe to setting ids and apply changes at safe points (video at frame boundaries, input immediately,
`needs_restart` on next launch). Presets are named partial maps of values (Original, Recommended, Steam Deck,
Handheld/Android, Accessibility) and are applied, not linked: changing a value after applying a preset is allowed.

Why not libultraship CVars: string-keyed globals readable from anywhere are convenient but invite hidden coupling;
the schema gives the same convenience with types, owners and migrations. Why not ImGui for players: RmlUi is already
proven in FFCCEOT-Recomp, themeable with our own art, and controller-navigable; Dear ImGui stays a developer-only tool if added.

## 2. Categories (initial content)

| Category | Settings (first versions in **bold**) |
|---|---|
| General | **Language of the menus**, game language (firmware language, FFCC `RuntimeOptions::language`), update check (opt-out), launch behaviour (Continue on start), data folder / portable mode |
| Graphics | **Renderer** (Software / OpenGL / Compute where available), **internal resolution** (auto, 1–8×), **scaling filter**, VSync, fullscreen/borderless, frame pacing, texture filtering (experimental), interpolation (experimental), widescreen (experimental) |
| Screens | **Layout mode**, swap, **PiP** anchor/size/opacity/click behaviour, split ratio/gap, separate-window options, Auto mode + pin, per-platform preset |
| Controls | **Keyboard & mouse / Controller pages**, active device, profile, contextual bindings including flute, automatic/manual Xbox/PlayStation/Nintendo symbols, dead zones, pointer sensitivity, draw mode, gyro pointer, Steam Deck trackpad mode, handedness preset |
| Audio | **Volume**, output device, latency; **microphone mode**, device, gain, noise gate, calibration, blow strength, hold/toggle |
| Gameplay | Quality-of-life and accessibility options, each marked `changes_gameplay` when it does; all off in Original |
| Saves | Save folder, backups (count, open folder), import/export `.sav`, save states (slots, resume on start) |
| Mods | (M7) installed, enabled, per-mod options, problems |
| Advanced | Logging level, diagnostics overlay (fps, frame time, interpreter %), problem report, rebuild game, reset all |

**Maintainer requirement, 2026-11-20 (Proposed):** Controls must be available in the launcher and the same
in-game configuration overlay, navigable with a gamepad. Input handling and on-screen prompts consume the same
action/binding model. Display overrides change symbols without silently remapping physical buttons. See
[controller-first requirements and flute candidates](CONTROLS_AND_MICROPHONE.md#controller-first-requirements-maintainer-2026-11-20).
The Windows app now has the shared Controls editor for existing DS/utility bindings and inactive instrument
bindings, with device-type label overrides. See the implementation/verification subsection in the linked document.
Remaining categories and complete modern gameplay profiles are still a specification, not shipped features.

## 3. Enhancements (gameplay options)

Built-in enhancements are code in `game/st/enhancements`, each registered with: id, settings it owns, the hooks it
uses, contexts where it is valid, and whether it affects saves. They follow the SoH pattern of small, independent
units subscribing to typed game events, but nothing is enabled by default in the Original preset, and anything that
changes rules is labelled. Candidates after M4 (each needs verification): faster text, skip intro logos, keyboard name
entry, map memo improvements (larger pen, undo), audio balance, colour-blind map icons, hold-to-blow.

## 4. Modding: incremental plan

### Stage 1 — content and configuration (M7 entry)
- **Configuration presets and UI themes** (our own assets).
- **Texture replacement** at the GPU texture cache: hash the decoded DS texture (data + palette + format) and
  substitute a user-supplied image (FFCC `docs/ARCHITECTURE.md` lists this plan; melonDS's renderers need a hook in
  their texture caches — Unknown effort; Software renderer excluded).
- Packs are folders or zips with a manifest; **no game assets ship with the project** and example packs use original art.

### HD texture discovery (2026-11-20)

**Verified search result, bounded:** the creator u/Past_Newspaper8788 publicly showed an
[HD menu/HUD study for Spirit Tracks](https://www.reddit.com/r/zelda/comments/1myc7ks/st_remaking_interface_of_zelda_spirit_tracks_in_hd/)
and described it as part of a Unity remake. In the same discussion the creator offered to try a texture mod as
a side project. This establishes a work-in-progress visual project, not a downloadable or compatible texture pack.
Their [earlier announcement](https://www.reddit.com/r/zelda/comments/1mkraqj/) likewise describes early train/model work.
No ready Spirit Tracks HD pack with verified format, port compatibility and reuse permission was confirmed in
this search. That is not proof none exists. No artwork, models or pack files were downloaded or imported.

**Proposed integration:** independently installable, optional content packs managed in **Graphics / Mods**,
with Original as a working fallback, per-pack enable/disable, author/credits, version, format and compatibility.
Unverified redistribution permission must not be treated as permission to bundle somebody else's work; user-local
installation and inclusion in our distribution are separate questions. Obtain clear author terms before adapting
or redistributing a candidate. Do not contact a creator without the maintainer's instruction.

**Technical status:** no texture-pack loader or replacement path has been verified in our app. Higher internal
3D resolution is distinct from replacing low-resolution textures; DS 2D art, fonts and HUD need separate handling
from 3D texture-cache replacement. A higher-resolution asset must also survive composition/presentation at that
resolution; loading it and then reducing it to a 256x192 plane is not an accepted HD result. Engine changes stay
generic and behind the rendering backend; the Software fallback remains usable with original assets. Do not add
a working-looking HD toggle until installation, fallback, rendering and resource costs are tested.

The first renderer test should use original synthetic artwork, then a locally installed, permitted candidate
if one becomes available. Test palette variants, animated/reused textures, cache invalidation, scale/filtering,
2D/3D separation, save neutrality and memory/frame-pacing costs on the supported backends. HD packs must not become
a prerequisite for the controller UI, one-canvas presentation or the developer alpha.

### Stage 2 — gameplay enhancements through stable hooks
- Typed events from `game/st` (scene change, item use, mic open, train enter/exit, save written…).
- Settings contributed by mods through the same schema registry (`owner = mod:<id>`).
- Save protection: mods declaring `affects_saves` force a backup before enabling and mark the save.

### Stage 3 — advanced modding (only after Stage 2 is stable)
- Versioned C ABI for code mods (the engine's own `NtrHostApi` style: a struct of function pointers with `abi` and
  `size` fields), registering hooks by **name of a stable event**, not raw guest addresses.
- Scripting: evaluate a sandboxable language (Lua/Luau or WASM) versus native code; libultraship uses TCC-compiled C,
  WWHD uses guest-code ELF mods with a trust prompt.
- Randomizer: separate project interest exists (ST randomizer prototype `Yanis002/strando`, Archipelago clients);
  integration is out of scope until Stage 3.

### Manifest (Proposed, modelled on WWHD `manifest.json` and Zelda64Recomp `mod.toml`)

```json
{
  "format_version": 1,
  "id": "example.big-map-pen",
  "name": "Bigger map pen",
  "version": "1.0.0",
  "game": {"id": "spirit-tracks", "versions": ["BKIE-rev0"]},
  "minimum_host_version": "0.5.0",
  "kind": "content | enhancement | code",
  "depends": [{"id": "other.mod", "version": ">=1.2 <2"}],
  "conflicts": ["another.mod"],
  "load_order": {"after": ["other.mod"]},
  "permissions": ["microphone"],
  "affects_saves": false,
  "options": [{"id": "size", "type": "int", "min": 1, "max": 4, "default": 2, "name": "Pen size"}]
}
```

Loader rules: semantic versions, dependency resolution with clear failure messages, deterministic load order,
per-mod enable/disable, a mod that fails to load is disabled for the session and reported, never a crash loop.

### Security
- Content packs: parse defensively (size limits, image decoder hardening, zip path traversal checks).
- Code mods run with the player's privileges and cannot be sandboxed in-process: off by default, explicit **trust
  prompt per package version** (WWHD), and a permission list (e.g. `microphone`) the player approves.
- No remote catalogue until signing and moderation exist.
- Never expose raw guest memory as a stable API; mods get typed events and documented accessors.
