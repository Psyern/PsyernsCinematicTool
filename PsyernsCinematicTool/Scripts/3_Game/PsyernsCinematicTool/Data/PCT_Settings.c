// Task P2-1 -- PCT_Settings, Code zeichengetreu aus Docs/Plan_B3_Datenmodell_Persistenz.md Section 2.11
// uebernommen (ASCII-normalisierte Kommentare, Felder/Defaults unveraendert). Wurzel-Datenklasse der Datei
// $profile:PsyernsCinematicTool\Settings.json (Client-Praeferenzen, Plan_B3 Section 1.2/4.1).
//
// Task P2-4 (Auto-Framing, Docs/Phase1_O1_SetFOV_Protokoll.md Nebenbefund + Docs/Plan_B6_Roadmap_Risiken.md
// Section "Auto-Framing"/O2): additive Erweiterung um `defaultSubjectHeightM` -- B3 Section 2.11 kennt
// dieses Feld NICHT (geprueft: die obigen 7 Felder sind die vollstaendige Section-2.11-Liste). Die
// Motivhoehen-Messung per BoundingBox-API fuer Nicht-Spieler-Objekte ist O2 (Plan_B6 Section 5,
// "Existiert eine nutzbare BoundingBox-API ... Fallback: konfigurierte Standardhoehe 1.8 m") -- O2 wird
// bewusst NICHT implementiert (Task-Brief), stattdessen liefert dieses Feld die feste Fallback-Motivhoehe
// fuer JEDES per Raycast gefundene Motiv (Spieler UND generische Objekte gleichermassen, siehe
// PCT_CinematicModule.ResolveAutoFramingTarget) -- additiv, kein bestehendes Feld veraendert, daher
// migrations-/kompatibilitaetsneutral (Regel M1, Plan_B3 Section 5).
class PCT_Settings
{
	int schemaVersion = 1;
	string lastSequenceId = "";
	string lastShotId = "";
	string defaultSensorPresetId = "sensor_fullframe";
	string defaultLensPresetId = "lens_50_f18";
	bool showCompositionGrid = true;	// Drittelraster-Overlay (Plan_A1 Section 9)
	string defaultAspectMask = "16:9";
	string csvDelimiter = ";";			// Plan_B3 Section 8.1
	float defaultSubjectHeightM = 1.8;	// Task P2-4 / O2-Fallback (Plan_B6 Section 5), additiv -- kein B3-Feld

	// Task P2-5 (Docs/Plan_B8_Objektivsystem_LensKits.md Section 3.3/5.2): Escape-Hatch fuer die
	// CloseFocus-Clamp (PCT_CinematicCamera.ApplyDepthOfField) -- true simuliert Extension-Tubes/
	// Naheinstellvorsaetze (Clamp auf rig.minFocusDistanceM aus, Live-Fokusdistanz wird ungeklemmt
	// angewendet). Default false = Clamp aktiv (ZEISS-Dokument: "Die virtuelle Kamera darf nicht
	// naeher fokussieren."). Additiv per M1 (Plan_B3 Section 5.1) -- kein B3-Feld, kein bestehendes
	// Feld veraendert.
	bool allowCloseFocusOverride = false;
}
