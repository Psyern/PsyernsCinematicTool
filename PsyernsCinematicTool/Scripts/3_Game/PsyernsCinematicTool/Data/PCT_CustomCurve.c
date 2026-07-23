// Task P3-1 -- PCT_TrackKey + PCT_CustomCurve, zeichengetreu aus Docs/Plan_B7_Kamerafahrten_Bewegungssteuerung.md
// Section 10.3 (PCT_TrackKey -- dort bereits "Phase 3" markiert) und Section 9.4 (PCT_CustomCurve -- dort
// als "Phase 5" markiert). Der Task-P3-1-Brief listet "CustomCurve nach Section 9.4 (Datenform +
// Auswertung -- was der Plan spezifiziert)" jedoch explizit unter "Zu implementieren" fuer diese Phase,
// weil PCT_Math.EaseCustom (Utils/PCT_Math.c, Section 9.3) ohne die Datenform nicht kompilieren wuerde.
// Deshalb wird hier NUR das minimale Datengeruest ergaenzt, das EaseCustom strukturell braucht: PCT_TrackKey
// als (time, value, easing)-Kontrollpunkt und PCT_CustomCurve als deren geordnete Liste. Der volle
// Multi-Track-Kanal-Apparat (PCT_Track, EPCT_TrackChannel, Section 10.1/10.2 -- UI-Editierbarkeit,
// Kanal-Zuordnung) bleibt AUSSERHALB des P3-1-Scopes (eigener spaeterer Multi-Track-Task); diese Datei
// fuehrt bewusst nur das ein, was CustomCurve/EaseCustom strukturell voraussetzen.
class PCT_TrackKey
{
	float time = 0.0;				// Sekunden ab Shot-Beginn (bei CustomCurve: normiert 0..1, siehe PCT_CustomCurve)
	float value = 0.0;				// Kanalwert (Einheit je Kanal; bei CustomCurve: normiert 0..1)
	string easing = "Smooth";		// EPCT_Easing-String fuer das Segment ZU diesem Key (allgemeiner Track-Kontext, Section 10.3) -- von PCT_CustomCurve/EaseCustom selbst NICHT ausgewertet (die Segmente zwischen Kontrollpunkten sind laut Section 9.4 linear)
}

// Plan_B7 Section 9.4 (Quelle Section 6 "benutzerdefinierte Geschwindigkeitskurve"): Kontrollpunkte
// (time in [0,1], value in [0,1]), monoton steigend in time. Ausgewertet von PCT_Math.EaseCustom
// (Utils/PCT_Math.c) -- stueckweise lineare Interpolation zwischen den Nachbar-Kontrollpunkten.
class PCT_CustomCurve
{
	ref array<ref PCT_TrackKey> controlPoints;	// (time in [0,1], value in [0,1]), monoton in time

	void PCT_CustomCurve()
	{
		controlPoints = new array<ref PCT_TrackKey>();
	}
}
