# Systemarchitektur & COT-Integration — PsyernsCinematicTool (PCT)

Stand: 2026-07-21 · Status: Entwurf v1 · Autor: Psyern · Dokument: Plan_B1_Architektur.md

**Abstract:**
PsyernsCinematicTool (PCT) ist ein Cinematic-/Previsualisierungs-Tool für DayZ, das als externes Modul auf der Kamera- und Modul-Infrastruktur von Community Online Tools (COT) und Community Framework (CF) aufsetzt.
Dieses Dokument legt die Systemarchitektur fest: Paketstruktur, Schichtenzuordnung, COT-Integrationspunkte, RPC-Design, Autoritäts-/Replikationsmodell und Sicherheitsmodell.
Zentrale Entscheidung: PCT nutzt einen **eigenen Enter-/Leave-Pfad** mit eigener Kameraklasse `PCT_CinematicCamera : JMCinematicCamera` (gespawnt über `g_Game.SelectSpectator`), statt die aktive COT-Freecam fernzusteuern.
Alle COT-Berührungspunkte werden in vier Klassen gekapselt, um Update-Risiken von COT/CF (Deprecated-Modulpfad, PermissionsOld) zu isolieren.
Nicht durch die Research-Dokumente belegte Annahmen sind durchgängig als „OFFEN — ZU VERIFIZIEREN" markiert (Abschnitt 10).

---

## 1. Ziel & Scope

PCT ist ein In-Game-Werkzeug für Machinima-Produktion, Server-Events und Content-Creation in DayZ: Es erweitert die COT-Freecam zu einer filmreifen Kinokamera mit Keyframe-Sequencer (Position, Rotation, FOV, Fokus, Roll), Shot-Presets (Einstellungsgrößen, Kamerawinkel, Brennweiten-Äquivalente), Kompositions-Overlays und Szenen-Werkzeugen (Wetter/Zeit einfrieren, Akteure, Requisiten, Licht). Zielgruppe sind Admins und berechtigte Creator auf COT-Servern; die Berechtigung läuft vollständig über den COT-Permission-Baum (`CinematicTool.*`). Der Scope dieses Dokuments ist die technische Architektur und COT-Integration; das fachliche Anforderungsprofil steht in Plan_A1.md, Datenmodell und Persistenz in Plan_B3_Datenmodell_Persistenz.md. Nicht im Scope: Video-Encoding im Spiel, externe Editor-Anbindung, Änderungen an COT selbst.

## 2. Systemkontext-Diagramm

```
+------------------------------ DayZ CLIENT ------------------------------+      +----------------------------- DayZ SERVER -----------------------------+
|                                                                         |      |                                                                       |
|  [PCT_CinematicForm : JMFormBase]   [PCT_OverlayHud (Widgets)]          |      |  [PCT_CinematicModule] (Server-Handler)                               |
|            |                                                            |      |     1. Permission-Check: HasPermission(perm, senderRPC)               |
|            v                                                            |      |     2. Payload lesen/validieren  3. Ausfuehren                        |
|  [PCT_CinematicModule : JMRenderableModuleBase]                         |      |            |                                                          |
|      |            |                                                     |      |            v                                                          |
|      |            +---- ScriptRPC (EPCT_RPC.*, IDs 10500+) ------------------> |  g_Game.SelectPlayer / g_Game.SelectSpectator("PCT_CinematicCamera") |
|      |            <---- Replies / Light-&Sequence-Broadcasts <---------------- |  World.SetDate / Weather.* (Welt-Autoritaet)                          |
|      v                                                                  |      |  CreateObject/Ex: Akteure & Props  ·  PlayerBase.COTSetGodMode        |
|  [PCT_SequencePlayer] --Kamerastate--> [PCT_CinematicCamera             |      |            |                                                          |
|      |                                  : JMCinematicCamera]            |      |            v                                                          |
|      v                                  (client-lokal aktiv)            |      |  [JMPermissionManager] (PermissionsOld, "CinematicTool.*")            |
|  PPEffects/DOF · Camera.SetFOV · JsonFileLoader ($profile)              |      |                                                                       |
|  MakeScreenshot · Kompositions-Widgets                                  |      |                                                                       |
+-------------------------------+-----------------------------------------+      +-----------------------------------+-----------------------------------+
                                |                                                                                    |
                                v                                                                                    v
        +---------------------------------------------------------------------------------------------------------------------+
        |  COT (JM_COT_Scripts / JM_COT_GUI):  JMModuleManager · JMModuleConstructor (modded durch PCT) ·                      |
        |  JMRenderableModuleBase · JMFormBase/CF_Window · JMCameraModule (Koexistenz) · JMCameraBase.CurrentActiveCamera      |
        |  JMPermissionManager · PlayerBase.COT*-Zustand (Godmode, Spectator-Body)                                             |
        +---------------------------------------------------------------------------------------------------------------------+
        |  CF (JM_CF_Scripts):  CF_ModuleGame/World-Manager · Modul-RPC-Routing (OnRPC-Bridge) · Lifecycle-Events ·            |
        |  Input-Bindings (JMModuleBinding) · NotificationSystem · CF_Log                                                      |
        +---------------------------------------------------------------------------------------------------------------------+
```

Leserichtung: Die PCT-UI (5_Mission) spricht ausschließlich mit dem PCT-Modul; das Modul sendet `ScriptRPC` an seinen eigenen Server-Handler (CF routet über den RPC-Bereich 10500–`COUNT` an das Modul). Der Server prüft jede RPC gegen den COT-Permission-Manager und führt Welt-Änderungen autoritativ aus. Kamera, Playback, Overlays und PPE bleiben rein client-lokal.

## 3. Mod-Paketstruktur

Mod-Paket auf Platte: `C:\Users\Administrator\Desktop\PsyernsCinematicTool\PsyernsCinematicTool\`. Der vorhandene leere Ordner `scripts` wird zu `Scripts` umbenannt. Es entstehen zwei PBOs: `Scripts` (CfgPatches `PCT_Scripts`) und `GUI` (CfgPatches `PCT_GUI`).

```
PsyernsCinematicTool\                          (Mod-Root, wird als @PsyernsCinematicTool ausgeliefert)
├─ Scripts\
│  ├─ config.cpp                               (CfgPatches PCT_Scripts + CfgMods PsyernsCinematicTool)
│  ├─ Data\
│  │  └─ Inputs.xml                            (Input-Actions UAPCT_*)
│  ├─ 3_Game\PsyernsCinematicTool\
│  │  ├─ PCT_RPC.c                             (enum EPCT_RPC)
│  │  ├─ Cameras\
│  │  │  └─ PCT_CinematicCamera.c             (: JMCinematicCamera)
│  │  ├─ Sequencer\
│  │  │  └─ PCT_SequencePlayer.c
│  │  ├─ Utils\
│  │  │  └─ PCT_Math.c
│  │  ├─ Data\                                 (JSON-Datenmodell, Member OHNE Praefix, je schemaVersion)
│  │  │  ├─ PCT_Shot.c
│  │  │  ├─ PCT_Sequence.c                    (+ PCT_Keyframe)
│  │  │  ├─ PCT_CameraRig.c                   (Sensor/Brennweite/Blende → FOV)
│  │  │  ├─ PCT_Presets.c                     (Preset-Familien + Container, siehe Plan_B3)
│  │  │  ├─ PCT_WorldState.c
│  │  │  ├─ PCT_ActorMark.c
│  │  │  ├─ PCT_LightSetup.c
│  │  │  ├─ PCT_PropSetup.c
│  │  │  └─ PCT_Settings.c
│  │  └─ Persistence\
│  │     └─ PCT_Storage.c                     (JsonFileLoader-Wrapper, $profile-Ordner, schemaVersion-Check)
│  ├─ 4_World\PsyernsCinematicTool\
│  │  ├─ Lights\
│  │  │  ├─ PCT_PointLight.c                  (: PointLightBase)
│  │  │  └─ PCT_SpotLight.c                   (: SpotLightBase)
│  │  ├─ Actors\
│  │  │  └─ PCT_ActorService.c                (Server: PlayerBase-Akteure spawnen/steuern)
│  │  └─ Props\
│  │     └─ PCT_PropService.c                 (Server: Props via CreateObjectEx, Registry)
│  └─ 5_Mission\PsyernsCinematicTool\
│     ├─ PCT_ModuleRegistration.c             (modded class JMModuleConstructor)
│     ├─ Modules\
│     │  └─ PCT_CinematicModule.c             (: JMRenderableModuleBase)
│     └─ GUI\
│        ├─ PCT_CinematicForm.c               (: JMFormBase)
│        └─ PCT_OverlayHud.c                  (Kompositions-Overlays, Letterbox)
└─ GUI\
   ├─ config.cpp                               (CfgPatches PCT_GUI)
   ├─ layouts\
   │  ├─ pct_cinematic_form.layout
   │  └─ pct_overlay.layout
   └─ textures\
      └─ modules\                              (Modul-Icon, Overlay-Masken, .edds/.paa)
```

### 3.1 Scripts\config.cpp (vollständig)

```cpp
class CfgPatches
{
	class PCT_Scripts
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] =
		{
			"DZ_Data",
			"JM_CF_Scripts",
			"JM_COT_Scripts",
			"JM_COT_GUI"
		};
	};
};

class CfgMods
{
	class PsyernsCinematicTool
	{
		dir = "PsyernsCinematicTool";
		name = "Psyerns Cinematic Tool";
		author = "Psyern";
		credits = "Psyern";
		version = "0.1.0";
		extra = 0;
		type = "mod";
		inputs = "PsyernsCinematicTool\Scripts\Data\Inputs.xml";

		dependencies[] =
		{
			"Game",
			"World",
			"Mission"
		};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] =
				{
					"PsyernsCinematicTool\Scripts\3_Game"
				};
			};
			class worldScriptModule
			{
				value = "";
				files[] =
				{
					"PsyernsCinematicTool\Scripts\4_World"
				};
			};
			class missionScriptModule
			{
				value = "";
				files[] =
				{
					"PsyernsCinematicTool\Scripts\5_Mission"
				};
			};
		};
	};
};
```

Anmerkungen: COT-Vorbild ist `JM\COT\Scripts\config.cpp` (CfgPatches `JM_COT_Scripts`, CfgMods `JM_CommunityOnlineTools` mit `inputs`-Eintrag und `defs`-Blöcken; Research_COT_Infrastructure.md §5). PCT nutzt kein `engineScriptModule` (kein 1_Core-Code geplant). Pfade in config.cpp mit einfachen Backslashes (Projekt-Konvention). Keine Expansion-/Dabs-Addons — deren Code ist nur Pattern-Vorlage; benötigte Utilities werden in `PCT_Math` nachimplementiert (Research_Framework_Patterns.md, Projekt-Entscheidung).

### 3.2 GUI\config.cpp (vollständig)

```cpp
class CfgPatches
{
	class PCT_GUI
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] =
		{
			"DZ_Data"
		};
	};
};
```

Das GUI-PBO enthält nur Layouts und Texturen (Vorbild: `JM\COT\GUI\config.cpp` mit CfgPatches `JM_COT_GUI`; die dortigen CfgWorlds-Einträge sind COT-spezifisch und werden nicht übernommen). `PCT_Scripts` verlangt `JM_COT_GUI` und `PCT_GUI` implizit über die Ladefolge — `PCT_GUI` selbst braucht nur `DZ_Data`.

### 3.3 -mod vs. -serverMod, Signierung

PCT ist **Client-Content**: Kameraklasse, GUI-Layouts, Input-Actions, Overlays und PPE laufen auf dem Client. PCT muss daher auf **Client und Server als `-mod`** geladen werden (wie COT und CF selbst) — ein Betrieb als reiner `-serverMod` ist ausgeschlossen. Beide PBOs werden mit dem Autoren-Schlüsselpaar signiert (`.biprivatekey` → `.bisign` je PBO); der öffentliche `.bikey` wird im `keys\`-Ordner des Servers hinterlegt, sonst lehnt der Server verifizierte Clients ab. CF und COT müssen in der Ladereihenfolge vor PCT stehen (erzwungen durch `requiredAddons`).

## 4. Schichtenarchitektur

| Klasse | Layer | Verantwortung | Abhängig von |
|---|---|---|---|
| `PCT_CinematicCamera : JMCinematicCamera` | 3_Game | Freecam-Verhalten geerbt; zusätzlich Anwendung des per Frame berechneten Kamerastates (Position, Rotation, FOV, Fokus/DOF, Roll), Handheld-Noise | `JMCinematicCamera`/`JMCameraBase` (COT, 3_Game), `Camera` (Vanilla, 3_Game), `PPEffects` (3_Game), `PCT_SequencePlayer`, `PCT_Math` |
| `PCT_SequencePlayer` | 3_Game | Playback-Engine: wertet `PCT_Sequence`-Kanäle über der Zeit aus (`Math3D.Curve` CatmullRom, Easing), liefert einen Kamerastate; Scrub/Pause/Loop | `PCT_Math`, Datenmodell, `Math3D`/`Math` (Engine) — **COT-frei** |
| `PCT_Math` | 3_Game (Utils) | Clean-Room-Utilities: winkelbewusste Interpolation, SmoothStep/SCurve, Orbit um Achse, Pfadverdichtung, FOV↔Brennweite (`fov = 2*atan(sensorHeight/(2*focalLength))`) | nur Engine (`Math`, `Math3D`, `vector`) — **COT-frei** |
| `PCT_Shot`, `PCT_Sequence`, `PCT_Keyframe`, `PCT_CameraRig`, Preset-Klassen (`PCT_SensorPreset` … `PCT_WorldStatePreset`), `PCT_WorldState`, `PCT_ActorMark`, `PCT_LightSetup`, `PCT_PropSetup`, `PCT_Settings` | 3_Game (Data) | Reines JSON-Datenmodell; Member ohne Präfix (JSON-Key-Treue), je ein `int schemaVersion` | keine (POD) — **COT-frei** |
| `PCT_Storage` | 3_Game (Persistence) | `JsonFileLoader<T>`-Wrapper; legt `$profile:PsyernsCinematicTool\{Shots,Sequences,Presets,Exports}\` + `Settings.json` an; schemaVersion-Prüfung/Migration | `JsonFileLoader` (Vanilla 3_Game), Datenmodell — **COT-frei** |
| `PCT_PointLight : PointLightBase`, `PCT_SpotLight : SpotLightBase` | 4_World | Client-gerenderte Kinolichter (Helligkeit/Radius/Farbe/Fade) auf Basis `ScriptedLightBase` | `ScriptedLightBase`-Familie (Vanilla, 4_World), `PCT_LightSetup` |
| `PCT_ActorService` | 4_World | Server: Akteure (`PlayerBase`) spawnen, ausrüsten, Animationskommandos (`StartCommand_Action`, Movement-Override). Das Vanilla-CameraTools-Muster ist nur **client-lokal** verifiziert (CTActor/CTObjectFollower spawnen mit `create_local=true`; Research_Vanilla_APIs.md §8) — Übertragbarkeit auf den Server: OFFEN — ZU VERIFIZIEREN (#10) | `PlayerBase`, `human.c`-Kommandos (Vanilla), `PCT_ActorMark` — **COT-frei** |
| `PCT_PropService` | 4_World | Server: Props via `CreateObjectEx` (ECE_-Flags), Registry, Entfernen | `CGame.CreateObjectEx`, `PCT_PropSetup` — **COT-frei** |
| `PCT_CinematicModule : JMRenderableModuleBase` | 5_Mission | Modul-Lifecycle, Permission-Registrierung, RPC-Dispatch (`OnRPC`, `GetRPCMin/Max`), Enter/Leave-Orchestrierung, Koexistenz-Logik mit `JMCameraModule`, Aufruf der 4_World-Services | `JMRenderableModuleBase` (COT), `EPCT_RPC`, Services (4_World), Datenmodell |
| `PCT_CinematicForm : JMFormBase` | 5_Mission | Haupt-UI (UIActionManager-Widgets: Slider, Keyframe-Liste, Preset-Buttons) | `JMFormBase` (COT), `PCT_CinematicModule` |
| `PCT_OverlayHud` | 5_Mission | Kompositions-Overlays (Drittelraster, Safe Areas, Letterbox-Masken) als Workspace-Widgets | Engine-Widgets (`EnWidgets`) — **COT-frei** |
| `modded class JMModuleConstructor` | 5_Mission | Registrierung von `PCT_CinematicModule` im COT-Modulsystem | `JMModuleConstructor` (COT, 5_Mission) |

**Warum die Kameraklasse in 3_Game liegt:** COT definiert `JMCameraBase : Camera` und `JMCinematicCamera` selbst in 3_Game (`JM\COT\Scripts\3_Game\...\Entities\Cameras\`; Research_COT_Infrastructure.md §1); eine Subklasse muss in derselben oder einer höheren Schicht kompilieren, und die exakte Spiegelung des COT-Vorbilds hält den Vererbungspfad minimal. Zusätzlich ist die Vanilla-Basisklasse `Camera` in 3_Game definiert und `PPEffects` (DOF) ebenfalls — die Kamera braucht nichts aus 4_World/5_Mission. Da `g_Game.SelectSpectator` die Kamera per Klassennamen-String spawnt, muss die Klasse lediglich kompiliert vorliegen; 3_Game ist die früheste sinnvolle Schicht.

**Warum das Modul in 5_Mission liegt:** COT registriert alle konkreten (renderbaren) Module in `5_Mission\...\modules\` und modded dort auch den `JMModuleConstructor` (Research_COT_Infrastructure.md §2). Das Modul orchestriert Mission-Lifecycle, GUI und Eingaben — Mission-Ebene ist die korrekte Schicht; die tiefer liegenden Services (4_World) und das Datenmodell (3_Game) bleiben dadurch von Mission-Code frei referenzierbar (untere Schichten referenzieren nie nach oben).

## 5. COT-Integrationspunkte im Detail

### 5.1 Modul-Registrierung

Registrierung ausschließlich über den COT-Konstruktor-Hook (kein Macro-System; Research_COT_Infrastructure.md §2):

```c
// 5_Mission\PsyernsCinematicTool\PCT_ModuleRegistration.c
modded class JMModuleConstructor
{
	override void RegisterModules( out TTypenameArray modules )
	{
		super.RegisterModules( modules );

		modules.Insert( PCT_CinematicModule );
	}
}
```

Das Modulgerüst folgt dem COT-Beispielmodul (`JMExampleModule`) und `JMCameraModule`:

```c
// 5_Mission\PsyernsCinematicTool\Modules\PCT_CinematicModule.c (Skelett)
class PCT_CinematicModule : JMRenderableModuleBase
{
	void PCT_CinematicModule()
	{
		GetPermissionsManager().RegisterPermission( "CinematicTool.View" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Sequencer" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.World" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Actors" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Lights" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Props" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Export" );
	}

	override bool HasAccess()
	{
		return GetPermissionsManager().HasPermission( "CinematicTool.View" );
	}

	override string GetLayoutRoot()
	{
		return "PsyernsCinematicTool/GUI/layouts/pct_cinematic_form.layout";
	}

	override string GetInputToggle()
	{
		return "UAPCT_ToggleMenu";
	}

	override string GetTitle()
	{
		return "Psyerns Cinematic Tool";
	}

	override int GetRPCMin()
	{
		return EPCT_RPC.INVALID;
	}

	override int GetRPCMax()
	{
		return EPCT_RPC.COUNT;
	}

	override void OnRPC( PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx )
	{
		switch ( rpc_type )
		{
			case EPCT_RPC.Enter:
				RPC_Enter( ctx, sender, target );
				break;
			case EPCT_RPC.Leave:
				RPC_Leave( ctx, sender, target );
				break;
			// ... weitere Handler gemaess Abschnitt 6
		}
	}
}
```

Permissions müssen im Konstruktor registriert werden — nach Mission-Load wirft `RegisterPermission` einen Error (Research_COT_Infrastructure.md §3). Input-Actions (`UAPCT_ToggleMenu`, `UAPCT_AddKeyframe`, …) werden in `Scripts\Data\Inputs.xml` deklariert und über `RegisterKeyMouseBindings()` mit `Bind(new JMModuleBinding(...))` an Handler gebunden (COT-Muster, `JMCameraModule.c:203`).

### 5.2 Kamera-Kooperation — ZENTRALE ARCHITEKTUR-ENTSCHEIDUNG

**Entscheidung: PCT nutzt einen eigenen Enter-/Leave-Pfad mit eigener Kameraklasse `PCT_CinematicCamera`, gespawnt über COTs Server-Mechanik (`g_Game.SelectPlayer(sender, NULL)` + `g_Game.SelectSpectator(sender, "PCT_CinematicCamera", position)`), exakt nach dem Vorbild `JMCameraModule.Server_Enter` (JMCameraModule.c:329).**

```c
// Server-Pfad in PCT_CinematicModule (Skizze, Spiegel von JMCameraModule.Server_Enter)
private void Server_Enter( PlayerIdentity sender, Object target, vector position )
{
	PlayerBase targetPlayer = PlayerBase.Cast( target );
	if ( targetPlayer )
		targetPlayer.COT_RememberVehicle();

	// COTs modded PlayerBase.OnSelectPlayer wuerde beim Spieler->Spectator-Wechsel feuern;
	// COT unterdrueckt das gezielt vor SelectPlayer (JMCameraModule.c:348) — PCT spiegelt das.
	PlayerBase senderPlayer = PlayerBase.Cast( sender.GetPlayer() );
	if ( senderPlayer )
		senderPlayer.COT_TempDisableOnSelectPlayer();

	g_Game.SelectPlayer( sender, NULL );
	g_Game.SelectSpectator( sender, "PCT_CinematicCamera", position );

	ScriptRPC rpc = new ScriptRPC();
	rpc.Send( NULL, EPCT_RPC.Enter, true, sender );
}
```

**COT-Vorbereitungsaufrufe (Pflichtbestandteil des Spiegels):** Client-seitig ruft der PCT-`Enter()`-Pfad vor dem RPC-Versand — wie COTs `Enter()` (JMCameraModule.c:255-257; Research_COT_Infrastructure.md §1 Schritt 2) — `GetPlayer().COT_TempDisableOnSelectPlayer()` und `GetPlayer().COT_RememberVehicle()` auf. `COT_TempDisableOnSelectPlayer` setzt ein one-shot-Flag, das COTs modded `PlayerBase.OnSelectPlayer`-Logik (inkl. `super`-Kette und GUI-Reset; COT PlayerBase.c:187-201) für genau einen Spieler↔Spectator-Wechsel unterdrückt. Da PCT laut K5 dieselben `PlayerBase`-COT-Felder mitbenutzt, würden ohne diese Aufrufe COTs `OnSelectPlayer`-Seiteneffekte beim PCT-Enter ungewollt feuern. Der `Leave`-Pfad unterdrückt bewusst **nicht** — beim Wieder-Anheften (`SelectPlayer(sender, player)`) soll `OnSelectPlayer` normal laufen (COT-Verhalten).

**Begründung (3 Kernpunkte):**
1. **Kanal-Hoheit:** Nur mit eigener Kameraklasse kann PCT `OnUpdate` überschreiben und pro Frame exakte Keyframe-Werte (Position, Rotation, FOV, Fokus, Roll) anwenden. COTs `SetupTraveling` ist positions-only und linear (Research_COT_Infrastructure.md §1) — der Sequencer braucht ohnehin eine eigene Playback-Schleife.
2. **Konfliktfreiheit:** `JMCameraModule.OnUpdate` schreibt jeden Frame FOV (`Math.Lerp` gegen `m_TargetFOV`, JMCameraModule.c:95-107) und bei aktivem DOF auch `SetFocus`/`PPEffects.OverrideDOF` (109-136) auf `CurrentActiveCamera` — verifiziert am Quellcode. Zwei Schreiber auf denselben Kanälen sind für frame-genaue Kamerafahrten inakzeptabel.
3. **Eigenes Permission-Gate:** Der eigene Pfad wird mit `CinematicTool.View` statt `Camera.View` abgesichert; COT-Kamera-Rechte und PCT-Rechte bleiben getrennt vergebbar.

**Koexistenz-Regeln mit `JMCameraModule`** (verifizierte Fakten: `CurrentActiveCamera` wird nur von `JMCameraModule` gesetzt, nicht von den Kameraklassen selbst; `ToggleCamera` prüft `CurrentActiveCamera.IsInherited(JMCinematicCamera)`, JMCameraModule.c:619):

| Regel | Verhalten |
|---|---|
| K1 — Kein Doppel-Spawn | Vor dem PCT-Enter prüft der Client `JMCameraBase.CurrentActiveCamera`. Ist eine COT-Kamera aktiv, wird **kein** eigener Enter gesendet. |
| K2 — Übernahme statt Doppel-Spawn | Ist die COT-Freecam aktiv: aktuelle Position/Richtung merken → `GetModuleManager().GetModule(JMCameraModule)` holen → dessen `Leave()` aufrufen → warten bis `CurrentActiveCamera == null` und der Spieler wieder steuerbar ist → dann PCT-Enter mit der gemerkten Position (sequenzieller Handoff). Direkte Server-seitige Übernahme via zweitem `SelectSpectator` ohne Leave: OFFEN — ZU VERIFIZIEREN (#2). |
| K3 — `CurrentActiveCamera` bleibt unangetastet | PCT setzt `CurrentActiveCamera` **nicht** auf die eigene Kamera (sonst greift Punkt 2 der Begründung: COT schreibt FOV/DOF jeden Frame). PCT führt eine eigene statische Referenz `s_PCT_ActiveCamera` in `PCT_CinematicCamera`. `CurrentActiveCamera` wird nur **gelesen** (Detektion). |
| K4 — Defensive Rückrichtung | Drückt der Operator COTs Freecam-Toggle, während die PCT-Kamera aktiv ist, startet COT `Enter()` (da `CurrentActiveCamera` null ist) und übernimmt den Spectator-Slot. `PCT_CinematicModule.OnUpdate` erkennt das (`CurrentActiveCamera` wird non-null, während `s_PCT_ActiveCamera` existiert), stoppt sofort Playback/Overlays und räumt die verwaiste PCT-Kamera client-seitig auf (`g_Game.ObjectDeleteOnClient`, COT-Leave-Muster). |
| K5 — Geteilter Zustand | `JMCameraBase.s_CurrentSpeed` (Fluggeschwindigkeit) ist statisch und wird von PCT mitbenutzt/geerbt — bewusst akzeptiert, da nur Editier-Komfort betroffen ist. Server-seitig nutzt der PCT-Pfad dieselben `PlayerBase`-COT-Felder (`m_JM_CameraPosition`, Godmode, Body-Streaming, `m_COT_TempDisableOnSelectPlayer`) wie COT selbst (Abschnitt 7); deshalb spiegelt der PCT-Enter-Pfad auch COTs `COT_TempDisableOnSelectPlayer()`/`COT_RememberVehicle()`-Vorbereitung (Abschnitt 5.2), sonst feuert COTs modded `PlayerBase.OnSelectPlayer` beim Wechsel ungewollt. Wechselwirkungen bei schnellen PCT↔COT-Wechseln: OFFEN — ZU VERIFIZIEREN (#8). |

**Verworfene Alternative:** Betrieb der PCT-Logik auf der bereits aktiven `JMCinematicCamera` via `CurrentActiveCamera` (kein eigener Enter-Pfad). Risiken: (a) `JMCameraModule.OnUpdate` überschreibt FOV jeden Frame mit geglättetem Lerp — FOV-Keyframes und Dolly-Zoom sind so nicht exakt fahrbar; (b) DOF-Schreibkonflikt bei aktivem `m_DOF`; (c) Freelook-Input der COT-Kamera läuft während des Playbacks weiter (Mausbewegung stört die Fahrt), Freeze nur über COT-globale Statics; (d) Kopplung an COT-UI-Zustand (Formular-Slider überschreiben Werte) und an COT-Interna, die sich mit jedem COT-Update ändern können; (e) Permission-Vermischung mit `Camera.View`. Vorteil wäre nur der entfallende eigene Enter-Pfad — das wiegt die Risiken nicht auf.

### 5.3 Weitere Integrationspunkte

| Integrationspunkt | Mechanik (verifiziert) | PCT-Nutzung |
|---|---|---|
| UI-Fenster | `JMRenderableModuleBase.Show()` lädt `GetLayoutRoot()` und bindet `PCT_CinematicForm : JMFormBase` via `Init(window, this)` | Hauptformular; Widgets über `UIActionManager`-Factory |
| Permissions | `GetPermissionsManager().RegisterPermission/HasPermission` (PermissionsOld, aktiv) | Baum `CinematicTool.*`, Gate in jedem Server-Handler |
| RPC-Routing | CF routet RPC-IDs im Bereich `GetRPCMin()`..`GetRPCMax()` an `OnRPC` des Moduls | Bereich 10500..`COUNT` (Abschnitt 6) |
| Godmode/Body | `PlayerBase.COTSetGodMode(true, false)` + `COTUpdateSpectatorPosition()` (COT-Bestand) | Wiederverwendung im eigenen Enter-Pfad (Abschnitt 7) |
| Notifications | CF `NotificationSystem.Create(...)`, COT `COTCreateLocalAdminNotification(...)` | Feedback (z. B. Handoff-Meldung, Permission verweigert) |

## 6. RPC-Design

Zentrale Enum-Datei `3_Game\PsyernsCinematicTool\PCT_RPC.c` (COTs höchste Basis ist 10460/Loadout; PCT startet bei 10500 — Research_COT_Infrastructure.md §4/§5):

```c
enum EPCT_RPC
{
	INVALID = 10500,
	Enter,              // 10501
	Leave,              // 10502
	Leave_Finish,       // 10503
	UpdatePosition,     // 10504
	SetWorldState,      // 10505
	SpawnActor,         // 10506
	RemoveActor,        // 10507
	ActorCommand,       // 10508
	SpawnLight,         // 10509
	UpdateLight,        // 10510
	RemoveLight,        // 10511
	SpawnProp,          // 10512
	RemoveProp,         // 10513
	SequenceBroadcast,  // 10514
	COUNT
}
```

| RPC | Richtung | Payload (als EIN `Param`-Objekt via `ctx.Read(param)`) | Permission-Gate (Server) | Phase |
|---|---|---|---|---|
| `Enter` | Client→Server; Reply Server→Client (gleiche ID, COT-Muster) | `Param1<vector>` position | `CinematicTool.View` | 1 |
| `Leave` | Client→Server; Reply Server→Client mit Idle-Timeout | Hin: leer · Reply: `Param1<float>` timeout | `CinematicTool.View` | 1 |
| `Leave_Finish` | Client→Server (nach Idle-Animation; COT-Muster `Leave_Finish`) | leer | `CinematicTool.View` | 1 |
| `UpdatePosition` | Client→Server (periodisch ~1 s, Body-Streaming) | `Param1<vector>` cameraPosition | `CinematicTool.View` | 1 |
| `SetWorldState` | Client→Server | `Param1<PCT_WorldState>` (Datum/Uhrzeit, Overcast/Fog/Rain/Snow, Wind, freezeTime, freezeWeather) | `CinematicTool.World` | 4 |
| `SpawnActor` | Client→Server; Reply Server→Client (`actorId`) | `Param1<PCT_ActorMark>` (Typ, Position, Orientierung, Loadout) | `CinematicTool.Actors` | 4 |
| `RemoveActor` | Client→Server | `Param1<int>` actorId | `CinematicTool.Actors` | 4 |
| `ActorCommand` | Client→Server | `Param1<PCT_ActorCommand>` (actorId, commandType, Parameter) | `CinematicTool.Actors` | 4 |
| `SpawnLight` | Client→Server; Broadcast Server→alle Clients | `Param1<PCT_LightSetup>` | `CinematicTool.Lights` | 4 |
| `UpdateLight` | Client→Server; Broadcast Server→alle Clients | `Param1<PCT_LightSetup>` (mit lightId) | `CinematicTool.Lights` | 4 |
| `RemoveLight` | Client→Server; Broadcast Server→alle Clients | `Param1<int>` lightId | `CinematicTool.Lights` | 4 |
| `SpawnProp` | Client→Server; Reply Server→Client (`propId`) | `Param1<PCT_PropSetup>` (Typ, Position, Orientierung) | `CinematicTool.Props` | 4 |
| `RemoveProp` | Client→Server | `Param1<int>` propId | `CinematicTool.Props` | 4 |
| `SequenceBroadcast` | Client→Server; Broadcast Server→alle Clients (synchronisierte Wiedergabe/Verteilung) | `Param1<PCT_Sequence>` | `CinematicTool.Sequencer` | 4+ (optional) |

Konventionen: Versand über `ScriptRPC` (`rpc.Send(target, EPCT_RPC.X, true, identity)`), Dispatch ausschließlich im `OnRPC`-Override, jeder Server-Handler prüft die Permission mit `GetPermissionsManager().HasPermission(perm, senderRPC)` **vor** dem Lesen der Payload-Wirkung. Payloads werden als ein `Param`-Objekt gelesen, nicht feldweise. Ob verschachtelte Datenklassen (`PCT_WorldState` etc.) von `ctx.Write/Read` automatisch serialisiert werden, ist OFFEN — ZU VERIFIZIEREN (#4); Fallback ist feldweises `OnSend/OnRecieve` nach Expansion-Settings-Muster (Research_Framework_Patterns.md §3) — die RPC-Tabelle bleibt davon unberührt.

## 7. Server-Autorität & Replikationsmodell

| Bereich | Ort | Mechanik (Beleg) |
|---|---|---|
| Kameraflug, Keyframe-Playback, Scrub | **Client-only** | `PCT_CinematicCamera`/`PCT_SequencePlayer`; Spectator-Kamera ist client-lokal (COT-Kamera-Flow, Research_COT_Infrastructure.md §1) |
| FOV, DOF/PPE, Vignette, Blur | **Client-only** | `Camera.SetFOV`, `PPEffects.OverrideDOF`/`SetVignette`/... (Research_Vanilla_APIs.md §2–3) |
| Kompositions-Overlays, Letterbox, Safe Areas | **Client-only** | Workspace-Widgets (Research_Vanilla_APIs.md §10) |
| Shots/Sequences/Presets/Settings (JSON) | **Client-only** | `JsonFileLoader<T>` nach `$profile:PsyernsCinematicTool\` (Research_Framework_Patterns.md §3; Details Plan_B3_Datenmodell_Persistenz.md) |
| Screenshots/Storyboard-Frames | **Client-only** | `MakeScreenshot(name)` → DDS (Research_Vanilla_APIs.md §11) |
| Lokale Vorschau-Props (nur eigener Bildausschnitt) | **Client-only** | `CreateObject(type, pos, true)` bzw. `ECE_LOCAL` (Research_Vanilla_APIs.md §7) |
| Enter/Leave der Kamera (Spectator-Wechsel) | **Server-autoritativ** | `g_Game.SelectPlayer` + `g_Game.SelectSpectator` nur im Server-Handler (COT-Muster) |
| Godmode während Kamera | **Server-autoritativ** | COT-Bestand: `EnterFullmap` → `player.COTSetGodMode(true, false)` — automatisch aktiviert, vorheriger Zustand gemerkt (JMCameraModule.c:594-601) |
| Body-Streaming | **Server-autoritativ** | Der Spielerkörper bleibt in der Welt; `UpdatePosition`-RPC → `m_JM_CameraPosition` → `COTUpdateSpectatorPosition()` zieht den Körper der Kamera nach (Streaming-Bubble), Rückfall auf `m_JMLastPosition` bei < 150 m (Research_COT_Infrastructure.md §1) |
| Datum/Uhrzeit setzen + einfrieren | **Server-autoritativ** | `World.SetDate(...)`, `SetTimeMultiplier(0)` (Research_Vanilla_APIs.md §6) |
| Wetter setzen + einfrieren | **Server-autoritativ** | `Weather`-Phänomene `Set(...)` + `SetWeatherUpdateFreeze(true)`; Wetter ist server-autoritativ (Research_Vanilla_APIs.md §6) |
| Akteur-Spawns & -Kommandos (für alle sichtbar) | **Server-autoritativ (Ziel-Design)** | `PCT_ActorService`: Server-`CreateObject`/Kommandos. Das Vanilla-Vorbild (CTActor) ist rein client-lokal; ob ein server-seitig gespawnter, identity-loser `PlayerBase` sauber funktioniert und ob server-seitig ausgelöste `StartCommand_Action`/Movement-Overrides zu allen Clients replizieren (Spieleranimation ist normalerweise client-getrieben), ist unbelegt: OFFEN — ZU VERIFIZIEREN (#10) |
| Prop-Spawns (für alle sichtbar) | **Server-autoritativ** | `PCT_PropService`: `CreateObjectEx` mit `ECE_PLACE_ON_SURFACE` u. a. |
| Licht-Setups (für alle sichtbar) | **Server-autoritativ im Zustand, client-gerendert** | Server hält die Licht-Registry und broadcastet `SpawnLight/UpdateLight/RemoveLight`; jeder Client erzeugt lokal `PCT_PointLight/PCT_SpotLight` via `ScriptedLightBase.CreateLight` (Research_Vanilla_APIs.md §12). Direkte Server-Spawn-Replikation von Lichtern: OFFEN — ZU VERIFIZIEREN (#5) |

Grundsatz: Alles, was nur der Operator sieht, bleibt lokal (kein RPC-Traffic); alles, was Dritte sehen oder was Spielzustand ändert, läuft ausschließlich über den Server-Handler.

## 8. Sicherheitsmodell

- **Jede** Server-RPC prüft die Permission erneut — unabhängig davon, dass die UI clientseitig bereits über `HasAccess()`/`OnClientPermissionsUpdated()` gefiltert ist. Vorbild ist COTs `RPC_Enter`, das server-seitig `GetPermissionsManager().HasPermission("Camera.View", senderRPC)` wiederholt (JMCameraModule.c:361 ff.; Research_COT_Infrastructure.md §4).
- **Keine Client-Trust-Annahmen:** Der Client liefert nur Wünsche (Positionen, Setups); der Server validiert Wertebereiche (z. B. Datum/Wetter-Grenzen, Positions-Plausibilität) und führt aus. Spawn-Typen für `SpawnActor`/`SpawnProp` werden gegen eine server-seitige Whitelist geprüft (Konfiguration siehe Plan_B3_Datenmodell_Persistenz.md), damit ein `CinematicTool.Props`-Inhaber keine beliebigen Gameplay-Objekte erzeugen kann.
- Handler-Muster (verbindlich):

```c
private void RPC_SetWorldState( ParamsReadContext ctx, PlayerIdentity senderRPC, Object target )
{
	if ( !IsMissionHost() )
		return;

	if ( !GetPermissionsManager().HasPermission( "CinematicTool.World", senderRPC ) )
		return;

	Param1< PCT_WorldState > param = new Param1< PCT_WorldState >( null );
	if ( !ctx.Read( param ) )
		return;

	PCT_WorldState state = param.param1;
	if ( !state )
		return;

	// Wertebereichs-Validierung, danach autoritative Ausfuehrung
	Server_ApplyWorldState( state );
}
```

- Fehlende Registrierung/falscher RPC-Bereich führt zu stillem Nichtstun (CF routet nur registrierte Bereiche) — deshalb sind `GetRPCMin/Max` Pflichtbestandteil des Moduls und werden in Phase 0 mit einem Roundtrip-Test abgenommen.
- Broadcasts (`SpawnLight`-Familie, `SequenceBroadcast`) gehen nur vom Server aus; Clients akzeptieren diese Richtungen nur im Client-Zweig des Handlers (`IsMissionHost()`-Weiche, COT-Muster).

## 9. Abhängigkeitsrisiken

| Risiko | Beleg | Auswirkung |
|---|---|---|
| COTs `JMModuleBase` basiert auf CFs **Deprecated**-Modulpfad (`JM\CF\Scripts\4_World\...\Module\Deprecated\JMModuleBase.c`) | Research_COT_Infrastructure.md §2 | CF könnte den Pfad in Zukunft entfernen/ändern; Modul-Lifecycle und `OnRPC`-Bridge brechen dann |
| `PermissionsOld` ist die **aktive** Permission-Implementierung (trotz Ordnername) | Research_COT_Infrastructure.md §3 | Ein COT-Umbau auf ein neues Permission-System ändert Registrier-/Prüf-API |
| COT-Updates können Interna ändern (Kamera-Tuning-Konstanten, `ToggleCamera`-Logik, `PlayerBase.COT*`-Felder, RPC-Basen) | Research_COT_Infrastructure.md §1/§4/§6 | Koexistenz-Regeln (K1–K5) und Enter/Leave-Spiegelung müssen je COT-Release nachgeprüft werden |
| RPC-ID-Kollision, falls COT neue Modul-Basen > 10460 vergibt | Research_COT_Infrastructure.md §5 | Bereich 10500–10515 pro COT-Release gegen `RPC.c` diffen |

**Kapselungsstrategie:** Alle COT-Berührungspunkte werden in genau vier Klassen gebündelt — `PCT_CinematicModule` (Lifecycle, RPC, Permissions, Enter/Leave, Koexistenz), `PCT_CinematicForm` (JMFormBase/UIActionManager), `PCT_CinematicCamera` (einzige Vererbung von COT-Code) und `modded class JMModuleConstructor` (eine Zeile Registrierung). Datenmodell, `PCT_Math`, `PCT_SequencePlayer`, `PCT_Storage`, die 4_World-Services und `PCT_OverlayHud` referenzieren **keine** `JM*`-Typen: Der Sequencer liefert einen neutralen Kamerastate, den die Kameraklasse anwendet — bricht COT, ist nur die dünne Integrationsschale zu reparieren, nicht der Kern. Bei jedem COT-Update werden die vier Schalen-Klassen gegen das COT-Diff geprüft (Checkliste im Release-Prozess, siehe Plan_B6_Roadmap_Risiken.md).

## 10. Offene Punkte mit Verifikationsplan

| # | Offener Punkt (OFFEN — ZU VERIFIZIEREN) | Risiko | Konkreter Testschritt | Phase |
|---|---|---|---|---|
| 1 | `g_Game.SelectSpectator(sender, "PCT_CinematicCamera", position)` mit **eigener** Klasse — COT belegt den Aufruf nur mit `"JMCinematicCamera"` | hoch | Phase-0-Skeleton auf Dedicated-Server: Enter-RPC senden, auf dem Client `Camera.GetCurrentCamera()` casten und `ClassName()` loggen; Steuerbarkeit der Kamera prüfen | 0/1 |
| 2 | Zweiter `SelectSpectator`-Aufruf während aktiver Spectator-Session (direkte Übernahme COT→PCT ohne vorheriges `Leave`) | mittel | Testserver: COT-Freecam aktivieren, dann PCT-`Server_Enter` ohne Handoff ausführen; Verhalten/Fehler loggen. Bis zur Verifikation gilt Regel K2 (sequenzieller Handoff) | 1 |
| 3 | Clamping von `Camera.SetFOV` außerhalb `OPTIONS_FIELD_OF_VIEW_MIN/MAX` (Research vermutet „nicht geclampt", da COT-Slider 0.001–4.0 rad erlaubt) | niedrig | In-Game: `SetFOV(0.1)` und `SetFOV(2.0)` setzen, Screenshots vergleichen; effektive FOV-Grenzen dokumentieren | 1 |
| 4 | Auto-Serialisierung verschachtelter Datenklassen über `ScriptRPC`/`ctx.Read(Param1<PCT_WorldState>)` | mittel | Phase-0-Test-RPC mit Dummy-Datenklasse Server↔Client (alle Feldtypen); bei Fehlschlag feldweises `OnSend/OnRecieve` nach Expansion-Muster implementieren | 0 |
| 5 | `ScriptedLightBase` auf Dedicated Server: repliziert ein Server-Spawn zu Clients, oder ist Broadcast+lokaler Spawn zwingend? Zudem voller `EntityLightSource`-API-Umfang (SetColorRGB, Schatten) | mittel | 1_Core-Proto von `EntityLightSource` lesen; Testserver mit 2 Clients: Server-Spawn vs. Broadcast-Spawn vergleichen (Sichtbarkeit beim zweiten Client) | 4 |
| 6 | `MakeScreenshot`: exakter DDS-Ausgabepfad, Benennung, Konvertierungs-Pipeline für Storyboard-/Shot-Thumbnails | niedrig | Client-Aufruf mit und ohne `$`-Präfix, `$profile:ScreenShotes` inspizieren; externen DDS→PNG-Workflow festlegen (siehe Plan_B3) | 2 |
| 7 | Externe Mod-Inputs (`UAPCT_*` aus eigener `Inputs.xml`) in Kombination mit CF-`JMModuleBinding` — COT bindet nur eigene UA-Namen | mittel | Phase 0: `UAPCT_ToggleMenu` deklarieren, binden, Log im Handler; Taste im Spiel drücken | 0 |
| 8 | Wechselwirkung PCT↔COT auf den `PlayerBase`-COT-Feldern (`m_JM_CameraPosition`, Godmode-Restore, one-shot-Flag `m_COT_TempDisableOnSelectPlayer`) bei schnellen Wechseln zwischen PCT-Kamera und COT-Freecam/Spectate | mittel | Code-Review `PlayerBase.c` (COT) + Testserver: Zyklen PCT-Enter→Leave→COT-Enter→Leave in beiden Reihenfolgen; Godmode-/Positionszustand und `OnSelectPlayer`-Verhalten (Flag gesetzt/verbraucht, GUI-Reset) nach jedem Schritt loggen | 1 |
| 9 | Nutzbarkeit anderer COT-Module (ESP-Objektmanipulation, Teleport-an-Cursor) während aktiver PCT-Kamera bei **nicht** gesetztem `CurrentActiveCamera` | niedrig | Testserver: PCT-Kamera aktiv, ESP-Selektion und `UATeleportModuleTeleportCursor` testen; ggf. dokumentierte Einschränkung statt Workaround | 4 |
| 10 | Server-Spawn von `PlayerBase`-Akteuren ohne Identity + Replikation server-seitig ausgelöster `StartCommand_Action`/`OverrideMovementSpeed/Angle` zu den Clients — Vanilla-CameraTools belegt das Muster nur client-lokal (`CTObjectFollower.c:23`, `create_local=true`; Research_Vanilla_APIs.md §8); DayZ-Spieleranimation ist normalerweise client-getrieben | hoch | Testserver mit 2 Clients: Akteur server-seitig via `CreateObjectEx` spawnen; Emote (`StartCommand_Action`) und Bewegung (Movement-Override) server-seitig kommandieren; Sichtbarkeit der Entität und Animationswiedergabe beim zweiten Client prüfen. Fallback bei Fehlschlag: Akteur-Kommandos per Server-Broadcast-RPC an alle Clients verteilen und lokal ausführen (analog Licht-Muster #5) | 4 |

## Querverweise

- Plan_A1.md — Stoff- und Anforderungssammlung (fachliches Zielbild)
- Plan_B1_Architektur.md — dieses Dokument
- Plan_B2_Feature_Mapping.md — Feature-Mapping Plan_A1 → DayZ-Machbarkeit (Phaseneinstufung, Ersatzstrategien)
- Plan_B3_Datenmodell_Persistenz.md — Datenmodell, JSON-Schemata, `$profile`-Layout
- Plan_B4_Kamera_Engine.md — Kamera-Kern, Rig-Mathematik, Keyframes/Playback, Assistenten
- Plan_B5_UI_UX.md — UI/UX: Formulare, Overlays, Timeline, Keybinds
- Plan_B6_Roadmap_Risiken.md — Phasenplan, Teststrategie, Risiken, konsolidierte OFFEN-Liste
- Docs/Research/Research_COT_Infrastructure.md — verifizierte COT-Fakten (Module, RPC, Permissions, Kamera-Flow)
- Docs/Research/Research_Vanilla_APIs.md — verifizierte Vanilla-1.29-APIs (Camera, PPE/DOF, Splines, Weather, Lights, Screenshots)
- Docs/Research/Research_Framework_Patterns.md — Framework-Patterns & Abhängigkeits-Entscheidung (nur CF+COT als Runtime-Dependencies)
