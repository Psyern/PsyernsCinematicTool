// Task P3-1 -- EPCT_Easing, zeichengetreu aus Docs/Plan_B7_Kamerafahrten_Bewegungssteuerung.md Section 9.1
// uebernommen ("Erweiterung des in Plan_B3 Section 2.1 / Plan_B4 Section 4.6 definierten Sets"). Plan_B3
// Section 2.1 deklariert dasselbe Enum bislang nur als 3-Werte-ENTWURF (LINEAR/SMOOTH/SMOOTHER) OHNE
// Code-Konsumenten (siehe Persistence/PCT_Storage.c Kopfkommentar "EPCT_Easing/EPCT_CurveType bewusst
// NICHT eingefuehrt") -- Plan_B7 Section 9.1 ist die massgebliche, VOLLSTAENDIGE Fassung und wird hier
// zum ersten Mal tatsaechlich als Code eingefuehrt. Werte 0-2 sind absichtlich identisch zum B3-Entwurf
// (additive Erweiterung, keine Neuvergabe bestehender Werte -- Plan_B7 Section 9.1 Kommentar "additiv,
// Migration M1").
//
// Ablageort-Entscheidung (Task-P3-1-Brief: "eigene Datei Data\PCT_Enums.c falls B7/B1 das nahelegen, sonst
// zu PCT_Math"): Docs/Plan_B1_Architektur.md Section 3 listet fuer Data\ nur konkrete Datenklassen-Dateien
// (PCT_Shot.c, PCT_CameraRig.c, ...) und fuer eigenstaendige Enum-Dateien nur das Einzelbeispiel PCT_RPC.c
// (1 Enum, 1 Datei -- Section 3 "PCT_RPC.c (enum EPCT_RPC)"). Plan_B7 kuendigt aber FUENF neue/erweiterte
// Laufzeit-Enums ueber die Sequencer-Phasen an (Section 13-Uebersichtstabelle: EPCT_PathType, EPCT_LookMode,
// EPCT_PointBehavior, EPCT_Easing [hier], EPCT_TrackChannel) -- keines davon ist das 1:1-Alleinstellungs-
// merkmal einer einzelnen Datenklassen-Datei wie EPCT_RPC/PCT_RPC.c. Eine gemeinsame Data\PCT_Enums.c
// vermeidet, dass diese Enums nachtraeglich in fachfremde Dateien (z. B. PCT_Math.c fuer ein reines
// Positions-/Blickmodus-Enum) gequetscht werden muessen. Diese Datei ist der vorgesehene Sammelort fuer
// ALLE kommenden B7-Laufzeit-Enums, nicht nur EPCT_Easing -- spaetere Tasks ergaenzen hier additiv.
enum EPCT_Easing
{
	LINEAR = 0,				// Plan_B4 Section 4.6: f(t) = t
	SMOOTH,					// Plan_B4 Section 4.6: f(t) = t*t*(3-2t) (Smoothstep, COT JMCinematicCamera.SmoothStep)
	SMOOTHER,				// Plan_B4 Section 4.6: f(t) = t^3*(t*(6t-15)+10) (Smootherstep, quintisch)
	EASE_IN,				// Plan_B4 Section 4.6 Laufzeit-Erweiterung: f(t) = t*t
	EASE_OUT,				// Plan_B4 Section 4.6 Laufzeit-Erweiterung: f(t) = 1-(1-t)*(1-t)
	EASE_IN_OUT,			// Plan_B7 Section 9.1/9.2: Quelle "Ease In/Out" == SMOOTH-Semantik
	HARD_START,				// Plan_B7 Section 9.1/9.2 (Quelle Section 6): f(t) = 1-(1-t)^3
	HARD_STOP,				// Plan_B7 Section 9.1/9.2 (Quelle Section 6): f(t) = t^3
	FAST_TRANSITION,		// Plan_B7 Section 9.1/9.2 (Quelle Section 6): EaseRamped(t, 0.15)
	SMOOTH_TRANSITION,		// Plan_B7 Section 9.1/9.2 (Quelle Section 6): EaseRamped(t, 0.45)
	CUSTOM_CURVE			// Plan_B7 Section 9.1/9.2/9.4 (Quelle Section 6): PCT_CustomCurve-Kontrollpunkte, siehe PCT_Math.EaseCustom
}

// Task P3-2 -- EPCT_PathType, Member exakt in der Reihenfolge der Plan_B7-Section-4-Tabelle
// (Kurventyp des ABGEHENDEN Segments eines PCT_PathPoint; im JSON als String, Plan_B7 Section 3.2/3.4).
enum EPCT_PathType
{
	LINEAR = 0,				// direkte Gerade (vector.Lerp)
	SMOOTH_CURVE,			// per-Segment-lokales Catmull-Rom (PCT_Math.CatmullRomSegment, Plan_B7 Section 4 "Pro-Segment-Lokalitaet")
	BEZIER,					// kubische Bezier mit bezierOut/bezierIn-Handles (PCT_Math.CubicBezier)
	ARC,					// Kreisbogen (Phase 4)
	ORBIT,					// Bewegung um ein Motiv (Pfadtyp UND impliziter LookAt, Plan_B7 Section 4)
	RAIL,					// Spline wie SMOOTH_CURVE, aber mit gesperrter Blickfreiheit (Phase 5)
	HANDHELD_PATH			// Basis-Spline + additiver Handheld-Offset (Plan_B4 Section 5.8)
}

// Task P3-2 -- EPCT_PointBehavior, Member exakt in der Reihenfolge der Plan_B7-Section-3.3-Tabelle
// (Punktverhalten eines PCT_PathPoint; im JSON als String).
enum EPCT_PointBehavior
{
	EXACT_THROUGH = 0,		// Kamera faehrt exakt durch den Punkt (Durch-Punkt, volle Arc-Length-Behandlung)
	DIRECTION_MARKER_ONLY,	// nur Richtungsmarke/Kontrollpunkt, kein Ankunftswegpunkt (an B4 V4 gekoppelt)
	STOP_AT,				// Kamera stoppt am Punkt (tEased -> 1, Geschwindigkeit auf 0)
	WAIT_HOLD,				// Kamera wartet holdSeconds am Punkt (Position friert, globale Zeit laeuft weiter)
	CONTINUE_PASS			// ohne Unterbrechung geschwindigkeitsstetig weiterfahren
}

// Task P3-2 -- EPCT_LookMode, Member exakt in der Reihenfolge der Plan_B7-Section-5.2-Tabelle
// (Blickrichtungsmodus eines PCT_LookPoint; im JSON als String).
enum EPCT_LookMode
{
	LOOK_AT = 0,			// Basis-Yaw/Pitch aus (target - camPos).VectorToAngles() (Plan_B7 Section 5.2)
	FOLLOW_TARGET,			// Ziel-Anker ueber Math.SmoothCD geglaettet (Plan_B4 Section 5.7)
	LEAD_TARGET,			// Vorhalt: target + velocity * leadSeconds (Phase 4)
	OFFSET_TARGET,			// Blickziel = target + offset (Phase 4)
	FREE_ROTATION,			// Orientierung allein aus den Pan/Tilt/Roll-Tracks (Migrations-Modus alter Shots)
	HORIZON_LOCK,			// additiver Modifikator: Roll-Komponente nach Track-Ueberlagerung auf 0 (ueberlagert, ersetzt nicht)
	TARGET_LOCK				// per-Frame-Constraint: Ziel auf fester Bildposition (screenAnchor, Phase 4, V8)
}

// Task P3-2 -- EPCT_TrackChannel, zeichengetreu aus Plan_B7 Section 10.3 ("Beispiel-Kanal-Enum
// (Laufzeit, Strings im JSON -- analog EPCT_Easing)").
// Review-Fix-Wave (Minor-Punkt 5): der Dispatch (SampleTracks/ApplyChannel, PCT_MotionPlayer.c) laeuft
// bewusst per String (JSON-Form; kein ParseTrackChannel noetig, die Kanal-Menge ist klein) -- dieses
// Enum dient nur als dokumentierte Kanalliste, nicht als Laufzeit-Parser-Ziel (Plan_B7 Section 10.3).
enum EPCT_TrackChannel
{
	FOCAL_LENGTH = 0,
	FOCUS_DISTANCE,
	APERTURE,
	HEIGHT,
	PAN,
	TILT,
	ROLL,
	HANDHELD,
	STABILIZE,
	SUBJECT_SCREEN_X,
	SUBJECT_SCREEN_Y,
	SPEED
}
