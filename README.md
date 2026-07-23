<p align="center">
  <img src="assets/pct_banner.png" alt="Psyerns Cinematic Tool" width="820">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/DayZ-1.29%2B-5b8fb9?logo=steam&logoColor=white" alt="DayZ 1.29+">
  <img src="https://img.shields.io/badge/Enforce%20Script-EnScript-2b2b2b" alt="Enforce Script">
  <img src="https://img.shields.io/badge/Built%20on-Community%20Online%20Tools-4b9e6a" alt="Built on COT">
  <img src="https://img.shields.io/badge/Requires-Community%20Framework-4b9e6a" alt="Requires CF">
  <img src="https://img.shields.io/badge/version-0.1.0-blue" alt="Version 0.1.0">
  <img src="https://img.shields.io/badge/status-in%20development-orange" alt="Status: in development">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Zero-Expansion%20Dependency-8e44ad" alt="Zero Expansion Dependency">
  <img src="https://img.shields.io/badge/Persistence-%24profile%20JSON-1abc9c" alt="$profile JSON">
  <img src="https://img.shields.io/badge/Camera-Photographic%20Model-e67e22" alt="Photographic camera model">
</p>

<h3 align="center">A film-grade cinematic &amp; previz camera for DayZ — built on the COT freecam.</h3>

<p align="center">
Turns the Community Online Tools spectator camera into a real cinema rig: a photographic camera model
(sensor · focal length · aperture · focus), lens kits, composition overlays, shot presets, and a
dual-path keyframe sequencer for camera moves — with everything saved as plain JSON in your profile.
</p>

<p align="center"><em>A Psyern project · powered by Community Online Tools &amp; Community Framework</em></p>

***

## Overview

**Psyerns Cinematic Tool (PCT)** extends the **Community Online Tools (COT)** freecam into a filmmaker's
camera. Instead of a flat FOV slider, you work the way a DP does: pick a **sensor**, set a **focal length**,
open or close the **aperture**, pull **focus**, and let PCT compute the field of view from real optics
(`fovV = 2·atan(sensorHeight / (2·focalLength))`).

On top of that lives a **shot system**, **composition overlays**, an **auto-framing** assistant, and a
**dual-path movement sequencer** that separates *where the camera travels* from *where it looks* — so a
dolly, an orbit, or a dolly-zoom is a couple of clicks, not a guessing game.

PCT has **no Expansion or Dabs dependency**. It needs only DayZ, CF, and COT. Every shot, preset, and
setting is a human-readable JSON file under `$profile:PsyernsCinematicTool\`.

***

## Repository Layout

```
PsyernsCinematicTool/                 # git repo root
├─ PsyernsCinematicTool/              # the mod package (this is what gets packed)
│  ├─ Scripts/                        # → PCT_Scripts.pbo
│  └─ GUI/                            # → PCT_GUI.pbo
├─ Docs/                              # architecture, feature mapping, data model, camera engine, DoD protocols
├─ Tools/                            # Build_PCT.ps1 / Build_PCT.cmd (pack · sign · deploy)
├─ Build/                           # build output (@PsyernsCinematicTool) — gitignored
├─ PLANNING.md                      # planning index
└─ Plan_A1.md                       # requirements / cinematic vocabulary
```

***

## DayZ Mod Structure

```
PsyernsCinematicTool/                                  # mod folder (CfgMods dir)
├─ Scripts/
│  ├─ config.cpp                                       # CfgPatches PCT_Scripts + CfgMods
│  ├─ Data/Inputs.xml                                  # UAPCT_* input actions (no default binds)
│  ├─ 3_Game/PsyernsCinematicTool/
│  │  ├─ Cameras/PCT_CinematicCamera.c                 # own Enter/Leave camera + per-frame rig apply + DOF
│  │  ├─ Data/                                         # Shot, CameraRig, MotionPath, Presets, Settings, WorldState, LightSetup, Enums, CustomCurve
│  │  ├─ Sequencer/                                    # PCT_MotionPlayer, PCT_MotionPresets
│  │  ├─ Persistence/PCT_Storage.c                     # load / save / migrate / validate
│  │  ├─ Utils/                                        # PCT_Math, PCT_ArcLength, PCT_LensLibrary
│  │  ├─ PCT_Constants.c                               # version, $profile paths, FOV/focal clamps
│  │  ├─ PCT_RPC.c                                     # EPCT_RPC (base 10500)
│  │  ├─ PCT_Framing.c                                 # pinhole auto-framing geometry
│  │  └─ PCT_Export.c                                  # CSV (31 cols) + JSON export
│  ├─ 4_World/PsyernsCinematicTool/PCT_World.c
│  └─ 5_Mission/PsyernsCinematicTool/
│     ├─ Modules/PCT_CinematicModule.c                 # COT module (permissions, RPC range, sidebar entry)
│     ├─ GUI/PCT_CinematicForm.c, PCT_OverlayHud.c     # form + overlay HUD
│     └─ PCT_ModuleRegistration.c                      # modded JMModuleConstructor
└─ GUI/
   ├─ config.cpp                                       # CfgPatches PCT_GUI
   ├─ layouts/  pct_cinematic_form.layout · pct_overlay.layout
   └─ textures/modules/
```

> **Note:** Two PBOs ship — `PCT_Scripts.pbo` (all logic) and `PCT_GUI.pbo` (layouts + textures). PCT loads
> as a client **`-mod`**, *not* a `-serverMod` (it needs client-side script layers for the camera and UI).

***

## Features

### Photographic Camera Model
- **Sensor presets** (6, incl. Full Frame / Super 35) drive the FOV — no more eyeballing a raw angle.
- **Focal length** slider (8–400 mm); vertical FOV computed from real optics and clamped to the engine's
  displayable range (14–200 mm effective).
- **Aperture** (T/f 0.7–32) with a bokeh model — depth of field via `PPEffects.OverrideDOF`.
- **Focus distance** pull with a live focal-plane; cleanly reset when you leave the camera.

### Lens Kits
- Built-in **ZEISS Supreme Prime Radiance** set (8 primes) with true T-stop labels and per-lens aperture limits.
- **Prime-lock** mode snaps you to real focal lengths; **close-focus clamp** mirrors each lens's minimum
  focus distance (with an optional override for extension-tube looks).

### Composition & Overlays
- Rule-of-**thirds** grid, **center** cross, and **aspect masks** — 2.39:1, 16:9, 4:3, 9:16, 1:1.
- Every overlay toggles independently; masks are pixel-accurate at matching resolutions.

### Auto-Framing
- Raycast a subject, then apply a **framing preset** (10 built-ins: CU, MS, WS, …) and an **angle preset**
  (11 built-ins: eye level, low, high, overhead, …). Exact pinhole geometry places the camera for you.

### Shots & Presets
- **Capture / apply** a full rig state (position, orientation, sensor, focal length, aperture, focus, mask).
- **Shot browser** — list, apply, rename, delete, all persisted as `Shots\<id>.json`.
- **Seven preset families** (sensors, lenses, lens kits, framings, angles, movements, lights, world states),
  each with built-ins plus your own user presets.

### Camera Movement — Dual-Path Sequencer
- Separates the **camera path** (`PCT_CameraPath`) from the **look path** (`PCT_LookPath`) over one shared timeline.
- **7 path types** and **7 look modes** (LookAt, Follow, Free-rotation, Horizon-lock, …).
- **Multi-track keyframes** for focal length, aperture, focus and height; an **11-easing** catalog
  (Linear → Smoother, HardStart/Stop, custom curves), **Catmull-Rom** splines with arc-length reparameterization.
- **Deterministic scrubbing**, an on-screen **path visualization** overlay, and one-click **movement presets**
  (Dolly In/Out, Truck, Orbit 90°, Dolly-Zoom).

### Export
- **CSV** (31-column shot list, RFC-4180 escaping, configurable delimiter) and **JSON** export.
- **Screenshots** via the engine's `MakeScreenshot`, named per shot.

### COT Integration
- Appears as a native entry in the **COT sidebar**; gated by COT permissions (`CinematicTool.View`,
  `CinematicTool.Export`, …).
- Its **own** Enter/Leave camera path (COT's freecam isn't remote-controlled), plus an automatic
  **hand-off** from an active COT freecam into the cinematic camera.

***

## Requirements

| Dependency | Purpose | Load as |
|---|---|---|
| **DayZ** 1.29+ | `DZ_Data` | — |
| **Community Framework (CF)** | RPC / module lifecycle | `@CF` |
| **Community Online Tools (COT)** | camera base, module UI, permissions | `@COT` |
| **Psyerns Cinematic Tool** | this mod (`JM_CF_Scripts`, `JM_COT_Scripts`, `JM_COT_GUI`) | `@PsyernsCinematicTool` |

> **Note:** `requiredAddons[]` is exactly `DZ_Data`, `JM_CF_Scripts`, `JM_COT_Scripts`, `JM_COT_GUI`.
> There is **no** Expansion or Dabs dependency — any framework utilities PCT needs are reimplemented in `PCT_Math`.

***

## Installation

1. Subscribe to / install **Community Framework** and **Community Online Tools**.
2. Drop `@PsyernsCinematicTool` next to `@CF` and `@COT` in your DayZ install.
3. Add `Psyern.bikey` to the server's `keys\` folder (client joins with `verifySignatures=2`).
4. Launch with the mods loaded — **PCT is a client `-mod`, not a `-serverMod`**:

```
-mod=@CF;@COT;@PsyernsCinematicTool
```

5. In-game, open the **COT menu** and pick **Psyerns Cinematic Tool** from the sidebar.

> **Note:** Grant `CinematicTool.View` (and `CinematicTool.Export` for exporting) in the COT Permissions
> editor. Offline / singleplayer grants everything by default, so you can iterate without touching permissions.

***

## Building from Source

The build pipeline packs, signs, and stages the mod in one step — no `P:\` work-drive required.

```powershell
# from the repo root
Tools\Build_PCT.ps1            # pack (AddonBuilder -packonly) → sign (DSSignFile) → deploy to Build\@PsyernsCinematicTool
```

Or double-click `Tools\Build_PCT.cmd`.

- Produces `PCT_Scripts.pbo` + `PCT_GUI.pbo` with `.bisign` signatures.
- Auto-generates a signing key at `%USERPROFILE%\DayZKeys\Psyern` (`Psyern.bikey` / `.biprivatekey`) on first run.
- Point `-DeployDir` at a live server's mod folder to stage straight to a test server.

> **Note:** Requires **DayZ Tools** (AddonBuilder + DSSignFile) installed. The build machine does not need the
> DayZ game itself — packing and signing are self-contained.

***

## Profile Structure

Everything PCT writes lives under your DayZ profile — portable, hand-editable JSON:

```
$profile:PsyernsCinematicTool\
├─ Settings.json                 # client preferences (schemaVersion 1)
├─ Shots\      <id>.json         # saved shots (schemaVersion 2)
├─ Sequences\  <id>.json         # sequences (planned)
├─ Presets\                      # Sensors · Lenses · LensKits · Framings · Angles · Movements · Lights · WorldStates
└─ Exports\    <name>.csv / .json / screenshots
```

Every root file carries a `schemaVersion`; corrupt files are logged and **never** overwritten, and older
shots migrate forward losslessly (v1 single-stream → v2 dual-path).

***

## Configuration

### `Settings.json`

```json
{
  "schemaVersion": 1,
  "lastSequenceId": "",
  "lastShotId": "",
  "defaultSensorPresetId": "sensor_fullframe",
  "defaultLensPresetId": "lens_50_f18",
  "showCompositionGrid": true,
  "defaultAspectMask": "16:9",
  "csvDelimiter": ";",
  "defaultSubjectHeightM": 1.8,
  "allowCloseFocusOverride": false
}
```

| Field | Default | Description |
|---|---|---|
| `defaultSensorPresetId` | `sensor_fullframe` | Sensor preset applied to new shots. |
| `defaultLensPresetId` | `lens_50_f18` | Starting lens preset. |
| `showCompositionGrid` | `true` | Show the rule-of-thirds overlay by default. |
| `defaultAspectMask` | `16:9` | Aspect mask used on enter. |
| `csvDelimiter` | `;` | Delimiter for CSV export. |
| `defaultSubjectHeightM` | `1.8` | Fallback subject height (m) for auto-framing non-player objects. |
| `allowCloseFocusOverride` | `false` | Allow focusing closer than the lens's minimum (extension-tube look). |

### `Shots\<id>.json` (excerpt)

```json
{
  "schemaVersion": 2,
  "id": "sh_010_cu_b",
  "name": "Approach — close up",
  "sceneNumber": "1",
  "shotNumber": "10B",
  "framingPresetId": "framing_cu",
  "anglePresetId": "angle_eye_level",
  "movementPresetId": "move_dolly_in",
  "priority": 2,
  "status": "planned",
  "cameraRig": { "...": "sensor / focal length / aperture / focus" },
  "cameraPath": { "...": "authoritative camera travel" },
  "lookPath":   null,
  "tracks":     [ "...focal / aperture / focus / height keyframes..." ],
  "dollyZoomLock": false
}
```

***

## Workflow

1. **Enter** the cinematic camera from the PCT form (bind `UAPCT_ToggleCamera`).
2. **Dial the lens** in the *Rig* panel — sensor, focal length, aperture, focus — or pick a **ZEISS prime**
   from the *Lens Kit* panel.
3. **Compose** with the thirds grid, center cross, and your aspect mask.
4. **Auto-frame** a subject: aim, then apply a **framing** + **angle** preset (`UAPCT_ApplyFraming`).
5. **Build a move:** drop **camera points** (`UAPCT_AddCamPoint`) and **look points** (`UAPCT_AddLookPoint`),
   toggle the **path viz** (`UAPCT_TogglePathViz`), then **Play / Pause** (`UAPCT_PlayPause`) and scrub.
6. **Save as Shot** — reopen, apply, rename, or delete it from the browser.
7. **Export** the shot list to CSV/JSON and grab a screenshot.

> **Note:** All `UAPCT_*` inputs ship **without default binds** (so they never clash with other mods) — assign
> them once in *Options → Controls*.

***

## Project Status

PCT is under active development. Implemented phases are code-complete and reviewed; in-game acceptance for
later phases is ongoing.

| Phase | Scope | Status |
|---|---|---|
| **0** | Mod skeleton · COT module · build pipeline | Accepted |
| **1** | Camera core · photographic model · DOF · overlays | Implemented (in-game DoD in progress) |
| **2** | Shots · presets · auto-framing · lens kits · export | Implemented (in-game DoD in progress) |
| **3** | Dual-path sequencer · movement presets · path viz | Implemented (in-game DoD in progress) |
| **4** | World (time/weather) · actors · lights · continuity assistants | Planned |
| **5** | Polish · release | Planned |

See [`Docs/`](Docs/) for the full architecture, data model, camera engine, and per-phase DoD protocols.

***

## Credits

- **[Psyern](https://github.com/Psyern)** — author & maintainer.
- **[Community Online Tools](https://github.com/Jacob-Mango/DayZ-Community-Online-Tools)** — camera, module,
  and permission base PCT builds on.
- **[Community Framework](https://github.com/Arkensor/DayZ-CommunityFramework)** — RPC and module lifecycle.
- Lens data models the real-world **ZEISS Supreme Prime Radiance** line (reference only).

***

## License & Attribution

Built on and requires **Community Online Tools** and **Community Framework**; respect their licenses when
redistributing. PCT does not bundle Expansion or Dabs code.

> This repository does not yet ship a formal `LICENSE` file. Until one is added, all rights are reserved by
> the author — add a license before publishing if you intend others to reuse the code.
