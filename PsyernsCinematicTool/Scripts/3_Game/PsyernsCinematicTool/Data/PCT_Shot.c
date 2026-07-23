// Task P2-1 -- PCT_Shot + PCT_Keyframe, Code zeichengetreu aus Docs/Plan_B3_Datenmodell_Persistenz.md
// Section 2.2 (PCT_Keyframe) und Section 2.4 (PCT_Shot) uebernommen. schemaVersion liegt nur an der
// Wurzeldatei PCT_Shot (Plan_B3 Section 5) -- PCT_Keyframe ist eingebettet und traegt keine eigene Version.
// Non-ASCII-Sonderzeichen der Plan-Prosa (Umlaute, Gedankenstrich, Section-Zeichen) sind in den //-Kommentaren
// unten auf ASCII normalisiert (ae/oe/ue/ss, "--", "Section n"); Feldnamen/Typen/Defaults sind unveraendert
// zeichengetreu aus dem Plan uebernommen.
//
// Dokumentierte Abweichung vom Task-Scope-Text (siehe Task-P2-1-Report fuer die volle Begruendung): der
// Task-Brief listet "PCT_Keyframe" unter den NICHT-Klassen (zusammen mit PCT_Sequence/Sequencer), und
// Plan_B1 Section 3 ordnet PCT_Keyframe organisatorisch bei PCT_Sequence.c ein. PCT_Shot (Plan_B3 Section 2.4,
// zeichengetreu gefordert) hat aber selbst in schemaVersion 1 das Member "ref array<ref PCT_Keyframe>
// keyframes" -- ohne die Klasse waere PCT_Shot nicht kompilierbar und die JSON-Gegenprobe (Plan_B3
// Section 3.1, Feld "keyframes") nicht erfuellbar. Plan_B3 Section 5.3 bestaetigt zudem explizit, dass
// "keyframes[] (alle Kanaele aus Section 2.2)" Teil von schemaVersion 1 ist, und Plan_B7 Section 13.1 zeigt,
// dass "PCT_Keyframe" auch nach dem Phase-3-Dual-Path-Schema als gesampelte Kompat-/Export-Sicht bestehen
// bleibt -- es ist keine reine Sequencer-Playback-Klasse, sondern Teil des Shot-Datenmodells selbst.
// Entscheidung: PCT_Keyframe wird hier (statt in einer separaten PCT_Sequence.c) mitgeliefert, weil es in
// schemaVersion 1 ausschliesslich von PCT_Shot referenziert wird. Ausgeschlossen bleiben PCT_Sequence,
// PCT_Project, PCT_ActorMark und alles, was Mehrere-Shots-Container/Sequencer-Playback-Logik betrifft.
class PCT_Keyframe
{
	float time = 0.0;					// Sekunden ab Shot-Beginn, >= 0, aufsteigend
	vector position = "0 0 0";			// Weltkoordinaten (Meter)
	vector orientation = "0 0 0";		// <Yaw, Pitch, Roll> in GRAD (wie Object.GetOrientation)
	float fovDegrees = 27.0;			// vertikaler FOV in GRAD (27.0 ~= 50 mm Vollformat); Anwendung: SetFOV(fovDegrees * Math.DEG2RAD)
	float focusDistance = 2.0;			// Meter, 0..1000 (PPEDOF.PARAM_FOCUS_DIST-Range; Default = PPEDOF-Default 2.0)
	float blurStrength = 0.0;			// 0..100 (COT-Slider-Range "Blur strength"); 0 = DOF aus
	string easing = "Smooth";			// "Linear" | "Smooth" | "Smoother" -- Zeitverlauf des Segments zu DIESEM Keyframe hin
}

// PCT_Shot -- Wurzel-Datenklasse einer Shot-Datei (Shots\<id>.json). Kein separater PCT_ShotFile-Wrapper:
// PCT_Shot traegt schemaVersion bereits selbst und wird direkt ueber JsonFileLoader<PCT_Shot> gespeichert/
// geladen (Plan_B3 Section 4.3) -- ein Wurzel-Container ist fuer Shots nicht vorgesehen.
// Rig-Einbettung: nutzt die bestehende PCT_CameraRig-Klasse (Scripts/3_Game/PsyernsCinematicTool/Data/
// PCT_CameraRig.c, Task P1-5) unveraendert -- Feld-Deckung 1:1 gegen Plan_B3 Section 2.3, siehe Task-P2-1-
// Report fuer die Diff-Tabelle. Keine Aenderung an der bestehenden Klasse.
// Task P3-2 -- Dual-Path-Erweiterung nach Plan_B7 Section 13.1: schemaVersion-Bump 1 -> 2 (nur PCT_Shot;
// PCT_Sequence bleibt v1, Section 13.3), neue Member cameraPath/lookPath/tracks/dollyZoomLock.
// keyframes[] BLEIBT als gesampelte Kompat-/Export-Sicht (Section 10.2); positionCurveType ist ab v2
// deprecated (nur noch Migrations-Seed, Section 3.5/13.3 -- vom Player ignoriert, autoritativ ist
// PCT_PathPoint.pathType mit cameraPath.curveDefault als Fallback).
class PCT_Shot
{
	int schemaVersion = 2;
	string id = "";						// = Dateiname ohne .json, z. B. "sh_010_cu_b"
	string name = "";
	string description = "";
	string sceneNumber = "";
	string shotNumber = "";
	string framingPresetId = "";		// Referenz in Presets\Framings.json (z. B. "framing_cu")
	string anglePresetId = "";			// Referenz in Presets\Angles.json (z. B. "angle_eye_level")
	string movementPresetId = "";		// Referenz in Presets\Movements.json (z. B. "move_dolly_in")
	string positionCurveType = "CatmullRom";	// "Linear" | "CatmullRom" | "NaturalCubic" | "UniformCubic" (Positions-Spline ueber alle Keyframes)
	string thumbnailFile = "";			// Dateiname in Exports\ (Screenshot, Plan_B3 Section 8.3)
	int priority = 2;					// 1 = hoch, 2 = normal, 3 = optional
	string status = "planned";			// "planned" | "blocked" | "approved" | "shot"
	string notes = "";
	ref PCT_CameraRig cameraRig;
	ref array<ref PCT_Keyframe> keyframes;	// BLEIBT: gesampelte Kompat-/Export-Sicht (Plan_B7 Section 10.2)
	ref PCT_WorldState worldStateOverride;	// null = Sequenz-Weltzustand erben
	ref PCT_CameraPath cameraPath;			// NEU (v2): autoritativer Kamerapfad (Plan_B7 Section 13.1)
	ref PCT_LookPath lookPath;				// NEU (v2): autoritativer Blickpfad (null = kein separater Blick)
	ref array<ref PCT_Track> tracks;		// NEU (v2): skalare Kanaele (Plan_B7 Section 10)
	bool dollyZoomLock = false;				// NEU (v2): Dolly-Zoom-Auto-Kompensation (Plan_B7 Section 7)

	void PCT_Shot()
	{
		cameraRig = new PCT_CameraRig();
		keyframes = new array<ref PCT_Keyframe>();
		worldStateOverride = null;
		cameraPath = new PCT_CameraPath();
		lookPath = null;
		tracks = new array<ref PCT_Track>();
	}
}
