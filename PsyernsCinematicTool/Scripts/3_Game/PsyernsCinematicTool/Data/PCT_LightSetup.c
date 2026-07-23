// Task P2-1 -- PCT_LightSetup, Code zeichengetreu aus Docs/Plan_B3_Datenmodell_Persistenz.md Section 2.9
// uebernommen (ASCII-normalisierte Kommentare, Felder/Defaults unveraendert). Eingebettete Klasse (kein
// eigenes schemaVersion -- Version liegt an der jeweiligen Wurzeldatei, Plan_B3 Section 5).
// Referenziert von: PCT_LightPreset.lights (PCT_Presets.c, Task P2-1). PCT_Sequence.lights (Plan_B3
// Section 2.5) ist Phase 3 und hier NICHT implementiert (siehe Task-P2-1-Report).
// Anwendung ist Phase 4 (ScriptedLightBase.CreateLight/SetBrightnessTo/SetRadiusTo); die exakte API fuer
// Farbe/Schattenwurf/Spot-Oeffnungswinkel ist laut Plan_B3 Section 2.9 "OFFEN -- ZU VERIFIZIEREN". Die Felder
// sind trotzdem jetzt definiert (Migrationsregel M1-sicher, Plan_B3 Section 2.9); diese Klasse ist reine
// POD-Datenhaltung ohne Logik.
class PCT_LightSetup
{
	string id = "";
	string displayName = "";
	string lightType = "point";			// "point" | "spot"
	vector position = "0 0 0";			// Weltkoordinaten; in Presets relativ zum Preset-Anker
	vector orientation = "0 0 0";		// nur fuer Spot relevant
	float colorR = 1.0;					// 0..1 -- Anwendung OFFEN (Plan_B3 Section 2.9)
	float colorG = 1.0;
	float colorB = 1.0;
	float brightness = 1.0;				// SetBrightnessTo
	float radiusM = 10.0;				// SetRadiusTo
	bool castsShadow = true;			// Anwendung OFFEN (Plan_B3 Section 2.9)
	float spotAngleDegrees = 45.0;		// Anwendung OFFEN (Plan_B3 Section 2.9)
}
