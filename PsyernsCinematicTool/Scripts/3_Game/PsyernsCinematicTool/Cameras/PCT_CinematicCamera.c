// Phase-1-minimal: Erbt Freecam-Bewegung/Input 1:1 von JMCinematicCamera
// (JMCinematicCamera.c). Einzige eigene OnUpdate-Logik bisher ist der FOV-Hold (P1-4,
// s.u.) -- Frame-genaue Kamerastate-Anwendung fuer Position, Rotation, Fokus und Roll
// folgt erst mit dem Sequencer in einer spaeteren Phase.
class PCT_CinematicCamera: JMCinematicCamera
{
	// K3 (Plan_B1_Architektur.md Section 5.2): COTs globales "CurrentActiveCamera"
	// (JMCameraBase.c:11, Datei-Scope, kein `ref`) wird von PCT NIE geschrieben, nur
	// gelesen (K1-Guard in PCT_CinematicModule.Enter). s_PCT_ActiveCamera ist PCTs
	// eigenes Aequivalent, deklariert ohne `ref` (identisch zu CurrentActiveCamera) und
	// als Klassen-Static (identisch zu JMCinematicCamera.s_COT_CinematicCamera,
	// JMCinematicCamera.c:3), qualifiziert erreichbar als
	// PCT_CinematicCamera.s_PCT_ActiveCamera.
	//
	// Lebenszyklus (Setzen/Loeschen) ist modul-seitig, exakt wie COT es fuer
	// CurrentActiveCamera handhabt: gesetzt in JMCameraModule.Client_Enter
	// (JMCameraModule.c:280, Cast von Camera.GetCurrentCamera()), geloescht in
	// JMCameraModule.Client_Leave (JMCameraModule.c:418, "CurrentActiveCamera = null").
	// Weder JMCameraBase noch JMCinematicCamera setzen/loeschen CurrentActiveCamera in
	// ihrem eigenen Konstruktor/Destruktor (verifiziert: JMCameraBase.c:39-64 hat keinen
	// Zugriff auf CurrentActiveCamera). PCT_CinematicModule.Client_Enter/Client_Leave
	// spiegeln diese Zuweisung entsprechend fuer s_PCT_ActiveCamera.
	static PCT_CinematicCamera s_PCT_ActiveCamera;

	// P1-4/FOV-Hold: Feldtest zeigt, dass gesetzte FOV-Werte sofort wieder auf ~1.0
	// zurueckspringen (fremder Schreiber -- vermutlich COTs eigener Modul-Lerp auf der
	// Freecam, moeglicherweise zusaetzlich engine-/versionsspezifische Ruecksetzer).
	// PCT wendet seinen FOV-Zielwert deshalb ab jetzt selbst jeden Frame an ("Hold"),
	// nachdem super.OnUpdate() (JMCinematicCamera.OnUpdate, s.u.) gelaufen ist -- das ist
	// zugleich das geplante Phase-1-Rig-Verhalten (Docs/Plan_B4_Kamera_Engine.md
	// Section 1-2: Kamera wendet ihren Zustand selbst pro Frame an). 0.0 = kein Hold
	// aktiv (Default, damit unveraendertes COT-FOV-Verhalten bestehen bleibt, solange
	// RunFovProbe() nie einen Hold-Wert setzt).
	float m_PCT_HoldFovRad = 0.0;

	// Task P1-6 (Architektur-Vorgabe): Das Rig ist Besitz des MODULS (5_Mission, editiert von der Form);
	// die KAMERA (3_Game) wendet es pro Frame an. 3_Game darf 5_Mission nie referenzieren -> die Kamera
	// erhaelt stattdessen eine eigene Referenz auf die Rig-DATENKLASSE (3_Game/Data/PCT_CameraRig.c,
	// keine Layer-Verletzung). `ref`, weil Member-Objektreferenz (CLAUDE.md: ref auf Membern, die
	// Objektreferenzen halten). Gesetzt vom Modul beim client-seitigen Kamera-Erhalt (Client_Enter UND
	// Offline-Zweig von Server_Enter, siehe PCT_CinematicModule.c). null = kein Rig aktiv (Diag-Faelle
	// ohne Rig, z. B. RunFovProbe/O1-Panel) -- OnUpdate faellt dann auf den bisherigen FOV-Hold zurueck.
	ref PCT_CameraRig m_PCT_Rig;

	// Task P2-5 (Docs/Plan_B8_Objektivsystem_LensKits.md Section 5.2, CloseFocus-Clamp): dieselbe
	// Andockart wie m_PCT_Rig oben -- die Kamera (3_Game) haelt eine eigene Referenz auf die Settings-
	// DATENKLASSE (3_Game/Data/PCT_Settings.c, keine Layer-Verletzung), Besitz bleibt beim Modul
	// (5_Mission), das m_Settings bereits einmalig in OnMissionLoaded() laedt (Task P2-3). Gesetzt vom
	// Modul beim client-seitigen Kamera-Erhalt (Client_Enter UND Offline-Zweig von Server_Enter, exakt wie
	// m_PCT_Rig/m_PCT_FocusDistanceM). null = Settings noch nicht geladen/kein Modul-Zugriff (Diag-Faelle)
	// -- ApplyDepthOfField() faellt dann sicherheitshalber auf "Clamp aktiv" zurueck (siehe dort).
	ref PCT_Settings m_PCT_Settings;

	// Live-Fokusdistanz der Kamera in Metern. Bewusst KEIN Rig-Member (Plan_B4 Section 2.2-Hinweis: "Das
	// Rig traegt... keine focusDistance -- die Fokusdistanz ist Keyframe-Kanal... bzw. Live-Zustand der
	// Kamera, nicht fotografische Rig-Absicht"). Default 5.0 m als neutraler Phase-1-Platzhalter, bis
	// Autofokus-Raycast/Keyframes existieren (Plan_B4 Section 3.3/3.4, Phase 3).
	float m_PCT_FocusDistanceM = 5.0;

	// Task P1-6 / Plan_B4 Section 9 Dirty-Flag-Regel ("PPE nur bei Aenderung... Dirty-Flag"): Werte von
	// (focusDistance, blur, focalLength), fuer die zuletzt tatsaechlich OverrideDOF/SetFocus aufgerufen
	// wurde. -1.0 als Sentinel (kein gueltiger fStop-/blur-/focalLength-Wert kann negativ sein) erzwingt
	// beim allerersten DOF-Frame isDirty=true unabhaengig vom Epsilon-Vergleich.
	float m_PCT_LastFocusDistance = -1.0;
	float m_PCT_LastBlur = -1.0;
	float m_PCT_LastFocalLength = -1.0;

	// true, solange die Engine aktuell eine von PCT gesetzte OverrideDOF(true, ...) haelt. Entscheidet,
	// ob beim Ueberschreiten von aperture >= PCT_DOF_MAX_APERTURE (f/22, DOF aus) einmalig
	// ResetDOFOverride() noetig ist, statt es (unnoetig) jeden Frame erneut aufzurufen.
	bool m_PCT_DofActive = false;

	// Task P1-6 / Plan_B4 Section 3.1-Parametertabelle: focusDepthOffset-Konstante (COT
	// CAMERA_DOFFSET-Muster, JMCameraModule.c:131 uebergibt dort ebenfalls eine feste Konstante).
	static const float PCT_DOF_FOCUS_DEPTH_OFFSET = 10.0;

	// Plan_B4 Section 3.2-Tabelle, letzte Zeile: f-Stop 22 -> blurBase 0.00 ("DOF aus"). Ab hier bleibt
	// DOF deaktiviert (Task-Brief Punkt 2).
	static const float PCT_DOF_MAX_APERTURE = 22.0;

	// Epsilon fuer den Dirty-Flag-Vergleich oben. focusDistance ist Meter, blur ist 0..1-normiert,
	// focalLength ist mm -- 0.001 ist fuer alle drei klein genug, um echte Wertaenderungen zuverlaessig
	// von Gleitkomma-Rauschen zu unterscheiden (kein Kanal aendert sich in Phase 1 kontinuierlich pro
	// Frame; Rack-Focus-Interpolation folgt erst mit dem Sequencer, Plan_B4 Section 3.4/4, Phase 3).
	static const float PCT_DOF_EPSILON = 0.001;

	// override von JMCinematicCamera.OnUpdate( float timeslice ) (JMCinematicCamera.c:44,
	// selbst override von JMCameraBase.OnUpdate( float timeslice ), JMCameraBase.c:117 --
	// dort keine eigene FOV-Logik, nur eine leere Basis-Implementierung). Exakte
	// Basisklassen-Signatur uebernommen. super.OnUpdate() zuerst (Pflicht,
	// CLAUDE.md/JMCinematicCamera.c:46-Muster) -- laesst COTs Freecam-Bewegung/Input
	// unveraendert laufen, bevor PCT danach seinen eigenen FOV-Hold erzwingt.
	//
	// Task P1-6 Prioritaetsreihenfolge (Task-Brief Punkt 1): Rig gesetzt -> Rig-FOV + DOF-Anwendung
	// haben Vorrang, der alte FOV-Hold greift dann NICHT. Kein Rig (z. B. Diag-Panel ohne Rig,
	// RunFovProbe/O1-Test) -> bisheriges Hold-Verhalten unveraendert (P1-4).
	// Task P3-3: Dual-Path-Playback (Plan_B7 Section 14.1 -- "Die Kamera haelt ref PCT_MotionPlayer;
	// im Zustand PLAYBACK ruft OnUpdate m_MotionPlayer.Tick(this, timeslice)"). Der Tick laeuft VOR der
	// Rig-Anwendung unten: die Track-Kanaele schreiben focalLength/aperture/focusDistance ins Rig bzw.
	// die Kamera, und der bestehende Rig-Pfad wendet daraus FOV/DOF an -- EIN Schreiber je Kanal
	// (Plan_B7 Section 14.4 Kompositionsregel).
	ref PCT_MotionPlayer m_PCT_MotionPlayer;

	override void OnUpdate( float timeslice )
	{
		super.OnUpdate( timeslice );

		if ( m_PCT_MotionPlayer && m_PCT_MotionPlayer.IsPlaying() )
			m_PCT_MotionPlayer.Tick( this, timeslice );

		if ( m_PCT_Rig )
		{
			float fovRad = m_PCT_Rig.GetVerticalFovRad();
			SetFOV( fovRad );

			ApplyDepthOfField();
		}
		else if ( m_PCT_HoldFovRad > 0.0 )
		{
			SetFOV( m_PCT_HoldFovRad );
		}
	}

	// Task P1-6 / Plan_B4 Section 3.1 -- DOF anwenden (COT-Muster: SetFocus + OverrideDOF kombiniert,
	// JMCameraModule.c:130-131), an die Brief-Architektur angepasst: die Kamera besitzt m_PCT_Rig/
	// m_PCT_FocusDistanceM bereits als eigene Member, daher keine Cam/Rig/focusDistance-Parameter wie im
	// Plan-Code (Section-3.1-Skizze `ApplyDepthOfField(Camera cam, PCT_CameraRig rig, float focusDistance)`) --
	// nur intern aus OnUpdate aufgerufen, m_PCT_Rig ist dort bereits als gesetzt bekannt (kein
	// Doppel-Null-Check).
	//
	// Signaturen nachgeschlagen (nicht geraten), Docs/Research/Research_Vanilla_APIs.md Section 3 UND
	// direkt in der Vanilla-Quelle verifiziert:
	//   - Camera.SetFocus(float distance, float blur) -- scripts-1.29 3_Game/DayZ/Entities/Camera.c:74.
	//   - PPEffects.OverrideDOF(bool enable, float focusDistance, float focusLength,
	//     float focusLengthNear, float blur, float focusDepthOffset) -- PPEffects.c:503-505 (ruft
	//     g_Game.OverrideDOF(...) mit identischer Parameterreihenfolge auf; g_Game.OverrideDOF selbst:
	//     Game.c:1354, exakt dieselbe Signatur).
	//   - PPEffects.ResetDOFOverride() -- PPEffects.c:518-521: `OverrideDOF(false,0,0,0,0,1);` (reiner
	//     Passthrough zu g_Game.OverrideDOF ohne eigenen State/Guard) -- daher gefahrlos aufrufbar auch
	//     ohne vorheriges Override (setzt einfach enable=false, unabhaengig vom vorherigen Zustand).
	//
	// Bei aperture >= PCT_DOF_MAX_APERTURE (f/22) ist DOF aus (Plan_B4 Section 3.2-Tabelle, letzte
	// Zeile). PPEffects.ResetDOFOverride() dann NUR EINMALIG beim Uebergang (m_PCT_DofActive-Flag),
	// nicht jeden Frame -- Performance-Budget Plan_B4 Section 9 "PPE nur bei Aenderung". Kein `new` in
	// dieser Funktion (Risiko R5, Task-Brief: keine Allokationen im Per-Frame-Pfad).
	private void ApplyDepthOfField()
	{
		float aperture = m_PCT_Rig.aperture;

		if ( aperture >= PCT_DOF_MAX_APERTURE )
		{
			if ( m_PCT_DofActive )
			{
				PPEffects.ResetDOFOverride();
				m_PCT_DofActive = false;
			}

			return;
		}

		// Review-Fix P1-7 (Fix 1): der Brennweiten-Slider erlaubt 8-400 mm, aber
		// PCT_CameraRig.GetVerticalFovRad() clampt das gerenderte FOV auf den zu PCT_FOCAL_MIN_MM/
		// MAX_MM (14-200 mm) aequivalenten Bereich (PCT_Constants.PCT_FOV_MIN_RAD/MAX_RAD, O1-
		// Protokoll). BlurFromAperture/OverrideDOF muessen dieselbe effektive Brennweite sehen wie das
		// FOV, sonst laeuft die DOF-Staerke bei Rohwerten ausserhalb 14-200 mm dem sichtbaren Bild davon.
		float effectiveFocalMm = Math.Clamp( m_PCT_Rig.focalLengthMm, PCT_Constants.PCT_FOCAL_MIN_MM, PCT_Constants.PCT_FOCAL_MAX_MM );
		float blur = PCT_Math.BlurFromAperture( aperture, effectiveFocalMm );
		float focusLength = effectiveFocalMm;
		float focusLengthNear = focusLength * 0.5;

		// Task P2-5 (Plan_B8 Section 5.2, CloseFocus-Clamp): harter Clamp der Live-Fokusdistanz auf
		// rig.minFocusDistanceM ("Die virtuelle Kamera darf nicht naeher fokussieren", ZEISS-Dokument),
		// sofern nicht per PCT_Settings.allowCloseFocusOverride abgeschaltet (Extension-Tube-Simulation).
		// m_PCT_Settings kann null sein (Diag-Pfade ohne Modul-Zuweisung) -- overrideAllowed faellt dann
		// sicherheitshalber auf false zurueck (lieber clampen als ein unmoegliches Nah-Fokus zulassen).
		// Abweichung vom Plan-Code-Schnipsel (Section 5.2 zeigt nur die zwei Zeilen isoliert): hier direkt
		// in effectiveFocus verrechnet, weil diese Kamera (anders als die Plan-Skizze) KEINE separate
		// Warnung-bei-Unterschreitung implementiert (Plan_B4 Section 7 "Paragraph 7 Warnung" existiert im
		// realen Code bislang nur als Kommentar-Verweis auf PCT_CameraRig.minFocusDistanceM, siehe dort --
		// kein bestehender Warn-Konsument, den es hier zusaetzlich zu bedienen gaebe; ausserhalb des
		// AP-A-Task-3-Scopes "CloseFocus-Clamp exakt wie Section 5.2", siehe Task-P2-5-Report).
		bool overrideAllowed = false;
		if ( m_PCT_Settings )
			overrideAllowed = m_PCT_Settings.allowCloseFocusOverride;

		float effectiveFocus = m_PCT_FocusDistanceM;
		if ( !overrideAllowed && m_PCT_Rig.minFocusDistanceM > 0.0 )
			effectiveFocus = Math.Max( m_PCT_FocusDistanceM, m_PCT_Rig.minFocusDistanceM );

		// Dirty-Flag-Regel (Plan_B4 Section 9, Task-Brief Punkt 2): OverrideDOF/SetFocus nur bei
		// tatsaechlicher Aenderung von (focusDistance, blur, focalLength) seit dem letzten angewendeten
		// Frame aufrufen. Zwischenvariablen fuer jeden Epsilon-Vergleich (Segfault-Regel: komplexe
		// Ausdruecke nicht inline verschachteln). Vergleich/Anwendung nutzen ab jetzt effectiveFocus (den
		// tatsaechlich angewendeten, ggf. geclampten Wert) statt des rohen m_PCT_FocusDistanceM-Wunschwerts.
		float deltaFocus = Math.AbsFloat( effectiveFocus - m_PCT_LastFocusDistance );
		float deltaBlur = Math.AbsFloat( blur - m_PCT_LastBlur );
		float deltaFocal = Math.AbsFloat( focusLength - m_PCT_LastFocalLength );
		bool changedFocus = deltaFocus > PCT_DOF_EPSILON;
		bool changedBlur = deltaBlur > PCT_DOF_EPSILON;
		bool changedFocal = deltaFocal > PCT_DOF_EPSILON;
		bool isDirty = changedFocus || changedBlur || changedFocal || !m_PCT_DofActive;

		if ( !isDirty )
			return;

		SetFocus( effectiveFocus, blur );
		PPEffects.OverrideDOF( true, effectiveFocus, focusLength, focusLengthNear, blur, PCT_DOF_FOCUS_DEPTH_OFFSET );

		m_PCT_LastFocusDistance = effectiveFocus;
		m_PCT_LastBlur = blur;
		m_PCT_LastFocalLength = focusLength;
		m_PCT_DofActive = true;
	}
}
