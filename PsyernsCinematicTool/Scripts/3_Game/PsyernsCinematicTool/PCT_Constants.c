class PCT_Constants
{
	static const string PCT_VERSION = "0.1.0";

	static const string DIR_ROOT = "$profile:PsyernsCinematicTool\\";

	static const string DIR_SHOTS = DIR_ROOT + "Shots\\";
	static const string DIR_SEQUENCES = DIR_ROOT + "Sequences\\";
	static const string DIR_PRESETS = DIR_ROOT + "Presets\\";
	static const string DIR_EXPORTS = DIR_ROOT + "Exports\\";

	static const string FILE_SETTINGS = DIR_ROOT + "Settings.json";

	// Task P2-2 -- Preset-Container-Pfade je Familie, zeichengetreu aus Docs/Plan_B3_Datenmodell_Persistenz.md
	// Section 4.1 uebernommen (dort bereits vollstaendig als PCT_Constants-Code-Block vorgegeben). DIR_PROJECTS/
	// SCHEMA_VERSION_SEQUENCE/SCHEMA_VERSION_PROJECT sind bewusst NICHT ergaenzt -- PCT_Sequence/PCT_Project
	// existieren als Datenklassen noch nicht (Phase 3, siehe PCT_Shot.c-Kopfkommentar zu P2-1), ein Pfad/eine
	// Versionskonstante ohne Konsumenten waere spekulativ (CLAUDE.md "Avoid speculative or nice-to-have changes").
	static const string FILE_PRESETS_SENSORS = DIR_PRESETS + "Sensors.json";
	static const string FILE_PRESETS_LENSES = DIR_PRESETS + "Lenses.json";
	// Task P2-5 (Docs/Plan_B8_Objektivsystem_LensKits.md Section 3.3) -- Kit-Container-Datei, folgt
	// demselben DIR_PRESETS-Muster wie die uebrigen Familien-Pfade.
	static const string FILE_PRESETS_LENSKITS = DIR_PRESETS + "LensKits.json";
	static const string FILE_PRESETS_FRAMINGS = DIR_PRESETS + "Framings.json";
	static const string FILE_PRESETS_ANGLES = DIR_PRESETS + "Angles.json";
	static const string FILE_PRESETS_MOVEMENTS = DIR_PRESETS + "Movements.json";
	static const string FILE_PRESETS_LIGHTS = DIR_PRESETS + "Lights.json";
	static const string FILE_PRESETS_WORLDSTATES = DIR_PRESETS + "WorldStates.json";

	static const string EXT_JSON = ".json";
	static const string EXT_CSV = ".csv";

	static const int SCHEMA_VERSION_SHOT = 2;	// Task P3-2: Dual-Path-Bump (Plan_B7 Section 13.3; nur PCT_Shot)
	static const int SCHEMA_VERSION_PRESETS = 1;
	static const int SCHEMA_VERSION_SETTINGS = 1;

	// O1/V1-Diagnose (Plan_B4 Section 2.5, Plan_B6 Section 5, Task P1-2): User-Options FOV-Grenzen,
	// exakt aus gameplay.c:1316-1317 (OPTIONS_FIELD_OF_VIEW_MIN/MAX -- dort auskommentiert, PCT
	// verwendet sie nur als Log-Referenz fuer den O1-Test, liest sie nicht aus den echten
	// User-Options aus).
	static const float FOV_OPTIONS_MIN = 0.75242724772;
	static const float FOV_OPTIONS_MAX = 1.30322025726;

	// Task P1-5 / O1-Konsequenz #1 (Docs/Phase1_O1_SetFOV_Protokoll.md Section "Konsequenzen"): Camera.SetFOV
	// clampt selbst NICHT -- PCT_CameraRig.GetVerticalFovRad() clampt deshalb auf diese Grenzen, damit das Rig
	// nie einen undarstellbaren Wert liefert. 0.1199 rad = 200 mm, 1.4173 rad = 14 mm (Plan_B4 Section 2.4
	// Brennweiten-Preset-Reihe 14-200 mm, komplett abgedeckt); beide Werte im Feldtest 22.07.2026 bis
	// mindestens 2.5 rad unclamped bestaetigt (O1-Protokoll).
	static const float PCT_FOV_MIN_RAD = 0.1199;
	static const float PCT_FOV_MAX_RAD = 1.4173;

	// Review-Fix P1-7 (Fix 1): Brennweiten-Aequivalent zum obigen PCT_FOV_MIN_RAD/MAX_RAD-Clamp
	// (O1-Protokoll, Docs/Phase1_O1_SetFOV_Protokoll.md), als eigene mm-Konstanten fuer den DOF-Pfad
	// (PCT_CinematicCamera.ApplyDepthOfField): der Brennweiten-Slider erlaubt 8-400 mm, das
	// gerenderte FOV wird ueber PCT_CameraRig.GetVerticalFovRad() aber auf den 14-200-mm-Bereich
	// geclampt -- BlurFromAperture/OverrideDOF muessen dieselbe effektive Brennweite verwenden,
	// sonst laeuft die Bokeh-Staerke bei Werten ausserhalb 14-200 mm dem sichtbaren FOV davon.
	static const float PCT_FOCAL_MIN_MM = 14.0;
	static const float PCT_FOCAL_MAX_MM = 200.0;
}
