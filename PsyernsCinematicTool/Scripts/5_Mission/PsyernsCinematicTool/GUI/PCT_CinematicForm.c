class PCT_CinematicForm: JMFormBase
{
	protected PCT_CinematicModule m_Module;

	protected UIActionButton m_EnterLeaveButton;

	// P1-3: pct_panel (pct_cinematic_form.layout) ist eine feste, NICHT scrollende FrameWidgetClass
	// (size 1 1 / hexactsize 0 vexactsize 0 -> exakt 500px hoch, siehe Layout). Ohne Scroller wird
	// Inhalt, der die Panelhoehe ueberschreitet (urspruenglich P1-3: Enter/Leave-Button + Live-FOV-
	// Text + 2x4-Button-Grid + Session-Header-Button; seit P1-7: Enter/Leave-Button + Live-FOV-Text +
	// Rig-Panel SelectionBox/Slider, siehe OnInit), fuer Maus-Events unerreichbar -- exakt der
	// O1-Feldbefund (nur die zuletzt erzeugten 4 Buttons klickbar). Fix: COTs eigenes Muster fuer genau dieses Problem
	// uebernehmen -- JMCameraForm.OnInit (JMCameraForm.c:87-88: UIActionScroller ueber "panel" +
	// GetContentWidget() als Eltern-Widget fuer ALLE Inhalte) bzw. JMExampleForm.OnInit
	// (JMExampleForm.c:61-62, identisches Muster). Beleg, dass die Scroller-Huelle die eigentliche
	// Loesung ist, nicht die Panel-Pixelhoehe: JMCameraForm nutzt camera_form.layout mit nur 600x275
	// (JM/COT/GUI/layouts/camera_form.layout:4) -- kleiner als PCTs 600x500 -- und traegt trotzdem
	// zwei 7-Zeilen-Slider-Grids dank Scroller. pct_cinematic_form.layout (600x500) muss deshalb
	// NICHT vergroessert werden; die Layout-Groesse passt bereits zum COT-Referenzmuster.
	protected UIActionScroller m_Scroller;

	// P1-2 (O1/V1-Diagnose): Live-FOV-Anzeige, siehe RefreshFovDisplay(). Vollformat-Sensorhoehe
	// fuer die mm-Aequivalent-Berechnung (focalMm = 24.0 / (2.0 * tan(fovRad / 2.0))), Anforderung 1
	// im Task-Brief.
	protected UIActionText m_FovText;
	protected const float SENSOR_HEIGHT_MM = 24.0;

	// Throttle fuer RefreshFovDisplay() auf ~4x/s (Anforderung 1) -- Update() feuert pro
	// engine-Frame (siehe Fundstelle unten), g_Game.GetTickTime() liefert Sekunden seit Spielstart
	// (Game.c:913, proto native float GetTickTime()).
	protected const float FOV_DISPLAY_INTERVAL_S = 0.25;
	protected float m_LastFovDisplayUpdate;

	// Task P1-7 (Rig-Panel, Docs/Plan_B5_UI_UX.md Section 2.1 Phase-1-Zeilen "Sensor-Preset" /
	// "Brennweite (mm)" / "Fokusdistanz (m)"; Blende siehe ApplySensorPreset/OnChange_Aperture-
	// Kommentare unten). Alle vier Widgets editieren m_Module.m_CameraRig direkt -- die aktive Kamera
	// haelt eine Referenz auf DASSELBE Objekt (m_PCT_Rig, siehe PCT_CinematicModule.Client_Enter/
	// Server_Enter), Aenderungen wirken daher ohne Apply-Button sofort im naechsten Kamera-Frame
	// (PCT_CinematicCamera.OnUpdate liest das Rig jeden Frame, Task P1-6).
	protected UIActionSelectBox m_SensorSelectBox;
	protected UIActionSlider m_FocalLengthSlider;
	protected UIActionSlider m_ApertureSlider;
	protected UIActionSlider m_FocusDistanceSlider;

	// ===== Task P2-5 (Docs/Plan_B8_Objektivsystem_LensKits.md Section 6): Lens-Kit-Panel, direkt unter
	// der Sensor-Zeile (siehe OnInit). m_LensButtonToLens haelt das RESOLVED PCT_LensPreset-Objekt direkt
	// (nicht nur die id) -- vermeidet einen Storage-Reload je Klick (Abweichung/Konsistenz-Entscheidung,
	// siehe Modul-Feldkommentar m_LensPresets/m_LensKits). m_LensButtonRows wird bei jedem Kit-/Einzellinsen-
	// Wechsel per Unlink()+Neubau ersetzt (identisches Muster zu m_ShotListRows/RebuildShotList). =====
	protected UIActionSelectBox m_LensKitSelectBox;
	protected Widget m_LensSectionParent;
	protected Widget m_LensButtonRows;
	protected ref map< UIActionBase, ref PCT_LensPreset > m_LensButtonToLens;
	protected UIActionText m_LensInfoText;
	protected UIActionText m_LensCharacterText;
	protected UIActionText m_LensCoverageHintText;
	// Zuletzt in der Kit-SelectionBox BROWSED (nicht zwingend gebundenes) Kit -- treibt nur die
	// Charakter-Zeile; Prime-Lock/Blenden-Grenzen/Info-Zeile haengen am tatsaechlich GEBUNDENEN
	// rig.lensPresetId (siehe RefreshLensStateWidgets), nicht an dieser Browse-Auswahl.
	protected PCT_LensKit m_SelectedLensKit;

	// Task P1-8 (Docs/Plan_B5_UI_UX.md Section 4; Architektur-Vorgabe Task-Brief Punkt 4:
	// "Form-Toggles: je Overlay einzeln zu-/abschaltbar (Checkboxen) + Masken-SelectionBox"). Beide
	// Checkboxen und die SelectBox editieren m_Module direkt (SetOverlayThirdsVisible/
	// SetOverlayCenterVisible/SetOverlayAspectMask) -- "Auswahl wirkt sofort", gleiches Muster wie die
	// Rig-Panel-Handler oben (kein Apply-Button).
	protected UIActionCheckbox m_OverlayThirdsCheckbox;
	protected UIActionCheckbox m_OverlayCenterCheckbox;
	protected UIActionSelectBox m_AspectMaskSelectBox;

	// ===== Task P2-3: Shot-Browser (Docs/Plan_B5_UI_UX.md Section 2.2/6, Docs/Plan_B6_Roadmap_Risiken.md
	// Section 1 Phase 2 "Muster JMCameraForm-Positionsliste, Anwenden/Loeschen/Umbenennen") =====
	// EIN gemeinsames Namensfeld fuer "Shot speichern" (name-Parameter von CaptureCurrentShot) UND
	// "Umbenennen" (aktualisiert PCT_Shot.name der Zeile) -- B5 Section 2.2 nennt nur "Shot-Name" als
	// einzelnes Textfeld, keine getrennten Rename-/Save-Felder.
	protected UIActionEditableText m_ShotNameField;
	protected UIActionButton m_SaveShotButton;

	// Elternwidget fuer die Zeilenliste = derselbe Scroller-Content wie alle anderen Widgets dieses Forms
	// (m_Scroller aus OnInit, siehe P1-3-Kommentar dort zur Scroller-Huelle) -- kein zweiter, eigener
	// Scroller noetig (B5 Section 9 erlaubt reine UIActionManager-Komposition; der aeussere Scroller traegt
	// bereits beliebig hohen Inhalt, s. Klassenkommentar oben).
	protected Widget m_ShotsSectionParent;

	// Widget-Lifecycle (Selbstpruefung Punkt 3): UIActionManager.CreateActionRows() erzeugt ein
	// "UIActionContentRows"-Widget mit 10 fest im Layout definierten Unterknoten Content_Row_00..Content_Row_09
	// (je bis zu 100 Zeilen, JM/COT/GUI/layouts/uiactions/UIActionContentRows.layout) -- verifiziertes COT-
	// Muster fuer dynamische Listen (JMESPForm.ESPFilters/JMPlayerForm.ShowPermissions/ShowRoles). Rebuild ohne
	// Leaks: VOR jedem Neuaufbau `m_ShotListRows.Unlink()` (zerstoert das komplette Widget samt aller
	// Zeilen-Kinder in einem Schritt), exakt das Muster aus JMPlayerForm.HidePermissions/HideRoles
	// ("m_PermissionsRows.Unlink();" vor "m_PermissionsRows = UIActionManager.CreateActionRows(...)").
	protected Widget m_ShotListRows;

	// ===== Task P3-5: Sequencer-Sektion (Kamerafahrt im Aufbau, Plan_B7 Section 15 -- Editier-Gruppen
	// KAMERAPFAD/BLICKPFAD als Punktlisten; Spur-Key-Editing folgt mit dem B5-Rework). Gleiche
	// Lifecycle-Muster wie der Shot-Browser (CreateActionRows + Unlink vor Neuaufbau). =====
	protected UIActionEditableText m_SeqNameField;
	protected UIActionSlider m_SeqScrubSlider;
	protected UIActionText m_SeqStatusText;
	protected Widget m_SeqSectionParent;
	protected Widget m_SeqCamRows;
	protected Widget m_SeqLookRows;
	protected ref map< UIActionBase, int > m_SeqCamRowToIndex;
	protected ref map< UIActionBase, int > m_SeqLookRowToIndex;

	// Zeilen-Buttons sind reine UIActionManager-Buttons mit `this` als Callback-Instanz (kein eigenes
	// Zeilen-Widget-Script, B5 Section 9 "reine UIActionManager-Komposition zulaessig") -- ein Klick liefert
	// dem Handler nur das geklickte UIActionButton selbst (`action`-Parameter), keinen Zeilen-Kontext. Diese
	// Map loest genau das: pro erzeugtem Zeilen-Button wird seine Shot-id hinterlegt (ResolveShotRowId liest
	// sie zurueck). UIActionBase.m_Instance ist NICHT `ref` (UIActionBase.c) -- die Buttons selbst haelt daher
	// bereits das Widget-Baum-Unlink am Leben/entfernt sie, diese Map haelt nur die Zuordnung, keine
	// zusaetzlichen Objektreferenzen, die sonst leaken koennten.
	protected ref map< UIActionBase, string > m_ShotRowActionToId;

	// Cache der zuletzt geladenen Shot-Liste (PCT_Storage.LoadAllShots) -- ausschliesslich fuer
	// RebuildShotList()/BuildShotRow() gehalten, kein Formular-weiter Sekundaerzustand.
	protected ref array< ref PCT_Shot > m_Shots;

	// Merker zwischen Click_ShotRowDelete (oeffnet die Bestaetigung) und ConfirmDeleteShot (Bestaetigungs-
	// Callback ohne Parameter, Muster JMTeleportForm.Click_RemoveLocation/RemoveLocation_Confirmed) -- die
	// Bestaetigungs-Callback-Signatur traegt keinen eigenen Payload, der Ziel-Shot muss daher zwischengespeichert
	// werden (anders als JMTeleportForm, das eine einzelne TextListboxWidget-Selektion hat; PCT hat pro Zeile
	// eigene Buttons statt einer Listenauswahl).
	protected string m_PendingDeleteShotId = "";

	// Reihenfolge/Bezeichner 1:1 aus dem Task-Brief ("2.39:1, 16:9, 4:3, 9:16, 1:1 (Maske 'aus' =
	// keine)"); Index 0 = "Aus" -> PCT_OverlayHud.AspectRatioForMask() liefert dafuer 0.0 (kein
	// Maskenbalken). Array-Literal-Syntax wie m_SensorPresetLabels oben.
	protected ref array< string > m_AspectMaskLabels = {
		"Aus",
		"2.39:1",
		"16:9",
		"4:3",
		"9:16",
		"1:1"
	};

	// Sensor-Presets als statische Liste im Code (Phase 1) -- Preset-DATEIEN (Presets\*.json,
	// Plan_B3_Datenmodell_Persistenz.md) kommen erst Phase 2. Anzeigetexte fuer die 6 Presets aus
	// Plan_B4_Kamera_Engine.md Section 2.3 (Reihenfolge = Tabellenreihenfolge dort); die zugehoerigen
	// sensorPresetId/Breite/Hoehe-Werte stehen in ApplySensorPreset (unten), Indizes 1:1 zu dieser
	// Liste. Array-Literal-Syntax fuer array<string> verifiziert gegen COT-Vorbild
	// (JMCameraForm.c:6-10, m_SelectBoxText).
	protected ref array< string > m_SensorPresetLabels = {
		"Full Frame",
		"Super 35",
		"APS-C",
		"MFT",
		"1 Inch",
		"Smartphone"
	};

	// ===== Task P2-4: Brennweiten-Preset-Reihe (Docs/Plan_B4_Kamera_Engine.md Section 2.4, Docs/
	// Plan_B5_UI_UX.md Section 2.1 "Brennweiten-Preset"-Zeile -- laut B5 Phase 2, Task-Brief zieht sie als
	// Button-Reihe vor: "jetzt mit umsetzen als Button-Reihe, die focalLengthMm setzt"). Zwei parallele
	// Arrays (Label/Wert) statt Laufzeit-float->string-Formatierung -- vermeidet jede Unsicherheit ueber das
	// exakte Nachkommastellen-Verhalten von float.ToString() (EnConvert.c:115, "simple"-Parameter nicht
	// naeher spezifiziert) fuer die 15 fest bekannten B4-Section-2.4-Werte.
	protected ref array< string > m_FocalLengthPresetLabels = {
		"14", "18", "21", "24", "28", "32", "35", "40", "50", "65", "75", "85", "100", "135", "200"
	};
	protected ref array< float > m_FocalLengthPresetValues = {
		14.0, 18.0, 21.0, 24.0, 28.0, 32.0, 35.0, 40.0, 50.0, 65.0, 75.0, 85.0, 100.0, 135.0, 200.0
	};

	// Button->Wert/-id-Zuordnung, identisches Muster zu m_ShotRowActionToId (s. u.): ein Klick liefert dem
	// Handler nur das geklickte UIActionButton, keinen Zeilen-/Index-Kontext.
	protected ref map< UIActionBase, float > m_FocalButtonToValue;
	protected ref map< UIActionBase, string > m_FramingButtonToId;
	protected ref map< UIActionBase, string > m_AngleButtonToId;

	// Kompakter Rueckmeldetext ueber das zuletzt angewendete Framing-/Angle-Preset-Paar (Task-Brief:
	// Auto-Framing-Buttons sollen sofort wirken -- dieselbe "Auswahl wirkt sofort"-Erwartung wie bei den
	// uebrigen Reglern, hier zusaetzlich sichtbar rueckgemeldet, analog m_FovText oben).
	protected UIActionText m_AutoFramingStatusText;

	// ===== Task P2-6 (Docs/Plan_B3_Datenmodell_Persistenz.md Section 8, Docs/Plan_B5_UI_UX.md, Docs/
	// Plan_B6_Roadmap_Risiken.md Section 1 Phase 2): Export-Sektion. Alle drei Buttons sind PERMISSION-GATED
	// auf "CinematicTool.Export" (Task-Brief) -- Gate-Mechanik ist das verifizierte COT-Bordmittel
	// UIActionBase.UpdatePermission(string) (DayZ-CommunityOnlineTools-production/JM/COT/Scripts/5_Mission/
	// CommunityOnlineTools/gui/Actions/UIActionBase.c:181-184: "void UpdatePermission(string permission) {
	// SetEnabled(GetPermissionsManager().HasPermission(permission)); }" -- SetEnabled toggelt Enable()/
	// Disable(), zeigt bei fehlender Permission das "action_wrapper_disable"-Overlay UND deaktiviert den
	// Klick (layoutRoot.Enable(false), UIActionBase.c:197-202)). Aufgerufen in OnClientPermissionsUpdated()
	// (siehe dort) -- identisches Muster zu COTs eigenem JMPlayerForm.OnClientPermissionsUpdated
	// (UpdatePermission(m_HealPlayer, "Admin.Player.Heal") usw., JMPlayerForm.c:157ff), das denselben
	// privaten Wrapper (UIActionBase base, string permission) nutzt wie unten UpdatePermissionSafe().
	protected UIActionButton m_ExportCsvButton;
	protected UIActionButton m_ExportJsonButton;
	protected UIActionButton m_ExportScreenshotButton;

	protected override bool SetModule( JMRenderableModuleBase mdl ) {
		return Class.CastTo( m_Module, mdl );
	}

	// Widget-Erzeugung nach dem Muster von JMExampleForm.OnInit / JMCameraForm.OnInit. P1-3-Korrektur:
	// beide Referenzformen erzeugen NICHT direkt auf layoutRoot.FindAnyWidget("panel"), sondern
	// wrappen zuerst per UIActionManager.CreateScroller(...) + GetContentWidget() (JMCameraForm.c:
	// 87-88, JMExampleForm.c:61-62) -- genau dieser Schritt fehlte urspruenglich und war die Ursache
	// des O1-Befunds (nur die letzten 4 der 8 FOV-Testbuttons erreichbar, siehe Klassenkommentar
	// oben). Enter/Leave-Button unveraendert inhaltlich (P1-1, nicht anfassen), haengt aber wie alle
	// anderen Widgets am Scroller-Content statt direkt am Panel. Live-FOV-Text unveraendert (P1-2,
	// zeigt jetzt faktisch das Rig-FOV, siehe RefreshFovDisplay).
	//
	// Task P1-7: die 8 SetFOV-Testbuttons (2x4 GridSpacer) und der "Log Session Header"-Button aus
	// P1-2 sind entfernt (O1/V1 beantwortet, siehe Docs/Phase1_O1_SetFOV_Protokoll.md) -- an ihrer
	// Stelle das Rig-Panel: Sensor-SelectionBox, Brennweiten-/Blenden-/Fokusdistanz-Slider (B5 Section
	// 2.1 Phase-1-Zeilen, siehe Feld-Kommentare oben und Handler-Kommentare unten).
	// UIActionManager.CreateSelectionBox/CreateSlider-Signaturen verifiziert in UIActionManager.c:
	// 438/463; Callback-Signatur (UIEvent eid, UIActionBase action) verifiziert ueber
	// UIActionBase.CallEvent (UIActionBase.c:343-352, Param2<UIEvent,UIActionBase>) -- identisch fuer
	// SelectBox (UIActionSelectBox.OnSelectionChange -> CallEvent(UIEvent.CHANGE), UIActionSelectBox.c:
	// 66-72) und Slider (UIActionSlider.OnChange -> CallEvent(UIEvent.CHANGE), UIActionSlider.c:
	// 157-171); Anwendungsbeispiel JMCameraForm.c:293-381 (OnChange_*-Handler).
	// Kein neues .layout-File noetig -- der Scroller wird zur Laufzeit ueber "pct_panel" gelegt, exakt
	// wie JMCameraForm/JMExampleForm es vormachen. Abschliessendes UpdateScroller() (JMCameraForm.c:
	// 127) stoesst ein Re-Layout des Scroll-Contents an, nachdem alle Widgets erzeugt wurden.
	override void OnInit() {
		m_Scroller = UIActionManager.CreateScroller( layoutRoot.FindAnyWidget( "pct_panel" ) );
		Widget actions = m_Scroller.GetContentWidget();

		m_EnterLeaveButton = UIActionManager.CreateButton( actions, "Kamera Enter/Leave", this, "OnClick_EnterLeave" );

		m_FovText = UIActionManager.CreateText( actions, "Live FOV", "keine Kamera" );

		m_SensorSelectBox = UIActionManager.CreateSelectionBox( actions, "Sensor", m_SensorPresetLabels, this, "OnChange_Sensor" );
		m_SensorSelectBox.SetSelection( ResolveSensorPresetIndex(), false );

		// ===== Task P2-5 (Plan_B8 Section 6): Lens-Kit-Panel, direkt unter der Sensor-Zeile. Nur die
		// SelectBox + Text-Widgets werden hier schon erzeugt (richtige visuelle Position); der eigentliche
		// Linsen-Button-Bau (RebuildLensButtons) und der Erst-Refresh (RefreshLensStateWidgets, braucht
		// m_ApertureSlider/m_FocalButtonToValue) laufen erst am Ende von OnInit, siehe dort -- gleiches
		// "erst spaeter befuellen"-Vorgehen wie m_ShotListRows (RebuildShotList wird ebenfalls erst nach
		// OnInit, in OnShow(), aufgerufen). =====
		array< string > lensKitLabels = BuildLensKitLabels();
		m_LensKitSelectBox = UIActionManager.CreateSelectionBox( actions, "Lens Kit", lensKitLabels, this, "OnChange_LensKit" );
		m_LensKitSelectBox.SetSelection( 0, false );

		m_LensSectionParent = actions;
		m_LensButtonToLens = new map< UIActionBase, ref PCT_LensPreset >();

		m_LensInfoText = UIActionManager.CreateText( actions, "Objektiv-Info", "" );
		m_LensCharacterText = UIActionManager.CreateText( actions, "Optischer Charakter", "" );
		m_LensCoverageHintText = UIActionManager.CreateText( actions, "Hinweis", "" );

		// Brennweite: B5 Section 2.1 Phase-1-Zeile "Brennweite (mm)", Wertebereich 8-400 mm 1:1 aus
		// der Tabelle (Task-Brief nennt 14-200 aus B4 Section 2.4 -- das ist dort aber die separate
		// "Brennweiten-Preset"-SelectionBox-Zeile, laut B5 explizit Phase 2; B5 ist gemaess
		// Task-Auftrag massgeblich, daher hier NICHT vorgezogen). PCT_CameraRig.GetVerticalFovRad()
		// clampt sein Ergebnis ohnehin auf PCT_FOV_MIN_RAD/MAX_RAD (200/14 mm FF-Aequivalent) --
		// Werte ausserhalb der 14-200-mm-Presetreihe bleiben also gefahrlos darstellbar.
		m_FocalLengthSlider = UIActionManager.CreateSlider( actions, "Brennweite (mm)", 8, 400, this, "OnChange_FocalLength" );
		m_FocalLengthSlider.SetStepValue( 1 );
		m_FocalLengthSlider.SetCurrent( m_Module.m_CameraRig.focalLengthMm );

		// Blende: kein exakter Phase-1-Treffer in B5 Section 2.1 (dort nur "f-Stop-Aequivalent"
		// SelectionBox, Phase 2, gebunden an einen m_FStopIndex -> Blur-Mapping -- andere Semantik).
		// Task-Brief fordert explizit einen direkten Slider auf m_CameraRig.aperture (bereits ein
		// Phase-1-Feld, von der P1-6-DOF-Pipeline/PCT_Math.BlurFromAperture live konsumiert).
		// Wertebereich aus Plan_B3_Datenmodell_Persistenz.md ("Gueltige Wertebereiche"-Tabelle:
		// aperture min 0.7 / max 32.0 / default 2.8) -- die im Task-Brief genannte Alternative
		// "0.7-22" ist in B3 NICHT belegt (22 ist PCT_CinematicCamera.PCT_DOF_MAX_APERTURE, die
		// DOF-AUS-Schwelle der Blur-Heuristik, kein Feld-Wertebereich); B3 als
		// Datenmodell-Validierungsquelle hat hier Vorrang.
		m_ApertureSlider = UIActionManager.CreateSlider( actions, "Blende (f-Stop)", 0.7, 32.0, this, "OnChange_Aperture" );
		m_ApertureSlider.SetStepValue( 0.1 );
		m_ApertureSlider.SetCurrent( m_Module.m_CameraRig.aperture );

		// Fokusdistanz: B5 Section 2.1 Phase-1-Zeile "Fokusdistanz (m)", Wertebereich 0-1000 m.
		// focusDistance ist bewusst KEIN Rig-Member (Plan_B4 Section 2.2) -- Live-Zustand der Kamera,
		// daher Modul-Merker m_PendingFocusDistanceM fuer den naechsten Kamera-Erhalt (siehe
		// OnChange_FocusDistance).
		m_FocusDistanceSlider = UIActionManager.CreateSlider( actions, "Fokusdistanz (m)", 0, 1000, this, "OnChange_FocusDistance" );
		m_FocusDistanceSlider.SetStepValue( 0.1 );
		m_FocusDistanceSlider.SetCurrent( m_Module.m_PendingFocusDistanceM );

		// ===== Task P2-4: Brennweiten-Preset-Reihe (B5 Section 2.1 "Brennweiten-Preset", vom Task-Brief
		// als Button-Reihe statt SelectionBox vorgezogen) -- 15 Werte aus Plan_B4_Kamera_Engine.md
		// Section 2.4, 3 Zeilen a 5 Buttons (GridSpacer-Muster wie BuildShotRow weiter unten). =====
		UIActionManager.CreateText( actions, "Brennweiten-Presets (mm)", "" );
		m_FocalButtonToValue = new map< UIActionBase, float >();
		BuildFocalPresetButtonRow( actions, 0, 5 );
		BuildFocalPresetButtonRow( actions, 5, 5 );
		BuildFocalPresetButtonRow( actions, 10, 5 );

		// Task P1-8: Overlay-Toggles (B5 Section 4, Architektur-Vorgabe Task-Brief Punkt 4).
		// CreateCheckbox-Signatur verifiziert in UIActionManager.c:377 (parent, label, instance,
		// funcname, checked, width); Callback-Event ist UIEvent.CLICK (UIActionCheckbox.OnClick ->
		// CallEvent(UIEvent.CLICK), UIActionCheckbox.c:41-54) -- anders als die CHANGE-Events von
		// SelectBox/Slider oben.
		m_OverlayThirdsCheckbox = UIActionManager.CreateCheckbox( actions, "Overlay: Drittelraster", this, "OnChange_OverlayThirds", m_Module.m_OverlayThirdsEnabled );
		m_OverlayCenterCheckbox = UIActionManager.CreateCheckbox( actions, "Overlay: Bildzentrum", this, "OnChange_OverlayCenter", m_Module.m_OverlayCenterEnabled );

		m_AspectMaskSelectBox = UIActionManager.CreateSelectionBox( actions, "Seitenverhaeltnis-Maske", m_AspectMaskLabels, this, "OnChange_AspectMask" );
		m_AspectMaskSelectBox.SetSelection( ResolveAspectMaskIndex(), false );

		// ===== Task P2-3: Shot-Browser-Sektion (B5 Section 2.2/6) — haengt an DENSELBEN Scroller-Content
		// wie alle Widgets oben (siehe m_ShotsSectionParent-Kommentar); CreateEditableText ohne
		// instance/funcname (Default NULL/"") -- kein Live-Change-Handler noetig, der Text wird nur beim
		// Klick auf "Shot speichern"/"Umbenennen" gelesen (UIActionManager.CreateEditableText-Signatur
		// UIActionManager.c:226, Parameter instance/funcname/text/button alle mit Defaults belegt).
		m_ShotsSectionParent = actions;

		m_ShotNameField = UIActionManager.CreateEditableText( actions, "Shot-Name" );
		m_SaveShotButton = UIActionManager.CreateButton( actions, "Shot speichern", this, "OnClick_SaveShot" );

		m_ShotRowActionToId = new map< UIActionBase, string >();
		m_Shots = new array< ref PCT_Shot >();

		// ===== Task P2-4: Framing-/Angle-Preset-Reihen (B5 Section 2.2 "Framing-Preset"/"Angle-Preset",
		// vom Task-Brief als Button-Reihen statt SelectionBox vorgezogen, GridSpacer-Muster). Buttons werden
		// aus den vom Modul geladenen Preset-Listen (PCT_Storage.LoadFramingPresets/LoadAnglePresets,
		// Task P2-4) aufgebaut -- automatisch synchron mit Builtins/kuenftigen Nutzer-Presets, keine
		// zweite hartkodierte Liste. =====
		UIActionManager.CreateText( actions, "Einstellungsgroesse (Framing)", "" );
		m_FramingButtonToId = new map< UIActionBase, string >();
		BuildFramingPresetButtons( actions );

		UIActionManager.CreateText( actions, "Kamerawinkel (Angle)", "" );
		m_AngleButtonToId = new map< UIActionBase, string >();
		BuildAnglePresetButtons( actions );

		m_AutoFramingStatusText = UIActionManager.CreateText( actions, "Auto-Framing", "noch nicht angewendet" );

		// Task P2-5: Erst-Refresh des Lens-Kit-Panels -- braucht m_ApertureSlider/m_FocalLengthSlider/
		// m_FocalButtonToValue (Prime-Lock-Ziele), die erst oben in diesem OnInit erzeugt wurden, daher
		// bewusst am Ende. Rig startet mit lensPresetId="" (PCT_CameraRig-Default) -- Ergebnis: SelectBox
		// bleibt auf "-- (frei)", keine Linsen-Buttons, Blende auf 0.7-32.0, Brennweiten-Steuerung aktiv.
		SyncLensKitTabToRig();
		RefreshLensStateWidgets();

		// ===== Task P3-5: Sequencer-Sektion (Plan_B7 Section 15) -- Kamerafahrt im Aufbau: Punkte aus der
		// aktuellen Kameraposition/Blickrichtung, Transport (Play/Pause/Stop), Scrub, Speichern als Shot.
		// Punktlisten werden in RebuildSeqLists() aufgebaut (Show + nach jeder Aktion). =====
		m_SeqSectionParent = actions;
		UIActionManager.CreateText( actions, "Kamerafahrt (Sequencer)", "" );
		m_SeqNameField = UIActionManager.CreateEditableText( actions, "Fahrt-Name" );

		Widget seqButtons = UIActionManager.CreateGridSpacer( actions, 2, 3 );
		UIActionManager.CreateButton( seqButtons, "Kamerapunkt +", this, "OnClick_SeqAddCamPoint" );
		UIActionManager.CreateButton( seqButtons, "Blickpunkt +", this, "OnClick_SeqAddLookPoint" );
		UIActionManager.CreateButton( seqButtons, "Fahrt neu", this, "OnClick_SeqNew" );
		UIActionManager.CreateButton( seqButtons, "Play/Pause", this, "OnClick_SeqPlayPause" );
		UIActionManager.CreateButton( seqButtons, "Stop", this, "OnClick_SeqStop" );
		UIActionManager.CreateButton( seqButtons, "Als Shot speichern", this, "OnClick_SeqSave" );

		// Task P3-7: Bewegungs-Presets (Generatoren) -- jeder Klick erzeugt eine frische Kamerafahrt
		// aus dem aktuellen Kamerazustand (Plan_B7 Section 1.3: Presets erzeugen Pfad-Strukturen).
		UIActionManager.CreateText( actions, "Bewegungs-Presets", "" );
		Widget seqPresetButtons = UIActionManager.CreateGridSpacer( actions, 2, 3 );
		UIActionManager.CreateButton( seqPresetButtons, "Dolly In", this, "OnClick_SeqPresetDollyIn" );
		UIActionManager.CreateButton( seqPresetButtons, "Dolly Out", this, "OnClick_SeqPresetDollyOut" );
		UIActionManager.CreateButton( seqPresetButtons, "Truck Rechts", this, "OnClick_SeqPresetTruck" );
		UIActionManager.CreateButton( seqPresetButtons, "Orbit 90", this, "OnClick_SeqPresetOrbit" );
		UIActionManager.CreateButton( seqPresetButtons, "Dolly-Zoom", this, "OnClick_SeqPresetDollyZoom" );

		m_SeqScrubSlider = UIActionManager.CreateSlider( actions, "Timeline (Scrub %)", 0, 100, this, "OnChange_SeqScrub" );
		m_SeqStatusText = UIActionManager.CreateText( actions, "Kamerafahrt", "keine Punkte" );

		m_SeqCamRowToIndex = new map< UIActionBase, int >();
		m_SeqLookRowToIndex = new map< UIActionBase, int >();

		// ===== Task P2-6: Export-Sektion (B3 Section 8, B6 Phase 2) -- haengt wie alle Sektionen oben am
		// gemeinsamen Scroller-Content ("actions"). Reihenfolge Buttons: CSV, JSON, Screenshot (Task-Brief-
		// Reihenfolge); Hinweistext (CreateText, R7/R8-Vorgabe) danach als letztes Element der Sektion. Initialer
		// Enable-Zustand wird NICHT hier gesetzt -- JMFormBase.Init() ruft OnClientPermissionsUpdated() direkt
		// nach OnInit() auf (JMFormBase.c:71-73), das setzt den korrekten Anfangszustand bereits vor dem ersten
		// sichtbaren Frame (siehe OnClientPermissionsUpdated unten).
		UIActionManager.CreateText( actions, "Export", "" );
		m_ExportCsvButton = UIActionManager.CreateButton( actions, "Shot-Liste CSV", this, "OnClick_ExportCsv" );
		m_ExportJsonButton = UIActionManager.CreateButton( actions, "Shot-Liste JSON", this, "OnClick_ExportJson" );
		m_ExportScreenshotButton = UIActionManager.CreateButton( actions, "Screenshot", this, "OnClick_ExportScreenshot" );
		UIActionManager.CreateText( actions, "Hinweis", "Screenshots: DDS-Format, Konvertierung extern (siehe README)" );

		m_Scroller.UpdateScroller();
	}

	override void OnShow() {
		super.OnShow();

		// Task P2-3 (Task-Brief: "Liste bei Show/nach jeder Aktion neu aufbauen"). JMWindowBase.Show() ruft
		// m_Form.OnShow() bei JEDEM Uebergang versteckt->sichtbar erneut auf (m_IsShown wird in Hide()
		// zurueckgesetzt, JMWindowBase.c:239-242/272-273) -- die Liste wird also bei jedem erneuten Oeffnen
		// des Fensters aktuell aufgebaut, nicht nur beim allerersten Init().
		RebuildShotList();
		RebuildSeqLists();
	}

	override void OnHide() {
		super.OnHide();
	}

	override void OnSettingsUpdated() {
	}

	// ===== Task P3-5: Sequencer-Handler + Punktlisten (Muster: Shot-Browser oben) =====

	void OnClick_SeqAddCamPoint( UIEvent eid, UIActionBase action ) {
		m_Module.EditShot_AddCamPoint();
		RebuildSeqLists();
	}

	void OnClick_SeqAddLookPoint( UIEvent eid, UIActionBase action ) {
		m_Module.EditShot_AddLookPoint();
		RebuildSeqLists();
	}

	void OnClick_SeqNew( UIEvent eid, UIActionBase action ) {
		m_Module.EditShot_Reset();
		RebuildSeqLists();
	}

	void OnClick_SeqPlayPause( UIEvent eid, UIActionBase action ) {
		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if ( cam && cam.m_PCT_MotionPlayer ) {
			m_Module.Playback_TogglePause();
		} else {
			m_Module.EditShot_Play();
		}
		RefreshSeqStatus();
	}

	void OnClick_SeqStop( UIEvent eid, UIActionBase action ) {
		m_Module.Playback_Stop();
		RefreshSeqStatus();
	}

	void OnClick_SeqSave( UIEvent eid, UIActionBase action ) {
		PCT_Shot editShot = m_Module.GetOrCreateEditShot();
		string enteredName = m_SeqNameField.GetText();
		if ( enteredName != "" )
			editShot.name = enteredName;

		m_Module.EditShot_Save();
		RebuildShotList();
		RefreshSeqStatus();
	}

	void OnChange_SeqScrub( UIEvent eid, UIActionBase action ) {
		float pct = action.GetCurrent();
		float frac = pct / 100.0;
		m_Module.Playback_SeekNormalized( frac );
		RefreshSeqStatus();
	}

	void OnClick_SeqPresetDollyIn( UIEvent eid, UIActionBase action ) {
		m_Module.EditShot_PresetDollyIn();
		RebuildSeqLists();
	}

	void OnClick_SeqPresetDollyOut( UIEvent eid, UIActionBase action ) {
		m_Module.EditShot_PresetDollyOut();
		RebuildSeqLists();
	}

	void OnClick_SeqPresetTruck( UIEvent eid, UIActionBase action ) {
		m_Module.EditShot_PresetTruckRight();
		RebuildSeqLists();
	}

	void OnClick_SeqPresetOrbit( UIEvent eid, UIActionBase action ) {
		m_Module.EditShot_PresetOrbit();
		RebuildSeqLists();
	}

	void OnClick_SeqPresetDollyZoom( UIEvent eid, UIActionBase action ) {
		m_Module.EditShot_PresetDollyZoom();
		RebuildSeqLists();
	}

	void OnClick_SeqCamRowDelete( UIEvent eid, UIActionBase action ) {
		if ( !m_SeqCamRowToIndex.Contains( action ) )
			return;
		int index = m_SeqCamRowToIndex.Get( action );
		m_Module.EditShot_RemoveCamPoint( index );
		RebuildSeqLists();
	}

	void OnClick_SeqLookRowDelete( UIEvent eid, UIActionBase action ) {
		if ( !m_SeqLookRowToIndex.Contains( action ) )
			return;
		int index = m_SeqLookRowToIndex.Get( action );
		m_Module.EditShot_RemoveLookPoint( index );
		RebuildSeqLists();
	}

	protected void RefreshSeqStatus() {
		if ( !m_SeqStatusText )
			return;

		PCT_Shot editShot = m_Module.GetEditShot();
		if ( !editShot ) {
			m_SeqStatusText.SetText( "keine Punkte" );
			return;
		}

		int camCount = editShot.cameraPath.points.Count();
		int lookCount = 0;
		if ( editShot.lookPath )
			lookCount = editShot.lookPath.points.Count();

		string line = "Kamerapunkte: " + camCount.ToString() + " | Blickziele: " + lookCount.ToString();

		PCT_CinematicCamera cam = PCT_CinematicCamera.s_PCT_ActiveCamera;
		if ( cam && cam.m_PCT_MotionPlayer ) {
			PCT_MotionPlayer player = cam.m_PCT_MotionPlayer;
			string timeText = player.GetTime().ToString( false );
			string durText = player.GetDuration().ToString( false );
			line = line + " | t=" + timeText + "/" + durText + "s";
		}

		m_SeqStatusText.SetText( line );
	}

	// Punktlisten: eine Zeile je Kamerapunkt/Blickziel (Label + Loeschen), CreateActionRows-/Unlink-Muster
	// wie RebuildShotList; Punktzahlen bleiben klein (eine Content-Row genuegt, Kapazitaet 100).
	protected void RebuildSeqLists() {
		if ( m_SeqCamRows ) {
			m_SeqCamRows.Unlink();
			m_SeqCamRows = null;
		}
		if ( m_SeqLookRows ) {
			m_SeqLookRows.Unlink();
			m_SeqLookRows = null;
		}

		m_SeqCamRowToIndex.Clear();
		m_SeqLookRowToIndex.Clear();

		PCT_Shot editShot = m_Module.GetEditShot();
		if ( editShot ) {
			m_SeqCamRows = UIActionManager.CreateActionRows( m_SeqSectionParent );
			GridSpacerWidget camContentRow;
			Class.CastTo( camContentRow, m_SeqCamRows.FindAnyWidget( "Content_Row_00" ) );
			if ( camContentRow ) {
				camContentRow.Show( true );

				int camCount = editShot.cameraPath.points.Count();
				for ( int i = 0; i < camCount; i++ ) {
					PCT_PathPoint point = editShot.cameraPath.points.Get( i );
					if ( !point )
						continue;

					Widget row = UIActionManager.CreateGridSpacer( camContentRow, 1, 2 );
					int displayIndex = i + 1;
					string label = "P" + displayIndex.ToString() + " " + point.position.ToString() + " (" + point.speed.durationSeconds.ToString( false ) + "s, " + point.easing + ")";
					UIActionManager.CreateText( row, label, "" );
					UIActionButton deleteBtn = UIActionManager.CreateButton( row, "Loeschen", this, "OnClick_SeqCamRowDelete" );
					deleteBtn.SetColor( COLOR_RED );
					m_SeqCamRowToIndex.Set( deleteBtn, i );
				}
			}

			if ( editShot.lookPath ) {
				m_SeqLookRows = UIActionManager.CreateActionRows( m_SeqSectionParent );
				GridSpacerWidget lookContentRow;
				Class.CastTo( lookContentRow, m_SeqLookRows.FindAnyWidget( "Content_Row_00" ) );
				if ( lookContentRow ) {
					lookContentRow.Show( true );

					int lookCount = editShot.lookPath.points.Count();
					for ( int l = 0; l < lookCount; l++ ) {
						PCT_LookPoint lookPoint = editShot.lookPath.points.Get( l );
						if ( !lookPoint )
							continue;

						Widget lookRow = UIActionManager.CreateGridSpacer( lookContentRow, 1, 2 );
						int lookDisplayIndex = l + 1;
						string lookLabel = "Z" + lookDisplayIndex.ToString() + " " + lookPoint.worldTarget.ToString() + " (" + lookPoint.lookMode + ")";
						UIActionManager.CreateText( lookRow, lookLabel, "" );
						UIActionButton lookDeleteBtn = UIActionManager.CreateButton( lookRow, "Loeschen", this, "OnClick_SeqLookRowDelete" );
						lookDeleteBtn.SetColor( COLOR_RED );
						m_SeqLookRowToIndex.Set( lookDeleteBtn, l );
					}
				}
			}
		}

		RefreshSeqStatus();
		m_Scroller.UpdateScroller();
	}

	// Task P2-6: Permission-Gate der Export-Buttons (siehe m_ExportCsvButton-Feldkommentar oben fuer die
	// verifizierte COT-Mechanik). JMFormBase.OnClientPermissionsUpdated() (Basisklasse, no-op) wird laut
	// JMFormBase.c:73 einmalig direkt nach OnInit() aufgerufen UND jedesmal, wenn sich die Client-Permissions
	// aendern (Server-seitige Rollen-/Permission-Aenderung, COT-Sync) -- deckt damit sowohl den Erstzustand
	// (Buttons vor dem ersten sichtbaren Frame korrekt aktiv/inaktiv) als auch Live-Aenderungen waehrend das
	// Fenster offen ist ab.
	override void OnClientPermissionsUpdated() {
		super.OnClientPermissionsUpdated();

		UpdatePermissionSafe( m_ExportCsvButton, "CinematicTool.Export" );
		UpdatePermissionSafe( m_ExportJsonButton, "CinematicTool.Export" );
		UpdatePermissionSafe( m_ExportScreenshotButton, "CinematicTool.Export" );
	}

	// Null-sicherer Wrapper um UIActionBase.UpdatePermission (COT-Muster JMPlayerForm.UpdatePermission,
	// JMPlayerForm.c:205-211) -- Buttons koennen hier theoretisch noch null sein, falls OnClientPermissionsUpdated
	// vor OnInit liefe (laut JMFormBase.c:71-73 nicht der Fall, Guard trotzdem defensiv wie im COT-Vorbild).
	protected void UpdatePermissionSafe( UIActionBase action, string permission ) {
		if ( !action )
			return;

		action.UpdatePermission( permission );
	}

	// P1-2 Live-Update-Hook: JMFormBase.Update() (JMFormBase.c:126, no-arg, per Default leer) wird
	// jeden GUI-Frame automatisch aufgerufen, solange dieses Fenster sichtbar ist -- JMWindowBase
	// registriert seine eigene Update(float timeSlice)-Methode ueber
	// g_Game.GetUpdateQueue(CALL_CATEGORY_GUI).Insert(Update) in Show() und entfernt sie wieder via
	// .Remove(Update) in Hide() (JMWindowBase.c:250/264) -- die Form-Anzeige laeuft also NUR
	// waehrend das Fenster offen ist, ganz ohne eigenes Sichtbarkeits-Flag. JMWindowBase.Update()
	// selbst reicht pro Frame an m_Form.Update() durch (JMWindowBase.c:368-371). Konkretes Vorbild
	// fuer "Form ueberschreibt Update() und aktualisiert Live-Werte jeden Aufruf":
	// JMCameraForm.Update() -> OnSliderUpdate() (JMCameraForm.c:245-248, aktualisiert dort u.a. den
	// FOV-Slider aus Modul-State). PCT drosselt zusaetzlich selbst auf ~4x/s (Anforderung 1),
	// da Update() ohne Timeslice-Parameter jeden Frame feuert.
	override void Update() {
		super.Update();

		if ( !g_Game )
			return;

		float now = g_Game.GetTickTime();
		float elapsed = now - m_LastFovDisplayUpdate;
		if ( elapsed < FOV_DISPLAY_INTERVAL_S )
			return;

		m_LastFovDisplayUpdate = now;

		RefreshFovDisplay();
	}

	// Anforderung 1: "Cam: <ClassName> | FOV: <rad> rad = <deg>° ≈ <mm> mm (FF)". Kamera-Aufloesung
	// exakt wie RunFovProbe im Modul (gleiche Prioritaet, siehe PCT_CinematicModule.ResolveActiveCamera,
	// static aufrufbar ohne Modul-Instanz). Zwischenvariablen fuer die mm-Formel gemaess
	// CLAUDE.md-Segfault-Regel (komplexe Ausdruecke in Array-Zuweisungen/verschachtelten Aufrufen
	// vermeiden), inkl. Division-durch-0-Guard auf tan(fovRad/2).
	protected void RefreshFovDisplay() {
		if ( !m_FovText )
			return;

		Camera cam = PCT_CinematicModule.ResolveActiveCamera();
		if ( !cam ) {
			m_FovText.SetText( "keine Kamera" );
			return;
		}

		float fovRad = Camera.GetCurrentFOV();
		float fovDeg = fovRad * Math.RAD2DEG;

		float halfFovRad = fovRad / 2.0;
		float tanHalfFov = Math.Tan( halfFovRad );

		float focalMm = 0.0;
		if ( tanHalfFov != 0.0 )
			focalMm = SENSOR_HEIGHT_MM / ( 2.0 * tanHalfFov );

		float focalMmRounded = Math.Round( focalMm );

		string line = string.Format( "Cam: %1 | FOV: %2 rad = %3° ≈ %4 mm (FF)", cam.ClassName(), fovRad.ToString(), fovDeg.ToString(), focalMmRounded.ToString() );
		m_FovText.SetText( line );
	}

	// Spiegel von JMExampleForm.OnClick_Button (Permission-Check vor Ausfuehrung, obwohl
	// server-seitig ohnehin erneut geprueft wird -- B1 Section 8).
	void OnClick_EnterLeave( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		m_Module.Enter();
	}

	// ===== Task P1-7: Rig-Panel Handler (B5 Section 2.1 Phase-1-Zeilen) =====
	// Gleiches Permission-Guard-Muster wie OnClick_EnterLeave/vormals OnClick_Probe* (Permission vor
	// Ausfuehrung pruefen, obwohl das Fenster selbst schon HasAccess()-gated ist -- Konsistenz mit dem
	// Rest der Datei). Callback-Signatur (UIEvent eid, UIActionBase action) fuer SelectBox/Slider
	// identisch zum bereits genutzten Button-Muster, siehe OnInit-Kommentar (UIActionBase.c:343-352).

	void OnChange_Sensor( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CHANGE )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		ApplySensorPreset( action.GetSelection() );

		// Task P2-5 (Plan_B8 Section 5.3 Punkt 3): Sensor-Wechsel kann die Large-Format-Hinweiszeile
		// fuer die aktuell GEBUNDENE Linse ein-/ausblenden (fullFrame-Kit + sensor_super35).
		RefreshLensCoverageHint( ResolveBoundLensKit() );
	}

	void OnChange_FocalLength( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CHANGE )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		m_Module.m_CameraRig.focalLengthMm = action.GetCurrent();
	}

	void OnChange_Aperture( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CHANGE )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		m_Module.m_CameraRig.aperture = action.GetCurrent();
	}

	// Fokusdistanz ist bewusst KEIN Rig-Member (Plan_B4 Section 2.2) -- setzt deshalb zwei Ziele:
	// den Modul-Merker m_PendingFocusDistanceM (fuer den naechsten Kamera-Erhalt, siehe
	// PCT_CinematicModule.Client_Enter/Server_Enter) UND, falls die PCT-Kamera gerade aktiv ist,
	// direkt deren Live-Fokusdistanz (sofortige Wirkung ohne Apply-Button, wie von allen anderen
	// Rig-Feldern verlangt).
	void OnChange_FocusDistance( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CHANGE )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		float value = action.GetCurrent();
		m_Module.m_PendingFocusDistanceM = value;

		if ( PCT_CinematicCamera.s_PCT_ActiveCamera )
			PCT_CinematicCamera.s_PCT_ActiveCamera.m_PCT_FocusDistanceM = value;
	}

	// ===== Task P1-8: Overlay-Toggle-Handler (B5 Section 4) =====
	// Checkbox-Events feuern UIEvent.CLICK (siehe OnInit-Kommentar), nicht CHANGE. Gleiches
	// Permission-Guard-Muster wie die uebrigen Handler in dieser Datei.

	void OnChange_OverlayThirds( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		m_Module.SetOverlayThirdsVisible( action.IsChecked() );
	}

	void OnChange_OverlayCenter( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		m_Module.SetOverlayCenterVisible( action.IsChecked() );
	}

	void OnChange_AspectMask( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CHANGE )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		ApplyAspectMaskSelection( action.GetSelection() );
	}

	// Gleiches Bereichs-Guard-Muster wie ApplySensorPreset unten (Review-Fix P1-7 Fix 3).
	void ApplyAspectMaskSelection( int index ) {
		if ( index < 0 || index >= m_AspectMaskLabels.Count() )
			return;

		string mask = m_AspectMaskLabels.Get( index );
		m_Module.SetOverlayAspectMask( mask );
	}

	// Liest die aktuelle Maske aus m_CameraRig.aspectMask zurueck (einzige Quelle der Wahrheit, siehe
	// PCT_CinematicModule.SetOverlayAspectMask-Kommentar), analog zu ResolveSensorPresetIndex unten.
	protected int ResolveAspectMaskIndex() {
		string mask = m_Module.m_CameraRig.aspectMask;

		if ( mask == "2.39:1" )
			return 1;
		if ( mask == "16:9" )
			return 2;
		if ( mask == "4:3" )
			return 3;
		if ( mask == "9:16" )
			return 4;
		if ( mask == "1:1" )
			return 5;

		return 0;
	}

	// Sensor-Presets als statische Liste im Code (Phase 1) -- Preset-DATEIEN (Presets\*.json,
	// Plan_B3_Datenmodell_Persistenz.md) kommen erst Phase 2. Werte 1:1 aus
	// Plan_B4_Kamera_Engine.md Section 2.3 (6 Presets, Reihenfolge = m_SensorPresetLabels oben). B5
	// Section 2.1 nennt als 6. Option "Custom" (freie Breite/Hoehe) -- das ist B4s separat als
	// "erweiterbar" markiertes sensor_custom (Section 2.3, Plan_A1 Section 5), hier bewusst NICHT
	// vorgezogen; stattdessen der tatsaechliche 6. Preset aus der B4-Section-2.3-Tabelle
	// (Smartphone), wie vom Task-Brief ("6 Presets aus B4 Section 2.3") verlangt. Switch statt Array-
	// Lookup: vermeidet jede komplexe Ausdruecke-in-Array-Zuweisung-Konstruktion (CLAUDE.md-
	// Segfault-Regel) und haelt die Preset-Daten an einer einzigen, leicht lesbaren Stelle.
	void ApplySensorPreset( int index ) {
		// Review-Fix P1-7 (Fix 3): expliziter Bereichs-Guard -- die SelectionBox liefert index nur aus
		// m_SensorPresetLabels (0-5), aber ohne Guard wuerde ein Ausserhalb-Wert (z. B. -1 bei
		// fehlgeschlagener Selection) die switch-Kette einfach lautlos durchfallen lassen, statt den
		// Fehlerfall sichtbar/eindeutig abzubrechen.
		if ( index < 0 || index >= m_SensorPresetLabels.Count() )
			return;

		switch ( index ) {
			case 0:
				m_Module.m_CameraRig.sensorPresetId = "sensor_fullframe";
				m_Module.m_CameraRig.sensorWidthMm = 36.0;
				m_Module.m_CameraRig.sensorHeightMm = 24.0;
				break;
			case 1:
				m_Module.m_CameraRig.sensorPresetId = "sensor_super35";
				m_Module.m_CameraRig.sensorWidthMm = 24.89;
				m_Module.m_CameraRig.sensorHeightMm = 18.66;
				break;
			case 2:
				m_Module.m_CameraRig.sensorPresetId = "sensor_apsc";
				m_Module.m_CameraRig.sensorWidthMm = 23.6;
				m_Module.m_CameraRig.sensorHeightMm = 15.6;
				break;
			case 3:
				m_Module.m_CameraRig.sensorPresetId = "sensor_mft";
				m_Module.m_CameraRig.sensorWidthMm = 17.3;
				m_Module.m_CameraRig.sensorHeightMm = 13.0;
				break;
			case 4:
				m_Module.m_CameraRig.sensorPresetId = "sensor_1inch";
				m_Module.m_CameraRig.sensorWidthMm = 13.2;
				m_Module.m_CameraRig.sensorHeightMm = 8.8;
				break;
			case 5:
				m_Module.m_CameraRig.sensorPresetId = "sensor_smartphone";
				m_Module.m_CameraRig.sensorWidthMm = 9.8;
				m_Module.m_CameraRig.sensorHeightMm = 7.3;
				break;
		}
	}

	// Liest den aktuellen Rig-Zustand beim OnInit zurueck (Modul lebt laenger als die Form-Widgets --
	// erneutes Oeffnen des Fensters soll den zuletzt gewaehlten Sensor zeigen statt immer auf Full
	// Frame zurueckzuspringen), damit SetSelection(..., false) in OnInit ohne Extra-CHANGE-Event den
	// richtigen Index vorwaehlt (gleiches sendEvent=false-Muster wie JMCameraForm.c:92).
	protected int ResolveSensorPresetIndex() {
		string presetId = m_Module.m_CameraRig.sensorPresetId;

		if ( presetId == "sensor_super35" )
			return 1;
		if ( presetId == "sensor_apsc" )
			return 2;
		if ( presetId == "sensor_mft" )
			return 3;
		if ( presetId == "sensor_1inch" )
			return 4;
		if ( presetId == "sensor_smartphone" )
			return 5;

		return 0;
	}

	// ===== Task P2-3: Shot-Browser-Handler (B5 Section 2.2/6) =====

	void OnClick_SaveShot( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		string shotName = m_ShotNameField.GetText();
		m_Module.CaptureCurrentShot( shotName );

		RebuildShotList();
	}

	void OnClick_ShotRowApply( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		string shotId = ResolveShotRowId( action );
		if ( shotId == "" )
			return;

		PCT_Shot shot = PCT_Storage.LoadShot( shotId );
		if ( !shot )
			return;

		m_Module.ApplyShot( shot );
	}

	void OnClick_ShotRowRename( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		string shotId = ResolveShotRowId( action );
		if ( shotId == "" )
			return;

		string newName = m_ShotNameField.GetText();
		if ( newName == "" ) {
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: Bitte zuerst einen Namen im Textfeld eintragen." ) );
			return;
		}

		m_Module.RenameShotDisplayName( shotId, newName );
		RebuildShotList();
	}

	// Loeschen mit Bestaetigungsdialog (B5 Section 6 "Delete mit CreateConfirmation_Two (JMFormBase)"), Muster
	// JMTeleportForm.Click_RemoveLocation/RemoveLocation_Confirmed (JMTeleportForm.c:111-132): 6-Parameter-
	// Ueberladung von CreateConfirmation_Two (JMFormBase.c:154) — Button-Label + Callback-Methodenname je
	// Knopf, Bestaetigungs-Callback selbst ohne Parameter.
	void OnClick_ShotRowDelete( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		string shotId = ResolveShotRowId( action );
		if ( shotId == "" )
			return;

		m_PendingDeleteShotId = shotId;
		CreateConfirmation_Two( JMConfirmationType.INFO, "Shot loeschen?", "Shot '" + shotId + "' wirklich loeschen?", "#STR_COT_GENERIC_YES", "ConfirmDeleteShot", "#STR_COT_GENERIC_NO", "" );
	}

	void ConfirmDeleteShot() {
		if ( m_PendingDeleteShotId == "" )
			return;

		m_Module.DeleteShotById( m_PendingDeleteShotId );
		m_PendingDeleteShotId = "";

		RebuildShotList();
	}

	// ===== Task P2-6: Export-Handler (B3 Section 8, B6 Phase 2). Klick-Zeit-Permission-Check als ZWEITE
	// Verteidigungslinie zusaetzlich zum SetEnabled-Gate (m_ExportCsvButton-Feldkommentar) -- identisches
	// Doppel-Muster wie OnClick_SaveShot/OnClick_ShotRowApply usw. oben (Permission-Check vor Ausfuehrung,
	// obwohl die zugehoerigen Buttons theoretisch schon deaktiviert sind): ein deaktivierter Button blockt
	// den Klick clientseitig zuverlaessig, aber der Handler bleibt aus Konsistenz zum Rest dieser Datei
	// trotzdem eigenstaendig sicher, falls ein Klick den Handler doch erreicht (z. B. Enable-Zustand
	// waehrend eines laufenden Permission-Updates). Bei Verweigerung: Notification statt stillem Abbruch
	// (Task-Brief "bei Verweigerung Notification").
	void OnClick_ExportCsv( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.Export" ) ) {
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: Keine Berechtigung fuer Export (CinematicTool.Export)." ) );
			return;
		}

		string filePath = m_Module.ExportShotListCsv();
		if ( filePath == "" ) {
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: CSV-Export fehlgeschlagen." ) );
			return;
		}

		COTCreateLocalAdminNotification( new StringLocaliser( "PCT: Shot-Liste exportiert: " + filePath ) );
	}

	void OnClick_ExportJson( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.Export" ) ) {
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: Keine Berechtigung fuer Export (CinematicTool.Export)." ) );
			return;
		}

		string filePath = m_Module.ExportShotListJson();
		if ( filePath == "" ) {
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: JSON-Export fehlgeschlagen." ) );
			return;
		}

		COTCreateLocalAdminNotification( new StringLocaliser( "PCT: Shot-Liste exportiert: " + filePath ) );
	}

	// Shot-Name aus DEMSELBEN Textfeld wie "Shot speichern"/"Umbenennen" (m_ShotNameField, B5 nennt kein
	// eigenes Namensfeld fuer den Screenshot) -- leeres Feld wird abgewiesen mit Notification statt eines
	// Screenshots mit generischem "shot"-Dateinamen, identisches Leerfeld-Verhalten wie OnClick_ShotRowRename.
	void OnClick_ExportScreenshot( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.Export" ) ) {
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: Keine Berechtigung fuer Export (CinematicTool.Export)." ) );
			return;
		}

		string shotName = m_ShotNameField.GetText();
		if ( shotName == "" ) {
			COTCreateLocalAdminNotification( new StringLocaliser( "PCT: Bitte zuerst einen Namen im Textfeld eintragen." ) );
			return;
		}

		string filePath = m_Module.CaptureShotThumbnail( shotName );
		COTCreateLocalAdminNotification( new StringLocaliser( "PCT: Screenshot ausgeloest: " + filePath ) );
	}

	// Loest die Shot-id fuer einen geklickten Zeilen-Button auf (siehe m_ShotRowActionToId-Kommentar oben).
	protected string ResolveShotRowId( UIActionBase action ) {
		if ( !action )
			return "";

		string shotId;
		if ( !m_ShotRowActionToId.Find( action, shotId ) )
			return "";

		return shotId;
	}

	// Rebuild-ohne-Leaks (Selbstpruefung Punkt 3): alte Zeilen zuerst per Unlink() entfernen (zerstoert das
	// komplette Content-Rows-Widget samt aller Zeilen-Kinder), DANACH die Map leeren (sonst zeigen stale
	// Eintraege auf bereits zerstoerte Widgets) und frisch aus PCT_Storage.LoadAllShots() aufbauen. Aufrufer:
	// OnShow() (Task-Brief "bei Show") sowie nach jeder Save/Apply/Rename/Delete-Aktion (Task-Brief "nach
	// jeder Aktion").
	protected void RebuildShotList() {
		if ( m_ShotListRows ) {
			m_ShotListRows.Unlink();
			m_ShotListRows = null;
		}

		m_ShotRowActionToId.Clear();

		if ( !m_Shots )
			m_Shots = new array< ref PCT_Shot >();

		PCT_Storage.LoadAllShots( m_Shots );

		// CreateActionRows-Zeilenverteilung 1:1 aus dem verifizierten COT-Muster (JMESPForm.ESPFilters:
		// "totalInContentRow >= 100" -> naechster Content_Row_0N) uebernommen -- 10 Content-Rows x 100
		// Zeilenplaetze = 1000 Kapazitaet, weit ueber der Task-Brief-Anforderung ">20 Shots weiterhin
		// bedienbar" (Scroller traegt beliebige Hoehe, s. Klassenkommentar oben).
		m_ShotListRows = UIActionManager.CreateActionRows( m_ShotsSectionParent );

		int currentContentRow = 0;
		int totalInContentRow = 0;

		GridSpacerWidget contentRow;
		Class.CastTo( contentRow, m_ShotListRows.FindAnyWidget( "Content_Row_0" + currentContentRow ) );
		if ( contentRow )
			contentRow.Show( true );

		int count = m_Shots.Count();
		for ( int i = 0; i < count; i++ ) {
			if ( totalInContentRow >= 100 ) {
				currentContentRow++;
				totalInContentRow = 0;
				Class.CastTo( contentRow, m_ShotListRows.FindAnyWidget( "Content_Row_0" + currentContentRow ) );
				if ( contentRow )
					contentRow.Show( true );
			}

			PCT_Shot shot = m_Shots.Get( i );
			if ( !shot )
				continue;

			BuildShotRow( contentRow, shot );
			totalInContentRow++;
		}

		m_Scroller.UpdateScroller();
	}

	// Eine Zeile: Name-Label + Anwenden/Umbenennen/Loeschen (Task-Brief); reine UIActionManager-Komposition
	// (GridSpacer + Text + 3 Buttons), kein eigenes Zeilen-Layout noetig (B5 Section 9).
	protected void BuildShotRow( Widget parent, PCT_Shot shot ) {
		if ( !parent || !shot )
			return;

		Widget row = UIActionManager.CreateGridSpacer( parent, 1, 4 );

		string rowLabel = shot.name;
		if ( rowLabel == "" )
			rowLabel = shot.id;

		UIActionManager.CreateText( row, rowLabel, "" );

		UIActionButton applyBtn = UIActionManager.CreateButton( row, "Anwenden", this, "OnClick_ShotRowApply" );
		UIActionButton renameBtn = UIActionManager.CreateButton( row, "Umbenennen", this, "OnClick_ShotRowRename" );
		UIActionButton deleteBtn = UIActionManager.CreateButton( row, "Loeschen", this, "OnClick_ShotRowDelete" );
		deleteBtn.SetColor( COLOR_RED );

		m_ShotRowActionToId.Set( applyBtn, shot.id );
		m_ShotRowActionToId.Set( renameBtn, shot.id );
		m_ShotRowActionToId.Set( deleteBtn, shot.id );
	}

	// Task P2-3 (ApplyShot-Aufrufer, PCT_CinematicModule.ApplyShot): Slider/SelectBox nach einem Apply auf den
	// neuen Rig-Zustand nachziehen. SetSelection(..., false)/SetCurrent() ohne Event-Ausloesung, wo verfuegbar
	// (SetSelection nimmt den sendEvent-Parameter explizit entgegen, Muster bereits in OnInit/
	// ResolveSensorPresetIndex verwendet) -- SetCurrent() auf UIActionSlider loest laut UIActionSlider.c
	// ohnehin nur beim tatsaechlichen Drag/Scroll ein CHANGE-Event aus (OnChange), nicht bei einem
	// programmatischen Set, daher kein zusaetzlicher Event-Loop-Schutz fuer die Slider noetig.
	void RefreshRigWidgets() {
		if ( m_SensorSelectBox )
			m_SensorSelectBox.SetSelection( ResolveSensorPresetIndex(), false );

		if ( m_FocalLengthSlider )
			m_FocalLengthSlider.SetCurrent( m_Module.m_CameraRig.focalLengthMm );

		if ( m_ApertureSlider )
			m_ApertureSlider.SetCurrent( m_Module.m_CameraRig.aperture );

		if ( m_FocusDistanceSlider )
			m_FocusDistanceSlider.SetCurrent( m_Module.m_PendingFocusDistanceM );

		if ( m_AspectMaskSelectBox )
			m_AspectMaskSelectBox.SetSelection( ResolveAspectMaskIndex(), false );

		// Task P2-5 (Plan_B8 Section 6, AP-A-DoD Punkt 5 "Shot speichern/laden stellt lensPresetId +
		// geclampte Aperture wieder her"): Lens-Kit-Panel (Kit-Tab/Buttons/Prime-Lock/Blenden-Grenzen/
		// Info-/Charakter-Zeile) auf den ggf. per ApplyShot wiederhergestellten rig.lensPresetId
		// nachziehen. SyncLensKitTabToRig() springt dabei auch auf den passenden Kit-/Einzellinsen-Tab,
		// falls die aktive Linse nicht zum aktuell angezeigten Tab gehoert.
		SyncLensKitTabToRig();
		RefreshLensStateWidgets();

		// Task P2-4: nach ApplyAutoFraming() (ggf. empfohlene Brennweite) bzw. ApplyShot() ist die
		// Auto-Framing-Statuszeile mit auf dem neusten Stand zu halten -- RefreshRigWidgets() ist bereits
		// der zentrale "Form nach externer Modul-Aenderung synchronisieren"-Hook (siehe ApplyShot/
		// ApplyAutoFraming-Aufrufer in PCT_CinematicModule).
		RefreshAutoFramingStatusText();
	}

	// ===== Task P2-4: Brennweiten-/Framing-/Angle-Preset-Button-Reihen (Handler + Builder) =====

	// Baut EINE GridSpacer-Zeile aus m_FocalLengthPresetLabels/-Values[startIndex .. startIndex+count).
	// Aufrufer (OnInit) reicht feste 5er-Bloecke fuer die 15 B4-Section-2.4-Werte durch (3 Zeilen a 5).
	protected void BuildFocalPresetButtonRow( Widget parent, int startIndex, int count ) {
		int total = m_FocalLengthPresetLabels.Count();
		int endIndexExclusive = startIndex + count;
		if ( endIndexExclusive > total )
			endIndexExclusive = total;

		int columns = endIndexExclusive - startIndex;
		if ( columns <= 0 )
			return;

		Widget row = UIActionManager.CreateGridSpacer( parent, 1, columns );

		int i = startIndex;
		while ( i < endIndexExclusive ) {
			string label = m_FocalLengthPresetLabels.Get( i );
			float value = m_FocalLengthPresetValues.Get( i );

			UIActionButton button = UIActionManager.CreateButton( row, label, this, "OnClick_FocalPreset" );
			m_FocalButtonToValue.Set( button, value );

			i = i + 1;
		}
	}

	void OnClick_FocalPreset( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		float value;
		if ( !m_FocalButtonToValue.Find( action, value ) )
			return;

		// Direktes Feld-Set + Slider-Nachziehen, identisches Muster zu OnChange_FocalLength (P1-7) --
		// "Auswahl wirkt sofort", kein Apply-Button (m_CameraRig ist von der aktiven Kamera per Referenz
		// geteilt, siehe PCT_CinematicCamera.m_PCT_Rig-Kommentar).
		m_Module.m_CameraRig.focalLengthMm = value;

		if ( m_FocalLengthSlider )
			m_FocalLengthSlider.SetCurrent( value );
	}

	// Baut die komplette Framing-Preset-Button-Reihe aus m_Module.m_FramingPresets (5 Buttons je
	// GridSpacer-Zeile, letzte Zeile kann kuerzer sein -- 10 Builtins ergeben exakt 2 volle Zeilen).
	protected void BuildFramingPresetButtons( Widget parent ) {
		if ( !m_Module.m_FramingPresets ) {
			UIActionManager.CreateText( parent, "", "Framing-Presets nicht geladen" );
			return;
		}

		int perRow = 5;
		int count = m_Module.m_FramingPresets.Count();
		int index = 0;

		while ( index < count ) {
			int remaining = count - index;
			int columns = perRow;
			if ( remaining < perRow )
				columns = remaining;

			Widget row = UIActionManager.CreateGridSpacer( parent, 1, columns );

			int col = 0;
			while ( col < columns ) {
				PCT_FramingPreset preset = m_Module.m_FramingPresets.Get( index );
				if ( preset ) {
					UIActionButton button = UIActionManager.CreateButton( row, preset.shortCode, this, "OnClick_FramingPreset" );
					m_FramingButtonToId.Set( button, preset.id );
				}

				index = index + 1;
				col = col + 1;
			}
		}
	}

	void OnClick_FramingPreset( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		string presetId;
		if ( !m_FramingButtonToId.Find( action, presetId ) )
			return;

		// Setzt die Auswahl UND wendet Auto-Framing sofort an (PCT_CinematicModule.SetSelectedFramingPreset
		// -> ApplyAutoFraming), "Auswahl wirkt sofort" wie die uebrigen Regler dieses Formulars.
		m_Module.SetSelectedFramingPreset( presetId );
		RefreshAutoFramingStatusText();
	}

	// Baut die komplette Angle-Preset-Button-Reihe aus m_Module.m_AnglePresets (gleiches Muster wie
	// BuildFramingPresetButtons oben; 11 Builtins ergeben 5+5+1).
	protected void BuildAnglePresetButtons( Widget parent ) {
		if ( !m_Module.m_AnglePresets ) {
			UIActionManager.CreateText( parent, "", "Angle-Presets nicht geladen" );
			return;
		}

		int perRow = 5;
		int count = m_Module.m_AnglePresets.Count();
		int index = 0;

		while ( index < count ) {
			int remaining = count - index;
			int columns = perRow;
			if ( remaining < perRow )
				columns = remaining;

			Widget row = UIActionManager.CreateGridSpacer( parent, 1, columns );

			int col = 0;
			while ( col < columns ) {
				PCT_AnglePreset preset = m_Module.m_AnglePresets.Get( index );
				if ( preset ) {
					UIActionButton button = UIActionManager.CreateButton( row, preset.displayName, this, "OnClick_AnglePreset" );
					m_AngleButtonToId.Set( button, preset.id );
				}

				index = index + 1;
				col = col + 1;
			}
		}
	}

	void OnClick_AnglePreset( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		string presetId;
		if ( !m_AngleButtonToId.Find( action, presetId ) )
			return;

		m_Module.SetSelectedAnglePreset( presetId );
		RefreshAutoFramingStatusText();
	}

	// Liest die zuletzt angewendeten Preset-ids direkt vom Modul zurueck (m_SelectedFramingPresetId/
	// m_SelectedAnglePresetId sind wie m_CameraRig NICHT `protected`, siehe dortiger Kommentar) --
	// aufgerufen nach jedem Framing-/Angle-Klick UND aus RefreshRigWidgets() (ApplyShot/ApplyAutoFraming-
	// Synchronisationspunkt).
	protected void RefreshAutoFramingStatusText() {
		if ( !m_AutoFramingStatusText )
			return;

		string line = "Framing: " + m_Module.m_SelectedFramingPresetId + " | Winkel: " + m_Module.m_SelectedAnglePresetId;
		m_AutoFramingStatusText.SetText( line );
	}

	// ===== Task P2-5 (Docs/Plan_B8_Objektivsystem_LensKits.md Section 6/8 Task 4): Lens-Kit-Panel
	// Handler + Builder. Datenquelle sind die vom Modul einmalig geladenen m_Module.m_LensPresets/
	// m_LensKits (siehe dortiger Feldkommentar) -- kein wiederholter PCT_Storage-Reload pro Klick/Rebuild.
	// =====

	// SelectionBox-Optionen: Index 0 = "-- (frei)", danach ein Eintrag je Kit (Reihenfolge =
	// m_Module.m_LensKits), zuletzt "Einzellinsen" NUR falls mindestens eine Builtin-/Nutzer-Linse mit
	// kitId="" existiert (Plan_B8 Section 6: "Einzellinsen ausserhalb eines Kits (65 mm) erscheinen ...
	// unter dem Eintrag 'Einzellinsen'").
	protected array< string > BuildLensKitLabels() {
		array< string > labels = new array< string >();
		labels.Insert( "-- (frei)" );

		if ( m_Module.m_LensKits ) {
			int count = m_Module.m_LensKits.Count();
			int i = 0;
			while ( i < count ) {
				PCT_LensKit kit = m_Module.m_LensKits.Get( i );
				if ( kit )
					labels.Insert( kit.displayName );

				i = i + 1;
			}
		}

		if ( HasStandaloneLenses() )
			labels.Insert( "Einzellinsen" );

		return labels;
	}

	protected bool HasStandaloneLenses() {
		if ( !m_Module.m_LensPresets )
			return false;

		int count = m_Module.m_LensPresets.Count();
		int i = 0;
		while ( i < count ) {
			PCT_LensPreset lens = m_Module.m_LensPresets.Get( i );
			if ( lens && lens.kitId == "" )
				return true;

			i = i + 1;
		}

		return false;
	}

	protected PCT_LensPreset FindLensPresetInModule( string id ) {
		if ( !m_Module.m_LensPresets || id == "" )
			return null;

		int count = m_Module.m_LensPresets.Count();
		int i = 0;
		while ( i < count ) {
			PCT_LensPreset lens = m_Module.m_LensPresets.Get( i );
			if ( lens && lens.id == id )
				return lens;

			i = i + 1;
		}

		return null;
	}

	protected PCT_LensKit FindLensKitById( string kitId ) {
		if ( !m_Module.m_LensKits || kitId == "" )
			return null;

		int count = m_Module.m_LensKits.Count();
		int i = 0;
		while ( i < count ) {
			PCT_LensKit kit = m_Module.m_LensKits.Get( i );
			if ( kit && kit.id == kitId )
				return kit;

			i = i + 1;
		}

		return null;
	}

	// Auflisten der Linsen eines Kits IN DER REIHENFOLGE von kit.lensPresetIds (Plan_B8 Section 6: Buttons
	// "21/25/29/35/50/85/100" -- massgeblich ist die Kit-Referenzliste, siehe PCT_LensKit-Kommentar).
	protected array< ref PCT_LensPreset > ResolveKitLenses( PCT_LensKit kit ) {
		array< ref PCT_LensPreset > result = new array< ref PCT_LensPreset >();
		if ( !kit || !kit.lensPresetIds )
			return result;

		int count = kit.lensPresetIds.Count();
		int i = 0;
		while ( i < count ) {
			string lensId = kit.lensPresetIds.Get( i );
			PCT_LensPreset lens = FindLensPresetInModule( lensId );
			if ( lens )
				result.Insert( lens );

			i = i + 1;
		}

		return result;
	}

	protected array< ref PCT_LensPreset > ResolveStandaloneLenses() {
		array< ref PCT_LensPreset > result = new array< ref PCT_LensPreset >();
		if ( !m_Module.m_LensPresets )
			return result;

		int count = m_Module.m_LensPresets.Count();
		int i = 0;
		while ( i < count ) {
			PCT_LensPreset lens = m_Module.m_LensPresets.Get( i );
			if ( lens && lens.kitId == "" )
				result.Insert( lens );

			i = i + 1;
		}

		return result;
	}

	// SelectBox-Index eines Kits (1-basiert -- Index 0 ist "-- (frei)", siehe BuildLensKitLabels).
	// -1 = Kit nicht (mehr) in m_Module.m_LensKits vorhanden.
	protected int ResolveKitSelectIndex( string kitId ) {
		if ( !m_Module.m_LensKits || kitId == "" )
			return -1;

		int count = m_Module.m_LensKits.Count();
		int i = 0;
		while ( i < count ) {
			PCT_LensKit kit = m_Module.m_LensKits.Get( i );
			if ( kit && kit.id == kitId )
				return i + 1;

			i = i + 1;
		}

		return -1;
	}

	// SelectBox-Index des "Einzellinsen"-Eintrags -- liegt immer direkt hinter allen Kit-Eintraegen
	// (siehe BuildLensKitLabels). -1 = kein solcher Eintrag vorhanden (keine kitId=""-Linse geladen).
	protected int ResolveStandaloneSelectIndex() {
		if ( !HasStandaloneLenses() )
			return -1;

		int kitCount = 0;
		if ( m_Module.m_LensKits )
			kitCount = m_Module.m_LensKits.Count();

		return kitCount + 1;
	}

	// Liefert das Kit der aktuell GEBUNDENEN Linse (rig.lensPresetId), oder null (keine Linse gebunden
	// bzw. Einzellinse ohne Kit) -- fuer die Sensor-Empfehlungszeile (Plan_B8 Section 5.3 Punkt 3), die
	// sich auf den tatsaechlich angewendeten Zustand bezieht, nicht auf den gerade nur BROWSED Kit-Tab.
	protected PCT_LensKit ResolveBoundLensKit() {
		PCT_LensPreset lens = FindLensPresetInModule( m_Module.m_CameraRig.lensPresetId );
		if ( !lens )
			return null;

		return FindLensKitById( lens.kitId );
	}

	// Baut/ersetzt die Linsen-Button-Reihe(n) fuer die uebergebene Liste (Kit-Lenses, Einzellinsen oder
	// null/leer fuer "-- (frei)"). Unlink-vor-Neubau-Muster identisch zu RebuildShotList/m_ShotListRows.
	protected void RebuildLensButtons( array< ref PCT_LensPreset > lenses ) {
		if ( m_LensButtonRows ) {
			m_LensButtonRows.Unlink();
			m_LensButtonRows = null;
		}

		m_LensButtonToLens.Clear();

		if ( !lenses || lenses.Count() == 0 )
			return;

		m_LensButtonRows = UIActionManager.CreateActionRows( m_LensSectionParent );

		GridSpacerWidget contentRow;
		Class.CastTo( contentRow, m_LensButtonRows.FindAnyWidget( "Content_Row_00" ) );
		if ( contentRow )
			contentRow.Show( true );

		int perRow = 5;
		int count = lenses.Count();
		int index = 0;

		while ( index < count ) {
			int remaining = count - index;
			int columns = perRow;
			if ( remaining < perRow )
				columns = remaining;

			Widget row = UIActionManager.CreateGridSpacer( contentRow, 1, columns );

			int col = 0;
			while ( col < columns ) {
				PCT_LensPreset lens = lenses.Get( index );
				if ( lens ) {
					int focalInt = Math.Round( lens.focalLengthMm );
					string label = focalInt.ToString();

					UIActionButton button = UIActionManager.CreateButton( row, label, this, "OnClick_LensPreset" );
					m_LensButtonToLens.Set( button, lens );

					if ( lens.id == m_Module.m_CameraRig.lensPresetId )
						button.SetColor( COLOR_GREEN );
				}

				index = index + 1;
				col = col + 1;
			}
		}

		m_Scroller.UpdateScroller();
	}

	// Kit-SelectionBox-Wechsel (reines Browsing der Button-Reihe): "-- (frei)" gibt zusaetzlich die
	// Prime-Lock frei (Plan_B8 Section 5.3 "Freie Brennweite waehlen setzt rig.lensPresetId=''") --
	// jeder andere Eintrag aendert rig.lensPresetId NICHT, er zeigt nur die zugehoerigen Linsen-Buttons.
	void OnChange_LensKit( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CHANGE )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		int index = action.GetSelection();

		if ( index <= 0 ) {
			m_Module.m_CameraRig.lensPresetId = "";
			m_SelectedLensKit = null;
			RebuildLensButtons( null );
			RefreshLensCharacterText( null );
			RefreshLensStateWidgets();
			return;
		}

		int kitCount = 0;
		if ( m_Module.m_LensKits )
			kitCount = m_Module.m_LensKits.Count();

		if ( index <= kitCount ) {
			PCT_LensKit kit = m_Module.m_LensKits.Get( index - 1 );
			m_SelectedLensKit = kit;
			RebuildLensButtons( ResolveKitLenses( kit ) );
			RefreshLensCharacterText( kit );
		} else {
			m_SelectedLensKit = null;
			RebuildLensButtons( ResolveStandaloneLenses() );
			RefreshLensCharacterText( null );
		}

		RefreshLensStateWidgets();
	}

	// Linsen-Button-Klick: bindet die Linse an das Rig (Prime-Lock greift danach, siehe
	// RefreshLensStateWidgets) -- "Auswahl wirkt sofort", kein Apply-Button (identisches Prinzip wie
	// OnClick_FocalPreset/OnClick_FramingPreset).
	void OnClick_LensPreset( UIEvent eid, UIActionBase action ) {
		if ( eid != UIEvent.CLICK )
			return;

		if ( !GetPermissionsManager().HasPermission( "CinematicTool.View" ) )
			return;

		PCT_LensPreset lens;
		if ( !m_LensButtonToLens.Find( action, lens ) )
			return;
		if ( !lens )
			return;

		PCT_LensLibrary.ApplyLensToRig( lens, m_Module.m_CameraRig );

		RefreshRigWidgets();
	}

	// Deaktiviert/aktiviert die freie Brennweitenwahl (Slider + die 15 Brennweiten-Preset-Buttons aus
	// P2-4) -- Prime-Lock (Plan_B8 Section 5.3): "bei gebundener Prime-Linse Brennweiten-Slider/-Preset-
	// Buttons deaktivieren". UIActionBase.SetEnabled(bool) -- verifiziert in
	// DayZ-CommunityOnlineTools-production/JM/COT/Scripts/5_Mission/CommunityOnlineTools/gui/Actions/
	// UIActionBase.c:186-195 (toggelt Enable()/Disable(), zeigt/versteckt das "action_wrapper_disable"-
	// Overlay-Widget) -- gilt fuer JEDES UIActionBase-Widget (Slider UND Button), kein Cast noetig.
	protected void SetFocalControlsEnabled( bool enabled ) {
		if ( m_FocalLengthSlider )
			m_FocalLengthSlider.SetEnabled( enabled );

		if ( m_FocalButtonToValue ) {
			array< UIActionBase > buttons = m_FocalButtonToValue.GetKeyArray();
			int count = buttons.Count();
			int i = 0;
			while ( i < count ) {
				UIActionBase button = buttons.Get( i );
				if ( button )
					button.SetEnabled( enabled );

				i = i + 1;
			}
		}
	}

	// Blenden-Slider-Grenzen + "T"/"f"-Beschriftung dynamisch aus der gebundenen Linse (Plan_B8 Section
	// 6-Tabelle "Blenden-Slider"); ohne gebundene Linse gilt wieder der P1-7-Standardbereich 0.7-32.0
	// (f-Stop). UIActionSlider.SetMinMax/SetLabel -- verifiziert in .../gui/Actions/UIActionSlider.c:
	// 51-64/99-118 (SetMinMax skaliert den aktuellen Wert intern bereits mit, SetCurrent danach setzt ihn
	// trotzdem explizit auf den tatsaechlichen Rig-Wert -- keine Doppeldeutigkeit).
	protected void RefreshApertureSliderBounds( PCT_LensPreset lens ) {
		if ( !m_ApertureSlider )
			return;

		if ( lens ) {
			float minStop = lens.maxAperture;
			float maxStop = PCT_LensLibrary.ResolveMinAperture( lens );
			m_ApertureSlider.SetMinMax( minStop, maxStop );
			m_ApertureSlider.SetCurrent( m_Module.m_CameraRig.aperture );

			if ( lens.apertureScale == "T" )
				m_ApertureSlider.SetLabel( "Blende (T-Stop)" );
			else
				m_ApertureSlider.SetLabel( "Blende (f-Stop)" );
		} else {
			m_ApertureSlider.SetMinMax( 0.7, 32.0 );
			m_ApertureSlider.SetCurrent( m_Module.m_CameraRig.aperture );
			m_ApertureSlider.SetLabel( "Blende (f-Stop)" );
		}
	}

	// Info-Zeile (Plan_B8 Section 6-Tabelle): "T1.5-T22 - CF 0.35 m - 1.50 kg - (Ø) 95 mm". Null-Werte
	// (weightKg/frontDiameterMm == 0.0 -- "nicht dokumentiert") werden ausgelassen (65 mm: ohne Gewicht).
	// lengthMm erscheint NICHT in der Info-Zeile (Plan-Beispiel Section 6 nennt nur Blende/CF/Gewicht/
	// Frontdurchmesser, kein Laengenfeld).
	protected void RefreshLensInfoText( PCT_LensPreset lens ) {
		if ( !m_LensInfoText )
			return;

		if ( !lens ) {
			m_LensInfoText.SetText( "Keine Linse gebunden" );
			return;
		}

		float minAperture = PCT_LensLibrary.ResolveMinAperture( lens );

		string line = lens.apertureScale + lens.maxAperture.ToString() + "–" + lens.apertureScale + minAperture.ToString();
		line = line + " · CF " + lens.minFocusDistanceM.ToString() + " m";

		if ( lens.weightKg > 0.0 )
			line = line + " · " + lens.weightKg.ToString() + " kg";

		if ( lens.frontDiameterMm > 0.0 )
			line = line + " · Ø " + lens.frontDiameterMm.ToString() + " mm";

		m_LensInfoText.SetText( line );
	}

	// Charakter-Zeile (Plan_B8 Section 6-Tabelle): kit.opticalCharacter + kit.coating, statisch je
	// BROWSED Kit (siehe m_SelectedLensKit-Kommentar oben) -- leer fuer "-- (frei)"/"Einzellinsen".
	protected void RefreshLensCharacterText( PCT_LensKit kit ) {
		if ( !m_LensCharacterText )
			return;

		if ( !kit ) {
			m_LensCharacterText.SetText( "" );
			return;
		}

		string line = kit.opticalCharacter;
		if ( kit.coating != "" )
			line = line + " · " + kit.coating;

		m_LensCharacterText.SetText( line );
	}

	// Sensor-Empfehlungszeile (Plan_B8 Section 5.3 Punkt 3): Kit-coverage="fullFrame" + aktiver Sensor
	// sensor_super35 -> Hinweis, keine Sperre. Bezieht sich auf das Kit der GEBUNDENEN Linse (siehe
	// ResolveBoundLensKit-Kommentar), nicht auf den gerade browsed Tab.
	protected void RefreshLensCoverageHint( PCT_LensKit kit ) {
		if ( !m_LensCoverageHintText )
			return;

		bool showHint = false;
		if ( kit && kit.coverage == "fullFrame" && m_Module.m_CameraRig.sensorPresetId == "sensor_super35" )
			showHint = true;

		if ( showHint )
			m_LensCoverageHintText.SetText( "Large-Format-Linse auf S35: engerer Bildwinkel" );
		else
			m_LensCoverageHintText.SetText( "" );
	}

	// Faerbt die Buttons der AKTUELL ANGEZEIGTEN Reihe passend zu rig.lensPresetId ein (gruen = gebunden,
	// weiss = nicht gebunden) -- ergaenzt die Einfaerbung, die RebuildLensButtons bereits beim (Neu-)Bau
	// vornimmt, fuer den Fall, dass sich rig.lensPresetId AENDERT, OHNE dass die Reihe neu gebaut wird
	// (aktuell nicht erreichbar, da jede Bindung ueber OnClick_LensPreset->RefreshRigWidgets laeuft, siehe
	// dort -- als eigenstaendige, wiederverwendbare Methode dennoch sauberer als Duplikat-Code inline).
	protected void RefreshLensButtonHighlight() {
		if ( !m_LensButtonToLens )
			return;

		string currentLensId = m_Module.m_CameraRig.lensPresetId;

		array< UIActionBase > buttons = m_LensButtonToLens.GetKeyArray();
		int count = buttons.Count();
		int i = 0;
		while ( i < count ) {
			UIActionBase buttonBase = buttons.Get( i );
			UIActionButton button;
			if ( Class.CastTo( button, buttonBase ) ) {
				PCT_LensPreset lens;
				if ( m_LensButtonToLens.Find( buttonBase, lens ) && lens ) {
					if ( lens.id == currentLensId )
						button.SetColor( COLOR_GREEN );
					else
						button.SetColor( COLOR_WHITE );
				}
			}

			i = i + 1;
		}
	}

	// Alles, was NUR vom tatsaechlich gebundenen rig.lensPresetId abhaengt (Prime-Lock, Blenden-Grenzen,
	// Info-Zeile, Sensor-Hinweis, Button-Highlight) -- aufgerufen nach jedem Linsen-Klick, jedem Kit-Tab-
	// Wechsel und aus RefreshRigWidgets() (ApplyShot-Synchronisation). AENDERT NICHT die Kit-SelectionBox/
	// den angezeigten Tab (dafuer: SyncLensKitTabToRig, nur bei externen Rig-Aenderungen noetig).
	protected void RefreshLensStateWidgets() {
		PCT_LensPreset lens = FindLensPresetInModule( m_Module.m_CameraRig.lensPresetId );

		bool locked = false;
		if ( lens )
			locked = PCT_LensLibrary.IsPrime( lens );

		SetFocalControlsEnabled( !locked );

		RefreshApertureSliderBounds( lens );
		RefreshLensInfoText( lens );

		PCT_LensKit boundKit = null;
		if ( lens )
			boundKit = FindLensKitById( lens.kitId );
		RefreshLensCoverageHint( boundKit );

		RefreshLensButtonHighlight();
	}

	// Springt auf den zu rig.lensPresetId PASSENDEN Kit-/Einzellinsen-Tab und baut dessen Buttons neu
	// (aufgerufen aus OnInit/RefreshRigWidgets, also bei Erstaufbau und nach externen Rig-Aenderungen wie
	// ApplyShot/ApplyAutoFraming) -- NICHT aus OnChange_LensKit (dort wuerde ein Tab-Wechsel durch den
	// Nutzer sonst sofort wieder auf den zur gebundenen Linse gehoerenden Tab zurueckspringen, bevor der
	// Nutzer eine neue Linse aus dem gerade erst geoeffneten Tab anklicken konnte).
	protected void SyncLensKitTabToRig() {
		PCT_LensPreset lens = FindLensPresetInModule( m_Module.m_CameraRig.lensPresetId );

		if ( !lens ) {
			if ( m_LensKitSelectBox )
				m_LensKitSelectBox.SetSelection( 0, false );
			m_SelectedLensKit = null;
			RebuildLensButtons( null );
			RefreshLensCharacterText( null );
			return;
		}

		if ( lens.kitId == "" ) {
			int standaloneIndex = ResolveStandaloneSelectIndex();
			if ( m_LensKitSelectBox && standaloneIndex >= 0 )
				m_LensKitSelectBox.SetSelection( standaloneIndex, false );
			m_SelectedLensKit = null;
			RebuildLensButtons( ResolveStandaloneLenses() );
			RefreshLensCharacterText( null );
			return;
		}

		int kitIndex = ResolveKitSelectIndex( lens.kitId );
		if ( m_LensKitSelectBox && kitIndex >= 0 )
			m_LensKitSelectBox.SetSelection( kitIndex, false );

		m_SelectedLensKit = FindLensKitById( lens.kitId );
		RebuildLensButtons( ResolveKitLenses( m_SelectedLensKit ) );
		RefreshLensCharacterText( m_SelectedLensKit );
	}
}
