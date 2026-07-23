// Task P2-1 -- PCT_WorldState, Code zeichengetreu aus Docs/Plan_B3_Datenmodell_Persistenz.md Section 2.7
// uebernommen (ASCII-normalisierte Kommentare, Felder/Defaults unveraendert). Eingebettete Klasse (kein
// eigenes schemaVersion -- Version liegt an der jeweiligen Wurzeldatei, Plan_B3 Section 5).
// Referenziert von: PCT_Shot.worldStateOverride (PCT_Shot.c, Task P2-1) und PCT_WorldStatePreset.state
// (PCT_Presets.c, Task P2-1). PCT_Sequence.worldState (Plan_B3 Section 2.5) ist Phase 3 und hier NICHT
// implementiert (siehe Task-P2-1-Report). Anwendung der Felder (World.SetDate/SetTimeMultiplier/Weather.*)
// ist Phase 4 -- diese Klasse ist reine POD-Datenhaltung ohne Logik.
class PCT_WorldState
{
	int year = 2020;
	int month = 6;
	int day = 21;
	int hour = 12;
	int minute = 0;
	bool freezeTime = true;				// Anwendung: SetTimeMultiplier(0)
	float timeMultiplier = 0.0;			// 0..64; nur relevant, wenn freezeTime = false
	float overcast = 0.2;				// 0..1
	float fog = 0.05;					// 0..1
	float rain = 0.0;					// 0..1
	float snowfall = 0.0;				// 0..1
	float windMagnitude = 2.0;			// m/s
	float windDirectionDegrees = 225.0;	// Anwendung: Weather.AngleToWindDirection(...)
	bool freezeWeather = true;			// Anwendung: SetWeatherUpdateFreeze(true)
}
