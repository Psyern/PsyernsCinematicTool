class PCT_CinematicModule: JMRenderableModuleBase
{
	// Body-Streaming Timer-Akkumulator, Spiegel von JMCameraModule.m_UpdateTime (JMCameraModule.c:4).
	protected float m_UpdateTime;

	// Task P1-6 (Architektur-Vorgabe): Das Rig ist Besitz des MODULS (5_Mission, editiert von der Form
	// -- Form-Editierung folgt in Phase 2); die KAMERA (3_Game) erhaelt beim client-seitigen
	// Kamera-Erhalt eine eigene Referenz darauf (PCT_CinematicCamera.m_PCT_Rig, siehe Client_Enter/
	// Server_Enter unten) und wendet es selbst pro Frame an. `ref`, weil Member-Objektreferenz.
	ref PCT_CameraRig m_CameraRig;

	// Task P1-7 (Rig-Panel Fokusdistanz-Slider): Merkt den zuletzt per Formular gesetzten
	// Fokusdistanz-Wert, damit Client_Enter/der Offline-Zweig von Server_Enter ihn beim naechsten
	// Kamera-Erhalt an die neue PCT_CinematicCamera-Instanz uebertragen koennen (die alte Instanz
	// wird bei jedem Leave geloescht -- g_Game.ObjectDeleteOnClient in Client_Leave --, ohne diesen
	// Merker ginge ein per Slider gesetzter Fokuswert bei Leave/Enter verloren). Default 5.0 m,
	// identisch zu PCT_CinematicCamera.m_PCT_FocusDistanceM-Default.
	float m_PendingFocusDistanceM = 5.0;

	// Task P1-8 (Docs/Plan_B5_UI_UX.md Section 4, Architektur-Vorgabe Task-Brief): Overlay-Manager --
	// lazy erzeugt beim ersten ShowOverlay()-Aufruf (siehe EnsureOverlayHud()/PCT_OverlayHud.
	// EnsureCreated()), NICHT im Konstruktor (Workspace evtl. beim Modul-Bau noch nicht bereit). `ref`,
	// da Member-Objektreferenz (CLAUDE.md: ref nur auf Membern, die Objektreferenzen halten).
	protected ref PCT_OverlayHud m_OverlayHud;

	// Overlay-Zustand -- Besitz liegt beim MODUL (Architektur-Vorgabe Task-Brief: "Zustand ... haelt
	// das Modul"), NICHT bei PCT_OverlayHud (die Klasse spiegelt den Zustand nur in ihre Widgets).
	// Defaults: Drittel an, Zentrum an -- die Masken-Auswahl selbst braucht keinen eigenen Modul-Member,
	// sie lebt bereits in m_CameraRig.aspectMask (Default "16:9", siehe PCT_CameraRig.c) und wird von
	// dort gelesen/geschrieben (Task-Brief: "Maske schreibt zusaetzlich m_CameraRig.aspectMask").
	bool m_OverlayThirdsEnabled = true;
	bool m_OverlayCenterEnabled = true;

	// ===== Task P1-9: K2-Handoff (Docs/Plan_B1_Architektur.md Section 5.2, Regel K2) =====
	// Guard-Flag: waehrend ein Handoff laeuft (COTs JMCameraModule.Leave() wurde angestossen,
	// Poll wartet auf CurrentActiveCamera == null), werden weitere Enter()-Aufrufe (z. B.
	// Doppel-Tastendruck auf UAPCT_ToggleCamera) ignoriert -- sonst liefe ein zweiter
	// Leave()/Poll-Strang gegen denselben Handoff (Task-Brief: "Waehrend eines laufenden
	// Handoffs weitere Enter-Aufrufe ignorieren").
	protected bool m_CotHandoffActive;

	// Beim Handoff-Start gemerkte Position/Richtung der aktiven COT-Kamera (Plan_B1 K2-
	// Wortlaut: "Position/Richtung merken"). Quelle: g_Game.GetCurrentCameraPosition() /
	// GetCurrentCameraDirection() -- beides parameterlose native Instanzmethoden auf CGame,
	// liefern vector (scripts-1.29 3_Game/DayZ/Global/Game.c:730-731: "proto native vector
	// GetCurrentCameraPosition();" / "proto native vector GetCurrentCameraDirection();"), exakt
	// dieselben Aufrufe, die COTs eigenes JMCameraModule.Enter() fuer seine RPC-Payload nutzt
	// (JMCameraModule.c:261/265). m_CotHandoffPosition fliesst nach abgeschlossenem Handoff in
	// EnterAt() ein. m_CotHandoffDirection wird gemerkt, aber in diesem Task (noch) nicht auf
	// die neue Kamera angewendet: EnterAt() nimmt laut Task-Brief-Signatur bewusst nur eine
	// Position entgegen -- der Enter-RPC-Payload ist laut Plan_B1 Section 6 ein einzelnes
	// Param1<vector> (Position), ein Richtungskanal ist dort nicht vorgesehen.
	protected vector m_CotHandoffPosition;
	// Bewusst noch ungenutzt (siehe Begruendung oben) -- Richtungs-Uebergabe folgt erst mit einem
	// erweiterten Enter-RPC-Payload (Plan_B1 Section 6-Erweiterung, spaetere Phase).
	protected vector m_CotHandoffDirection;

	// Millisekunden-Zaehler fuer den Timeout des Handoff-Polls (Schrittweite
	// PCT_HANDOFF_POLL_MS, s. PollCotHandoff()).
	protected int m_CotHandoffElapsedMs;

	// Poll-Intervall spiegelt COTs eigenes Client_Check_Leave-Pollmuster (JMCameraModule.c:
	// 456-461, ebenfalls 250 ms per CallLater). Timeout (10000 ms) ist die vom Task-Brief
	// vorgegebene Obergrenze -- deutlich groesser als COTs eigener maximaler
	// waitForPlayerIdleTimeout (5000 ms, Server_Leave), damit ein regulaerer COT-Leave-Zyklus
	// auch dann durchlaeuft, wenn das Idle-Warten selbst schon die vollen 5 s beansprucht.
	static const int PCT_HANDOFF_POLL_MS = 250;
	static const int PCT_HANDOFF_TIMEOUT_MS = 10000;

	// ===== Task P2-3: Settings (Docs/Plan_B3_Datenmodell_Persistenz.md Section 2.11) =====
	// Besitz analog m_CameraRig: geladen einmalig ueber den client-seitigen Lifecycle-Hook OnMissionLoaded()
	// (siehe unten -- JMModuleBase.OnMissionLoaded(), CF Deprecated-Modulpfad JMModuleBase.c, per Init()
	// unbedingt ueber EnableMissionLoaded() aktiviert; COT-Praezedenzfall JMLoadoutModule.OnMissionLoaded()
	// verwendet denselben Hook fuer genau diesen Zweck -- eigene $profile-Daten beim Mission-Start laden).
	// null bis OnMissionLoaded gelaufen ist (dedizierter Server laedt nie, siehe Guard dort).
	protected ref PCT_Settings m_Settings;

	// ===== Task P2-4: Auto-Framing (Docs/Plan_B4_Kamera_Engine.md Section 6, Docs/Plan_B5_UI_UX.md
	// Section 2.2 "Framing-Preset"/"Angle-Preset") =====
	// Geladen einmalig in OnMissionLoaded() (identischer Lifecycle-Hook wie m_Settings oben), Besitz beim
	// Modul. NICHT `protected` (wie m_CameraRig oben) -- PCT_CinematicForm.OnInit liest diese Arrays direkt,
	// um die Framing-/Angle-Preset-Button-Reihen aus den geladenen Presets aufzubauen (gleiche Sichtbarkeits-
	// Konvention wie m_CameraRig/m_PendingFocusDistanceM). null auf dem dedizierten Server
	// (OnMissionLoaded-Guard greift dort ohnehin).
	ref array<ref PCT_FramingPreset> m_FramingPresets;
	ref array<ref PCT_AnglePreset> m_AnglePresets;

	// Task P2-5 (Docs/Plan_B8_Objektivsystem_LensKits.md Section 6/8 Task 4): gleiches Muster wie
	// m_FramingPresets/m_AnglePresets oben -- einmalig in OnMissionLoaded() geladen, NICHT protected
	// (PCT_CinematicForm baut die Lens-Kit-Panel-Buttons direkt aus diesen Arrays, siehe dortiges
	// BuildFramingPresetButtons-Vorbild). Abweichung vom woertlichen Plan-Text (Plan_B8 Section 8 Task 2
	// nennt "PCT_Storage.GetLensKits()/GetLensPresetById(string id)" als UI-Interfaces): der Plan wurde vor
	// der P2-4-Framing/Angle-Implementierung geschrieben und kennt das inzwischen etablierte "Modul laedt
	// einmalig, Form liest direkt"-Muster noch nicht. Diese Datei folgt dem REALEN, unmittelbar
	// vorangegangenen Praezedenzfall (P2-4) statt eines wiederholten Storage-Reloads pro Klick/Rebuild --
	// PCT_Storage.GetLensKits()/GetLensPresetById() existieren weiterhin (Task P2-5, PCT_Storage.c) und
	// bleiben als allgemeine Einzel-Lookups nutzbar, werden aber vom Formular-Hot-Path nicht wiederholt
	// aufgerufen (siehe Task-P2-5-Report "Abweichung Andockpunkte").
	ref array<ref PCT_LensPreset> m_LensPresets;
	ref array<ref PCT_LensKit> m_LensKits;

	// Zuletzt per Button gewaehlte Preset-id je Familie (B5 Section 2.2 "Framing-Preset"/"Angle-Preset"-
	// Zeilen) -- Defaults auf die im Task-Brief als Handrechnungsbeispiel genannten Presets (MS/Eye Level),
	// ein sinnvoller neutraler Startzustand.
	string m_SelectedFramingPresetId = "framing_ms";
	string m_SelectedAnglePresetId = "angle_eye_level";

	// ===== Task P2-3: ApplyShot ohne aktive Kamera (Docs/Plan_B5_UI_UX.md Abschnitt "Ersten Shot bauen";
	// Task-Brief: "sonst Position als Enter-Startposition merken -- naechster Enter() nutzt sie via
	// bestehendem EnterAt-Mechanismus") =====
	// Minimal-invasive Erweiterung von Enter()'s letzter Zeile (EnterAt(g_Game.GetCurrentCameraPosition())):
	// ist ein Pending-Merker gesetzt, gewinnt er einmalig gegenueber der aktuellen freien Kameraposition.
	protected bool m_PendingEnterPositionSet = false;
	protected vector m_PendingEnterPosition = vector.Zero;

	// Fix P2-3 (Plan_B6 Phase-2-DoD 1 "Shot speichern -> ... stellt Position, Orientierung, ... exakt
	// wieder her"): Orientierungs-Pendant zu m_PendingEnterPosition/m_PendingEnterPositionSet oben --
	// identisches One-Shot-Muster (gesetzt in ApplyShot(), konsumiert und sofort zurueckgesetzt in
	// Client_Enter() bzw. im Offline-Zweig von Server_Enter, sobald dort die neue Kamera-Instanz
	// verfuegbar ist). Rein client-lokal: der Enter-RPC-Payload bleibt unveraendert ein einzelnes
	// Param1<vector> (Position, B1 Section 6) -- kein zusaetzlicher Richtungskanal noetig, weil die
	// Orientierung serverseitig (Server_Enter/RPC_Enter) gar nicht gebraucht wird, sondern erst nach
	// dem Kamera-Erhalt client-seitig gesetzt wird.
	protected bool m_PendingEnterOrientationSet = false;
	protected vector m_PendingEnterOrientation = vector.Zero;

	void PCT_CinematicModule() {
		GetPermissionsManager().RegisterPermission( "CinematicTool.View" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Sequencer" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.World" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Actors" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Lights" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Props" );
		GetPermissionsManager().RegisterPermission( "CinematicTool.Export" );

		// Task P1-6: Rig-Default-Instanz -- die Kamera braucht ab dem ersten Enter sofort ein gueltiges
		// Rig (sonst FOV-Hold-Fallback statt Rig-FOV, siehe PCT_CinematicCamera.OnUpdate). Startwerte
		// = PCT_CameraRig-Defaults (50 mm/Vollformat/f2.8, siehe PCT_CameraRig.c). Task P1-7: das
		// Rig-Panel in PCT_CinematicForm editiert dieses Objekt ab jetzt direkt (Sensor/Brennweite/
		// Blende) -- die urspruenglich hier vermerkte "Editierbarkeit folgt erst Phase 2" ist damit
		// ueberholt, siehe PCT_CinematicForm.OnInit.
		m_CameraRig = new PCT_CameraRig();
	}

	override bool HasAccess() {
		return GetPermissionsManager().HasPermission( "CinematicTool.View" );
	}

	// Task P2-3: client-seitiger Settings-Load-Lifecycle-Hook. JMModuleBase.Init() (CF Deprecated-Modulpfad
	// JMModuleBase.c) ruft EnableMissionLoaded() unbedingt fuer JEDES Modul auf (Client UND dedizierter
	// Server) und feuert danach OnMissionLoaded() genau einmal pro Mission-Start -- exakt der Hook, den
	// JMLoadoutModule.OnMissionLoaded() bereits fuer denselben Zweck nutzt (eigene $profile-Daten laden).
	// Kandidaten "OnInit"/"OnClientReady" (Task-Brief-Formulierung) passen nicht: "OnInit" ist ein
	// Form-Lifecycle-Hook (JMFormBase.OnInit(), liefe bei jedem Fenster-Neuaufbau erneut, siehe
	// PCT_CinematicForm.OnInit) und wuerde die Settings-Datei bei jedem Oeffnen unnoetig neu laden;
	// "OnClientReady(PlayerBase player, PlayerIdentity identity)" (JMModuleBase.c) feuert ausschliesslich
	// SERVERSEITIG pro verbindendem Spieler, hat also keinen sinnvollen client-lokalen $profile-Bezug.
	// Guard exakt wie EnsureOverlayHud() oben (CLAUDE.md-konform: nur IsDedicatedServer(), keine
	// IsClient()/IsServer()-Autoritaetspruefung) -- ein dedizierter Server hat kein sinnvolles $profile fuer
	// Kreativ-Daten.
	override void OnMissionLoaded() {
		super.OnMissionLoaded();

		if ( !g_Game || g_Game.IsDedicatedServer() )
			return;

		m_Settings = PCT_Storage.LoadSettings();

		// Task-Brief: "Overlay-Defaults/Maske aus Settings uebernehmen". Nur die beiden B3-Section-2.11-Felder,
		// die tatsaechlich Overlay-/Masken-Bedeutung haben (showCompositionGrid = Drittelraster,
		// defaultAspectMask); m_OverlayCenterEnabled bleibt bei seinem Klassen-Default -- B3 Section 2.11
		// definiert kein eigenes Settings-Feld fuer die Bildzentrum-Markierung (keine erfundenen Felder).
		m_OverlayThirdsEnabled = m_Settings.showCompositionGrid;
		m_CameraRig.aspectMask = m_Settings.defaultAspectMask;

		// Task P2-4: Framing-/Angle-Preset-Familien einmalig laden (PCT_Storage regeneriert die Builtins
		// bei jedem Aufruf, siehe PCT_Storage.MergeBuiltin*Presets-Kommentar -- ein einmaliges Laden hier
		// reicht, Nutzer-Presets kommen erst mit dem Preset-Editor einer spaeteren Phase hinzu).
		PCT_FramingPresetFile framingFile = PCT_Storage.LoadFramingPresets();
		m_FramingPresets = framingFile.presets;

		PCT_AnglePresetFile angleFile = PCT_Storage.LoadAnglePresets();
		m_AnglePresets = angleFile.presets;

		// Task P2-5: Lens-/Kit-Familien einmalig laden (identisches Muster wie Framing/Angle oben --
		// PCT_Storage regeneriert die Builtins bei jedem Aufruf, ein einmaliges Laden hier reicht).
		PCT_LensPresetFile lensFile = PCT_Storage.LoadLensPresets();
		m_LensPresets = lensFile.presets;

		PCT_LensKitFile lensKitFile = PCT_Storage.LoadLensKits();
		m_LensKits = lensKitFile.kits;
	}

	override string GetInputToggle() {
		return "UAPCT_ToggleMenu";
	}

	override string GetLayoutRoot() {
		return "PsyernsCinematicTool/GUI/layouts/pct_cinematic_form.layout";
	}

	override string GetTitle() {
		return "Psyerns Cinematic Tool";
	}

	override string GetIconName() {
		return "C";
	}

	override bool ImageIsIcon() {
		return false;
	}

	override string GetWebhookTitle() {
		return "Psyerns Cinematic Tool";
	}

	override int GetRPCMin() {
		return EPCT_RPC.INVALID;
	}

	override int GetRPCMax() {
		return EPCT_RPC.COUNT;
	}

	// Spiegel von JMCameraModule.c:80-84 (EnableUpdate override auf SERVER: no-op, damit
	// OnUpdate serverseitig nie feuert; Body-Streaming ist ein reiner Client-Mechanismus).
	#ifdef SERVER
	override void EnableUpdate() {
	}
	#else
	// Spiegel des Timer-Teils von JMCameraModule.OnUpdate (JMCameraModule.c:85-164), reduziert
	// auf das Body-Streaming (Phase 1). COT gated diesen Block zusaetzlich hinter der Checkbox
	// "Enable fullmap freecam update" (m_EnableFullmapCamera, default false) und unterscheidet
	// per g_Game.IsClient() zwischen echtem Client (RPC) und Listen-Host (Direktaufruf ohne RPC,
	// JMCameraModule.c:151-162). PCT hat in Phase 1 keine solche Checkbox/UI und sendet daher
	// immer per RPC, solange die eigene Kamera aktiv ist -- g_Game.IsClient() waere hier ohnehin
	// ein von CLAUDE.md verbotener Authority-Check; der RPC-Weg funktioniert transparent auch im
	// Listen-Host-Fall (Selbst-RPC, von OnRPC lokal ueber IsMissionHost() zugestellt).
	override void OnUpdate( float timeslice ) {
		if ( !IsMissionClient() )
			return;

		if ( !PCT_CinematicCamera.s_PCT_ActiveCamera )
			return;

		// K4-Guard (Plan_B1 Section 5.2): COTs ToggleCamera prueft nur CurrentActiveCamera
		// (JMCameraModule.c:619) und uebernimmt so den Spectator-Slot, waehrend PCTs eigene Kamera
		// noch aktiv ist -- COT setzt CurrentActiveCamera in seinem eigenen Client_Enter
		// (JMCameraModule.c:280), das PCT nie schreibt (K3). Erkennung hier, defensive
		// Rueckrichtung: verwaiste PCT-Kamera client-seitig aufraeumen, exakt COTs
		// Client_Leave-Kameraaufraeum-Muster (JMCameraModule.c:408-418), aber OHNE den Rest von
		// Client_Leave -- kein RPC, kein Input-Controller-Toggle, denn der Spectator-Slot wurde
		// bereits von COTs eigenem Server_Enter server-seitig uebernommen; Body-Reattach macht
		// COTs eigenes Leave.
		if ( CurrentActiveCamera ) {
			// Task P1-6 / Reset-Disziplin (Plan_B4 Section 8, Phase-1-DoD 4): K4 raeumt die verwaiste
			// PCT-Kamera auf (siehe Kommentare unten) -- Bild muss danach ebenso neutral sein wie nach
			// einem regulaeren Leave (Client_Leave, s.o.), sonst bliebe ein DOF-Rest haengen, wenn COT
			// die Kamera uebernimmt, waehrend PCT gerade DOF gesetzt hatte.
			PPEffects.ResetDOFOverride();

			// Task P1-8: Kompositions-Overlays ausblenden -- dritter Verdrahtungspunkt (K4-Zweig).
			HideOverlay();

			m_UpdateTime = 0.0;

			PCT_CinematicCamera.s_PCT_ActiveCamera.SetActive( false );
			g_Game.ObjectDeleteOnClient( PCT_CinematicCamera.s_PCT_ActiveCamera );
			PCT_CinematicCamera.s_PCT_ActiveCamera = null;

			// P1-3-Diagnose-Logging: K4-Zweig -- COTs Kamera hat den Spectator-Slot uebernommen,
			// verwaiste PCT-Kamera wurde hier defensiv aufgeraeumt (siehe Kommentar oben).
			Print( "[PCT] K4 — COT camera took over, PCT camera cleaned up" );

			COTCreateLocalAdminNotification( new StringLocaliser( "PCT-Kamera durch COT-Kamera ersetzt" ) );
			return;
		}

		// Task P3-6: Pfad-Visualisierung -- pro Frame (Projektion folgt der Kamera), Canvas zeichnet
		// nur Linien, keine Allokationen (R5). Vor dem 1s-Streaming-Gate, das darunter frueh returnt.
		UpdatePathViz();

		m_UpdateTime += timeslice;

		if ( m_UpdateTime < 1.0 )
			return;

		m_UpdateTime = 0.0;

		ScriptRPC rpc = new ScriptRPC();
		Param1< vector > param = new Param1< vector >( PCT_CinematicCamera.s_PCT_ActiveCamera.GetPosition() );
		param.Serialize( rpc );
		rpc.Send( g_Game.GetPlayer(), EPCT_RPC.UpdatePosition, true, NULL );
	}
	#endif

	// ===== Task P1-9: Kamera-Toggle-Input (Docs/Plan_B1_Architektur.md Section 5.1-Ende,
	// COT-Muster JMCameraModule.c:203) =====
	// Registrierungsmuster identisch zu JMCameraModule.RegisterKeyMouseBindings
	// (JMCameraModule.c:203-213): super() ZUERST (JMRenderableModuleBase.
	// RegisterKeyMouseBindings, JMRenderableModuleBase.c:272-280, bindet dort bereits
	// GetInputToggle() == "UAPCT_ToggleMenu" auf Input_ToggleShow -- UAPCT_ToggleMenu wird von
	// dieser Methode selbst NICHT angefasst, s. Task-Brief), danach Bind( new JMModuleBinding(
	// ... ) ) fuer die neue Aktion. JMModuleBinding-Ctor-Signatur (JM CF Deprecated-Modulpfad
	// JMModuleBinding.c:8): "void JMModuleBinding(string callback, string input, bool menu =
	// false)" -- callback = Methodenname als String (per g_Script.CallFunctionParams aufgeloest,
	// CF_InputBindings.c:118/123), input = Name der Input-Action, dritter Parameter true wie bei
	// allen COT-Bindings in JMCameraModule (JMCameraModule.c:207-212). Bind(CF_InputBinding) ist
	// CF_ModuleGame.c:93.
	override void RegisterKeyMouseBindings() {
		super.RegisterKeyMouseBindings();

		Bind( new JMModuleBinding( "Input_ToggleCamera", "UAPCT_ToggleCamera", true ) );

		// Task P2-4 (Docs/Plan_B5_UI_UX.md Section 7 Keybind-Tabelle "UAPCT_ApplyFraming ... Framing-Preset
		// auf Ziel anwenden ... ohne Default (ueber Optionen belegbar)"): Action deklariert in
		// Scripts/Data/Inputs.xml OHNE <preset>-Eintrag -- kein Default-Bind, Nutzer kann in den
		// COT-Steuerungsoptionen selbst eine Taste zuweisen. Wiederholt zuletzt gewaehltes Framing-/Angle-
		// Preset-Paar (m_SelectedFramingPresetId/m_SelectedAnglePresetId), identisches Verhalten zu einem
		// erneuten Klick auf den zuletzt gedrueckten Framing-/Angle-Button (siehe ApplyAutoFraming).
		Bind( new JMModuleBinding( "Input_ApplyFraming", "UAPCT_ApplyFraming", true ) );
		// Task P3-5 (Plan_B7 Section 15 / Plan_B5 Section 7): Sequencer-Keybinds, gleiche Mechanik.
		Bind( new JMModuleBinding( "Input_AddCamPoint", "UAPCT_AddCamPoint", true ) );
		Bind( new JMModuleBinding( "Input_AddLookPoint", "UAPCT_AddLookPoint", true ) );
		Bind( new JMModuleBinding( "Input_PlayPause", "UAPCT_PlayPause", true ) );
		Bind( new JMModuleBinding( "Input_TogglePathViz", "UAPCT_TogglePathViz", true ) );
	}

	// Handler-Signatur exakt wie COTs gebundene Handler (z. B. JMCameraModule.ToggleCamera(
	// UAInput input), JMCameraModule.c:603 -- CF_InputBindings.Update ruft jeden gebundenen
	// Callback ueber g_Script.CallFunctionParams(..., new Param1<UAInput>(input)) auf,
	// CF_InputBindings.c:118/123, das erzwingt exakt diese Ein-Parameter-Signatur).
	// input.LocalPress() spiegelt COTs eigene ToggleCamera-Wortliste (JMCameraModule.c:605) --
	// nur bei tatsaechlichem Tastendruck ausloesen, nicht bei Release/gehaltenem Zustand.
	void Input_ToggleCamera( UAInput input ) {
		if ( !input.LocalPress() )
			return;

		Print( "[PCT] toggle via keybind" );

		if ( PCT_CinematicCamera.s_PCT_ActiveCamera ) {
			Leave();
		} else {
			Enter();
		}
	}

	// Task P2-4: Keybind-Handler fuer UAPCT_ApplyFraming (B5 Section 7) -- identische Ein-Parameter-
	// Signatur/LocalPress()-Guard wie Input_ToggleCamera oben.
	void Input_ApplyFraming( UAInput input ) {
		if ( !input.LocalPress() )
			return;

		ApplyAutoFraming();
	}

	// Spiegel von JMCameraModule.OnRPC (JMCameraModule.c:230-247).
	override void OnRPC( PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx ) {
		switch ( rpc_type ) {
			case EPCT_RPC.Enter:
				RPC_Enter( ctx, sender, target );
				break;
			case EPCT_RPC.Leave:
				RPC_Leave( ctx, sender, target );
				break;
			case EPCT_RPC.Leave_Finish:
				RPC_Leave_Finish( ctx, sender, target );
				break;
			case EPCT_RPC.UpdatePosition:
				RPC_UpdatePosition( ctx, sender, target );
				break;
		}
	}

	// Spiegel von JMCameraModule.Enter (JMCameraModule.c:249-268), mit PCT-Abweichung: Schritt 1
	// ist jetzt der K2-Handoff (Docs/Plan_B1_Architektur.md Section 5.2) statt des vormaligen
	// K1-Sofortabbruchs -- COTs globales CurrentActiveCamera (JMCameraBase.c:11) wird weiterhin
	// nur GELESEN, nie geschrieben (K3).
	void Enter() {
		// P1-3-Diagnose-Logging (Task-Brief "Client-Diagnose-Logging"): [PCT]-Praefix an allen
		// client-seitigen Enter()-Verzweigungen, damit im Client-Log nachvollziehbar ist, ob/wo
		// Enter() abgebrochen wird -- bisher komplett unsichtbar (Ausgangsproblem b) des Task-Briefs).
		Print( "[PCT] Enter requested" );

		// Task P1-9 Guard: waehrend ein K2-Handoff laeuft, weitere Enter()-Aufrufe ignorieren
		// (Task-Brief), sonst liefe ein zweiter Leave()/Poll-Strang gegen denselben Handoff.
		if ( m_CotHandoffActive ) {
			Print( "[PCT] Enter ignored — handoff already in progress" );
			return;
		}

		// Schritt 1 -- Task P1-9 K2-Handoff (Plan_B1 Section 5.2, ersetzt den vormaligen
		// K1-Sofortabbruch): COT-Kamera aktiv -> Position/Richtung merken, COTs
		// JMCameraModule.Leave() anstossen, per CallLater-Poll auf CurrentActiveCamera == null
		// warten, danach PCT-Enter an der gemerkten Position fortsetzen (StartCotHandoff()).
		// Faellt auf den alten K1-Abbruch zurueck, wenn COTs Modul nicht auffindbar ist
		// (Task-Brief: "K1-Verhalten bleibt als Fallback").
		if ( CurrentActiveCamera ) {
			StartCotHandoff();
			return;
		}

		// Schritt 2 -- Toggle-Verhalten wie COT.ToggleCamera (JMCameraModule.c:619-623): eigene
		// Kamera bereits aktiv -> Leave() statt erneutem Enter.
		if ( PCT_CinematicCamera.s_PCT_ActiveCamera ) {
			Print( "[PCT] Enter toggles to Leave" );
			Leave();
			return;
		}

		// Task P2-3 (ApplyShot ohne aktive Kamera, siehe m_PendingEnterPositionSet-Kommentar oben):
		// ein per ApplyShot() gemerkter Shot-Startpunkt gewinnt einmalig gegenueber der aktuellen freien
		// Kameraposition. Merker wird VOR EnterAt() geloescht (One-Shot -- ein spaeteres normales Enter()
		// soll wieder die aktuelle freie Position nutzen, nicht denselben Shot-Punkt wiederholen).
		if ( m_PendingEnterPositionSet ) {
			vector pendingPosition = m_PendingEnterPosition;
			m_PendingEnterPositionSet = false;
			EnterAt( pendingPosition );
			return;
		}

		EnterAt( g_Game.GetCurrentCameraPosition() );
	}

	// Task P1-9: gemeinsamer Enter-Fortsetzungspfad fuer den regulaeren Enter() (Position =
	// aktuelle Kameraposition) UND fuer den K2-Handoff-Abschluss (Position = die beim
	// Handoff-Start gemerkte m_CotHandoffPosition, s. PollCotHandoff()). Enthaelt exakt die
	// Schritte 3+4 von COTs JMCameraModule.Enter (JMCameraModule.c:255-267): COT-Vorbereitung,
	// dann Offline-/Client-Weiche. Normales Enter() verhaelt sich dadurch unveraendert
	// (identische Positionslogik wie vorher -- g_Game.GetCurrentCameraPosition() wird nur jetzt
	// zentral hier statt inline in Enter() gelesen).
	private void EnterAt( vector position ) {
		// COT-Vorbereitung, Pflichtbestandteil (B1 Section 5.2), 1:1 wie JMCameraModule.Enter
		// (JMCameraModule.c:255-257).
		GetPlayer().COT_TempDisableOnSelectPlayer();
		GetPlayer().COT_RememberVehicle();

		// Exakt COTs Mission-Kontext-Weiche (JMCameraModule.c:259-267): Offline -> direkter
		// Server_Enter-Aufruf; echter Client -> ScriptRPC. IsMissionOffline()/IsMissionClient()
		// sind CF-Mission-Kontext-Helper, keine Authority-Checks (siehe CLAUDE.md-Ausnahme, im
		// Report begruendet).
		if ( IsMissionOffline() ) {
			Server_Enter( NULL, g_Game.GetPlayer(), position );
		} else if ( IsMissionClient() ) {
			ScriptRPC rpc = new ScriptRPC();
			Param1< vector > param = new Param1< vector >( position );
			param.Serialize( rpc );
			rpc.Send( g_Game.GetPlayer(), EPCT_RPC.Enter, true, NULL );
		}
	}

	// Task P1-9 (Plan_B1 Section 5.2, Regel K2): angestossen aus Enter(), wenn
	// CurrentActiveCamera beim Enter-Versuch non-null ist (COT-Freecam aktiv). Merkt
	// Position/Richtung, holt COTs JMCameraModule ueber GetModuleManager().GetModule(...)
	// (JM CF Deprecated-Modulpfad JMModuleManager.c:15: "JMModuleBase GetModule(typename type)",
	// GetModuleManager() selbst JMModuleManager.c:112, globale static Funktion --
	// Research_COT_Infrastructure.md Section 2), stoesst dessen Leave() an und startet den Poll
	// (PollCotHandoff). Kein COT-Modul auffindbar -> Fallback auf den alten K1-Abbruch
	// (Task-Brief: "K1-Verhalten bleibt als Fallback").
	private void StartCotHandoff() {
		JMCameraModule cot = JMCameraModule.Cast( GetModuleManager().GetModule( JMCameraModule ) );
		if ( !cot ) {
			Print( "[PCT] Enter aborted — COT camera active, JMCameraModule not found (K1 fallback)" );
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: COT-Kamera ist aktiv - bitte zuerst dort verlassen." ) );
			return;
		}

		Print( "[PCT] handoff started" );

		m_CotHandoffPosition = g_Game.GetCurrentCameraPosition();
		m_CotHandoffDirection = g_Game.GetCurrentCameraDirection();
		m_CotHandoffElapsedMs = 0;
		m_CotHandoffActive = true;

		cot.Leave();

		PollCotHandoff();
	}

	// Task P1-9: Wartepoll ueber CallLater (kein OnUpdate -- CLAUDE.md verbietet Allokationen/
	// Polling in Per-Frame-Pfaden), alle PCT_HANDOFF_POLL_MS. Abschlussbedingung ist
	// CurrentActiveCamera == null (K3: nur LESEN, nie schreiben). Bricht nach
	// PCT_HANDOFF_TIMEOUT_MS mit Notification + Log ab. CallQueue-Null-Check gemaess CLAUDE.md
	// ("g_Game.GetCallQueue() kann null sein"), Spiegel von Client_Check_Leave weiter unten.
	private void PollCotHandoff() {
		// Fix P1-9/K2 (Review-Fund P1-9): CurrentActiveCamera == null alleine reicht nicht als
		// Abschlussbedingung. COTs Client_Leave setzt CurrentActiveCamera bereits synchron auf null
		// (JMCameraModule.c:418), der Server-Reattach (RPC_Leave_Finish -> SelectPlayer) kann aber bis
		// zu ~5s spaeter folgen (JMCameraModule.c Server_Leave waitForPlayerIdleTimeout, 250/5000 ms).
		// m_COT_EnableBonePositionUpdate (COT DayZPlayerImplement.c:18, gesetzt JMCameraModule.c:448,
		// geloescht JMCameraModule.c:465) markiert genau dieses Wartefenster -- wird hier nur GELESEN
		// (K3), Plan_B1 Section 5.2 K2 "Spieler wieder steuerbar".
		PlayerBase player = PlayerBase.Cast( g_Game.GetPlayer() );
		bool cotStillReattaching = false;
		if ( player )
			cotStillReattaching = player.m_COT_EnableBonePositionUpdate;

		if ( !CurrentActiveCamera && !cotStillReattaching ) {
			m_CotHandoffActive = false;
			Print( "[PCT] handoff complete — entering at " + m_CotHandoffPosition.ToString() );
			EnterAt( m_CotHandoffPosition );
			return;
		}

		m_CotHandoffElapsedMs = m_CotHandoffElapsedMs + PCT_HANDOFF_POLL_MS;

		if ( m_CotHandoffElapsedMs >= PCT_HANDOFF_TIMEOUT_MS ) {
			m_CotHandoffActive = false;
			Print( "[PCT] handoff timeout" );
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: Handoff fehlgeschlagen - COT-Kamera manuell verlassen." ) );
			return;
		}

		ScriptCallQueue callQueue = g_Game.GetCallQueue( CALL_CATEGORY_SYSTEM );
		if ( !callQueue ) {
			m_CotHandoffActive = false;
			return;
		}

		callQueue.CallLater( PollCotHandoff, PCT_HANDOFF_POLL_MS, false );
	}

	// Spiegel von JMCameraModule.Client_Enter (JMCameraModule.c:270-300), reduziert um den
	// COT_PreviousActiveCamera-Handoff (K2 verschoben, K3 verbietet ohnehin das Schreiben von
	// COTs CurrentActiveCamera/COT_PreviousActiveCamera). Kamerarichtung wird deshalb immer an
	// der Head-Bone ausgerichtet (COTs Zweig fuer den Fall "kein vorheriger Camera-Wechsel").
	private void Client_Enter() {
		if ( !Class.CastTo( PCT_CinematicCamera.s_PCT_ActiveCamera, Camera.GetCurrentCamera() ) )
			return;

		// P1-3-Diagnose-Logging: hier wird s_PCT_ActiveCamera im MP-Pfad erstmals client-seitig
		// gesetzt (Enter-Reply-Handler -- Server_Enter's g_Game.SelectSpectator(sender,
		// "PCT_CinematicCamera", position) + Rueck-RPC EPCT_RPC.Enter an sender laufen client-seitig
		// hier in RPC_Enter's else-Zweig auf Client_Enter() zusammen, siehe Kommentare dort).
		// Tatsaechlicher ClassName() beantwortet die offene Frage, ob SelectSpectator wirklich eine
		// PCT_CinematicCamera-Instanz spawnt.
		Print( "[PCT] camera active class=" + PCT_CinematicCamera.s_PCT_ActiveCamera.ClassName() );

		// Task P1-6 (Architektur-Vorgabe): client-seitiger Kamera-Erhalt -- Rig-Referenz an die frisch
		// aufgeloeste Kamera durchreichen, bevor sie aktiviert wird (SetActive/OnUpdate laufen danach
		// sofort mit gueltigem m_PCT_Rig).
		PCT_CinematicCamera.s_PCT_ActiveCamera.m_PCT_Rig = m_CameraRig;

		// Task P1-7 (Rig-Panel Fokusdistanz-Wertefluss): Pending-Fokusdistanz-Merker an die neue
		// Kamera uebertragen, siehe m_PendingFocusDistanceM-Kommentar oben.
		PCT_CinematicCamera.s_PCT_ActiveCamera.m_PCT_FocusDistanceM = m_PendingFocusDistanceM;

		// Task P2-5 (CloseFocus-Clamp, Plan_B8 Section 5.2): Settings-Referenz durchreichen, exakt
		// analog m_PCT_Rig oben (dieselbe Andockstelle, gleicher client-seitiger Kamera-Erhalt).
		PCT_CinematicCamera.s_PCT_ActiveCamera.m_PCT_Settings = m_Settings;

		PCT_CinematicCamera.s_PCT_ActiveCamera.SetActive( true );

		// Task P1-8: Kompositions-Overlays einblenden -- client-seitiger Kamera-Erhalt ist einer der
		// vier Verdrahtungspunkte aus der Architektur-Vorgabe (Client_Enter/Client_Leave/K4/
		// Offline-Enter).
		ShowOverlay();

		Human player = g_Game.GetPlayer();
		if ( player ) {
			// Fix P2-3 (m_PendingEnterOrientationSet-Kommentar oben): ein per ApplyShot() gemerkter
			// Orientierungs-Wert gewinnt einmalig gegenueber der COT-Head-Bone-Ausrichtung -- exakt
			// analog zum Positions-Merker in Enter() (m_PendingEnterPositionSet). Merker wird VOR
			// SetOrientation() geloescht (One-Shot -- ein spaeteres normales Enter() soll wieder die
			// Head-Bone-Ausrichtung nutzen, nicht dieselbe Shot-Orientierung wiederholen).
			if ( m_PendingEnterOrientationSet ) {
				vector pendingOrientation = m_PendingEnterOrientation;
				m_PendingEnterOrientationSet = false;
				PCT_CinematicCamera.s_PCT_ActiveCamera.SetOrientation( pendingOrientation );
			} else {
				vector headTransform[4];
				player.GetBoneTransformWS( player.GetBoneIndexByName( "Head" ), headTransform );
				PCT_CinematicCamera.s_PCT_ActiveCamera.SetDirection( headTransform[1] );
			}

			player.GetInputController().SetDisabled( true );
		}
	}

	// Spiegel von JMCameraModule.Server_Enter( sender, target, position ) (JMCameraModule.c:329-359,
	// B1 Section 5.2 Code-Skizze). COTs IsMissionOffline()-Zweig (JMCameraModule.c:337-345,
	// direktes g_Game.CreateObject statt SelectPlayer/SelectSpectator, noetig weil `sender` dort
	// NULL sein kann -- Enter() ruft Server_Enter(NULL, ...) offline) ist jetzt EXAKT gespiegelt
	// (Fix P1-1/1, vormals fehlend -> Nullpointer-Crash auf sender.GetPlayer()). Einzige
	// Abweichung: Zielvariable/Klassenname ist s_PCT_ActiveCamera / "PCT_CinematicCamera" statt
	// COTs CurrentActiveCamera / "JMCinematicCamera" (K3 -- CurrentActiveCamera bleibt ungeschrieben).
	private void Server_Enter( PlayerIdentity sender, Object target, vector position ) {
		PlayerBase targetPlayer = PlayerBase.Cast( target );
		if ( targetPlayer )
			targetPlayer.COT_RememberVehicle();

		if ( IsMissionOffline() ) {
			PCT_CinematicCamera.s_PCT_ActiveCamera = PCT_CinematicCamera.Cast( g_Game.CreateObject( "PCT_CinematicCamera", position, false ) );

			// Task P1-7-Fix (Kleinkram aus dem P1-6-Review): Null-Check nach dem Cast -- CreateObject/
			// Cast koennen fehlschlagen (z. B. Klasse nicht ladbar), s_PCT_ActiveCamera waere dann null;
			// ohne diesen Guard wuerden die folgenden Member-Zuweisungen mit einem Nullpointer crashen.
			if ( !PCT_CinematicCamera.s_PCT_ActiveCamera )
				return;

			// Task P1-6 (Architektur-Vorgabe): Offline-Zweig ist der zweite client-seitige
			// Kamera-Erhalt (kein separater RPC-Rundlauf -- Server_Enter laeuft hier direkt client-lokal,
			// siehe Enter()-Kommentar oben), daher hier ebenfalls die Rig-Referenz durchreichen.
			PCT_CinematicCamera.s_PCT_ActiveCamera.m_PCT_Rig = m_CameraRig;

			// Task P1-7 (Rig-Panel Fokusdistanz-Wertefluss): Pending-Fokusdistanz-Merker auch im
			// Offline-Zweig uebertragen, siehe m_PendingFocusDistanceM-Kommentar oben.
			PCT_CinematicCamera.s_PCT_ActiveCamera.m_PCT_FocusDistanceM = m_PendingFocusDistanceM;

			// Task P2-5 (CloseFocus-Clamp, Plan_B8 Section 5.2): Settings-Referenz auch im
			// Offline-Zweig durchreichen, siehe Client_Enter-Kommentar oben.
			PCT_CinematicCamera.s_PCT_ActiveCamera.m_PCT_Settings = m_Settings;

			PCT_CinematicCamera.s_PCT_ActiveCamera.SetActive( true );

			// Task P1-8: Offline-Enter ist der zweite client-seitige Kamera-Erhalt (kein RPC-Rundlauf,
			// siehe Enter()-Kommentar) -- vierter Verdrahtungspunkt der Architektur-Vorgabe.
			ShowOverlay();

			// Fix P2-3 (m_PendingEnterOrientationSet-Kommentar oben): Offline-Enter hat -- anders als
			// Client_Enter() -- ohnehin keine Head-Bone-Ausrichtung, daher hier kein else-Zweig noetig,
			// nur der reine Pending-Konsum, gleiches One-Shot-Muster (Merker VOR SetOrientation()
			// geloescht).
			if ( m_PendingEnterOrientationSet ) {
				vector pendingOrientation = m_PendingEnterOrientation;
				m_PendingEnterOrientationSet = false;
				PCT_CinematicCamera.s_PCT_ActiveCamera.SetOrientation( pendingOrientation );
			}

			if ( g_Game.GetPlayer() )
				g_Game.GetPlayer().GetInputController().SetDisabled( true );
		} else {
			// COTs modded PlayerBase.OnSelectPlayer wuerde beim Spieler->Spectator-Wechsel feuern;
			// COT unterdrueckt das gezielt vor SelectPlayer (JMCameraModule.c:348) -- PCT spiegelt das.
			PlayerBase senderPlayer = PlayerBase.Cast( sender.GetPlayer() );
			if ( senderPlayer )
				senderPlayer.COT_TempDisableOnSelectPlayer();

			g_Game.SelectPlayer( sender, NULL );
			g_Game.SelectSpectator( sender, "PCT_CinematicCamera", position );

			ScriptRPC rpc = new ScriptRPC();
			rpc.Send( NULL, EPCT_RPC.Enter, true, sender );
		}

		GetCommunityOnlineToolsBase().Log( sender, "Entered the Cinematic Camera" );
	}

	// Spiegel von JMCameraModule.RPC_Enter (JMCameraModule.c:361-383). Permission-Check
	// ("CinematicTool.View" statt "Camera.View") VOR jedem weiteren Schritt, wie B1 Section 8
	// verlangt. Payload als EIN Param-Objekt (Param1<vector>) gemaess B1 Section 6 RPC-Tabelle --
	// Abweichung von COT, das hier einen rohen vector schreibt/liest (JMCameraModule.c:265,373).
	private void RPC_Enter( ParamsReadContext ctx, PlayerIdentity senderRPC, Object target ) {
		if ( IsMissionHost() ) {
			if ( !GetPermissionsManager().HasPermission( "CinematicTool.View", senderRPC ) )
				return;

			Param1< vector > param = new Param1< vector >( vector.Zero );
			if ( !ctx.Read( param ) )
				return;

			Server_Enter( senderRPC, target, param.param1 );
		} else {
			// RPC kam vom Server, Permission wurde dort bereits geprueft.
			Client_Enter();
		}
	}

	// Spiegel von JMCameraModule.Leave (JMCameraModule.c:385-400). SetFreezeMouse(false)
	// (JMCameraModule.c:395) ist NICHT portiert: das ist COTs eigener static Helper, der
	// CurrentActiveCamera.LookFreeze setzt -- K3 verbietet PCT das Anfassen von
	// CurrentActiveCamera, und Phase 1 hat kein eigenes Freeze-Mouse-Feature.
	void Leave() {
		if ( GetPlayer().COT_GetOutVehicle() )
			return;

		if ( IsMissionOffline() ) {
			Server_Leave( NULL, g_Game.GetPlayer() );
		} else if ( IsMissionClient() ) {
			ScriptRPC rpc = new ScriptRPC();
			rpc.Send( g_Game.GetPlayer(), EPCT_RPC.Leave, true, NULL );
		}
	}

	// Spiegel von JMCameraModule.Client_Leave (JMCameraModule.c:402-454), reduziert um den
	// COT_PreviousActiveCamera-Wiederherstellungspfad (K2 verschoben, K3 -- PCT kennt/schreibt
	// COT_PreviousActiveCamera nicht). Kamera-Aufraeumen (g_Game.ObjectDeleteOnClient +
	// s_PCT_ActiveCamera = null) exakt wie COTs Muster (JMCameraModule.c:410-418).
	private void Client_Leave( int waitForPlayerIdleTimeout = 0 ) {
		if ( !PCT_CinematicCamera.s_PCT_ActiveCamera )
			return;

		// Task P1-6 / Reset-Disziplin (Plan_B4 Section 8 "Reset-Disziplin (zwingend...)", Phase-1-DoD 4
		// "nach Verlassen ist das Bild wieder neutral, kein DOF-Rest", Plan_B6 Section 1). Immer
		// resetten statt bedingt auf "DOF war je gesetzt" zu pruefen -- einfacher/robuster, und
		// ResetDOFOverride() ist gefahrlos auch ohne vorheriges Override (PPEffects.c:518-521 ruft nur
		// OverrideDOF(false,0,0,0,0,1) auf, reiner State-loser Passthrough zu g_Game.OverrideDOF, siehe
		// PCT_CinematicCamera.ApplyDepthOfField-Kommentar).
		PPEffects.ResetDOFOverride();

		// Task P1-8: Kompositions-Overlays ausblenden (nur Hide(), kein Unlink() -- Architektur-
		// Vorgabe, Widgets bleiben fuer den naechsten Enter erhalten) -- zweiter Verdrahtungspunkt.
		HideOverlay();

		// Fix P1-1/4: Body-Streaming-Timer beim Leave zuruecksetzen (Spiegel des K4-Zweigs in
		// OnUpdate), damit ein spaeteres erneutes Enter wieder mit vollen 1.0s startet statt mit
		// einem stehengebliebenen Bruchteil aus dem vorherigen Kamera-Zyklus.
		m_UpdateTime = 0.0;

		PCT_CinematicCamera.s_PCT_ActiveCamera.SetActive( false );

		g_Game.ObjectDeleteOnClient( PCT_CinematicCamera.s_PCT_ActiveCamera );
		PCT_CinematicCamera.s_PCT_ActiveCamera = null;

		// P1-3-Diagnose-Logging: PCT-Kamera-Aufraeumen (SetActive(false) + ObjectDeleteOnClient +
		// s_PCT_ActiveCamera = null, siehe Kommentar oben) ist an dieser Stelle abgeschlossen.
		Print( "[PCT] Leave — cleanup done" );

		if ( g_Game.GetPlayer() )
			g_Game.GetPlayer().GetInputController().SetDisabled( false );

		PlayerBase player;
		if ( waitForPlayerIdleTimeout && Class.CastTo( player, g_Game.GetPlayer() ) ) {
			player.COT_EnableBonePositionUpdate( true );
			Client_Check_Leave( player, waitForPlayerIdleTimeout );

			if ( waitForPlayerIdleTimeout > 1000 )
				COTCreateLocalAdminNotification( new StringLocaliser( "Leaving cinematic camera..." ) );
		}
	}

	// Spiegel von JMCameraModule.Client_Check_Leave (JMCameraModule.c:456-479). Poll-Timer via
	// g_Game.GetCallQueue, mit Null-Check gemaess CLAUDE.md ("GetCallQueue() kann null sein").
	void Client_Check_Leave( PlayerBase player, int waitForPlayerIdleTimeout ) {
		if ( !player.COT_IsAnimationIdle() && waitForPlayerIdleTimeout > 0 ) {
			ScriptCallQueue callQueue = g_Game.GetCallQueue( CALL_CATEGORY_SYSTEM );
			if ( !callQueue )
				return;

			callQueue.CallLater( Client_Check_Leave, 250, false, player, waitForPlayerIdleTimeout - 250 );
		} else {
			player.COT_EnableBonePositionUpdate( false );
			COTCreateLocalAdminNotification( new StringLocaliser( "Left cinematic camera." ) );

			if ( g_Game.IsMultiplayer() ) {
				ScriptRPC rpc = new ScriptRPC();
				rpc.Send( NULL, EPCT_RPC.Leave_Finish, true, NULL );
			} else {
				// Offline/SP.
				g_Game.SelectPlayer( null, player );
			}
		}
	}

	// Spiegel von JMCameraModule.Server_Leave (JMCameraModule.c:481-525). Reply-Payload als
	// Param1<float> statt COTs rohem int (B1 Section 6 RPC-Tabelle: "Reply: Param1<float>
	// timeout") -- die interne Millisekunden-Arithmetik bleibt int, nur die Wire-Payload wird
	// beim Senden/Empfangen nach float konvertiert (Abweichung siehe Report).
	private void Server_Leave( PlayerIdentity sender, Object target ) {
		PlayerBase player;
		if ( Class.CastTo( player, target ) ) {
			vector spectatorPosition = player.GetPosition();
			int waitForPlayerIdleTimeout;

			if ( !player.m_JM_SpectatedObject && player.m_JM_CameraPosition != vector.Zero ) {
				player.COTResetSpectator();

				if ( player.HasLastPosition() ) {
					if ( COT_SurfaceIsWater( player.GetLastPosition() ) )
						waitForPlayerIdleTimeout = 250;
					else if ( vector.DistanceSq( player.GetLastPosition(), spectatorPosition ) > 0.01 )
						waitForPlayerIdleTimeout = 5000;
				}
			}

			player.m_JM_CameraPosition = vector.Zero;

			if ( g_Game.IsMultiplayer() ) {
				ScriptRPC rpc = new ScriptRPC();
				Param1< float > param = new Param1< float >( waitForPlayerIdleTimeout );
				param.Serialize( rpc );
				rpc.Send( NULL, EPCT_RPC.Leave, true, sender );
			} else {
				Client_Leave( waitForPlayerIdleTimeout );
			}

			GetCommunityOnlineToolsBase().Log( sender, "Left the Cinematic Camera" );

			if ( player.m_JM_SpectatedObject )
				return;

			if ( !waitForPlayerIdleTimeout )
				g_Game.SelectPlayer( sender, player );
		}
	}

	// Spiegel von JMCameraModule.RPC_Leave (JMCameraModule.c:527-549). Payload als
	// Param1<float> gemaess B1 Section 6 (siehe Server_Leave); intern zurueck nach int
	// konvertiert, da Client_Leave/Client_Check_Leave mit Millisekunden-int rechnen.
	private void RPC_Leave( ParamsReadContext ctx, PlayerIdentity senderRPC, Object target ) {
		if ( IsMissionHost() ) {
			if ( !GetPermissionsManager().HasPermission( "CinematicTool.View", senderRPC ) )
				return;

			Server_Leave( senderRPC, target );
		} else {
			// RPC kam vom Server, Permission wurde dort bereits geprueft.
			Param1< float > param = new Param1< float >( 0.0 );
			if ( !ctx.Read( param ) )
				return;

			int waitForPlayerIdleTimeout = param.param1;
			Client_Leave( waitForPlayerIdleTimeout );
		}
	}

	// Spiegel von JMCameraModule.RPC_Leave_Finish (JMCameraModule.c:551-565). Kein
	// IsMissionHost()-Zweig, genau wie im COT-Original: dieser RPC wird ausschliesslich vom
	// Client an den Server geschickt (Client_Check_Leave, target = NULL), lauft also per
	// Transport ohnehin nur serverseitig auf.
	private void RPC_Leave_Finish( ParamsReadContext ctx, PlayerIdentity senderRPC, Object target ) {
		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View", senderRPC ) )
			return;

		PlayerBase player;
		if ( !Class.CastTo( player, senderRPC.GetPlayer() ) || player.m_JM_SpectatedObject )
			return;

		g_Game.SelectPlayer( senderRPC, player );
	}

	// Spiegel von JMCameraModule.RPC_UpdatePosition (JMCameraModule.c:567-592). g_Game.
	// IsDedicatedServer() statt IsMissionHost() -- exakt wie COTs Original an dieser einen
	// Stelle (kein PCT-Abweichung, deckt sich zusaetzlich mit der CLAUDE.md-Vorgabe). Payload
	// als Param1<vector> gemaess B1 Section 6.
	private void RPC_UpdatePosition( ParamsReadContext ctx, PlayerIdentity senderRPC, Object target ) {
		if ( g_Game.IsDedicatedServer() ) {
			if ( !GetPermissionsManager().HasPermission( "CinematicTool.View", senderRPC ) )
				return;

			Param1< vector > param = new Param1< vector >( vector.Zero );
			if ( !ctx.Read( param ) || param.param1 == vector.Zero )
				return;

			PlayerBase player;
			if ( Class.CastTo( player, target ) ) {
				if ( !player.m_JM_SpectatedObject )
					EnterFullmap( player );

				player.m_JM_CameraPosition = param.param1;

				if ( !player.m_JM_SpectatedObject )
					player.COTUpdateSpectatorPosition();
			}
		}
	}

	// Spiegel von JMCameraModule.EnterFullmap (JMCameraModule.c:594-601). Das ist der
	// tatsaechliche Ort, an dem COT waehrend der Freecam Godmode aktiviert -- NICHT
	// Server_Enter (siehe Kommentar dort). Aufgerufen von RPC_UpdatePosition, exakt wie im
	// COT-Original.
	void EnterFullmap( PlayerBase player ) {
		if ( player.m_JM_CameraPosition == vector.Zero ) {
			player.SetLastPosition();
			player.COTSetGodMode( true, false );
		}
	}

	// ===== Task P1-8: Kompositions-Overlays (Docs/Plan_B5_UI_UX.md Section 4) =====
	// Lazy-Erzeugung nach Architektur-Vorgabe -- IsDedicatedServer()-Guard, weil ein dedizierter Server
	// keinen echten Workspace/keine Rendering-GUI hat (g_Game.GetWorkspace().CreateWidgets waere dort
	// sinnlos bzw. riskant); ShowOverlay/HideOverlay werden ohnehin nur aus rein client-seitigen Pfaden
	// aufgerufen (Client_Enter, Offline-Zweig von Server_Enter, Client_Leave, OnUpdate-K4-Zweig -- der
	// gesamte OnUpdate-Block ist per #ifdef SERVER/#else bereits client-only, s. o.), der Guard ist
	// zusaetzliche Verteidigung, CLAUDE.md-konform (einzig erlaubter Authority-Check).
	protected void EnsureOverlayHud() {
		if ( m_OverlayHud )
			return;

		if ( !g_Game || g_Game.IsDedicatedServer() )
			return;

		m_OverlayHud = new PCT_OverlayHud();
	}

	void ShowOverlay() {
		EnsureOverlayHud();
		if ( !m_OverlayHud )
			return;

		m_OverlayHud.Show( m_OverlayThirdsEnabled, m_OverlayCenterEnabled, m_CameraRig.aspectMask );
	}

	void HideOverlay() {
		if ( m_OverlayHud )
			m_OverlayHud.Hide();
	}

	// Einzel-Toggles fuer die Form-Checkboxen/SelectBox (PCT_CinematicForm.OnChange_OverlayThirds/
	// OnChange_OverlayCenter/OnChange_AspectMask) -- "Auswahl wirkt sofort" (Phase-1-DoD 5): Zustand
	// wird immer auf dem Modul gemerkt (auch ohne aktive Kamera/Overlay), zusaetzlich sofort an ein
	// bereits vorhandenes Overlay durchgereicht.
	void SetOverlayThirdsVisible( bool enabled ) {
		m_OverlayThirdsEnabled = enabled;
		if ( m_OverlayHud )
			m_OverlayHud.SetThirdsVisible( enabled );

		// Task P2-3: Settings-Wegschreiben bei Aenderung (Task-Brief: "bei Aenderungen (Overlay-Toggles,
		// Maske) Settings aktualisieren + speichern -- debounced ist unnoetig, einfach bei Aenderung
		// speichern"). m_Settings ist null, bevor OnMissionLoaded() gelaufen ist (bzw. auf einem
		// dedizierten Server dauerhaft) -- Guard verhindert dort ein sinnloses Neuanlegen von Settings nur
		// zum sofortigen Verwerfen.
		if ( m_Settings ) {
			m_Settings.showCompositionGrid = enabled;
			PCT_Storage.SaveSettings( m_Settings );
		}
	}

	void SetOverlayCenterVisible( bool enabled ) {
		m_OverlayCenterEnabled = enabled;
		if ( m_OverlayHud )
			m_OverlayHud.SetCenterVisible( enabled );
	}

	// Schreibt zusaetzlich m_CameraRig.aspectMask (Task-Brief Punkt 4: "Maske schreibt zusaetzlich
	// m_CameraRig.aspectMask") -- das Rig ist damit weiterhin die einzige Quelle der Wahrheit fuer die
	// aktuelle Maske (kein doppelter String-Zustand auf dem Modul).
	void SetOverlayAspectMask( string aspectMask ) {
		m_CameraRig.aspectMask = aspectMask;
		if ( m_OverlayHud )
			m_OverlayHud.SetAspectMask( aspectMask );

		// Task P2-3: siehe SetOverlayThirdsVisible-Kommentar oben (gleiches Muster).
		if ( m_Settings ) {
			m_Settings.defaultAspectMask = aspectMask;
			PCT_Storage.SaveSettings( m_Settings );
		}
	}

	// ===== Task P2-3: Shots speichern/anwenden (Docs/Plan_B3_Datenmodell_Persistenz.md Section 2.4/4,
	// Docs/Plan_B5_UI_UX.md Section 2.2/6, Docs/Plan_B6_Roadmap_Risiken.md Section 1 Phase 2) =====

	// id-Generierung: Task-Brief-Vorgabe "shot_<UnixZeit oder fortlaufend>", Engine-Zeit-API nachgeschlagen
	// statt geraten -- GetYearMonthDay(out year, out month, out day)/GetHourMinuteSecond(out hour, out
	// minute, out second) sind globale proto-Funktionen (kein Klassenkontext), direkt am 1.29-Quellcode
	// verifiziert: scripts - 1.29\1_Core\DayZ\proto\EnSystem.c:28/52 ("proto void GetHourMinuteSecond(out int
	// hour, out int minute, out int second);"/"proto void GetYearMonthDay(out int year, out int month, out
	// int day);"). Beantwortet nebenbei Plan_B6 Section 5 O23 (dort als "unverifiziert" gefuehrt) direkt aus
	// der Quelle: die Funktionen existieren mit genau dieser Signatur. Format uebernimmt das bereits
	// dokumentierte Muster aus Docs/Plan_B5_UI_UX.md Section 6 ("QuickSaveShot ... Auto-Namen
	// Shot_JJJJMMTT_HHMMSS"), mit "shot_"-Praefix in Kleinschreibung passend zur B3-Section-2.4-id-Konvention
	// (Beispiele "sh_030_cu_b", "world_golden_hour" -- durchgehend lowercase/snake_case).
	private string GenerateShotId() {
		int year;
		int month;
		int day;
		GetYearMonthDay( year, month, day );

		int hour;
		int minute;
		int second;
		GetHourMinuteSecond( hour, minute, second );

		string datePart = year.ToString() + ZeroPad2( month ) + ZeroPad2( day );
		string timePart = ZeroPad2( hour ) + ZeroPad2( minute ) + ZeroPad2( second );
		string baseId = "shot_" + datePart + "_" + timePart;

		// Fix P2-3 (Kollisionsschutz): der obige Zeitstempel hat Sekunden-Aufloesung -- zwei Shots
		// innerhalb derselben Sekunde (schnelles Doppel-Speichern, Skript-Aufruf) haetten sonst
		// dieselbe id und wuerden sich in PCT_Storage.SaveShot stillschweigend gegenseitig
		// ueberschreiben. PCT_Storage bietet keine eigene Exists-Helper-API (nachgeschaut) --
		// PCT_Storage.LoadShot(id) waere hier ungeeignet, weil es bei jedem NICHT-Kollisionsfall
		// (dem Normalfall) zusaetzlich einen CF_Log.Warn "Shot-Datei fehlt" ausloesen wuerde (siehe
		// PCT_Storage.c LoadShot) sowie unnoetig Migration/Validation durchliefe. Direkter
		// FileExist()-Check ueber dieselben Storage-Pfad-Konstanten, die auch PCT_Storage.SaveShot/
		// LoadShot verwenden (PCT_Constants.DIR_SHOTS/EXT_JSON), ist die schlankere Alternative.
		// Suffix _2, _3, ... bei Kollision, Obergrenze 99 (danach wird die letzte Kandidaten-id trotz
		// verbleibender Kollision zurueckgegeben -- kein Endlos-Risiko; 99 Shots in derselben Sekunde
		// sind praktisch ausgeschlossen).
		string candidateId = baseId;
		bool collisionFound = false;
		int suffix = 2;

		while ( FileExist( PCT_Constants.DIR_SHOTS + candidateId + PCT_Constants.EXT_JSON ) ) {
			collisionFound = true;

			if ( suffix > 99 )
				break;

			candidateId = baseId + "_" + suffix.ToString();
			suffix = suffix + 1;
		}

		if ( collisionFound )
			Print( "[PCT] shot id collision avoided, using " + candidateId );

		return candidateId;
	}

	private static string ZeroPad2( int value ) {
		if ( value < 10 )
			return "0" + value;

		return value.ToString();
	}

	// Task P2-3: Shot aus dem aktuellen Kamera-/Rig-Zustand erzeugen und speichern (B5 Section 2.2 "Shot
	// speichern"-Button, B6 Phase-2-DoD 1 "Shot speichern -> ... stellt Position, Orientierung, Sensor,
	// Brennweite, Blende, Fokus, Maske exakt wieder her"). cameraRig = m_CameraRig.Clone() (Task-Brief:
	// "tiefe KOPIE ... keine Referenz-Teilung!", siehe PCT_CameraRig.Clone()/CopyInto()). Position/
	// Orientierung/FOV/Fokus/Blur werden als EINZELNER Keyframe (time=0) abgelegt -- PCT_Shot (Section 2.4)
	// hat selbst KEINE direkten Positions-/Orientierungsfelder, nur `keyframes[]`; ein Mehrfach-Keyframe-Flug
	// ist Sequencer-Scope (Phase 3, Plan_B6 Section 1).
	PCT_Shot CaptureCurrentShot( string name ) {
		PCT_Shot shot = new PCT_Shot();
		shot.id = GenerateShotId();
		shot.name = name;
		shot.cameraRig = m_CameraRig.Clone();

		// Aktive Kamera bevorzugt (Task-Brief "Kameraposition/-orientierung der aktiven Kamera"), sonst
		// irgendeine aktive Engine-Kamera (z. B. COT-Freecam) -- dieselbe Prioritaet/derselbe Helper wie
		// RunFovProbe/PCT_CinematicForm.RefreshFovDisplay weiter unten (ResolveActiveCamera(), static).
		Camera cam = ResolveActiveCamera();

		float focusDistanceM = m_PendingFocusDistanceM;
		if ( PCT_CinematicCamera.s_PCT_ActiveCamera )
			focusDistanceM = PCT_CinematicCamera.s_PCT_ActiveCamera.m_PCT_FocusDistanceM;

		if ( cam ) {
			PCT_Keyframe keyframe = new PCT_Keyframe();
			keyframe.time = 0.0;
			// Object.GetPosition()/GetOrientation() -- scripts - 1.29\3_Game\DayZ\Entities\Object.c:293/311
			// (Camera : Entity : ObjectTyped : Object, Camera.c/Entity.c/ObjectTyped.c verifiziert die
			// Vererbungskette). GetOrientation() liefert <Yaw,Pitch,Roll> in GRAD -- exakt PCT_Keyframe.
			// orientation-Konvention (Section 2.2 "wie Object.GetOrientation").
			keyframe.position = cam.GetPosition();
			keyframe.orientation = cam.GetOrientation();
			keyframe.fovDegrees = m_CameraRig.GetVerticalFovDeg();
			keyframe.focusDistance = focusDistanceM;

			// blurStrength (0..100, Section 2.2) ist die gespeicherte KEYFRAME-Groesse; die Engine selbst
			// konsumiert einen 0..1-normierten Wert (PCT_CinematicCamera.ApplyDepthOfField uebergibt
			// PCT_Math.BlurFromAperture(...) direkt an SetFocus/OverrideDOF) -- *100 skaliert auf die
			// Keyframe-Konvention. effectiveFocalMm spiegelt exakt den Clamp aus ApplyDepthOfField (Review-
			// Fix P1-7 Fix 1), damit der gespeicherte Blur-Wert zum tatsaechlich sichtbaren FOV passt.
			float effectiveFocalMm = Math.Clamp( m_CameraRig.focalLengthMm, PCT_Constants.PCT_FOCAL_MIN_MM, PCT_Constants.PCT_FOCAL_MAX_MM );
			float blurNormalized = PCT_Math.BlurFromAperture( m_CameraRig.aperture, effectiveFocalMm );
			keyframe.blurStrength = blurNormalized * 100.0;

			shot.keyframes.Insert( keyframe );
		} else {
			Print( "[PCT] shot capture — no active camera, position/orientation not recorded" );
		}

		bool saved = PCT_Storage.SaveShot( shot );
		if ( saved )
			Print( "[PCT] shot saved " + shot.id );
		else
			CF_Log.Warn( "PCT: CaptureCurrentShot — SaveShot fehlgeschlagen fuer '" + shot.id + "'." );

		return shot;
	}

	// Task P2-3: gespeicherten Shot auf Rig/Kamera/Overlay anwenden (B5 Section 6 "Apply (Kamera + Optik-
	// Zustand setzen)", B6 Phase-2-DoD 1). Rig-Felder werden IN die bestehende m_CameraRig-Instanz kopiert
	// (PCT_CameraRig.CopyInto(), NICHT die Referenz ersetzt) -- die aktive Kamera haelt m_PCT_Rig bereits als
	// Referenz auf genau dieses Objekt (Task P1-6), ein Referenzwechsel wuerde sie von den neuen Werten
	// abschneiden.
	void ApplyShot( PCT_Shot shot ) {
		if ( !shot )
			return;

		if ( shot.cameraRig )
			shot.cameraRig.CopyInto( m_CameraRig );

		PCT_Keyframe firstKeyframe = null;
		if ( shot.keyframes && shot.keyframes.Count() > 0 )
			firstKeyframe = shot.keyframes.Get( 0 );

		// Fokus: Pending-Merker IMMER aktualisieren (identisches Muster zu
		// PCT_CinematicForm.OnChange_FocusDistance) + aktive Kamera, falls vorhanden.
		float focusDistanceM = m_PendingFocusDistanceM;
		if ( firstKeyframe )
			focusDistanceM = firstKeyframe.focusDistance;

		m_PendingFocusDistanceM = focusDistanceM;
		if ( PCT_CinematicCamera.s_PCT_ActiveCamera )
			PCT_CinematicCamera.s_PCT_ActiveCamera.m_PCT_FocusDistanceM = focusDistanceM;

		// Position/Orientierung: nur setzen, wenn ein Keyframe existiert (Capture ohne Kamera legt keinen an,
		// siehe CaptureCurrentShot-Kommentar oben).
		if ( firstKeyframe ) {
			if ( PCT_CinematicCamera.s_PCT_ActiveCamera ) {
				// Object.SetPosition(vector)/SetOrientation(vector) -- Object.c:300/317, dieselbe
				// Vererbungskette wie GetPosition/GetOrientation oben.
				PCT_CinematicCamera.s_PCT_ActiveCamera.SetPosition( firstKeyframe.position );
				PCT_CinematicCamera.s_PCT_ActiveCamera.SetOrientation( firstKeyframe.orientation );
			} else {
				// Task-Brief: "sonst Position als Enter-Startposition merken (naechster Enter() nutzt sie via
				// bestehendem EnterAt-Mechanismus)". Nur Position, keine Orientierung -- Enter()s RPC-Payload
				// ist laut Plan_B1 Section 6 ein einzelnes Param1<vector> (Position), kein Richtungskanal
				// (identische Einschraenkung wie beim K2-Handoff, siehe m_CotHandoffDirection-Kommentar oben).
				m_PendingEnterPosition = firstKeyframe.position;
				m_PendingEnterPositionSet = true;

				// Fix P2-3 (m_PendingEnterOrientationSet-Kommentar oben, Plan_B6 Phase-2-DoD 1 "stellt
				// Orientierung wieder her"): Orientierung analog zur Position merken -- rein
				// client-lokal konsumiert (Client_Enter()/Offline-Zweig von Server_Enter oben), KEINE
				// Enter-RPC-Payload-Aenderung noetig.
				m_PendingEnterOrientation = firstKeyframe.orientation;
				m_PendingEnterOrientationSet = true;
			}
		}

		// Maske: SetOverlayAspectMask() statt direktem Feld-Set -- aktualisiert HUD + Settings (Task P2-3,
		// siehe SetOverlayAspectMask-Kommentar oben) in einem Aufruf; m_CameraRig.aspectMask wurde durch
		// CopyInto() oben bereits auf den Shot-Wert gesetzt, dieser Aufruf ist daher ein No-Op-Feld-Set +
		// Seiteneffekt-Ausloeser, keine Doppelquelle.
		SetOverlayAspectMask( m_CameraRig.aspectMask );

		// Form-Widgets aktualisieren, falls das Fenster gerade offen ist (Task-Brief: "Form-Widgets
		// aktualisieren (Slider/SelectBox-Refresh)"). GetForm() ist JMRenderableModuleBase-API (dasselbe
		// Muster wie OnSettingsUpdated/OnClientPermissionsUpdated oben in der Basisklasse -- siehe
		// JMRenderableModuleBase.c), liefert null, solange das Fenster nie gezeigt wurde.
		PCT_CinematicForm form = PCT_CinematicForm.Cast( GetForm() );
		if ( form )
			form.RefreshRigWidgets();

		Print( "[PCT] shot applied " + shot.id );
	}

	// Task P2-3: Loeschen/Umbenennen als duenne Modul-Wrapper um PCT_Storage (Form ruft ausschliesslich
	// Modul-Methoden auf, keine direkten Storage-Aufrufe -- Konsistenz mit CaptureCurrentShot/ApplyShot oben
	// und mit dem bestehenden Form->Modul-Delegationsmuster, z. B. OnClick_EnterLeave -> m_Module.Enter()).
	bool DeleteShotById( string shotId ) {
		bool deleted = PCT_Storage.DeleteShot( shotId );
		if ( deleted )
			Print( "[PCT] shot deleted " + shotId );

		return deleted;
	}

	// "Umbenennen" aendert das Anzeige-Feld PCT_Shot.name (Zeilen-Label im Browser), NICHT die id/den
	// Dateinamen: das Formular hat nur EIN Textfeld ("Shot-Name"), das beim Speichern bereits `name` fuellt
	// (CaptureCurrentShot-Parameter) -- dieselbe Bedeutung bei "Umbenennen" beizubehalten ist konsistent und
	// braucht keine Dateinamen-Kollisionspruefung. PCT_Storage.RenameShot(oldId,newId) (id-basiert, Task P2-2)
	// bleibt fuer eine spaetere echte id-Umbenennung nutzbar, wird hier bewusst NICHT aufgerufen: mit
	// identischer alter/neuer id waere es ohnehin ein No-Op (frueher Return in RenameShot, kein Save).
	bool RenameShotDisplayName( string shotId, string newName ) {
		PCT_Shot shot = PCT_Storage.LoadShot( shotId );
		if ( !shot )
			return false;

		shot.name = newName;
		bool saved = PCT_Storage.SaveShot( shot );
		if ( saved )
			Print( "[PCT] shot renamed " + shotId );

		return saved;
	}

	// ===== Task P2-4: Auto-Framing (Docs/Plan_B4_Kamera_Engine.md Section 6, Docs/Plan_B5_UI_UX.md
	// Section 2.2) =====
	// Praeferenzreihenfolge/Aufbau spiegelt CaptureCurrentShot/ApplyShot oben: Modul loest Zielobjekt +
	// Preset-Referenzen auf (5_Mission-Zustand/PlayerBase-Erkennung), die eigentliche Geometrie liegt in
	// PCT_Framing (3_Game, COT-frei, siehe dortiger Datei-Kopfkommentar).

	// Preset-Lookup nach id (lineare Suche -- Preset-Familien sind klein, <20 Eintraege, siehe
	// PCT_Storage-Builtin-Listen; kein Lookup-Dictionary noetig, CLAUDE.md "Avoid unnecessary abstraction").
	protected PCT_FramingPreset ResolveFramingPresetById( string id ) {
		if ( !m_FramingPresets )
			return null;

		int count = m_FramingPresets.Count();
		for ( int i = 0; i < count; i++ ) {
			PCT_FramingPreset preset = m_FramingPresets.Get( i );
			if ( preset && preset.id == id )
				return preset;
		}

		return null;
	}

	protected PCT_AnglePreset ResolveAnglePresetById( string id ) {
		if ( !m_AnglePresets )
			return null;

		int count = m_AnglePresets.Count();
		for ( int i = 0; i < count; i++ ) {
			PCT_AnglePreset preset = m_AnglePresets.Get( i );
			if ( preset && preset.id == id )
				return preset;
		}

		return null;
	}

	// Review P2-5: gleiches Lookup-Muster wie ResolveFramingPresetById/ResolveAnglePresetById oben, aber
	// ueber den bereits geladenen Modul-Lens-Cache m_LensPresets (siehe OnMissionLoaded) statt ueber
	// PCT_Storage.GetLensPresetById -- ApplyAutoFraming ist ein Hot-Path (Framing-/Angle-Button-Klick,
	// siehe SetSelectedFramingPreset/SetSelectedAnglePreset), ein Storage-Reload pro Klick ist unnoetig
	// (PCT_Storage.GetLensPresetById-Kommentar: "vom Formular NICHT im Button-Bau-Hot-Path verwendet").
	protected PCT_LensPreset ResolveLensPresetById( string id ) {
		if ( !m_LensPresets || id == "" )
			return null;

		int count = m_LensPresets.Count();
		for ( int i = 0; i < count; i++ ) {
			PCT_LensPreset preset = m_LensPresets.Get( i );
			if ( preset && preset.id == id )
				return preset;
		}

		return null;
	}

	// Task-Brief O2-Fallback (Docs/Phase1_O1_SetFOV_Protokoll.md Nebenbefund, Docs/Plan_B6_Roadmap_Risiken.md
	// Section 5 O2): BoundingBox-/Bone-Motivhoehenmessung ist NICHT implementiert -- jedes gefundene Motiv
	// (Spieler UND generisches Objekt) erhaelt dieselbe konfigurierte Standardhoehe. m_Settings kann vor
	// OnMissionLoaded() bzw. auf einem dedizierten Server null sein (siehe m_Settings-Kommentar oben) --
	// Fallback auf den PCT_Settings-Klassendefault (1.8 m) ohne eine neue Instanz zu erzeugen.
	protected float ResolveDefaultSubjectHeightM() {
		if ( m_Settings )
			return m_Settings.defaultSubjectHeightM;

		return 1.8;
	}

	// Zielauswahl (Task-Brief Punkt 2 "Zielauswahl (Modul-seitig, 5_Mission)"): Raycast aus der
	// Kameramitte, identisches Muster zu Plan_B4 Section 3.3 (Autofokus) und dem verifizierten
	// Vanilla-Vorbild scripts - 1.29\5_Mission\DayZ\GUI\CameraTools\CameraToolsMenu.c:527-544
	// ("DayZPhysics.RaycastRV(from, to, contact_pos, contact_dir, contact_component, null, null, ev_obj)").
	// RaycastRV-Signatur exakt aus scripts - 1.29\3_Game\DayZ\Global\DayZPhysics.c:199 uebernommen (siehe
	// Task-P2-4-Report fuer die volle Signatur-Zitation); `results` wird HIER (anders als im
	// CameraToolsMenu-Vorbild) als echtes `set<Object>` uebergeben, weil das getroffene Objekt gebraucht
	// wird (Vorbild scripts - 1.29\4_World\DayZ\Entities\DayZPlayerImplementMeleeCombat.c:577/623:
	// "hitObjects.Count() > 0" / "hitObjects.Get(0)"). `new set<Object>()` ist hier unkritisch
	// (ereignisgetrieben per Knopfdruck, kein Per-Frame-Pfad, CLAUDE.md-Allokationsregel gilt fuer
	// OnUpdate/Tick).
	//
	// Treffer-Objekt-Regel (Task-Brief Punkt 2, woertlich): PlayerBase/DayZPlayer -> Fusspunkt
	// (Object.GetPosition(), fuer PlayerBase bereits die Bodenposition) + Hoehe-1.8-Fallback (siehe
	// ResolveDefaultSubjectHeightM-Kommentar -- AUCH fuer Spieler wird bewusst NICHT die tatsaechliche
	// Bone-Hoehe gemessen, O2 bleibt offen); generisches Objekt -> Treffpunkt selbst als Motiv-Fusspunkt
	// (kein verifizierter Bounding-Box-Zugriff, O2).
	protected bool ResolveAutoFramingTarget( out vector footPos, out float subjectHeightM ) {
		footPos = vector.Zero;
		subjectHeightM = 0.0;

		if ( !g_Game )
			return false;

		vector from = g_Game.GetCurrentCameraPosition();
		vector dir = g_Game.GetCurrentCameraDirection();
		vector to = from + dir * 9999.0;

		vector contactPos;
		vector contactDir;
		int contactComponent;
		set<Object> hitObjects = new set<Object>();

		bool hit = DayZPhysics.RaycastRV( from, to, contactPos, contactDir, contactComponent, hitObjects, NULL, NULL, false, false, ObjIntersectIFire );
		if ( !hit )
			return false;

		Object hitObject = null;
		if ( hitObjects.Count() > 0 )
			hitObject = hitObjects.Get( 0 );

		PlayerBase player = PlayerBase.Cast( hitObject );
		if ( player ) {
			footPos = player.GetPosition();
		} else {
			footPos = contactPos;
		}

		subjectHeightM = ResolveDefaultSubjectHeightM();
		return true;
	}

	// Framing-/Angle-Preset-Buttons (PCT_CinematicForm) rufen diese beiden Setter, die jeweils sofort
	// erneut framen -- "Auswahl wirkt sofort", identisches Verhalten zu den bestehenden Rig-/Overlay-Reglern
	// (siehe OnChange_Sensor/SetOverlay*-Kommentare oben).
	void SetSelectedFramingPreset( string id ) {
		m_SelectedFramingPresetId = id;
		ApplyAutoFraming();
	}

	void SetSelectedAnglePreset( string id ) {
		m_SelectedAnglePresetId = id;
		ApplyAutoFraming();
	}

	// Task-Brief Punkt 2 letzter Absatz ("Anwendung"): Kamera aktiv -> Position/Orientierung direkt setzen
	// (+ Rig-Brennweite, falls PCT_Framing eine empfiehlt); Kamera inaktiv -> Pending-Mechanik (identische
	// Wiederverwendung von m_PendingEnterPosition/-Orientation aus ApplyShot oben, konsumiert beim naechsten
	// Enter() ueber EnterAt()/Client_Enter()/Server_Enter()-Offline-Zweig).
	void ApplyAutoFraming() {
		PCT_FramingPreset framing = ResolveFramingPresetById( m_SelectedFramingPresetId );
		PCT_AnglePreset angle = ResolveAnglePresetById( m_SelectedAnglePresetId );

		if ( !framing || !angle ) {
			CF_Log.Warn( "PCT: ApplyAutoFraming -- Framing-/Angle-Preset nicht gefunden (framing='" + m_SelectedFramingPresetId + "', angle='" + m_SelectedAnglePresetId + "')." );
			return;
		}

		vector footPos;
		float subjectHeightM;
		bool hasTarget = ResolveAutoFramingTarget( footPos, subjectHeightM );

		if ( !hasTarget ) {
			Print( "[PCT] autoframe no target" );
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: kein Motiv gefunden." ) );
			return;
		}

		vector currentCameraPos = g_Game.GetCurrentCameraPosition();
		PCT_FramingResult result = PCT_Framing.ComputeFraming( footPos, subjectHeightM, framing, angle, m_CameraRig, currentCameraPos );

		if ( result.hasRecommendedFocalLength ) {
			// Review P2-5: physische Festbrennweite -- eine gebundene Prime-Linse hat keine variable
			// Brennweite, die Framing-Empfehlung darf sie deshalb nicht ueberschreiben; die Distanz-
			// Anwendung (Position/Orientierung unten) bleibt unveraendert, Framing erfolgt bei Primes
			// rein ueber die Distanz.
			PCT_LensPreset boundLens = ResolveLensPresetById( m_CameraRig.lensPresetId );
			bool keepsPrimeLens = m_CameraRig.lensPresetId != "" && boundLens && PCT_LensLibrary.IsPrime( boundLens );

			if ( keepsPrimeLens ) {
				Print( string.Format( "[PCT] autoframe keeps prime lens %1 — focal recommendation skipped", m_CameraRig.lensPresetId ) );
			} else {
				m_CameraRig.focalLengthMm = result.recommendedFocalLengthMm;
			}
		}

		if ( PCT_CinematicCamera.s_PCT_ActiveCamera ) {
			PCT_CinematicCamera.s_PCT_ActiveCamera.SetPosition( result.position );
			PCT_CinematicCamera.s_PCT_ActiveCamera.SetOrientation( result.orientation );
		} else {
			m_PendingEnterPosition = result.position;
			m_PendingEnterPositionSet = true;

			m_PendingEnterOrientation = result.orientation;
			m_PendingEnterOrientationSet = true;
		}

		Print( string.Format( "[PCT] autoframe applied %1+%2 dist=%3", framing.id, angle.id, result.distanceM.ToString() ) );

		PCT_CinematicForm form = PCT_CinematicForm.Cast( GetForm() );
		if ( form )
			form.RefreshRigWidgets();
	}

	// ===== Task P3-3: Playback-Steuerung (Plan_B7 Section 11.2/14.1) -- Client-only; der Player lebt
	// an der aktiven PCT-Kamera (stirbt mit ihr bei Leave/K4, kein eigener Cleanup noetig). UI-Buttons
	// folgen in P3-5; [PCT]-Logzeilen fuer Feldtest-Nachvollzug. =====

	void Playback_Start(PCT_Shot shot)
	{
		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if (!cam)
		{
			COTCreateLocalAdminNotification(new StringLocaliser("PCT: Erst Kamera betreten (Enter)"));
			Print("[PCT] playback start skipped -- no camera");
			return;
		}
		if (!shot)
		{
			Print("[PCT] playback start skipped -- no shot");
			return;
		}

		PCT_MotionPlayer player = new PCT_MotionPlayer();
		bool prepared = player.Prepare(shot);
		if (!prepared)
		{
			COTCreateLocalAdminNotification(new StringLocaliser("PCT: Shot hat keinen abspielbaren Kamerapfad (min. 2 Punkte)"));
			Print("[PCT] playback start skipped -- no playable camera path in shot " + shot.id);
			return;
		}

		cam.m_PCT_MotionPlayer = player;
		player.Play();
		Print("[PCT] playback started shot=" + shot.id + " duration=" + player.GetDuration().ToString(false));
	}

	void Playback_TogglePause()
	{
		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if (!cam || !cam.m_PCT_MotionPlayer)
			return;

		PCT_MotionPlayer player = cam.m_PCT_MotionPlayer;
		if (player.IsPlaying())
		{
			player.Pause();
			Print("[PCT] playback paused at t=" + player.GetTime().ToString(false));
		}
		else
		{
			player.Play();
			Print("[PCT] playback resumed at t=" + player.GetTime().ToString(false));
		}
	}

	void Playback_Stop()
	{
		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if (!cam || !cam.m_PCT_MotionPlayer)
			return;

		cam.m_PCT_MotionPlayer.Pause();
		cam.m_PCT_MotionPlayer.SeekTo(0.0);
		cam.m_PCT_MotionPlayer = null;
		Print("[PCT] playback stopped");
	}

	// Scrub: Zeit setzen und den Zustand EINMAL anwenden (Playback pausiert waehrend Scrub, Plan_B7
	// Section 11.2/B5 Section 5).
	void Playback_Seek(float time)
	{
		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if (!cam || !cam.m_PCT_MotionPlayer)
			return;

		PCT_MotionPlayer player = cam.m_PCT_MotionPlayer;
		player.Pause();
		player.SeekTo(time);
		player.ApplyAt(cam, player.GetTime(), 0.0);
		Print("[PCT] playback seek t=" + player.GetTime().ToString(false));
	}

	// ===== Task P3-5: Sequencer-Editing -- Arbeits-Shot (Kamerafahrt im Aufbau) + Keybind-Handler.
	// Der Arbeits-Shot lebt im Modul (client-only) und wird erst bei "Als Shot speichern" persistiert
	// (PCT_Storage.SaveShot); die UI-Sektion liegt in PCT_CinematicForm (Plan_B7 Section 15,
	// Editier-Gruppen KAMERAPFAD/BLICKPFAD; Spur-Key-Editing folgt mit dem B5-Rework). =====

	protected ref PCT_Shot m_EditShot;
	protected bool m_PathVizEnabled = false;			// Task P3-6 rendert; hier nur der Schalter

	PCT_Shot GetOrCreateEditShot()
	{
		if (!m_EditShot)
		{
			m_EditShot = new PCT_Shot();
			m_EditShot.name = "Neue Kamerafahrt";
		}
		return m_EditShot;
	}

	PCT_Shot GetEditShot()
	{
		return m_EditShot;
	}

	bool IsPathVizEnabled()
	{
		return m_PathVizEnabled;
	}

	void EditShot_Reset()
	{
		m_EditShot = null;
		Print("[PCT] edit shot reset");
	}

	void EditShot_AddCamPoint()
	{
		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if (!cam)
		{
			COTCreateLocalAdminNotification(new StringLocaliser("PCT: Erst Kamera betreten (Enter)"));
			Print("[PCT] add cam point skipped -- no camera");
			return;
		}

		PCT_Shot shot = GetOrCreateEditShot();
		PCT_PathPoint point = new PCT_PathPoint();
		point.position = cam.GetPosition();
		point.pathType = shot.cameraPath.curveDefault;

		shot.cameraPath.points.Insert(point);

		int count = shot.cameraPath.points.Count();
		Print("[PCT] cam point added #" + count.ToString() + " at " + point.position.ToString());
	}

	// Blickziel = Punkt unter der Bildmitte (Raycast wie Auto-Framing); ohne Treffer 25 m voraus.
	void EditShot_AddLookPoint()
	{
		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if (!cam)
		{
			COTCreateLocalAdminNotification(new StringLocaliser("PCT: Erst Kamera betreten (Enter)"));
			Print("[PCT] add look point skipped -- no camera");
			return;
		}

		vector aimPos = ResolveAimPoint();

		PCT_Shot shot = GetOrCreateEditShot();
		if (!shot.lookPath)
			shot.lookPath = new PCT_LookPath();

		PCT_LookPoint lookPoint = new PCT_LookPoint();
		lookPoint.worldTarget = aimPos;
		shot.lookPath.points.Insert(lookPoint);

		int count = shot.lookPath.points.Count();
		Print("[PCT] look point added #" + count.ToString() + " at " + aimPos.ToString());
	}

	// Blickziel unter der Bildmitte (Raycast wie Auto-Framing); ohne Treffer 25 m voraus.
	protected vector ResolveAimPoint()
	{
		vector from = g_Game.GetCurrentCameraPosition();
		vector dir = g_Game.GetCurrentCameraDirection();
		vector to = from + dir * 500.0;
		vector contactPos;
		vector contactDir;
		int contactComponent;
		set<Object> hitObjects = new set<Object>();
		bool hit = DayZPhysics.RaycastRV(from, to, contactPos, contactDir, contactComponent, hitObjects, NULL, NULL, false, false, ObjIntersectIFire);

		if (hit)
			return contactPos;

		vector fallback = from + dir * 25.0;
		return fallback;
	}

	// ===== Task P3-7: Bewegungs-Preset-Generatoren (PCT_MotionPresets) -- jeder Aufruf erzeugt einen
	// FRISCHEN Arbeits-Shot aus dem aktuellen Kamerazustand (vorhersehbares Verhalten; danach im
	// Sequencer-Panel editier-/abspiel-/speicherbar). =====

	protected bool BeginPresetShot(string presetName, out vector camPos, out vector dirFlatNorm)
	{
		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if (!cam)
		{
			COTCreateLocalAdminNotification(new StringLocaliser("PCT: Erst Kamera betreten (Enter)"));
			return false;
		}

		camPos = cam.GetPosition();

		vector dir = g_Game.GetCurrentCameraDirection();
		dirFlatNorm = dir;
		dirFlatNorm[1] = 0.0;
		float flatLen = dirFlatNorm.Length();
		if (flatLen < 0.01)
		{
			dirFlatNorm = "0 0 1";
		}
		else
		{
			float invLen = 1.0 / flatLen;
			dirFlatNorm = dirFlatNorm * invLen;
		}

		m_EditShot = new PCT_Shot();
		m_EditShot.name = presetName;
		return true;
	}

	void EditShot_PresetDollyIn()
	{
		vector camPos;
		vector dirFlat;
		if (!BeginPresetShot("Dolly In", camPos, dirFlat))
			return;
		PCT_MotionPresets.GenerateDolly(m_EditShot, camPos, dirFlat, 5.0, 5.0);
		Print("[PCT] preset dolly-in generated (5 m / 5 s)");
	}

	void EditShot_PresetDollyOut()
	{
		vector camPos;
		vector dirFlat;
		if (!BeginPresetShot("Dolly Out", camPos, dirFlat))
			return;
		vector dirBack = dirFlat * -1.0;
		PCT_MotionPresets.GenerateDolly(m_EditShot, camPos, dirBack, 5.0, 5.0);
		Print("[PCT] preset dolly-out generated (5 m / 5 s)");
	}

	void EditShot_PresetTruckRight()
	{
		vector camPos;
		vector dirFlat;
		if (!BeginPresetShot("Truck Right", camPos, dirFlat))
			return;
		// Rechtsvektor in der XZ-Ebene: (x,z) -> (z,-x); Probe: Blick nach Norden (0,0,1) -> Osten (1,0,0).
		vector rightDir = "0 0 0";
		float rightX = dirFlat[2];
		float rightZ = -dirFlat[0];
		rightDir[0] = rightX;
		rightDir[2] = rightZ;
		PCT_MotionPresets.GenerateDolly(m_EditShot, camPos, rightDir, 5.0, 5.0);
		Print("[PCT] preset truck-right generated (5 m / 5 s)");
	}

	void EditShot_PresetOrbit()
	{
		vector camPos;
		vector dirFlat;
		if (!BeginPresetShot("Orbit 90", camPos, dirFlat))
			return;
		vector center = ResolveAimPoint();
		PCT_MotionPresets.GenerateOrbit(m_EditShot, camPos, center, 90.0, 8.0, 8);
		Print("[PCT] preset orbit generated (90 deg / 8 s um " + center.ToString() + ")");
	}

	void EditShot_PresetDollyZoom()
	{
		vector camPos;
		vector dirFlat;
		if (!BeginPresetShot("Dolly-Zoom", camPos, dirFlat))
			return;
		vector target = ResolveAimPoint();
		vector toTarget = target - camPos;
		float dist = toTarget.Length();
		float moveDist = dist * 0.5;
		float startFocal = m_CameraRig.focalLengthMm;
		PCT_MotionPresets.GenerateDollyZoom(m_EditShot, camPos, target, moveDist, 5.0, startFocal);
		Print("[PCT] preset dolly-zoom generated (dist " + dist.ToString(false) + " m, move " + moveDist.ToString(false) + " m)");
	}

	void EditShot_RemoveCamPoint(int index)
	{
		if (!m_EditShot)
			return;
		if (index < 0 || index >= m_EditShot.cameraPath.points.Count())
			return;
		m_EditShot.cameraPath.points.RemoveOrdered(index);
		Print("[PCT] cam point removed #" + index.ToString());
	}

	void EditShot_RemoveLookPoint(int index)
	{
		if (!m_EditShot)
			return;
		if (!m_EditShot.lookPath)
			return;
		if (index < 0 || index >= m_EditShot.lookPath.points.Count())
			return;
		m_EditShot.lookPath.points.RemoveOrdered(index);
		Print("[PCT] look point removed #" + index.ToString());
	}

	void EditShot_Play()
	{
		if (!m_EditShot)
		{
			COTCreateLocalAdminNotification(new StringLocaliser("PCT: Keine Kamerafahrt im Aufbau (Punkte hinzufuegen)"));
			return;
		}
		Playback_Start(m_EditShot);
	}

	void EditShot_Save()
	{
		if (!m_EditShot || m_EditShot.cameraPath.points.Count() < 2)
		{
			COTCreateLocalAdminNotification(new StringLocaliser("PCT: Kamerafahrt braucht min. 2 Kamerapunkte"));
			return;
		}

		string enteredName = m_EditShot.name;
		if (m_EditShot.id == "")
			m_EditShot.id = GenerateShotId();

		// Rig-Snapshot als Kompat-Kontext des Shots (Sensor/Brennweite fuer Export/SampleKeyframe).
		m_EditShot.cameraRig = m_CameraRig.Clone();

		bool saved = PCT_Storage.SaveShot(m_EditShot);
		if (saved)
		{
			Print("[PCT] edit shot saved " + m_EditShot.id + " ('" + enteredName + "')");
			COTCreateLocalAdminNotification(new StringLocaliser("PCT: Kamerafahrt gespeichert: " + m_EditShot.id));
		}
		else
		{
			CF_Log.Warn("PCT: EditShot_Save fehlgeschlagen fuer '" + m_EditShot.id + "'.");
		}
	}

	// Scrub-Ziel: aktiver Player; ohne Player wird er fuer den Arbeits-Shot pausiert vorbereitet.
	void Playback_SeekNormalized(float frac)
	{
		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if (!cam)
			return;

		if (!cam.m_PCT_MotionPlayer)
		{
			if (!m_EditShot)
				return;

			PCT_MotionPlayer player = new PCT_MotionPlayer();
			bool prepared = player.Prepare(m_EditShot);
			if (!prepared)
				return;
			cam.m_PCT_MotionPlayer = player;
		}

		PCT_MotionPlayer active = cam.m_PCT_MotionPlayer;
		active.Pause();
		float clampedFrac = Math.Clamp(frac, 0.0, 1.0);
		float targetTime = clampedFrac * active.GetDuration();
		active.SeekTo(targetTime);
		active.ApplyAt(cam, active.GetTime(), 0.0);
	}

	// ===== Task P3-5: Keybind-Handler (Signatur-Muster wie Input_ToggleCamera, LocalPress-Guard) =====

	void Input_AddCamPoint(UAInput input)
	{
		if (!input.LocalPress())
			return;
		Print("[PCT] add cam point via keybind");
		EditShot_AddCamPoint();
	}

	void Input_AddLookPoint(UAInput input)
	{
		if (!input.LocalPress())
			return;
		Print("[PCT] add look point via keybind");
		EditShot_AddLookPoint();
	}

	void Input_PlayPause(UAInput input)
	{
		if (!input.LocalPress())
			return;
		Print("[PCT] play/pause via keybind");

		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if (cam && cam.m_PCT_MotionPlayer)
		{
			Playback_TogglePause();
			return;
		}
		EditShot_Play();
	}

	void Input_TogglePathViz(UAInput input)
	{
		if (!input.LocalPress())
			return;
		m_PathVizEnabled = !m_PathVizEnabled;
		string vizState = "off";
		if (m_PathVizEnabled)
			vizState = "on";
		Print("[PCT] path viz toggled -> " + vizState);
	}

	// Task P3-6: pro Frame aus OnUpdate (nur bei aktiver PCT-Kamera erreicht). Zeichnet den
	// Arbeits-Shot; ausgeschaltet bzw. ohne Punkte wird das zuletzt Gezeichnete einmalig geloescht.
	protected bool m_PathVizWasDrawn = false;

	protected void UpdatePathViz()
	{
		bool wantsDraw = false;
		if (m_PathVizEnabled && m_EditShot)
			wantsDraw = true;

		if (!wantsDraw)
		{
			if (m_PathVizWasDrawn && m_OverlayHud)
			{
				m_OverlayHud.ClearPathViz();
			}
			m_PathVizWasDrawn = false;
			return;
		}

		EnsureOverlayHud();
		if (!m_OverlayHud)
			return;

		m_OverlayHud.DrawPathViz(m_EditShot);
		m_PathVizWasDrawn = true;
	}

	// ===== P1-2: O1/V1-Diagnose-Panel (Plan_B4 Section 2.5, Plan_B6 Section 5) =====
	// Testet, ob Camera.SetFOV() (scripts-1.29 3_Game/DayZ/Entities/Camera.c:67, Instanzmethode,
	// FOV in Radiant) ausserhalb der User-Options-Grenzen (0.752-1.303 rad) tatsaechlich clampt.
	// Rein client-seitig -- SetFOV ist eine reine Kamera-Rendering-Einstellung ohne Serverbezug,
	// deshalb kein RPC-Pfad noetig (Abweichung von Enter/Leave, die beide ueber RPC laufen).
	//
	// Task P1-7: UI-Anbindung entfernt nach O1-Abschluss, siehe Docs/Phase1_O1_SetFOV_Protokoll.md
	// (O1/V1 sind beantwortet: SetFOV clampt nicht). Die 8 Roh-FOV-Testbuttons und der "Log Session
	// Header"-Button wurden aus PCT_CinematicForm.OnInit entfernt; die folgenden Funktionen bleiben
	// als dokumentierte Diagnose-API fuer kuenftige Regressionstests erhalten und sind weiterhin per
	// Debug-Konsole/Script aufrufbar (RunFovProbe/LogFovProbeDelayed/LogO1SessionHeader).

	// Gemeinsame Kamera-Aufloesung fuer RunFovProbe/LogFovProbeDelayed UND fuer die Live-Anzeige in
	// PCT_CinematicForm (Prioritaet exakt wie im Task-Brief): bevorzugt die eigene
	// s_PCT_ActiveCamera, sonst Camera.GetCurrentCamera() (Camera.c:7 -- "Returns active Camera
	// instance (note: player's camera is not Camera instance - thus it return null)"). static,
	// damit die Form sie ohne Modul-Instanz aufrufen kann (analog zum bereits statischen Zugriff
	// auf PCT_CinematicCamera.s_PCT_ActiveCamera).
	static Camera ResolveActiveCamera() {
		if ( PCT_CinematicCamera.s_PCT_ActiveCamera )
			return PCT_CinematicCamera.s_PCT_ActiveCamera;

		return Camera.GetCurrentCamera();
	}

	// SetFOV-Testreihe (Anforderung 2 im Task-Brief). Sofortiges Zuruecklesen ueber
	// Camera.GetCurrentFOV() (Camera.c:13, static, "Returns FOV of current camera object") --
	// es gibt keine Instanzmethode GetFOV() auf Camera, verifiziert in Camera.c. Verzoegertes
	// Zuruecklesen nach 1000ms per CallLater, Spiegel des Musters aus Client_Check_Leave oben
	// (g_Game.GetCallQueue Null-Check gemaess CLAUDE.md).
	void RunFovProbe( float targetRad ) {
		Camera cam = ResolveActiveCamera();
		if ( !cam ) {
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: Erst Kamera betreten (Enter)" ) );
			Print( "[PCT_O1] probe skipped — no camera" );
			return;
		}

		// P1-4/FOV-Hold: gesetzte FOV-Werte springen im Feldtest sofort wieder auf ~1.0
		// zurueck (fremder Schreiber, vermutlich COTs Modul-Lerp auf der Freecam). Nur auf
		// der eigenen PCT-Kamera moeglich/gewollt (identisch mit der aufgeloesten
		// Probe-Kamera) -- auf einer fremden COT-Kamera bleibt das Verhalten bewusst
		// unveraendert, da m_PCT_HoldFovRad dort nicht existiert.
		if ( PCT_CinematicCamera.s_PCT_ActiveCamera && PCT_CinematicCamera.s_PCT_ActiveCamera == cam ) {
			PCT_CinematicCamera.s_PCT_ActiveCamera.m_PCT_HoldFovRad = targetRad;
			Print( string.Format( "[PCT_O1] hold engaged target=%1", targetRad.ToString( false ) ) );
		}

		cam.SetFOV( targetRad );

		float immediate = Camera.GetCurrentFOV();
		Print( string.Format( "[PCT_O1] probe target=%1 immediate=%2 cam=%3", targetRad.ToString( false ), immediate.ToString( false ), cam.ClassName() ) );

		ScriptCallQueue callQueue = g_Game.GetCallQueue( CALL_CATEGORY_SYSTEM );
		if ( !callQueue )
			return;

		callQueue.CallLater( LogFovProbeDelayed, 1000, false, targetRad );
	}

	// Kamera-Referenz wird HIER erneut ueber ResolveActiveCamera() aufgeloest statt aus dem
	// RunFovProbe-Aufruf uebernommen -- die PCT-Kamera kann in der Zwischenzeit per Leave()
	// verlassen worden sein (SetActive(false) + g_Game.ObjectDeleteOnClient(), siehe Client_Leave
	// oben), die alte Referenz waere dann ein haengender Zeiger auf ein geloeschtes Objekt.
	void LogFovProbeDelayed( float targetRad ) {
		Camera cam = ResolveActiveCamera();
		if ( !cam ) {
			Print( string.Format( "[PCT_O1] probe target=%1 after1s=SKIPPED camera gone", targetRad.ToString( false ) ) );
			return;
		}

		float after1s = Camera.GetCurrentFOV();
		Print( string.Format( "[PCT_O1] probe target=%1 after1s=%2 cam=%3", targetRad.ToString( false ), after1s.ToString( false ), cam.ClassName() ) );
	}

	// Button "Log Session Header" (Anforderung 2.6). Werte aus PCT_Constants.FOV_OPTIONS_MIN/MAX
	// (gameplay.c:1316-1317, dort auskommentiert) -- reine Log-Orientierung fuer die manuelle
	// O1-Auswertung, PCT liest die echten User-Options nicht aus.
	void LogO1SessionHeader() {
		Print( "[PCT_O1] === session start | options-min=0.75242724772 options-max=1.30322025726 ===" );
	}

	// ===== Task P2-6: Export (Docs/Plan_B3_Datenmodell_Persistenz.md Section 8, Docs/Plan_B6_Roadmap_Risiken.md
	// Section 1 Phase 2 "PCT_Export.c ... CSV ueber OpenFile/FPrintln ...", "MakeScreenshot(name) -> DDS",
	// Permission "CinematicTool.Export" gate) =====
	// Duenne Modul-Wrapper (identisches Muster wie DeleteShotById/RenameShotDisplayName oben): laedt die
	// Shots ueber PCT_Storage, loest den CSV-Delimiter aus m_Settings auf (COT-frei -- PCT_Export.c selbst
	// kennt weder Settings noch Permissions, siehe dortiger Datei-Kopfkommentar) und delegiert die eigentliche
	// Datei-Erzeugung an PCT_Export (3_Game). Rueckgabe wie PCT_Export selbst: Pfad bei Erfolg, "" bei Fehler
	// (PCT_Export loggt den technischen Fehler bereits per CF_Log.Warn -- hier nur der Erfolgs-Log gemaess
	// Task-Brief-Logzeilen-Vorgabe "export csv <pfad>"/"export json <pfad>"). Aufrufer: PCT_CinematicForm
	// OnClick_ExportCsv/OnClick_ExportJson (Permission-Check dort VOR diesem Aufruf, identisches Muster zu
	// OnClick_SaveShot/"CinematicTool.View").
	string ExportShotListCsv() {
		array< ref PCT_Shot > shots = new array< ref PCT_Shot >();
		PCT_Storage.LoadAllShots( shots );

		string delimiter = ";";
		if ( m_Settings && m_Settings.csvDelimiter != "" )
			delimiter = m_Settings.csvDelimiter;

		string filePath = PCT_Export.ExportShotListCsv( shots, delimiter );
		if ( filePath != "" )
			Print( "[PCT] export csv " + filePath );

		return filePath;
	}

	string ExportShotListJson() {
		array< ref PCT_Shot > shots = new array< ref PCT_Shot >();
		PCT_Storage.LoadAllShots( shots );

		string filePath = PCT_Export.ExportShotListJson( shots );
		if ( filePath != "" )
			Print( "[PCT] export json " + filePath );

		return filePath;
	}

	// Screenshot-Namenskonvention/Take-Zaehler-Aufbau liegt in PCT_Export.ResolveScreenshotBasePath (3_Game,
	// Plan_B3 Section 8.3-Muster) -- diese Methode ruft nur noch MakeScreenshot selbst auf (Task-Brief-Vorgabe:
	// "Modul-Funktion CaptureShotThumbnail(string shotName) -> MakeScreenshot(...)"). MakeScreenshot
	// (proto.c:142) ist void -- kein Erfolgs-/Fehlersignal aus der Engine, daher wird hier IMMER der erwartete
	// Pfad zurueckgegeben (".dds"-Anhaengung an einen "$"-Pfad ist laut Docstring plausibel, aber NICHT im
	// Spiel gegengetestet -- Plan_B6 Section 5 O11, siehe Task-Report "Bedenken"). shotName == "" faellt auf
	// "shot" zurueck (PCT_Export.ResolveScreenshotBasePath), damit ein Klick ohne ausgefuelltes Namensfeld
	// nicht mit einem leeren Dateinamen fehlschlaegt.
	string CaptureShotThumbnail( string shotName ) {
		string baseName = PCT_Export.ResolveScreenshotBasePath( shotName );

		MakeScreenshot( baseName );

		string resultPath = baseName + ".dds";
		Print( "[PCT] screenshot " + shotName );

		return resultPath;
	}
}
