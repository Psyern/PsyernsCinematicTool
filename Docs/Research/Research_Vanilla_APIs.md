# Research: DayZ 1.29 Vanilla Script API — Cinematic Camera Reference

> Source: `C:\Users\Administrator\Desktop\Mod Repositories\scripts - 1.29` (fallback: `scripts - 1.28`).
> Everything below was read directly from source on 2026-07-21; nothing is inferred.
> Layer prefixes: `1_Core` (engine proto), `2_GameLib`, `3_Game`, `4_World`, `5_Mission`.

**Headline:** there is a **complete vanilla keyframe camera tool** already in the game — `5_Mission\DayZ\GUI\CameraTools\` (`CameraToolsMenu.c`, `CTKeyframe.c`, `CTActor.c`, `CTEvent.c`, `CTSaveStructure.c`, `CTObjectFollower.c`). It is the single most valuable implementation reference for PsyernsCinematicTool.

---

## 1. Camera class, staticcamera, FreeDebugCamera

**File:** `3_Game\DayZ\Entities\Camera.c` — `class Camera extends Entity`

| Line | Signature | Notes |
|---|---|---|
| 7 | `static proto native Camera GetCurrentCamera()` | Active `Camera` instance. Returns **null** when the player's own camera is active (player camera is not a `Camera` instance). |
| 13 | `static proto native float GetCurrentFOV()` | FOV of current camera object |
| 24 | `static proto native void InterpolateTo(Camera targetCamera, float time, int type)` | type 0 = linear, 1 = ease-in/ease-out, 2 = ease-in / linear / ease-out |
| 29 | `static proto native bool IsInterpolationComplete()` | |
| 35 | `proto native void SetNearPlane(float nearPlane)` | clamped internally to min 0.01 m |
| 40 | `proto native float GetNearPlane()` | |
| 45 | `proto native void SetActive(bool active)` | |
| 50 | `proto native void EnableSmooth(bool enable)` | smoothing in interpolation |
| 55 | `proto native void StopInterpolation()` | call when an interpolation run finishes |
| 61 | `proto native bool IsActive()` | |
| 67 | `proto native void SetFOV(float fov)` | **fov is in RADIANS** (vertical) |
| 74 | `proto native void SetFocus(float distance, float blur)` | built-in DOF control on a camera object |
| 80 | `proto native void LookAt(vector targetPos)` | orient camera toward a world point |

`Camera extends Entity` → all `Object`/`Entity` transform methods apply: `GetPosition()`, `SetPosition()`, `GetOrientation()`, `SetOrientation()`, `GetTransform(out vector mat[])`, `SetTransform(vector mat[4])`, `SetYawPitchRoll(vector)`, `GetTransformAxis(int)` (§7).

**FreeDebugCamera** (same file, line 84) — `class FreeDebugCamera extends Camera`, singleton:
- `static proto native FreeDebugCamera GetInstance()` (90)
- `proto native bool IsPlayerMove()` (97)
- `proto native void SetFreezed(bool freezed)` (103)
- `proto native bool IsFreezed()` (109)
- `proto native Object GetCrosshairObject()` (115)

**staticcamera creation** — verified `5_Mission\DayZ\GUI\CameraTools\CameraToolsMenu.c:71-72`:

```c
m_Camera1 = Camera.Cast(g_Game.CreateObject("staticcamera", g_Game.GetPlayer().GetPosition(), true));
```

The `true` is `create_local` (client-only). Two static cameras are created and `InterpolateTo` blends between them (the vanilla pattern for a moving shot). Free-flight editing camera: `DeveloperFreeCamera.EnableFreeCameraSceneEditor(PlayerBase)` / `DeveloperFreeCamera.DisableFreeCamera(PlayerBase, false)` (`CameraToolsMenu.c:178, 92, 475`).

**Vanilla keyframe playback pattern** (`CameraToolsMenu.c:719-781`, `SetCameraData`): position/orientation set on `m_Camera1`, `SetFOV(deg * Math.DEG2RAD)`, `SetFocus(dof, blur)`, then `m_Camera1.InterpolateTo(m_Camera2, time, interpType)`. Keyframes store `Param6<vector pos, vector orient, float interpTime, float fov, float dof, int pin>`. Actor-follow branch uses `GetBonePositionWS`/`GetBoneRotationWS` (§8).

**Engine-level camera slots** (alternative to `Camera` objects) — `1_Core\DayZ\proto\EnWorld.c`:
- `proto native void SetCamera(int cam, vector origin, vector angle)` (53)
- `proto native void SetCameraEx(int cam, const vector mat[4])` (56)
- `proto native void GetCamera(int cam, out vector mat[4])` (59)
- `proto native void SetCameraVerticalFOV(int cam, float fovy)` (61)
- `proto native void SetCameraFarPlane(int cam, float farplane)` (62, default 160000)
- `proto native void SetCameraNearPlane(int cam, float nearplane)` (63, default 5)
- `proto native void SetCameraType(int cam, CameraType type)` (65) — `enum CameraType { PERSPECTIVE, ORTHOGRAPHIC }` (38-42)
- `proto native void SetListenerCamera(int camera)` (45)

`CGame.GetCurrentCameraPosition()` / `GetCurrentCameraDirection()` — `3_Game\DayZ\Global\Game.c:730-731`.

Note: `2_GameLib\DayZ\entities\ScriptCamera.c` (a `GenericEntity` free-fly camera on the engine slots) is wrapped in `#ifdef GAME_TEMPLATE` and is **not compiled** in the DayZ runtime — reference only.

---

## 2. FOV / zoom

FOV is per-user and expressed in **radians** (vertical).

**`3_Game\DayZ\DayZGame.c`** (`DayZGame extends CGame`):
- `void SetUserFOV(float pFov)` (3735) — clamps to `OPTIONS_FIELD_OF_VIEW_MIN/MAX`
- `float GetUserFOV()` (3746)
- `static float GetUserFOVFromConfig()` (3751)
- `float GetFOVByZoomType(ECameraZoomType type)` (3762)

**FOV limits** — `3_Game\DayZ\gameplay.c:1316-1317`:

```c
const float OPTIONS_FIELD_OF_VIEW_MIN = 0.75242724772f;   // ~43° vertical
const float OPTIONS_FIELD_OF_VIEW_MAX = 1.30322025726f;   // ~75° vertical
```

(Limits apply to the **user setting**; whether `Camera.SetFOV` clamps to them is not visible in script — COT's slider allows 0.001–4.0 rad, suggesting `Camera.SetFOV` is NOT clamped to user-option limits. Verify in-game.)

**Zoom FOV constants** — `3_Game\DayZ\constants.c:981-983` (`GameConstants`):

```c
const float DZPLAYER_CAMERA_FOV_EYEZOOM         = 0.3926;    // 45°
const float DZPLAYER_CAMERA_FOV_EYEZOOM_SHALLOW = 0.610865;  // 70°
const float DZPLAYER_CAMERA_FOV_IRONSIGHTS      = 0.5236;    // 60°
```

`enum ECameraZoomType { NONE=0, NORMAL=1, SHALLOW=2 }` — `3_Game\DayZ\Enums\ECameraZoomType.c`.

**FOV ↔ focal length:** There is **no** focal-length API and no vanilla conversion anywhere in the scripts. Implement the math yourself: `fov_vertical = 2 * atan(sensorHeight / (2 * focalLength))`. The engine treats FOV values as **vertical** FOV in radians.

---

## 3. Depth of field & post-processing (PPE system)

### Architecture
- `PPEManagerStatic.GetPPEManager()` returns the singleton `PPEManager` — `3_Game\DayZ\PPEManager\PPEManager.c:27, 53`.
- `PPERequesterBank.GetRequester(typename)` / `GetRequester(int index)` — `3_Game\DayZ\PPEManager\PPERequesterBank.c:76, 109`. Registered IDs: `REQ_*` constants (lines 12-47).
- `PPERequesterBase` — `3_Game\DayZ\PPEManager\Requesters\PPERequestPlatformsBase.c:2`:
  - `void Start(Param par = null)` (38), `void Stop(Param par = null)` (44), `bool IsRequesterRunning()` (53)
  - Protected setters (inside requester subclasses): `SetTargetValueFloat(int mat_id, int param_idx, bool relative, float val, int priority_layer, int operator = PPOperators.ADD_RELATIVE)` (155); also `...Bool` (78), `...Int` (117), `...Color` (197), `*Default` variants (100/138/178/221).
- `enum PPOperators { LOWEST, HIGHEST, ADD, ADD_RELATIVE, SUBSTRACT, ..., MULTIPLICATIVE, SET, OVERRIDE }` — `3_Game\DayZ\PPEManager\PPEConstants.c:52-65`.

### Effect material classes (`3_Game\DayZ\PPEManager\Materials\MatClasses\`)
Each is a `PPEClassBase` mapping to a `PostProcessEffectType` (enum in `1_Core\DayZ\proto\EnWorld.c:71-98`). Registered in `PPEManager.c:109-135`. Parameter IDs are `static const int PARAM_*` per class.

- **Depth of Field:** `PPEDepthOfField.c` — `PARAM_FOCALDISTANCE` (`"FocalDistance"`, default 0.1), `PARAM_HYPERFOCAL` (`"HyperFocal"` 0.85), `PARAM_FOCALOFFSET`, `PARAM_BLURFACTOR` (`"BlurFactor"` 4.0), `PARAM_SIMPLEDOF`, `PARAM_SIMPLEHFNEAR`, `PARAM_SIMPLEDOFSIZE`, `PARAM_SIMPLEDOFGAUSS`, `PARAM_DOFLQ` (lines 5-13). File comment: DOF mostly handled by the native `CGame.OverrideDOF` exception path.
- **DOF native exception:** `Exceptions\PPEDOF.c` — `class PPEDOF: PPEClassBase`, `GetPostProcessEffectID()` → `PPEExceptions.DOF` (50). Params: `PARAM_ENABLE`, `PARAM_FOCUS_DIST` (default 2.0, 0..1000), `PARAM_FOCUS_LEN` (default -1), `PARAM_FOCUS_LEN_NEAR` (-1), `PARAM_BLUR` (1.0), `PARAM_FOCUS_DEPTH_OFFSET` (0.0) (lines 8-13, 30-36). `SetFinalParameterValue` (50) calls `g_Game.OverrideDOF(true, focusDist, focusLen, focusLenNear, blur, focusDepthOffset);`
- **Film grain:** `PPEFilmGrain.c`; **Chromatic aberration:** `PPEChromAber.c`; **Color grading:** `PPEColorGrading.c` + `PPEColors.c`; **vignette / lens distortion / bloom** via `PPEGlow.c` material (params `PARAM_LENSDISTORT`, `PARAM_MAXCHROMABBERATION`, `PARAM_LENSCENTERX/Y`, plus Vignette/Bloom params); **motion/dynamic blur:** `PPEDynamicBlur.c`, `PPERotBlur.c`, `PPERadialBlur.c`; **gaussian blur:** `PPEGaussFilter.c` (`PARAM_INTENSITY`); **exposure/EV:** `Exceptions\PPEExposureNative.c` + `CGame.SetEVUser(float)`.

### Concrete vanilla usage (DOF via a requester) — `Requesters\PPERCameraADS_Opt.c:30-51`

```c
void SetValuesIronsights(out array<float> DOF_array)
{
	SetTargetValueBool (PPEExceptions.DOF, PPEDOF.PARAM_ENABLE,            DOF_array[0], PPEDOF.L_0_ADS, PPOperators.SET);
	SetTargetValueFloat(PPEExceptions.DOF, PPEDOF.PARAM_FOCUS_DIST, false, DOF_array[1], PPEDOF.L_1_ADS, PPOperators.SET);
	SetTargetValueFloat(PPEExceptions.DOF, PPEDOF.PARAM_FOCUS_LEN,  false, DOF_array[2], PPEDOF.L_2_ADS, PPOperators.SET);
	// ...
}
```

Starting a requester — `4_World\DayZ\Entities\ManBase\DayZPlayer\DayZPlayerCamera_Base.c:377`:

```c
PPERequesterBank.GetRequester(PPERequesterBank.REQ_CAMERANV).Start(new Param1<int>(PPERequester_CameraNV.NV_TRANSITIVE));
```

`StopAllEffects(int mask)` stops by category (`PPEManager.c:381`). `enum PPERequesterCategory { NONE=0, GAMEPLAY_EFFECTS=2, MENU_EFFECTS=4, MISC_EFFECTS=8, ALL=14 }` (`PPEConstants.c:28-35`).

### Native post-process entry points on CGame — `3_Game\DayZ\Global\Game.c`
- `proto native void OverrideDOF(bool enable, float focusDistance, float focusLength, float focusLengthNear, float blur, float focusDepthOffset)` (1354) — **simplest way to force cinematic DOF.**
- `proto native void AddPPMask(float ndcX, float ndcY, float ndcRadius, float ndcBlur)` (1356), `proto native void ResetPPMask()` (1358) — circular NDC mask (optics vignette).
- `proto native void SetEVUser(float value)` (1352, range -50..50).
- `SetCameraPostProcessEffect(int cam, int priority, PostProcessEffectType type, string materialPath)` — `EnWorld.c:107`; priorities `PostProcessPrioritiesCamera` in `PPEConstants.c:1-26`.

### Legacy PPEffects class (still functional) — `3_Game\DayZ\PPEffects.c`
Marked `//! Deprecated; 'PPEManager' used instead` (line 1) but fully functional static helpers (COT uses these):
- `SetBlur(float)` (168), `SetRadialBlur(px,py,ox,oy)` (156)
- `SetVignette(float intensity, float R, float G, float B, float A)` (434)
- `SetChromAbb(float)` (284), `SetLensEffect(float lens, float chromAbb, float centerX, float centerY)` (413)
- `SetBloom(float thres, float steep, float inten)` (737)
- `OverrideDOF(...)` (503, wraps `g_Game.OverrideDOF`), `ResetDOFOverride()` (518)
- `SetBloodSaturation(float)` (640) — `glow` material `"Saturation"` param
- `ResetAll()` (744)

These use `g_Game.GetWorld().GetMaterial("graphics/materials/postprocess/glow" | "radialblur" | "gauss" | "filmgrainNV")` then `Material.SetParam(name, value)`.

---

## 4. Interpolation / spline math

**`1_Core\DayZ\proto\EnMath3D.c`** — `class Math3D` (27), `enum ECurveType` (20):

- `enum ECurveType { CatmullRom, NaturalCubic, UniformCubic }` (20-25)
- `proto static native vector Curve(ECurveType type, float param, notnull array<vector> points)` (471) — spline evaluator; `param` is 0..1 across the whole point set. In-source example at 461-469.
- `proto static void DirectionAndUpMatrix(vector dir, vector up, out vector mat[4])` (150)
- `proto static void YawPitchRollMatrix(vector ang, out vector mat[3])` (133)
- `proto static void QuatLerp(out float qout[4], float q1[4], float q2[4], float frac)` (397) — **LERP only; there is NO QuatSlerp in vanilla.**
- `proto static void QuatMultiply(out float qout[4], float q1[4], float q2[4])` (414)
- `proto static void MatrixToQuat(vector mat[3], out float d[4])` (360) / `QuatToMatrix(float q[4], out vector mat[3])` (364)
- `proto static vector QuatToAngles(float q[4])` (417) / `MatrixToAngles(vector mat[3])` (379)
- Matrix helpers: `MatrixMultiply4/3`, `MatrixInvMultiply4/3`, `MatrixInverse4/3`, `MatrixOrthogonalize4/3`, `MatrixIdentity4/3`, `ScaleMatrix` (167-303)
- `proto static vector NearestPoint(vector beg, vector end, vector pos)` (478)
- `proto static float AngleFromPosition(vector origin, vector originDir, vector target)` (484)
- `proto static void BlendCartesian(vector samplePosition, array<vector> inPositions, array<float> outWeights)` (503)

**vector** — `1_Core\DayZ\proto\EnConvert.c`: `proto static native vector Lerp(vector v1, vector v2, float t)` (453); `vector.Direction(p1,p2)` (233), `vector.Distance`, `vector.VectorToAngles()` (353), constants `vector.Up/Aside/Forward/Zero` (120-123).

**Math** — `1_Core\DayZ\proto\EnMath.c`: `proto static float Lerp(float a, float b, float time)` (608, no clamp), `InverseLerp` (622), `Clamp` (540), `Remap(...)` (740), `SmoothCD(float val, float target, inout float velocity[], float smoothTime, float maxVelocity, float dt)` (678) — vanilla critically-damped smoother (player cameras use it; ideal for smooth follow/handheld). `RAD2DEG`/`DEG2RAD` constants (16-17).

---

## 5. Screen-space APIs (overlays / 3D→2D / visibility)

**CGame** (`3_Game\DayZ\Global\Game.c`):
- `proto native vector GetScreenPos(vector world_pos)` (965) — world → **screen pixels**; `.x/.y` pixels, `.z` = distance camera→point.
- `proto native vector GetScreenPosRelative(vector world_pos)` (967) — world → **0.0–1.0 relative**; `.z` = distance. Use for resolution-independent composition guides.
- `proto native vector GetWorldDirectionFromScreen(vector world_pos)` (963)
- `proto native vector GetPointerDirection()` (961) — mouse ray from camera.
- `proto native bool IsBoxCollidingGeometry(vector center, vector orientation, vector edgeLength, int iPrimaryType, int iSecondaryType, array<Object> excludeObjects, array<Object> collidedObjects = NULL)` (1344); `IsBoxCollidingGeometryProxy(...)` (1346).
- (`GetObjectsAtPosition` exists at 922/929 but is forbidden by project rules — use triggers/`SphereQuery` alternatives.)

**Engine World projection & visibility** (`1_Core\DayZ\proto\EnWorld.c`):
- `proto vector ProjectVector(int cam, IEntity ent, vector vec)` (110), `proto vector UnprojectVector(int cam, float x, float y, vector dir)` (111) — explicit world↔screen (ent may be NULL for world-space).
- `proto native bool IsBoxVisible(vector mins, vector maxs, int flags)` (274) — **frustum + optional PVS test** (flags&1 = also PVS). The on-screen/visibility check to use.
- `proto native int P2PVisibilityEx(vector from, vector to, int flags)` (265) — line-of-sight.
- `proto int VisEntities(vector origin, vector look, float angle, float radius, out IEntity ents[2], int maxents, int fmask)` (285), `SphereQuery(...)` (268).
- `class OcclusionQuery` (290) — GPU-side point visibility (`GetResult()`, `SetPosition()`).

**Screen size** — `1_Core\DayZ\proto\EnWidgets.c:154`: `proto void GetScreenSize(out float width, out float height)` (on workspace/root widget via `g_Game.GetWorkspace()`). No separate `CGame.GetScreenSize`.

---

## 6. World control — time of day, sun, weather

**`3_Game\DayZ\Global\World.c`** (`class World: Managed`, via `g_Game.GetWorld()`):
- `proto void GetDate(out int year, out int month, out int day, out int hour, out int minute)` (33)
- `proto native void SetDate(int year, int month, int day, int hour, int minute)` (51)
- `proto native void SetTimeMultiplier(float timeMultiplier)` (19) — 0–64, `-1` resets. **0 freezes time-of-day progression.**
- `proto native float GetSunOrMoon()` (55), `GetMoonIntensity()` (54), `IsNight()` (56), `GetLatitude()` (52), `GetLongitude()` (53). **No direct sun-direction vector getter** — sun direction derived by the engine from date+location.
- `GetEyeAccom()` / `SetEyeAccom(float)` (58-59), `SetAperture(float invDiameter)` (154).
- `SetViewDistance(float)` (71), `SetObjectViewDistance(float)` (73), `SetPreferredViewDistance(float)` (69).
- Lighting configs: `LoadNewLightingCfg(string)` (103), `LoadUserLightingCfg(string,string)` (108), `SetUserLightingLerp(float)` (113).

Usage examples: `5_Mission\DayZ\GUI\ScriptConsoleGeneralTab.c:485` (`SetDate`), `3_Game\DayZ\WorldData.c:107`, `4_World\DayZ\Classes\Worlds\Chernarus/Enoch/Sakhal.c`.

**Weather** — `3_Game\DayZ\Weather.c` (via `g_Game.GetWeather()`, declared `Game.c:1349`):
- Phenomena: `GetOvercast()` (183), `GetFog()` (186), `GetRain()` (189), `GetSnowfall()` (192), `GetWindDirection()` (199), `GetWindMagnitude()` (205). Each returns a `WeatherPhenomenon` (class line 28): `float GetActual()` (38), `GetForecast()` (41), `void Set(float forecast, float time=0, float minDuration=0)` (49), `SetLimits(float min, float max)` (74), `SetNextChange(float)` (54), `SetForecastChangeLimits` (95), `SetForecastTimeLimits` (113). `enum EWeatherPhenomenon { OVERCAST, FOG, RAIN, SNOWFALL, WIND_DIRECTION, WIND_MAGNITUDE, VOLFOG_HEIGHT_DENSITY, VOLFOG_DISTANCE_DENSITY, VOLFOG_HEIGHT_BIAS }` (10-21).
- Wind: `GetWind()` (220), `SetWind(vector)` (236), `GetWindSpeed()`/`SetWindSpeed()` (243/251), `SetWindMaximumSpeed()` (264), static `WindDirectionToAngle(vector)` / `AngleToWindDirection(float)` (335/342).
- Storm/lightning: `SetStorm(float density, float threshold, float timeOut)` (214), `SuppressLightningSimulation(bool)` (217).
- Volumetric fog: `SetDynVolFogDistanceDensity(float, time)` (355), `SetDynVolFogHeightDensity` (365), `SetDynVolFogHeightBias` (376), `IsDynVolFogEnabled()` (349).
- **Freezing weather:** `void SetWeatherUpdateFreeze(bool state)` (393) / `GetWeatherUpdateFrozen()` (398), `MissionWeather(bool use)` (383) / `GetMissionWeather()` (388). `OnBeforeChange` hook (125) checks both and blocks automatic forecast changes. To hold a look: set phenomena with `Set(value, 0, veryLargeDuration)` and freeze updates.

Weather is **server-authoritative** — set server-side (vanilla debug RPC pattern: `5_Mission\DayZ\GUI\ScriptConsoleWeatherTab.c`, `3_Game\DayZ\DebugWeatherRPCData.c`).

---

## 7. Object / prop spawning & positioning

**Creation (CGame, `3_Game\DayZ\Global\Game.c`):**
- `proto native Object CreateObject(string type, vector pos, bool create_local=false, bool init_ai=false, bool create_physics=true)` (690) — `create_local=true` → client-only (CameraTools uses this for `staticcamera`).
- `proto native Object CreateObjectEx(string type, vector pos, int iFlags, int iRotation=RF_DEFAULT)` (702).
- Deletion: `ObjectDelete(Object)` (704), `ObjectDeleteOnClient(Object)` (705), `RemoteObjectDelete` (706), `RemoteObjectTreeDelete` (707), `int ObjectRelease(Object)` (710).

**ECE_ flags** — `3_Game\DayZ\CE\CentralEconomy.c:7-40` (exact values):

```
ECE_NONE=0, ECE_SETUP=2, ECE_TRACE=4, ECE_CENTER=8, ECE_UPDATEPATHGRAPH=32,
ECE_ROTATIONFLAGS=512, ECE_CREATEPHYSICS=1024, ECE_INITAI=2048, ECE_AIRBORNE=4096,
ECE_EQUIP_ATTACHMENTS=8192, ECE_EQUIP_CARGO=16384, ECE_EQUIP=24576, ECE_EQUIP_CONTAINER=2097152,
ECE_LOCAL=1073741824, ECE_NOSURFACEALIGN=262144, ECE_KEEPHEIGHT=524288,
ECE_NOLIFETIME=4194304, ECE_NOPERSISTENCY_WORLD=8388608, ECE_NOPERSISTENCY_CHAR=16777216,
ECE_DYNAMIC_PERSISTENCY=33554432
// combos:
ECE_IN_INVENTORY=787456   (CREATEPHYSICS|KEEPHEIGHT|NOSURFACEALIGN)
ECE_PLACE_ON_SURFACE=1060 (CREATEPHYSICS|UPDATEPATHGRAPH|TRACE)
ECE_OBJECT_SWAP=787488
ECE_FULL=25126            (SETUP|TRACE|ROTATIONFLAGS|UPDATEPATHGRAPH|EQUIP)
```

Client-side cinematic props: `ECE_LOCAL|ECE_NOSURFACEALIGN|ECE_KEEPHEIGHT` (or `CreateObject(..., true)`). Server-side world props on terrain: `ECE_PLACE_ON_SURFACE`. Examples: `4_World\DayZ\Static\MiscGameplayFunctions.c:494-638`. `RF_*` rotation flags from `CentralEconomy.c:45`.

**Positioning (Object, `3_Game\DayZ\Entities\Object.c`):**
- `GetPosition()` (293) / `GetWorldPosition()` (297) / `SetPosition(vector)` (300)
- `PlaceOnSurface()` (305)
- `GetOrientation()` (311, yaw/pitch/roll **degrees**) / `SetOrientation(vector)` (317)
- `GetDirection()` (320) / `SetDirection(vector)` (327) / `GetDirectionUp()` (330) / `GetDirectionAside()` (333)
- `GetLocalPos`/`GetGlobalPos` (336-338)

**Transform matrix (Entity, `1_Core\DayZ\proto\EnEntity.c`):**
- `proto external void GetTransform(out vector mat[])` (288)
- `proto native external void SetTransform(vector mat[4])` (356)
- `proto native external vector GetTransformAxis(int axis)` (337)
- `proto native external vector GetYawPitchRoll()` (392) / `SetYawPitchRoll(vector angles)` (427)

---

## 8. Character posing / animation ("actor blocking")

Vanilla CameraTools already does this — spawn a `PlayerBase` "actor" and drive emotes.

**Spawning an actor** — `5_Mission\DayZ\GUI\CameraTools\CTActor.c`: creates a follower (`CTObjectFollower.CreateFollowedObject(type)`), `AddItem(string)` → `p.GetInventory().CreateAttachment(item)` (45), `SetHandsItem(string)` → `HumanInventory.Cast(...).CreateInHands(item)` (77). Actor is a `PlayerBase` (`GetActor()` in `CameraToolsMenu.c:187-193`).

**Playing an animation on an actor** — two verified patterns:

1. Direct action command (`CameraToolsMenu.c:195-206`, `CTEvent.c:43-63`):

```c
EmoteCB cb = EmoteCB.Cast(p.StartCommand_Action(anim, EmoteCB, DayZPlayerConstants.STANCEMASK_ALL));
cb.EnableCancelCondition(true);
```

`StartCommand_Action` — `3_Game\DayZ\human.c:1533`: `proto native HumanCommandActionCallback StartCommand_Action(int pActionID, typename pCallbackClass, int pStanceMask)`. Cancel via `HumanCommandActionCallback.Cancel()` (`CTEvent.c:69`).

2. Locomotion override for "walking" actors (`CTEvent.c:50-51`):

```c
player.GetInputController().OverrideMovementAngle(true, 1);
player.GetInputController().OverrideMovementSpeed(true, 1);
```

**Full command surface** (`3_Game\DayZ\human.c`, `Human`/`DayZPlayer`): `GetCurrentCommandID()` (1439), `StartCommand_Move()` (1445), `StartCommand_Script(HumanCommandScript)` (1589) / `StartCommand_ScriptInst(typename)` (1590) with `class HumanCommandScript` (1209) for **fully custom scripted animation commands**, `StartCommand_Melee/Vehicle/Ladder/Swim/...`, `AddCommandModifier_Action(int pActionID, typename pCallbackClass)` (1563). Bone access: `GetBoneIndexByName(string)`, `GetBonePositionWS(int)`, `GetBoneRotationWS(int, out float[4])` (used in `CameraToolsMenu.c:610-614, 758-760`).

**EmoteManager** — `4_World\DayZ\Classes\EmoteManager.c`; accessor `PlayerBase.GetEmoteManager()` (`PlayerBase.c:1834`). `bool PlayEmote(int id)` (`EmoteManager.c:673`) with IDs from `EmoteConstants`. For spawned dummies the direct `StartCommand_Action` route is the verified approach; `PlayEmote` is designed around the local controlled player + network sync.

Controllability limits: full-body gestures/emotes and locomotion via the command system; per-bone posing is **read-only** (bone WS transforms readable, not writable from script).

---

## 9. Camera shake / handheld feel

**`4_World\DayZ\Classes\CameraShake.c`** — `class CameraShake` (1). Ctor `CameraShake(float strength_factor, float radius, float smoothness, float radius_decay_speed)` (23); `Update(float delta, out float x_axis, out float y_axis)` (41) drives the player's aiming-model shake (`m_Player.GetAimingModel().SetCamShakeValues(x, y)`).

**Trigger API** — `3_Game\DayZ\dayzplayer.c` (`DayZPlayerImplement`):
- `void SpawnCameraShake(float strength=1, float radius=2, float smoothness=5, float radius_decay_speed=6)` (127)
- `void SpawnCameraShakeProper(float strength, float radius, float smoothness, float radius_decay_speed)` (132; native C++ side)

Player camera consumes it in `DayZPlayerCamera_Base.c`: `m_CameraShake` (76), `ProcessCameraShake(float delta, out float leftRight, out float upDown)` (109), `override void SpawnCameraShakeProper(...)` (345). **Tied to the player camera** — for a standalone `staticcamera`, replicate by perturbing camera orientation per frame.

**Noise:** `Math.RandomFloat(min,max)` (`EnMath.c:91`), `RandomFloatInclusive` (106), `RandomFloat01` (126), `RandomInt` (38), `Randomize(int seed)` (141). **No Perlin/Simplex noise exists in the scripts** — implement handheld drift via summed sines or filtered random with `Math.SmoothCD` (`EnMath.c:678`).

---

## 10. Aspect ratio / letterboxing

**No vanilla API to change aspect ratio or render-target size.** Only `Camera.SetFOV`, `SetCameraVerticalFOV`, near/far plane, `SetCameraType(PERSPECTIVE/ORTHOGRAPHIC)`.

**Fullscreen masks/overlays are widget layouts** sized to the workspace:
- Create from `.layout`: `g_Game.GetWorkspace().CreateWidgets("gui/layouts/...")` (`CameraToolsMenu.c:141`).
- Widget sizing (`1_Core\DayZ\proto\EnWidgets.c`): `SetPos/SetSize` (140-141, fractional), `SetScreenPos/SetScreenSize` (142-143, pixels), `GetScreenSize(out w, out h)` (154), `SetColor/SetAlpha/SetRotation` (144-149), `SetTransform(vector mat[4])` (156). Letterbox: two full-width panels anchored top/bottom, sized via `SetScreenSize`.
- **Optics circular vignette** is not a widget — native NDC mask `CGame.AddPPMask(ndcX, ndcY, ndcRadius, ndcBlur)` / `ResetPPMask()` (`Game.c:1356-1358`), driven by `PPERequester_CameraADS.SetValuesOptics` (`PPERCameraADS_Opt.c:3-27`). Reusable for a cinematic circular vignette.
- Widget masking primitives: `SetMaskProgress(float)` (296), `SetMaskTransitionWidth(float)` (310) in `EnWidgets.c` (radial/linear reveal masks).

---

## 11. Screenshots (supplemental, main-session verified)

**`1_Core\DayZ\proto\proto.c:142`** (global free function):

```c
proto native void MakeScreenshot(string name);
```

Doc comment (proto.c:136-141): makes a screenshot and stores it as **DDS**. If `name` begins with `$`, stored at that full path; otherwise stored at `"$profile:ScreenShotes/DATE TIME-NAME.dds"`. → In-game shot thumbnails / storyboard frames are possible from script. Note DDS output needs external conversion for sharing.

## 12. Scripted lights (supplemental, main-session verified)

**`4_World\DayZ\Entities\ScriptedLightBase.c`:**
- `class ScriptedLightBase extends EntityLightSource` (line 10)
- `static ScriptedLightBase CreateLightAtObjMemoryPoint(typename name, notnull Object target, string memory_point_start, string memory_point_target = "", vector global_pos = "0 0 0", float fade_in_time_in_s = 0)` (212)
- `static ScriptedLightBase CreateLight(typename name, vector global_pos = "0 0 0", float fade_in_time_in_s = 0)` (224)
- `void SetBrightnessTo(float value)` (255), `void SetRadiusTo(float value)` (300), `void SetFadeOutTime(float time_in_s)` (341)
- Subclasses: `class PointLightBase extends ScriptedLightBase` (`...\ScriptedLightBase\PointLightBase.c:1`), `class SpotLightBase extends ScriptedLightBase` (`...\ScriptedLightBase\SpotLightBase.c:1`).
- `EntityLightSource` (1_Core) provides color/brightness/radius/shadow natives — vanilla usages: flashlights, flares, fireplaces (e.g. `4_World\DayZ\Entities\ItemBase\FlashlightLight.c` style subclasses define color/brightness presets).

→ **Light planning is feasible**: spawn client-local or server-side light entities wrapped in PCT light classes. Full `EntityLightSource` surface should be verified during implementation (SetColorRGB, SetCastShadow etc. in `1_Core\DayZ\proto\EnEntity.c` / EntityLightSource proto).

---

### Best vanilla reference code
- Keyframe camera path + interpolation + FOV/DOF + actors + events: `5_Mission\DayZ\GUI\CameraTools\` (`CameraToolsMenu.c`, `CTKeyframe.c`, `CTActor.c`, `CTEvent.c`, `CTSaveStructure.c`, `CTObjectFollower.c`).
- Player camera math / smoothing / shake: `4_World\DayZ\Entities\ManBase\DayZPlayer\DayZPlayerCamera_Base.c`, `...CameraIronsights.c`.
- PPE requester authoring: `3_Game\DayZ\PPEManager\Requesters\PPERCameraADS_Opt.c` (DOF + mask + lens), `PPERInventoryBlur.c` (minimal example).
