// Task P3-2 -- Dual-Path-Datenmodell, Code zeichengetreu aus Docs/Plan_B7_Kamerafahrten_Bewegungssteuerung.md:
// PCT_SpeedProfile (Section 8.1), PCT_PathPoint/PCT_CameraPath (Section 3.4), PCT_LookPoint/PCT_LookPath
// (Section 5.3), PCT_Track (Section 10.3). PCT_TrackKey/PCT_CustomCurve liegen bereits in
// Data/PCT_CustomCurve.c (Task P3-1) und werden hier NICHT gedoppelt (Section 13.1 listet sie als
// "bereits gezeigt"). Non-ASCII der Plan-Kommentare ist auf ASCII normalisiert (ae/ue/ss, "--");
// Feldnamen/Typen/Defaults sind unveraendert zeichengetreu (JSON-Keys, Plan_B3 Section 1).
// JSON-Regeln: Member ohne Praefix, jeder mit Default, ref nur auf Objekt-/Array-Membern,
// Objekt-/Array-Init im Konstruktor, keine Logik (POD).

// Plan_B7 Section 8.1 -- Timing/Geschwindigkeit EINES Segments ("entweder feste Dauer oder feste
// Geschwindigkeit", Quelle Section 5). distanceM ist ABGELEITET (Arc-Length-Cache), nie editiert.
class PCT_SpeedProfile
{
	string mode = "duration";			// "duration" | "speed"
	float durationSeconds = 3.0;		// Modus "duration"
	float speedMS = 1.0;				// Modus "speed" (Meter/Sekunde)
	float accelMS2 = 0.0;				// Beschleunigung (0 = keine Rampe; Rampe sonst via Easing Section 9)
	float decelMS2 = 0.0;				// Verzoegerung
	float minSpeedMS = 0.0;
	float maxSpeedMS = 10.0;
	float distanceM = 0.0;				// abgeleitet (Arc-Length), NICHT editiert -- nur Cache/Anzeige
}

// Plan_B7 Section 3.4 -- ein Kamerapfad-Punkt; traegt die Eigenschaften des ABGEHENDEN Segments
// (Section 3.1: Segmenteigenschaften liegen am Startpunkt des Segments).
class PCT_PathPoint
{
	vector position = "0 0 0";
	float cameraHeight = 0.0;			// > 0 ueberschreibt position[1]
	string pathType = "SmoothCurve";	// EPCT_PathType-String -- abgehendes Segment
	string behavior = "ExactThrough";	// EPCT_PointBehavior-String
	float holdSeconds = 0.0;			// nur WaitHold
	string easing = "SmoothTransition";	// UNABHAENGIGES Kamerapfad-Easing (Section 9)
	ref PCT_SpeedProfile speed;			// Timing/Geschwindigkeit des Segments (Section 8)
	bool hasTiltOverride = false;
	float tiltDegrees = 0.0;
	float focalLengthMm = 0.0;			// 0 = kein Override
	float focusDistanceM = 0.0;			// 0 = kein Override
	vector bezierOut = "0 0 0";			// P1-Handle (nur Bezier)
	vector bezierIn = "0 0 0";			// P2-Handle des eingehenden Segments (nur Bezier)

	void PCT_PathPoint()
	{
		speed = new PCT_SpeedProfile();
	}
}

// Plan_B7 Section 3.4 -- autoritativer Kamerapfad. Praezedenz der Kurventyp-Felder (Section 3.5):
// point.pathType ist autoritativ je Segment; curveDefault ist NUR Fallback/Seed neuer Punkte;
// PCT_Shot.positionCurveType ist ab v2 deprecated (reiner Migrations-Seed, Section 13.3).
class PCT_CameraPath
{
	string curveDefault = "SmoothCurve";		// Fallback-Pfadtyp
	ref array<ref PCT_PathPoint> points;

	void PCT_CameraPath()
	{
		points = new array<ref PCT_PathPoint>();
	}
}

// Plan_B7 Section 5.3 -- ein Blickpfad-Punkt (eigene Zeitachse, unabhaengig vom Kamerapfad).
class PCT_LookPoint
{
	string lookMode = "LookAt";			// EPCT_LookMode-String
	vector worldTarget = "0 0 0";		// fester Weltpunkt (wenn targetRef == "")
	string targetRef = "";				// Objekt-/ActorMark-Referenz (Aufloesung Laufzeit; Schema B3)
	string targetBone = "";				// "Head"/"RightHand"/... ; "" = Objektursprung
	string aimRegion = "wholeFigure";	// Kopf/Augen/Oberkoerper/Haende/Figur/Fahrzeug/Requisite/Punkt
	vector offset = "0 0 0";			// OffsetTarget: Versatz (Weltmeter)
	vector screenAnchor = "0.5 0.5 0";	// TargetLock: Ziel-Bildposition (0..1); Drittel z. B. "0.333 0.5 0"
	string easing = "SmoothTransition";	// UNABHAENGIGES Blickpfad-Easing (Section 9)
	ref PCT_SpeedProfile timing;		// eigene Blick-Segmentzeit (Dauer bis zum naechsten Ziel)
	float smoothTime = 0.3;				// FollowTarget: SmoothCD-Daempfung
	float leadSeconds = 0.4;			// LeadTarget: Vorhaltzeit
	bool horizonLock = false;			// additiv (M1): ueberlagert LookAt/FreeRotation, ersetzt sie nicht (Plan_B7 Section 5.2)

	void PCT_LookPoint()
	{
		timing = new PCT_SpeedProfile();
	}
}

// Plan_B7 Section 5.3 -- autoritativer Blickpfad (null am Shot = kein separater Blick, Section 13.1).
class PCT_LookPath
{
	ref array<ref PCT_LookPoint> points;

	void PCT_LookPath()
	{
		points = new array<ref PCT_LookPoint>();
	}
}

// Plan_B7 Section 10.3 -- eine skalare Keyframe-Spur (Kanal + eigene Keys). Kamerapfad und Blickpfad
// sind SONDER-Spuren (PCT_CameraPath/PCT_LookPath); PCT_TrackKey liegt in Data/PCT_CustomCurve.c.
class PCT_Track
{
	string channel = "focalLength";	// EPCT_TrackChannel-String (Section 10.1)
	bool enabled = true;
	ref array<ref PCT_TrackKey> keys;

	void PCT_Track()
	{
		keys = new array<ref PCT_TrackKey>();
	}
}
