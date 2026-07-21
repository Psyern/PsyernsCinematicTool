# Research: COT Internals — Technical Reference for PsyernsCinematicTool

> Source repo: `C:\Users\Administrator\Desktop\Mod Repositories\DayZ-CommunityOnlineTools-production`
> COT mod dir: `JM\COT`. Community Framework (CF, dependency): sibling repo `DayZ-CommunityFramework-production\JM\CF`.
> Verified against source on 2026-07-21. All line numbers refer to the production branch snapshot on disk.

**Key architectural fact:** COT is *not* self-contained. Its module system, RPC dispatch, input-binding system and base event lifecycle come from **Community Framework** (`JM_CF_Scripts`). COT's `JMModuleBase`, `JMModuleManager`, `JMModuleConstructor` are `modded class` extensions of CF base classes. PsyernsCinematicTool therefore depends on **both** CF and COT.

---

## 1. Camera Module Flow

Files:
- `JM\COT\Scripts\5_Mission\CommunityOnlineTools\modules\Camera\JMCameraModule.c`
- `JM\COT\Scripts\5_Mission\CommunityOnlineTools\modules\Camera\JMCameraForm.c`
- `JM\COT\Scripts\3_Game\CommunityOnlineTools\Entities\Cameras\JMCameraBase.c` (base + globals + enum)
- `JM\COT\Scripts\3_Game\CommunityOnlineTools\Entities\Cameras\JMCinematicCamera.c` (the freecam)
- `JM\COT\Scripts\4_World\CommunityOnlineTools\Entities\Cameras\JMSpectatorCamera.c` (follow-a-player cam)

### Two distinct camera classes
- `JMCinematicCamera : JMCameraBase` (3_Game) — free-flying "freecam" driven by WASD/mouse in `OnUpdate()` (JMCinematicCamera.c:44). Movement uses engine input actions `UAMoveForward/Back/Right/Left`, `UALeanLeft/Right` (altitude), `UAAim*`; roll on `UALookAround`; speed via `UACameraToolSpeedIncrease/Decrease`. Supports position "traveling" (`SetupTraveling`, JMCinematicCamera.c:228) — **linear-only** waypoint travel: per-segment `t = currentTime/targetTime`, optional cubic `SmoothStep(t)`, `vector.Lerp(startPosition, endPosition, t)`. No rotation/FOV/focus keyframing, no splines.
- `JMSpectatorCamera : JMCameraBase` (4_World) — cinematic 1st/3rd-person/dolly camera following a `SelectedTarget` entity (JMSpectatorCamera.c:20). Heavy dolly-path logic in `OnUpdate` (line 121). Used when spectating another player.
- `JMCameraBase : Camera` (JMCameraBase.c:22) — shared base. Globals: `static JMCameraBase CurrentActiveCamera;` and `static JMCameraBase COT_PreviousActiveCamera;` (lines 11-12), `enum JMCamera3rdPersonMode { OFF, DEFAULT, DOLLY, AUTO }` (line 14). Tuning consts at top of file (`CAMERA_FOV_SPEED_MODIFIER=5.0`, `CAMERA_BOOST_MULT=5.0`, `CAMERA_VELDRAG=0.9`, `CAMERA_MSENS=35.0`, `CAMERA_SMOOTH=0.8`, `CAMERA_DOFFSET=10.0`). Uses `EntityEvent.FRAME|POSTFRAME`; drives everything through virtual `void OnUpdate(float timeslice)` called from `EOnPostFrame`. Every 0.5 s it calls `g_Game.UpdateSpectatorPosition(GetPosition())`.
- `JMCinematicCamera` easing helpers: private `SmoothStep(t)` = `t*t*(3-2t)`; `SmootherStep(t)` = quintic `t*t*t*(t*(6t-15)+10)`; `CalcAccelerationRate(inout float t, inout float rate, float dt, bool increaseSpeeds)` for slow-accel/instant-decel input shaping.

### Module class
`class JMCameraModule : JMRenderableModuleBase` (JMCameraModule.c:1). Ctor registers permission:

```c
void JMCameraModule()  // line 32
	GetPermissionsManager().RegisterPermission( "Camera.View" );
```

Key overrides:
- `override bool HasAccess()` → `GetPermissionsManager().HasPermission( "Camera.View" )` (line 45)
- `override string GetInputToggle()` → `"UACOTToggleCamera"` (line 50) — opens the form
- `override string GetLayoutRoot()` → `"JM/COT/GUI/layouts/camera_form.layout"` (line 55)
- `override string GetTitle()` → `"#STR_COT_CAMERA_MODULE_NAME"` (line 60)
- `override int GetRPCMin()` → `JMCameraModuleRPC.INVALID`; `GetRPCMax()` → `JMCameraModuleRPC.COUNT` (lines 220-228)

### Enable / disable — exact RPC flow
RPC enum (`JM\COT\Scripts\3_Game\CommunityOnlineTools\RPC.c:42`):

```c
enum JMCameraModuleRPC { INVALID = 10160, Enter, Leave, Leave_Finish, UpdatePosition, COUNT }
```

Dispatch in `override void OnRPC(...)` (JMCameraModule.c:230) switches on the enum to `RPC_Enter / RPC_Leave / RPC_Leave_Finish / RPC_UpdatePosition`.

Entry sequence:
1. Keybind/toggle → `ToggleCamera(UAInput)` (line 603). Guards `HasAccess()` and `GetCommunityOnlineToolsBase().IsActive()`. If a `JMCinematicCamera` is already active → `Leave()`, else `Enter()`.
2. `Enter()` (line 249): calls `GetPlayer().COT_TempDisableOnSelectPlayer()` and `COT_RememberVehicle()`. Offline → `Server_Enter(...)` directly. Client → `ScriptRPC` writing `g_Game.GetCurrentCameraPosition()`, `rpc.Send(g_Game.GetPlayer(), JMCameraModuleRPC.Enter, true, NULL)`.
3. `RPC_Enter` (line 361): on host, re-checks `GetPermissionsManager().HasPermission("Camera.View", senderRPC)`, reads position, calls `Server_Enter(senderRPC, target, position)`. On client, calls `Client_Enter()`.
4. `Server_Enter(sender, target, position)` (line 329) — the important server path:
   - `g_Game.SelectPlayer( sender, NULL )` — **detaches the identity's control from the player body**
   - `g_Game.SelectSpectator( sender, "JMCinematicCamera", position )` — engine spawns the freecam and makes the client spectate it
   - Replies `rpc.Send( NULL, JMCameraModuleRPC.Enter, true, sender )` so the client runs `Client_Enter()`.
   - Offline branch instead does `g_Game.CreateObject("JMCinematicCamera", position, false)` and `SetActive(true)`.
5. `Client_Enter()` (line 270): stores `COT_PreviousActiveCamera`, casts `Camera.GetCurrentCamera()` into `CurrentActiveCamera`, `SetActive(true)`, aligns direction to head bone, and **`player.GetInputController().SetDisabled(true)`** (freezes player input, line 297).

Leave sequence:
- `Leave()` (line 385) → client sends `JMCameraModuleRPC.Leave`.
- `RPC_Leave` (line 527) → server `Server_Leave` (line 481): computes `waitForPlayerIdleTimeout`, calls `player.COTResetSpectator()`, resets `m_JM_CameraPosition = vector.Zero`, and either RPCs `Leave` back to client (writing the timeout) or, at the end, `g_Game.SelectPlayer(sender, player)` to re-attach control.
- `Client_Leave(timeout)` (line 402): `SetActive(false)`, deletes the `JMCinematicCamera` via `g_Game.ObjectDeleteOnClient`, restores `COT_PreviousActiveCamera` if it was a spectator cam, `PPEffects.ResetDOFOverride()`, `GetInputController().SetDisabled(false)`, then waits for the player animation to be idle (`Client_Check_Leave`, line 456) before RPCing `Leave_Finish` → server `g_Game.SelectPlayer`.

### How the player entity is handled while camera active
- The player body **stays in the world** (not a dummy). The engine spectator system (`SelectPlayer(sender, NULL)` + `SelectSpectator`) detaches the camera/identity; the body remains.
- **Player input frozen** on client via `GetInputController().SetDisabled(true)` (line 297), re-enabled on leave.
- **Server-side state tracking** on `PlayerBase` (`JM\COT\Scripts\4_World\CommunityOnlineTools\Entities\Player\PlayerBase.c`):
  - `Object m_JM_SpectatedObject;` (line 42) — set when spectating another entity
  - `vector m_JM_CameraPosition;` (line 43) — current freecam position (server authoritative; `vector.Zero` == not in freecam)
  - `EnterFullmap(player)` (JMCameraModule.c:594): on first enter it `player.SetLastPosition()` and **`player.COTSetGodMode(true, false)`** — godmode is auto-enabled during fullmap/freecam and the previous state remembered.
  - `COTUpdateSpectatorPosition()` (PlayerBase.c:904) moves the real body toward the camera; when within 150 m of origin it drops the body back to `m_JMLastPosition`, toggles gravity/invisibility.
  - Optional "fullmap freecam update": if `m_EnableFullmapCamera`, the client periodically RPCs `JMCameraModuleRPC.UpdatePosition` with the cam position (JMCameraModule.c:151-162, RPC_UpdatePosition line 567) so the server relocates the streaming body.
- Head hiding when spectator cam is very close: `spectatedPlayer.SetHeadInvisible(true)` (JMSpectatorCamera.c:1119); `SetHeadInvisible` defined in `JM\COT\Scripts\4_World\CommunityOnlineTools\Entities\Player\DayZPlayerImplement.c:99` (hides Head/Headgear/Mask/Eyewear).

### Permission gating (exact strings)
- Single permission: **`"Camera.View"`** — registered (JMCameraModule.c:34), checked in `HasAccess()` (47), re-verified server-side in `RPC_Enter` (369), `RPC_Leave` (535), `RPC_Leave_Finish` (557), `RPC_UpdatePosition` (575).

### FOV / focus / blur / DOF (which Camera methods)
In `OnUpdate` (client-only block, JMCameraModule.c:85-199):
- FOV: `CurrentActiveCamera.GetCurrentFOV()` and `CurrentActiveCamera.SetFOV( m_CurrentFOV )` (lines 95, 106). Zoom key uses `GameConstants.DZPLAYER_CAMERA_FOV_EYEZOOM`.
- Depth of field: `CurrentActiveCamera.SetFocus( m_FDist, m_Blur )` (line 130) plus post-process `PPEffects.OverrideDOF( true, m_FDist, m_Flength, m_FNear, m_Blur, CAMERA_DOFFSET )` (line 131).
- Also `PPEffects.SetBlur`, `PPEffects.SetChromAbb`, `PPEffects.SetVignette`, `PPEffects.SetBloom` (lines 93, 132-134). Reset on leave via `PPEffects.ResetDOFOverride()`.
- Auto-focus raycasts from `g_Game.GetCurrentCameraPosition()` along `GetCurrentCameraDirection()` (lines 111-124).

### Camera form UI (fields/sliders)
`class JMCameraForm : JMFormBase` (JMCameraForm.c:1). Two panels via a SelectBox `{"Effects","Traveling"}`.
- Effects panel (`InitCameraEffects`, line 130) sliders: **Blur strength** (0-100), **Focus distance** (0-1000 m), **Focal length** (0-1000), **Focal near** (0-1000), **Exposure** (0-1), **Chrom Abb** (0-1), **Vignette** (0-1), **Speed** (0.001-10, bound to `JMCameraBase.s_CurrentSpeed`), **FOV** (0.001-4, bound to `m_Module.m_TargetFOV`). Checkboxes: "Enable fullmap freecam update", "Hide scope in 1st person spectate ADS", "Hide grass in freecam".
- Traveling panel (`InitCameraTraveling`, line 219): "Look at Object" button, editable Time, Smooth checkbox, editable Position vector (Set/Copy/Paste), a nav-button position list, "Travel through Positions" (calls `m_Module.GoToSelection`).
- Slider→module wiring in `OnChange_*` handlers (293-381); DOF auto on/off logic in `OnSliderUpdate` (250). Module↔form persistence in `OnInit` (85) and `OnHide` (52).

### Keybinds / input actions
Registered in code via `override void RegisterKeyMouseBindings()` (JMCameraModule.c:203):

```c
Bind( new JMModuleBinding( "ToggleCamera",   "UACameraToolToggleCamera",  true ) );
Bind( new JMModuleBinding( "ZoomForwards",   "UACameraToolZoomForwards",  true ) );
Bind( new JMModuleBinding( "ZoomBackwards",  "UACameraToolZoomBackwards", true ) );
Bind( new JMModuleBinding( "Toggle3rdPerson","UAPersonView",              true ) );
Bind( new JMModuleBinding( "LeftShoulder",   "UALeanLeft",                true ) );
Bind( new JMModuleBinding( "RightShoulder",  "UALeanRight",               true ) );
```

Input *actions* are declared in **`JM\COT\Scripts\Data\Inputs.xml`** (registered via `inputs = "JM/COT/Scripts/Data/Inputs.xml"` in `JM\COT\Scripts\config.cpp:33`, inside `CfgMods` class `JM_CommunityOnlineTools`). Relevant `<input>` names and default binds:
- `UACameraToolToggleCamera` (line 24; default `kInsert`, line 109) — toggles freecam
- `UACameraToolZoomForwards` / `UACameraToolZoomBackwards` (26-27; `kLControl`+mWheel)
- `UACameraToolSpeedIncrease` / `UACameraToolSpeedDecrease` (29-30; mWheelUp/Down)
- Menu-open bind `UACOTToggleCamera` (line 9; default `kRMenu`+`kY`, line 174)

---

## 2. Module System

### Base hierarchy (all in CF, `modded` by COT)
`JMExampleModule : JMRenderableModuleBase : JMModuleBase : CF_ModuleWorld : CF_ModuleGame : CF_ModuleCore`.

- CF base `JMModuleBase` (deprecated-but-active): `DayZ-CommunityFramework-production\JM\CF\Scripts\4_World\CommunityFramework\Module\Deprecated\JMModuleBase.c`. Its `Init()` (line 22) enables **every** lifecycle event and calls `RegisterKeyMouseBindings()`. It bridges CF's arg-based events to the "old" simple virtual methods you override:

```c
void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx);   // line 278
void OnMissionStart();  void OnMissionFinish();  void OnMissionLoaded();
void OnUpdate(float timeslice);
void OnSettingsUpdated();  void OnClientPermissionsUpdated();
void OnClientNew(...); void OnClientReady(...); void OnInvokeConnect(...); void OnRespawn(int time); // etc.
```

- COT's `modded class JMModuleBase`: `JM\COT\Scripts\4_World\CommunityOnlineTools\Classes\Module\JMModuleBase.c` (adds webhook + sub-command helpers; `OnMissionStart` grabs the webhook module via `GetModuleManager().GetModule(JMWebhookModule)`).
- `JMRenderableModuleBase : JMModuleBase`: `JM\COT\Scripts\4_World\CommunityOnlineTools\Classes\Module\JMRenderableModuleBase.c`. Base for **UI/menu modules**. Key members: `GetLayoutRoot()`, `GetInputToggle()`, `GetTitle()`, `GetIconName()`, `HasAccess()`, `Show()/Hide()/Close()/ToggleShow()` (lines 170-230), `InitButton()` (sidebar button, line 26), `SetForm/GetForm/InitForm` (85-99), auto-binds the toggle input in `RegisterKeyMouseBindings()` (272) → `Input_ToggleShow`. Implements `OnClientPermissionsUpdated()` (251) — hides the window if access is lost.

### Registration mechanism (NO macros — a constructor list)
Modules are registered by **modding `JMModuleConstructor.RegisterModules()`**:

```c
// JM\COT\Scripts\5_Mission\CommunityOnlineTools\modules\JMModuleConstructor.c
modded class JMModuleConstructor
{
	override void RegisterModules( out TTypenameArray modules )
	{
		super.RegisterModules( modules );
		modules.Insert( JMPlayerModule );
		modules.Insert( JMObjectSpawnerModule );
		modules.Insert( JMESPModule );
		modules.Insert( JMTeleportModule );
		modules.Insert( JMCameraModule );
		modules.Insert( JMWeatherModule );
		// ...
	}
}
```

- Core modules (`COTModule`, `JMWebhookModule`) registered by modding `JMModuleConstructorBase` in 4_World: `JM\COT\Scripts\4_World\CommunityOnlineTools\Classes\Module\JMModuleConstructor.c`.
- The manager: COT's `modded class JMModuleManager` (`...\Classes\Module\JMModuleManager.c`) keeps a second list `m_COTModules` of renderable ones (`InitModule` override line 17; `GetCOTModules()` line 27). Base CF manager: `CF_ModuleWorldManager`/`CF_ModuleGameManager`.
- Accessor: `GetModuleManager()` returns the `JMModuleManager`; retrieve a module with `GetModuleManager().GetModule( JMPlayerModule )` (see JMExampleForm.c:103, 247).

**→ For PsyernsCinematicTool: `modded class JMModuleConstructor { override void RegisterModules(out TTypenameArray modules) { super.RegisterModules(modules); modules.Insert(PCT_CinematicModule); } }` in the 5_Mission layer.**

### Minimal module example (canonical, shipped in COT)
`JM\COT\Scripts\5_Mission\CommunityOnlineTools\modules\Example\JMExampleModule.c` (48 lines) and `JMExampleForm.c` — commented "This file is an example class for anyone looking to learn how to mod COT". Registered only under `#ifdef DIAG` (JMModuleConstructor.c:18-19).

```c
class JMExampleModule: JMRenderableModuleBase
{
	void JMExampleModule()
	{
		GetPermissionsManager().RegisterPermission( "Admin.Example.View" );
		GetPermissionsManager().RegisterPermission( "Admin.Example.Button" );
	}
	override bool HasAccess()       { return GetPermissionsManager().HasPermission("Admin.Example.View"); }
	override string GetLayoutRoot() { return "JM/COT/GUI/layouts/Example_form.layout"; }
	override string GetTitle()      { return "Example Module"; }
	override string GetIconName()   { return "E"; }
}
```

Lifecycle to override: constructor + `OnMissionStart()/OnMissionLoaded()`; `OnUpdate(float timeslice)`; `OnClientPermissionsUpdated()`; `RegisterKeyMouseBindings()`; RPC via `OnRPC(...)` + `GetRPCMin/Max`.

### Forms / menus attach to modules
- Form base: `JMFormBase` at `JM\COT\Scripts\4_World\CommunityOnlineTools\Classes\GUI\JMFormBase.c`. Subclass it (e.g. `JMCameraForm : JMFormBase`).
- Attachment: `JMRenderableModuleBase.Show()` (JMRenderableModuleBase.c:170) creates a `CF_Window`/COT window from `GetLayoutRoot()`, then `widgets.GetScript(m_Form)` binds the layout's root script to your `JMFormBase`, then `m_Form.Init(window, this)`.
- Form entry points: `SetModule(JMRenderableModuleBase mdl)` (cast to concrete module — JMCameraForm.c:80), `OnInit()` (build widgets — line 85), `Update()`, `OnShow()/OnHide()`, `OnSettingsUpdated()`, `OnClientPermissionsUpdated()`.
- Widgets built with `UIActionManager` factory (`CreateScroller`, `CreateSlider`, `CreateCheckbox`, `CreateButton`, `CreateSelectionBox`, `CreateGridSpacer`, `CreateEditableVector`, etc. — see JMCameraForm.c and JMExampleForm.c).

---

## 3. Permissions

Manager: **`JMPermissionManager`** — `JM\COT\Scripts\4_World\CommunityOnlineTools\Classes\PermissionsOld\JMPermissionManager.c`. Global accessor `JMPermissionManager GetPermissionsManager()` at line 507 (backed by `ref JMPermissionManager g_cot_PermissionsManager;` line 505). (Folder is named `PermissionsOld` but it is the *active* implementation.)

Declare (**before mission load**, typically in the module constructor):

```c
void RegisterPermission( string permission )   // JMPermissionManager.c:162
```

Registering after mission load throws `Error("Cannot register new permissions once mission is loaded!")` (line 171).

Check (overloaded, client and server):

```c
bool HasPermission( string permission, out JMPlayerInstance instance = null )                    // line 194 (CLIENT: local instance)
bool HasPermission( string permission, PlayerIdentity ihp )                                      // line 214 (offline → true)
bool HasPermission( string permission, PlayerIdentity identity, out JMPlayerInstance instance )  // line 223 (SERVER: Players.Get(identity.GetId()))
bool HasPermissions( TStringArray permissions, PlayerIdentity identity, out JMPlayerInstance instance, bool requireAll = true ) // line 247
```

- On client, `HasPermission(permission)` returns true immediately if `IsMissionHost()` (SP/listen host), else consults the replicated `JMPlayerInstance`.
- On dedicated server, always pass the `PlayerIdentity` of the RPC sender: `GetPermissionsManager().HasPermission("Camera.View", senderRPC)` — the security gate to repeat in **every** server RPC handler.
- Permission strings are hierarchical dot-paths (`"Camera.View"`, `"Admin.Example.Button"`, `"ESP.View"`, ...). `JMPlayerInstance` lives at `JM\COT\Scripts\3_Game\CommunityOnlineTools\Permissions\JMPlayerInstance.c` (+ `4_World\...\JMPlayerInstanceWorld.c`).

---

## 4. RPC Framework

COT uses the **engine `ScriptRPC`** class for send, dispatched through **CF's module-RPC router**:

- Each module declares an RPC id range via `GetRPCMin()`/`GetRPCMax()` returning enum bounds (`CF_ModuleGame.c:137-145`, default `-1`; `EnableRPC()` line 147 registers the module into `CF_ModuleGameManager.s_RPC` when the range is valid). All COT ranges are centralized in `JM\COT\Scripts\3_Game\CommunityOnlineTools\RPC.c` (each enum: `INVALID = <base>` ... `COUNT`; Camera `10160`, Vehicles `10180`, Object `10220`, Teleport `10240`, Weather `10280`, ESP `10300`, Player `10320`, Loadout `10460`).
- Incoming: CF calls `JMModuleBase.OnRPC(Class sender, CF_EventArgs args)` (CF JMModuleBase.c:90) which unpacks a `CF_EventRPCArgs` and calls the old-style `OnRPC(rpc.Sender, rpc.Target, rpc.ID, rpc.Context)` you override.

Concrete example (freecam enter):

```c
// CLIENT SEND — JMCameraModule.c:264
ScriptRPC rpc = new ScriptRPC();
rpc.Write( g_Game.GetCurrentCameraPosition() );
rpc.Send( g_Game.GetPlayer(), JMCameraModuleRPC.Enter, true, NULL );
//   target = player object, rpcType = enum, guaranteed = true, recipient identity = NULL (to server)

// DISPATCH — JMCameraModule.c:230
override void OnRPC( PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx )
{
	switch ( rpc_type )
	{
		case JMCameraModuleRPC.Enter:
			RPC_Enter( ctx, sender, target );
			break;
		// ...
	}
}

// SERVER RECEIVE — JMCameraModule.c:361
private void RPC_Enter( ParamsReadContext ctx, PlayerIdentity senderRPC, Object target )
{
	if ( IsMissionHost() )
	{
		if ( !GetPermissionsManager().HasPermission( "Camera.View", senderRPC ) )
			return; // gate
		vector position;
		if ( !ctx.Read( position ) )
			return;
		Server_Enter( senderRPC, target, position );
	}
	else
	{
		Client_Enter(); // server→client reply path
	}
}
```

Server→client reply: `rpc.Send( NULL, JMCameraModuleRPC.Enter, true, sender )` (line 355) — 4th arg is the target `PlayerIdentity`.

CF also provides a separate `RPCManager` (`...\JM\CF\Scripts\3_Game\CommunityFramework\RPC\RPCManager.c`) plus `AddLegacyRPC(...)` (CF_ModuleGame.c:101) and NetSync helpers (`RegisterNetSyncVariable`, `SetSynchDirty` — CF_ModuleGame.c:109/114). COT modules use the `ScriptRPC` + enum-range pattern above.

---

## 5. External Extension — How a Separate Mod Adds a COT Module

### CfgPatches names in COT (candidate `requiredAddons`)
- `JM_COT_Scripts` — `JM\COT\Scripts\config.cpp` (the script module; **required to hook the module system**). Its own `requiredAddons[] = { "JM_CF_Scripts", "JM_COT_GUI", "DZ_Data" }` (Scripts/config.cpp:6-11).
- `JM_COT_GUI` — `JM\COT\GUI\config.cpp` (layouts + imagesets)
- `JM_COT_LanguageCore` — `JM\COT\languagecore\config.cpp`
- `JM_COT_Objects_Debug` — `JM\COT\Objects\Debug\config.cpp`
- `JM_CommunityOnlineTools` — the `CfgMods` mod container (Scripts/config.cpp:19); registers inputs and the four script modules.
- CF provides `JM_CF_Scripts` (from the CF repo).

**Recommendation for PsyernsCinematicTool `config.cpp` CfgPatches:**

```cpp
requiredAddons[] = { "DZ_Data", "JM_CF_Scripts", "JM_COT_Scripts", "JM_COT_GUI" };
```

### Script module layers COT uses (mirror these)
From `CfgMods → JM_CommunityOnlineTools → defs` (Scripts/config.cpp:63-104):
- `engineScriptModule`: `JM/COT/Scripts/Common`, `JM/COT/Scripts/1_Core`
- `gameScriptModule`: `JM/COT/Scripts/Common`, `JM/COT/Scripts/3_Game`
- `worldScriptModule`: `JM/COT/Scripts/Common`, `JM/COT/Scripts/4_World`
- `missionScriptModule`: `JM/COT/Scripts/Common`, `VPPAdminTools/Definitions`, `JM/COT/Scripts/5_Mission`

PsyernsCinematicTool declares its own `CfgMods` entry adding `PsyernsCinematicTool/Scripts/3_Game|4_World|5_Mission`. Put `modded class JMModuleConstructor` in **5_Mission**, camera/entity classes in **3_Game/4_World** matching where COT defines `JMCameraBase`/`JMSpectatorCamera`.

### Documentation
No developer doc in the repo — `README.md` covers packaging (`project.cfg`) only. The intended template is the shipped **Example module** (`modules\Example\JMExampleModule.c` / `JMExampleForm.c`).

---

## 6. Misc Cinematic-Relevant Features Already in COT

All in `JM\COT\Scripts\5_Mission\CommunityOnlineTools\modules\...`. Server-authoritative toggles live on `PlayerBase` (`4_World\...\Entities\Player\PlayerBase.c`) as `COT*` methods with matching RPC ids (RPC.c:154).

**Player module** — `modules\Player\JMPlayerModule.c` (enum `JMPlayerModuleRPC`, base 10320):
- `void SetGodMode( bool value, array<string> guids )` (line 1500) → `PlayerBase.COTSetGodMode(bool mode, bool preference=true)` (PlayerBase.c:683). Input `UAPlayerModuleGodMode` (default `kU`).
- `void SetFreeze( bool value, array<string> guids )` (line 1591) → `PlayerBase.COTSetFreeze(bool)` (PlayerBase.c:717). Input `UAPlayerModuleFreezePlayer`.
- `void SetInvisible( int value, array<string> guids )` (line 1785) → `COTSetInvisibility(JMInvisibilityType, ...)`, `COTIsInvisible()`; gated by `JM_COT_INVISIBILITY` define. Input `UAPlayerModuleInvisibility`.
- `void TeleportTo( vector position, array<string> guids )` (line 862).
- Spectating: `StartSpectating`/`EndSpectating`/`EndSpectating_Finish` RPCs; sets `playerSpectator.m_JM_SpectatedObject = spectateObject` (line 1165), uses `JMSpectatorCamera`.
- Other: `SetCannotBeTargetedByAI`, `SetUnlimitedAmmo`, `SetUnlimitedStamina`, `SetAdminNVG`, `SetReceiveDamageDealt`, `SetRemoveCollision`, `SetScale`, `Heal`, `SetBloodyHands`, vitals setters.

**Weather module** — `modules\Weather\JMWeatherModule.c` (enum base 10280):
- **Freeze time**: `void SetFreezeTime(bool state)` (line 173) — server `Exec_FreezeTime`, client `Send_FreezeTime`; RPC id `JMWeatherModuleRPC.FreezeTime`.
- `SetStorm(density, threshold, minTimeBetweenLightning)` (183), `SetFog(forecast, time, minDuration)` (199), plus RPCs: `Rain`, `RainThresholds`, `Snow`, `SnowThresholds`, `Overcast`, `DynamicFog`, `WindMagnitude/Direction/FunctionParams`, `Date`, presets (`UsePreset/CreatePreset/UpdatePreset/RemovePreset`), `SetWeatherBehavior`.

**Teleport module** — `modules\Teleport\JMTeleportModule.c` (enum base 10240): `Position`, `PositionRaycast` (teleport to cursor — input `UATeleportModuleTeleportCursor`, default `kH`), `Location`/`AddLocation`/`RemoveLocation`.

**Object spawner** — `modules\Object\` (`JMObjectSpawnerModule`, enum base 10220: `Position`, `Inventory`, `Delete`) — spawn arbitrary objects/props at position or cursor; inputs `UAObjectModuleSpawnInfected/Animal/Wolf`, `UAObjectModuleDeleteOnCursor` (default `kDelete`).

**ESP module** — `modules\ESP\JMESPModule.c` (perm `"ESP.View"`, enum base 10300). Object manipulation RPCs: `SetPosition`, `SetOrientation`, `SetHealth`, `DeleteObject`, `MakeItemSet`, `DuplicateAll`, `DeleteAll`, `MoveToCursor`. COT's in-world object selection/manipulation — natural companion for placing cinematic props.

**Loadout / Vehicles / Map** modules also exist (see JMModuleConstructor.c list).

---

## 7. Exact Top-Level Folder Layout (to mirror)

Repo root:

```
DayZ-CommunityOnlineTools-production\
├─ JM\COT\                (the mod)
├─ Missions\COT.ChernarusPlus\
├─ ServerProfile\{CommunityOnlineTools, PermissionsFramework, VPPAdminTools}\
├─ README.md, LICENSE, .gitattributes, .gitignore, SetupWorkdrive.bat
```

`JM\COT\` layout:

```
JM\COT\
├─ GUI\
│   ├─ config.cpp                      (CfgPatches JM_COT_GUI; also CfgWorlds names)
│   ├─ layouts\                        (*.layout: camera_form.layout, player_form.layout, esp_form.layout, ...)
│   └─ textures\modules\               (module icons, e.g. Camera.paa)
├─ languagecore\
│   └─ config.cpp                      (CfgPatches JM_COT_LanguageCore)
├─ Objects\Debug\
│   └─ config.cpp                      (CfgPatches JM_COT_Objects_Debug)
├─ Scripts\
│   ├─ config.cpp                      (CfgPatches JM_COT_Scripts + CfgMods JM_CommunityOnlineTools; inputs=Data/Inputs.xml)
│   ├─ Common\
│   ├─ Data\                           (Inputs.xml, Version.hpp, Credits.json)
│   ├─ 1_Core\
│   ├─ 3_Game\CommunityOnlineTools\    (RPC.c, Entities\Cameras\{JMCameraBase,JMCinematicCamera}.c, Permissions\, PPeManagers\)
│   ├─ 4_World\CommunityOnlineTools\
│   │     ├─ Classes\{Module, GUI, Permissions, PermissionsOld, Commands, ...}\
│   │     └─ Entities\{Cameras\JMSpectatorCamera.c, Player\{PlayerBase,DayZPlayerImplement}.c}\
│   └─ 5_Mission\CommunityOnlineTools\
│         ├─ CommunityOnlineTools.c, MissionGameplay.c, MissionServer.c, MissionMainMenu.c
│         ├─ gui\
│         └─ modules\                  (Camera, ESP, Object, Player, Teleport, Weather, Loadout, Vehicles, COTMap, Example, ...)
│               ├─ JMModuleConstructor.c   ← where modules are registered
│               └─ Camera\{JMCameraModule.c, JMCameraForm.c}
└─ Workbench\                          (build/pack tooling; not shipped)
```

### Extension entry points summary (cleanest hooks)
1. **Add a module:** `modded class JMModuleConstructor { override void RegisterModules(out TTypenameArray modules) { super.RegisterModules(modules); modules.Insert(YourModule); } }` (5_Mission).
2. **Reuse the camera:** drive the existing `JMCameraModule` via its public API — `Enter()`, `Leave()`, `GetCamera()` (returns `CurrentActiveCamera`), `SetTargetFOV(float)`, `SetFreezeCam(bool)`/`SetFreezeMouse(bool)` (static), `GoToSelection(positions, times, smooth)`, `LookAtSelection()` — or subclass `JMCameraBase`/`JMCinematicCamera` for a bespoke cinematic camera. `CurrentActiveCamera` / `COT_PreviousActiveCamera` globals (JMCameraBase.c:11-12) let you detect/cooperate with an active COT freecam.
3. **Permissions:** register your own in the module ctor via `GetPermissionsManager().RegisterPermission("CinematicTool.View")`, gate server RPCs with `HasPermission(perm, senderRPC)`.
4. **RPC:** pick an unused id base (COT's highest base is Loadout at 10460; use e.g. 10500+), define `enum PCT_ModuleRPC { INVALID = 10500, ..., COUNT }`, override `GetRPCMin/GetRPCMax` and `OnRPC`.
