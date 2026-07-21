# Research: Framework Reuse Patterns (Expansion / CF / Dabs) — for PsyernsCinematicTool

> Repo roots verified on disk 2026-07-21:
> - Expansion: `C:\Users\Administrator\Desktop\Mod Repositories\DayZExpansion`
> - CF: `C:\Users\Administrator\Desktop\Mod Repositories\DayZ-CommunityFramework-production`
> - Dabs: `C:\Users\Administrator\Desktop\Mod Repositories\DayZ-Dabs-Framework-production`
> - COT (dependency): `C:\Users\Administrator\Desktop\Mod Repositories\DayZ-CommunityOnlineTools-production`

**IMPORTANT PROJECT DECISION (main session):** PsyernsCinematicTool's runtime dependencies are **only CF + COT (+ DZ_Data)**. Expansion and Dabs are used as **pattern references only** — any utility we need from them (e.g. `InterpolateAngles`, eased lerps) gets its own clean-room implementation in a `PCT_Math` utility class. Do NOT add Expansion/Dabs to `requiredAddons`.

---

## 1. Existing camera / cinematic implementations

### COT (dependency — reuse directly) — HIGHEST VALUE
- `JMCameraBase : Camera` — `...\JM\COT\Scripts\3_Game\CommunityOnlineTools\Entities\Cameras\JMCameraBase.c`
  - Base free camera. Members: `LookFreeze`, `MoveFreeze`, `Object SelectedTarget`, `vector TargetPosition`, `static float s_CurrentSpeed`. Static globals `CurrentActiveCamera`, `COT_PreviousActiveCamera`. Uses `EntityEvent.FRAME|POSTFRAME`; drives everything through virtual `void OnUpdate(float timeslice)`. `SelectedTarget(Object)` toggles look/move freeze for look-at behavior.
- `JMCinematicCamera : JMCameraBase` — `...\Entities\Cameras\JMCinematicCamera.c` — **a near-complete cinematic camera**:
  - Path/keyframe playback: `void SetupTraveling(TVectorArray positions, TFloatArray time, TBoolArray smooth)`; members `ref TVectorArray travelPositions; ref TFloatArray travelTimes; ref TBoolArray travelSmooth;`. In `OnUpdate`, per-segment progression `t = currentTime/targetTime`, optional `SmoothStep(t)`, `vector.Lerp(startPosition, endPosition, t)`.
  - WASD/roll fly control with acceleration curves: `CalcAccelerationRate(inout float t, inout float rate, float dt, bool increaseSpeeds)`, private `SmoothStep` (`t*t*(3-2t)`), `SmootherStep` (quintic).
  - Look modes: freelook via angular-velocity integration, or `LookAt(TargetPosition/SelectedTarget)`.
  - Reuse verdict: extend this class (or pattern-copy) — add per-keyframe FOV/roll/orientation and Catmull-Rom instead of straight `Lerp` → playback engine done.
- `JMCameraModule : JMRenderableModuleBase` — `...\5_Mission\...\modules\Camera\JMCameraModule.c` — controller/RPC/UI glue to mirror. Notable: `Enter()/Leave()`, `Server_Enter(PlayerIdentity, Object, vector)`, `Client_Enter()`, `GoToSelection(TVectorArray, TFloatArray, TBoolArray)` (validates then `SetupTraveling`), `LookAtSelection()`, `SetTargetFOV(float)`, DOF via `PPEffects.OverrideDOF(...)`, `OnRPC` dispatch (`JMCameraModuleRPC.Enter/Leave/UpdatePosition`). Server-side uses `g_Game.SelectSpectator(sender, "JMCinematicCamera", position)`.
- `JMSpectatorCamera` — `...\4_World\...\Entities\Cameras\JMSpectatorCamera.c`; `JMCameraForm` (UI) — `...\5_Mission\...\modules\Camera\JMCameraForm.c`.

### Expansion — vehicle boom/orbit camera (best "smooth follow" reference)
- `modded class DayZPlayerCamera3rdPersonVehicle` — `DayZExpansion\Vehicles\Scripts\4_World\DayZExpansion_Vehicles\Entities\Players\Cameras\DayZPlayerCamera3rdPersonVehicle.c`. `OnUpdateHelicopter(...)` builds a lag-smoothed boom: position/orientation lag via `Math.SmoothCD(...)` (critically-damped spring), camera matrix via `Math3D.YawPitchRollMatrix(rotation, pOutResult.m_CameraTM)`, writes `pOutResult.m_CameraTM[3]`. **The pattern to copy for damped orbit/boom smoothing.**
- Other Expansion camera files are `modded class` overrides of vanilla `DayZPlayerCamera*` — reference only.

### CF / Dabs
- CF: only `modded class DayZPlayerCameras` (`...\4_World\CommunityFramework\Entities\ManBase\DayZPlayerCameras.c`) — no cinematic tooling.
- Dabs: no live camera; but see §8 for `EditorCameraTrackData` keyframe data model.

---

## 2. Spline / path / keyframe utilities

### Engine (available everywhere — the core spline primitive)
- `proto static native vector Math3D.Curve(ECurveType type, float param, notnull array<vector> points)` — `scripts\1_Core\...\proto\EnMath3D.c:471`. `enum ECurveType { CatmullRom, NaturalCubic, UniformCubic }` (line 20). `param` is 0..1 across the whole point set.

### Expansion `ExpansionMath` — `DayZExpansion\Core\Scripts\3_Game\DayZExpansion_Core\ExpansionMath.c` (PATTERN REFERENCE ONLY — reimplement needed pieces in PCT_Math)
- `static TVectorArray PathInterpolated(TVectorArray path, ECurveType curveType = ECurveType.CatmullRom, bool smooth = true)` — full pipeline: resample path, `Math3D.Curve`, moving-average smoothing. The pattern for turning sparse camera positions into a dense smooth path.
- `static float SmoothStep(float x, int N = 1, float edge0 = 0, float edge1 = 1)` (generalized), `static float SCurve(float x, float edge0=0, float edge1=1)`.
- `static vector InterpolateAngles(vector from, vector to, float time, float mult = 4.0, float pow = 2.0)` — angle-aware interpolation (handles wrap) — the model for orientation keyframes.
- `static float AngleDiff2(float a, float b)`, `RelAngle`, `AbsAngle`, `LinearConversion(...)`, `PowerConversion(...)`, `TFloatArray MovingAvg(TFloatArray, int passes, TFloatArray weights, TIntArray protect)`.
- `static vector ExRotateAroundPoint(vector point, vector pos, vector axis, float cosAngle, float sinAngle)` — orbit around an axis (model for "orbit target" mode).
- `static vector Direction2D(vector p1, vector p2)`, `Distance2D`, `Distance2DSq`, `Circumcenter(vector a, vector b, vector c)`, `Side(vector p1, vector p2, vector p)`, ray/circle/cylinder intersection helpers, `LookUp(...)` 1-D LUT interpolation, `Max/Min(TFloatArray)`.

### Dabs `modded class Math` — `DabsFramework\Scripts\1_Core\DabsFramework\Math.c` (PATTERN REFERENCE ONLY)
- `static float SmoothLerp(float a, float b, float t)`, `static float SmoothTime(float t)` (cosine ease `(-cos(PI*t)/2)+0.5`).
- `static vector LerpVector(vector p1, vector p2, float time)`, `static vector SmoothLerpVector(vector p1, vector p2, float time)` — cosine-eased vector lerp.
- `static float FMod`, `Exp`, `Ln`, `Rollover(int, int, int)`.

### CF
- No dedicated spline/keyframe helper; CF relies on engine `Math3D.Curve` / vanilla `Math`.

---

## 3. JSON settings / persistence patterns

### Engine `JsonFileLoader<Class T>` — `scripts\...\3_Game\DayZ\tools\JsonFileLoader.c` — **RECOMMENDED for shot/sequence data**
- `static void JsonLoadFile(string filename, out T data)` (line 105)
- `static void JsonSaveFile(string filename, T data)` (line 134)
- `static void JsonLoadData(string string_data, out T data)` (151); `static string JsonMakeData(T data)` (162)
- Verified idiom (VPP and others): `JsonFileLoader<ref array<ref EspFilterProperties>>.JsonLoadFile("$profile:ESPFilters.json", m_FilterProps)`.
- For PsyernsCinematicTool: keyframe/shot/sequence classes serialized via `JsonFileLoader<ref PCT_Sequence>` to/from `"$profile:PsyernsCinematicTool\..."` on the **client** (local creator data). Server-shared sequences: server-side files + RPC sync (Expansion model below).

### Expansion settings framework (server-authoritative + client sync) — PATTERN REFERENCE
- `class ExpansionSettingBase` — `DayZExpansion\Core\Scripts\3_Game\DayZExpansion_Core\Settings\ExpansionSettingBase.c`. Virtual surface: `bool OnRecieve(ParamsReadContext)`, `void OnSend(ParamsWriteContext)`, `bool Load()`/`protected bool OnLoad()`, `bool Save()`/`protected bool OnSave()`, `void Defaults()`, `int Send(PlayerIdentity)`, `void Update(ExpansionSettingBase)`, `ExpansionScriptRPC CreateRPC()`. `Load()` returns defaults on client, loads JSON only on dedicated server; `Save()` no-ops on client. Files under `EXPANSION_SETTINGS_FOLDER = "$profile:ExpansionMod\\Settings\\"` (`ExpansionConstants.c:75-78`). Canonical "load JSON on server, push to clients over RPC" model.

### Dabs config/settings (PATTERN REFERENCE; two systems)
- Profile (client, key/value, NOT JSON): `class ProfileSettings : Class` — `DabsFramework\Scripts\3_Game\DabsFramework\Settings\ProfileSettings.c`. Auto-`Load()`/`Save()` reflect over fields using `g_Game.GetProfileString/SetProfileString` (+`GetProfileStringList`). Good for scalar UI prefs; weak for nested arrays.
- Mission (server config): `class MissionSetting : SerializableBase` — `...\Settings\MissionSetting.c`; via `GetDayZGame().GetMissionSetting(typename)`. Also `JsonFileLoader<T>.MakeData(...)` in `Settings\GenericWrapper1.c`.
- Verdict: for whole shot sequences use engine `JsonFileLoader<T>`.

---

## 4. CF ModStorage (persist data on entities) — signature surface

Files under `...\CommunityFramework\ModStorage\`; guarded by `#ifdef CF_MODSTORAGE`. Entity-facing API (override on modded entities):
- `void CF_OnStoreSave(CF_ModStorageMap storage)` — call `super` first, then `auto ctx = storage["YourModName"]`, `if (!ctx) return;`.
- `bool CF_OnStoreLoad(CF_ModStorageMap storage)` — `if (!super.CF_OnStoreLoad(storage)) return false;`, `auto ctx = storage[...]; if (!ctx) return true;`.
- Context `CF_ModStorage` (`...\3_Game\...\ModStorage\CF_ModStorage.c`) — typed primitive streaming only:
  - `void Write(bool/int/float/vector/string value)` (overloaded)
  - `bool Read(out bool/int/float/vector/string value)` (overloaded; false on mismatch)
  - `int GetVersion()` (from `CfgMods` `storageVersion`), `string GetModName()`.
  - Arrays/classes: write a count then loop (see `docs\ModStorage\index.md`).
- Base hooks: `CF_ModStorageBase` (`OnStoreSave/OnStoreLoad`), generic `CF_ModStorageObject<Class T>`.
- Requires `CfgMods` entry with `storageVersion > 0`. **Relevance: only if camera/track data must live on a persistent in-world entity; for shot files, JSON (§3) is simpler.**

---

## 5. UI patterns for a heavy editor-style tool

Three stacks, all MVC/ViewBinding over engine widgets:

### Dabs MVC (cleanest for a big editor UI) — PATTERN REFERENCE ONLY (no Dabs dependency!)
- `class ScriptView : ScriptedViewBase` — `DabsFramework\Scripts\3_Game\DabsFramework\MVC\ScriptView.c`. Override `protected string GetLayoutFile()`, `protected typename GetControllerType()`. Auto-binds named widgets to fields via `LoadWidgetsAsVariables(...)`; optional `Update(float dt)` (`UseUpdateLoop()`). `QuickView<Class T>` helper.
- `class ViewController` (base for `Binding_Name`/`Relay_Command`), `class RelayCommand : Managed` — `...\MVC\Inputs\RelayCommand.c` (`bool Execute(Class sender, CommandArgs args)`, `bool CanExecute()`).
- Observables: `class ObservableCollection<Class TValue> : Observable` — `...\MVC\Observables\ObservableCollection.c` — `Insert`, `InsertAt`, `Remove(int)`, `Remove(TValue)`, `Set(int, TValue)`, `MoveIndex`, `SwapItems`, `Clear`, `Get(int)`, `Count()`, `GetArray()`, `Find`; fires `CollectionChanged`. Also `ObservableDictionary`, `ObservableSet`, `ObservableCollectionNonRef`.

### Expansion MVC — same engine `ScriptView`/`ViewController`/`ObservableCollection` layer wrapped with Expansion lifecycle (`ExpansionScriptViewBase`, `#ifdef EXPANSIONUI`) — PATTERN REFERENCE ONLY.

### COT / CF menu creation (matches the dependency; **the stack to actually use**)
- `class JMFormBase : COT_ScriptedWidgetEventHandler` — `DayZ-CommunityOnlineTools-production\JM\COT\Scripts\4_World\CommunityOnlineTools\Classes\GUI\JMFormBase.c`. COT window/form base: `Init(CF_Window, JMRenderableModuleBase)`, `OnInit/OnShow/OnHide`, `GetLayoutRoot()`, `CreateConfirmation_One/Two/Three(...)` dialog helpers. Paired with `JMRenderableModuleBase` (module registration, layout path, permissions, key bindings, `OnRPC`). CF supplies windowing (`CF_Window`, `#ifdef CF_WINDOWS`).
- `JMFormBase` is manual `ScriptedWidgetEventHandler` wiring (`OnClick`, `OnChange`, `CallByName`) — no data binding. COT builds all its complex forms this way with the `UIActionManager` widget factory. For list-heavy panels, COT forms use scrollers + per-row action widgets (see `JMCameraForm`, `JMESPForm`, `JMWeatherForm` as complexity references).

**Decision for PsyernsCinematicTool:** COT `JMRenderableModuleBase` + `JMFormBase` + `UIActionManager`, custom widgets/layouts where the factory falls short. No Dabs/Expansion UI dependency; their MVC is design inspiration only.

---

## 6. Notification / logging

- CF: `NotificationSystem.Create(StringLocaliser title, StringLocaliser text, string icon, int color, float time = 3, PlayerIdentity sendTo = NULL)` — `...\JM\CF\Scripts\3_Game\CommunityFramework\Notification\NotificationSystem.c` (auto server→client RPC; client can call directly). Logging: `CF_Log.Info/Warn/Error(...)`.
- COT local toast (handy in-camera): `COTCreateLocalAdminNotification(new StringLocaliser("..."))`.
- Expansion (`ExpansionNotification(...)`) and Dabs (`NotificationSystem`, `LoggerManager`) — reference only, not dependencies.

---

## 7. Math / geometry helpers — summary for PCT_Math clean-room implementation

Model implementations to port (with own code):
- Angle-wrap interpolation → from `ExpansionMath.InterpolateAngles` (uses `Math.NormalizeAngle` + shortest-path).
- Orbit around axis → from `ExpansionMath.ExRotateAroundPoint`.
- Generalized smoothstep/S-curve easing → from `ExpansionMath.SmoothStep/SCurve` + `JMCinematicCamera.SmoothStep/SmootherStep` (COT — declared `private` in JMCinematicCamera.c:243/248, NOT callable from subclasses or other classes; port the one-line formulas into PCT_Math, see Research_COT_Infrastructure.md §1).
- Cosine ease → from Dabs `Math.SmoothTime/SmoothLerpVector`.
- Path densification → from `ExpansionMath.PathInterpolated` (resample + `Math3D.Curve` + moving average).

Directly usable engine/dependency APIs (no porting needed): `Math3D.Curve`, `Math3D.YawPitchRollMatrix`, `Math3D.DirectionAndUpMatrix`, `Math3D.QuatLerp/QuatMultiply/MatrixToQuat/QuatToMatrix/QuatToAngles`, `Math.SmoothCD`, `vector.Lerp`, `vector.VectorToAngles`, `Math.NormalizeAngle`. (COT's `JMCinematicCamera` easing helpers are `private` — not directly usable, port instead.)

---

## 8. Timeline / keyframe-animation precedent

- **COT `JMCinematicCamera.SetupTraveling(...)`** (§1) — operational client-side keyframe player: positions + per-segment times + smooth flags, advancing `currentTargetIndex`, `Lerp`+`SmoothStep` per segment. Closest precedent; natural to extend (swap `Lerp`→`Math3D.Curve(CatmullRom, ...)`, add FOV/orientation/roll channels, add scrub/pause).
- **Dabs `EditorCameraTrackData : EditorObjectData`** — `DabsFramework\Scripts\3_Game\DabsFramework\_Legacy\Editor\EditorCameraTrackData.c`. Serialized camera keyframe: `float Time`, `Position`, `Orientation`, `Type = "DSLRCamera"`, `Create(notnull Camera camera, float time, EditorObjectFlags)`, `Write/Read(Serializer, int)`. Collected as `ref array<ref EditorCameraTrackData> CameraTracks` in `EditorSaveData.c`. Clean **data-model precedent** ("saved sequence = ordered list of camera keyframes with time") — mirror the shape (add FOV/roll/focus) for the PCT JSON schema. Under `_Legacy`, no playback loop in repo — reuse the shape, not runtime code.
- **Expansion path/animation-over-time** (secondary): `ExpansionMath.PathInterpolated`; AI waypoint traversal `eAIState_TraversingWaypoints` / `ExpansionPathHandler` / `ExpansionPathPoint` (`DayZExpansion\AI\Scripts\4_World\...\PathFinding\`), `eAICommandMove` (SmoothStep) — sampling a smoothed path over time, transferable to camera dollying. Helicopter "autopilot" only in deprecated `ExpansionVehicleHelicopter_OLD.c` — not worth reusing.

---

## Consolidated recommendation (amended by main-session dependency decision)
1. Camera engine: subclass/extend COT `JMCinematicCamera` (or pattern-copy into `PCT_CinematicCamera : JMCameraBase`); module mirrors `JMCameraModule` → COT permissions, freecam entry/exit, spectator-sync for free.
2. Interpolation: `Math3D.Curve(ECurveType.CatmullRom, t, points)` for position; own `PCT_Math` ports of `InterpolateAngles`, eased lerps, orbit; `SmoothStep`/`SmootherStep` easing.
3. Persistence: engine `JsonFileLoader<ref PCT_*>` to `$profile:PsyernsCinematicTool\...` client-side; Expansion's server-load-then-RPC model only if server-authored sequences are needed. Keyframe model shaped like Dabs `EditorCameraTrackData` + FOV/focus/roll.
4. UI: COT `JMRenderableModuleBase` + `JMFormBase` + `UIActionManager` (no new dependencies).
5. Feedback: CF `NotificationSystem.Create(...)` / COT local toast; `CF_Log` for logging.
