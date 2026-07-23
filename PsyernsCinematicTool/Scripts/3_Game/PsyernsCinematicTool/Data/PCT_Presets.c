// Task P2-1 -- Preset-Familien (Sensor/Lens/Framing/Angle/Movement/Light/WorldState) + Container-Klassen,
// Code zeichengetreu aus Docs/Plan_B3_Datenmodell_Persistenz.md Section 2.10 uebernommen (ASCII-normalisierte
// Kommentare, Felder/Defaults/Konstruktoren unveraendert).
// Dateiname: Plan_B1_Architektur.md Section 3 listet den Dateibaum-Eintrag als "PCT_Presets.c" (Plural,
// "Preset-Familien + Container, siehe Plan_B3") -- gemaess Task-Brief-Regel "B1-Baum gewinnt" wird dieser
// Name statt des im Task-Brief nur beispielhaft genannten "PCT_Preset.c" verwendet (siehe Task-P2-1-Report).
//
// Container-Muster: Plan_B3 Section 2.10 schreibt nur PCT_LensPresetFile und PCT_WorldStatePresetFile explizit
// aus ("hier exemplarisch zwei, die uebrigen folgen demselben Muster"). Die uebrigen 5 Container (Sensor/
// Framing/Angle/Movement/Light) sind hier nach demselben Muster ergaenzt: schemaVersion = 1,
// ref array<ref T> presets, Konstruktor initialisiert das Array leer. Jede Familie liegt in einer eigenen
// Container-Datei (Presets\Sensors.json, Lenses.json, Framings.json, Angles.json, Movements.json, Lights.json,
// WorldStates.json -- Plan_B3 Section 1.2/4.1).
//
// Gemeinsames Feld "isBuiltin": mitgelieferte Presets werden bei jedem Start aus Code-Defaults regeneriert,
// Nutzer-Presets (isBuiltin = false) bleiben unangetastet (Plan_B3 Section 2.10, Entscheidung E4).
//
// PCT_LightPreset referenziert PCT_LightSetup (PCT_LightSetup.c, Task P2-1); PCT_WorldStatePreset
// referenziert PCT_WorldState (PCT_WorldState.c, Task P2-1) -- beide Dateien liegen im selben Layer/PBO,
// datei-uebergreifende Typreferenzen sind in diesem Mod-Paket ueblich (z. B. PCT_Shot.c -> PCT_CameraRig).

class PCT_SensorPreset
{
	string id = "";
	string displayName = "";
	float widthMm = 36.0;
	float heightMm = 24.0;
	float cropFactor = 1.0;
	bool isBuiltin = true;
}

// Task P2-5 (Docs/Plan_B8_Objektivsystem_LensKits.md Section 3.1) -- additive Erweiterung um die
// ZEISS-Radiance/Lens-Kit-Metadatenfelder ("B7 additiv" im Plan-Kommentar, hier P2-5, siehe Task-P2-5-
// Report fuer die Andockpunkt-Abweichung). Alle sechs neuen Felder tragen eigene Defaults (M1-konform,
// Plan_B3 Section 5.1) -- eine bestehende Lenses.json ohne diese Keys laedt unveraendert weiter
// (JsonFileLoader: fehlender Key -> Default, siehe PCT_Storage.c O-B7-4-Kommentar).
class PCT_LensPreset
{
	string id = "";
	string displayName = "";
	float focalLengthMm = 50.0;
	float focalLengthMaxMm = 0.0;		// > 0 = Zoomobjektiv (Bereich min..max), 0 = Festbrennweite
	float maxAperture = 2.8;
	float minFocusDistanceM = 0.45;
	float anamorphicFactor = 1.0;
	bool isBuiltin = true;
	// --- Task P2-5 additiv (alle mit Default -> M1-konform, Plan_B8 Section 3.1) ---
	float minAperture = 0.0;			// kleinste Oeffnung (groesste Zahl), z. B. 22.0; 0 = unbekannt -> UI/Clamp nutzen 22
	string apertureScale = "f";			// "f" | "T" -- nur Anzeige (T1.5 vs f/1.5)
	float weightKg = 0.0;				// 0 = nicht dokumentiert
	float lengthMm = 0.0;				// 0 = nicht dokumentiert
	float frontDiameterMm = 0.0;		// 0 = nicht dokumentiert
	string kitId = "";					// Zugehoerigkeit (informativ; massgeblich ist PCT_LensKit.lensPresetIds)
	float hFovFullFrameDeg = 0.0;		// Datenblatt-Metadatum, NIE fuer Rendering (Plan_B8 Section 2.3)
	float hFovSuper35Deg = 0.0;			// Datenblatt-Metadatum
}

// Task P2-5 (Docs/Plan_B8_Objektivsystem_LensKits.md Section 3.2) -- Kit-Container: gruppiert Linsen per
// ID-Referenzliste (flache Linsenliste bleibt in PCT_LensPreset/Lenses.json, Entscheidung Plan_B8
// Section 2.1). Zeichengetreu aus dem Plan uebernommen.
class PCT_LensKit
{
	string id = "";
	string displayName = "";
	string manufacturer = "";
	string family = "";
	string mount = "";					// z. B. "PL"
	string focusScale = "metric";		// "metric" | "imperial"
	string coverage = "fullFrame";		// "fullFrame" | "super35" | "largeFormat"
	string opticalCharacter = "";		// Freitext-Anzeige, z. B. "Modern - Warm - Controlled Flare - Gentle Sharpness"
	string coating = "";				// z. B. "ZEISS T* blue"
	float frontDiameterMm = 0.0;
	bool hasExtendedData = false;		// Doku-Metadatum (ZEISS eXtended Data) -- keine Engine-Funktion
	string flareProfileId = "";			// AP-B (nicht Teil dieses Tasks): Referenz auf PCT_FlareProfile ("" = kein Look)
	bool isBuiltin = true;
	ref array<string> lensPresetIds;

	void PCT_LensKit()
	{
		lensPresetIds = new array<string>();
	}
}

class PCT_LensKitFile
{
	int schemaVersion = 1;
	ref array<ref PCT_LensKit> kits;

	void PCT_LensKitFile()
	{
		kits = new array<ref PCT_LensKit>();
	}
}

class PCT_FramingPreset					// Einstellungsgroessen, Plan_A1 Section 3
{
	string id = "";
	string displayName = "";
	string shortCode = "";				// "EWS", "WS", "MS", "MCU", "CU", "BCU", "ECU", ...
	float subjectScreenHeight = 0.5;	// Anteil der SICHTBAREN Motivspanne (Kopfoberkante..Schnittlinie) an der Bildhoehe (0..1) -- Fix-Wave/"Visible-Height-Reinterpretation", siehe PCT_Framing.c
	float headroom = 0.08;				// Anteil der Bildhoehe
	float lookRoom = 0.10;
	float bodyCutRatio = 0.0;			// Fix-Wave (Task P2-4-Review), additiv (Regel M1): Anteil der Koerperhoehe UNTERHALB der Schnittlinie -- 0 = Ganzkoerper, 0.53 = Schnitt an Huefte, 0.285 = Schnitt am Knie, 0.82 = Schnitt an Schulter, usw. (siehe PCT_Framing.HeightModeBaseHeight-Ratios). Dokumentiert die bisher offene Luecke aus Plan_B3 Section 2.10 (subjectScreenHeight ohne Bezug auf einen partiellen Bildausschnitt).
	float focalLengthMinMm = 24.0;		// empfohlener Brennweitenbereich (Startwerte, keine Regel -- Plan_A1 Section 3)
	float focalLengthMaxMm = 50.0;
	string description = "";
	bool isBuiltin = true;
}

class PCT_AnglePreset					// Kamerawinkel/-hoehen, Plan_A1 Section 4
{
	string id = "";
	string displayName = "";
	string heightMode = "eyeLevel";		// "eyeLevel" | "shoulder" | "hip" | "knee" | "ground" | "absolute"
	float heightOffsetM = 0.0;			// Zusatz-Offset zur Modus-Hoehe; bei "absolute": absolute Hoehe ueber Boden
	float pitchDegrees = 0.0;			// negativ = nach unten (High Angle)
	float rollDegrees = 0.0;			// Dutch Angle: 15 / 25 / 40 als Builtin-Varianten (Plan_A1 Section 4)
	string description = "";
	bool isBuiltin = true;
}

class PCT_MovementPreset				// Kamerabewegungen, Plan_A1 Section 6
{
	string id = "";
	string displayName = "";
	string movementType = "static";		// "static" | "dollyIn" | "dollyOut" | "truck" | "pedestal" | "pan" | "tilt" | "orbit" | "craneUp" | "handheld" | "zoomIn" | "zoomOut" | "dollyZoom"
	float durationSeconds = 5.0;
	string easing = "Smooth";			// wie PCT_Keyframe.easing
	float speedMS = 1.0;				// Metern/Sekunde bzw. Grad/Sekunde bei pan/tilt/orbit
	float orbitRadiusM = 3.0;
	float orbitDegrees = 90.0;
	float handheldAmplitude = 0.0;		// Handheld-Simulation (summierte Sinus-Drift)
	float handheldFrequency = 0.0;
	string description = "";
	bool isBuiltin = true;
}

class PCT_LightPreset					// Licht-Presets, Plan_A1 Section 8 (z. B. Three-Point, Rembrandt)
{
	string id = "";
	string displayName = "";
	string description = "";
	bool isBuiltin = true;
	ref array<ref PCT_LightSetup> lights;	// Positionen relativ zum Preset-Anker (Anker = Motiv)

	void PCT_LightPreset()
	{
		lights = new array<ref PCT_LightSetup>();
	}
}

class PCT_WorldStatePreset				// benannte Welt-Snapshots ("Golden Hour", "Moonlight", ...)
{
	string id = "";
	string displayName = "";
	bool isBuiltin = true;
	ref PCT_WorldState state;

	void PCT_WorldStatePreset()
	{
		state = new PCT_WorldState();
	}
}

// Container-Klassen -- je Familie eine Wurzel-Datei (Presets\<Familie>.json). PCT_LensPresetFile und
// PCT_WorldStatePresetFile sind in Plan_B3 Section 2.10 woertlich vorgegeben; die uebrigen 5 folgen
// demselben Muster (siehe Datei-Kopfkommentar).

class PCT_SensorPresetFile
{
	int schemaVersion = 1;
	ref array<ref PCT_SensorPreset> presets;

	void PCT_SensorPresetFile()
	{
		presets = new array<ref PCT_SensorPreset>();
	}
}

class PCT_LensPresetFile
{
	int schemaVersion = 1;
	ref array<ref PCT_LensPreset> presets;

	void PCT_LensPresetFile()
	{
		presets = new array<ref PCT_LensPreset>();
	}
}

class PCT_FramingPresetFile
{
	int schemaVersion = 1;
	ref array<ref PCT_FramingPreset> presets;

	void PCT_FramingPresetFile()
	{
		presets = new array<ref PCT_FramingPreset>();
	}
}

class PCT_AnglePresetFile
{
	int schemaVersion = 1;
	ref array<ref PCT_AnglePreset> presets;

	void PCT_AnglePresetFile()
	{
		presets = new array<ref PCT_AnglePreset>();
	}
}

class PCT_MovementPresetFile
{
	int schemaVersion = 1;
	ref array<ref PCT_MovementPreset> presets;

	void PCT_MovementPresetFile()
	{
		presets = new array<ref PCT_MovementPreset>();
	}
}

class PCT_LightPresetFile
{
	int schemaVersion = 1;
	ref array<ref PCT_LightPreset> presets;

	void PCT_LightPresetFile()
	{
		presets = new array<ref PCT_LightPreset>();
	}
}

class PCT_WorldStatePresetFile
{
	int schemaVersion = 1;
	ref array<ref PCT_WorldStatePreset> presets;

	void PCT_WorldStatePresetFile()
	{
		presets = new array<ref PCT_WorldStatePreset>();
	}
}
