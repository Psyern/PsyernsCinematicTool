# Plan B3 — Datenmodell, JSON-Schemata & Persistenz

**Stand:** 2026-07-21 · **Status:** Entwurf v1 · **Mod:** PsyernsCinematicTool (PCT) · **Autor:** Psyern

**Abstract:**
Dieses Dokument definiert das vollständige Datenmodell des PsyernsCinematicTool: die Hierarchie `PCT_Project → PCT_Sequence → PCT_Shot → PCT_Keyframe`, den Kamera-Zustand `PCT_CameraRig`, sechs Preset-Familien, den Welt-Snapshot `PCT_WorldState` sowie `PCT_ActorMark` und `PCT_LightSetup`. Alle Datenklassen werden als EnScript-Klassen mit JSON-konformen Membern (ohne Präfix, mit Defaultwerten) spezifiziert und über den Engine-`JsonFileLoader<T>` nach `$profile:PsyernsCinematicTool\` persistiert. Zusätzlich festgelegt: Schema-Versionierung mit Migrationsregeln, Validierung/Clamping nach dem Laden, das Sync-Modell (Phase 1–3 rein clientseitig, Phase 4+ optionaler Sequence-Broadcast) und die Export-Formate (ShotList-CSV/JSON, Screenshot-Konvention). Jede API-Aussage stützt sich auf die verifizierten Research-Dokumente; Unbelegtes ist als „OFFEN — ZU VERIFIZIEREN" markiert.

---

## 1. Datenmodell-Übersicht

### 1.1 Struktur (ASCII-Baum)

```
PCT_Project  (optional, ab Phase 3)                        PCT_Settings  (Settings.json)
 └─ sequenceIds[] ─────► PCT_Sequence  (Sequences\<id>.json)
                          ├─ shotIds[] ─────► PCT_Shot  (Shots\<id>.json)
                          │                    ├─ PCT_CameraRig          (eingebettet: Sensor/Objektiv-Zustand)
                          │                    ├─ PCT_Keyframe[]        (eingebettet: Kamera-Animationskanäle)
                          │                    └─ PCT_WorldState        (eingebettet, optionaler Override; null = erbt)
                          ├─ PCT_WorldState                (eingebettet: Datum/Zeit/Wetter der Sequenz)
                          ├─ PCT_ActorMark[]               (eingebettet: Darstellermarken/Blocking)
                          └─ PCT_LightSetup[]              (eingebettet: Lichtaufbau der Sequenz)

Preset-Familien  (Presets\<Familie>.json, je Familie EINE Container-Datei):
  PCT_SensorPresetFile    ─► PCT_SensorPreset[]      (Sensorformate)
  PCT_LensPresetFile      ─► PCT_LensPreset[]        (Objektive)
  PCT_FramingPresetFile   ─► PCT_FramingPreset[]     (Einstellungsgrößen, Plan_A1 §3)
  PCT_AnglePresetFile     ─► PCT_AnglePreset[]       (Kamerawinkel/-höhen, Plan_A1 §4)
  PCT_MovementPresetFile  ─► PCT_MovementPreset[]    (Kamerabewegungen, Plan_A1 §6)
  PCT_LightPresetFile     ─► PCT_LightPreset[] ─► PCT_LightSetup[]  (Licht-Presets, Plan_A1 §8)
  PCT_WorldStatePresetFile─► PCT_WorldStatePreset[] ─► PCT_WorldState  (benannte Welt-Snapshots)
```

### 1.2 Datei-Ablage (Mapping Klasse → Datei)

| Klasse (Wurzel der Datei) | Datei | Phase | Anmerkung |
|---|---|---|---|
| `PCT_Settings` | `$profile:PsyernsCinematicTool\Settings.json` | 1 | Client-Präferenzen |
| `PCT_Shot` | `...\Shots\<shotId>.json` | 1 | eine Datei pro Shot; Schema v1 = volle Feldmenge aus §2.4 |
| `PCT_Sequence` | `...\Sequences\<sequenceId>.json` | 3 | eine Datei pro Sequenz |
| `PCT_Project` | `...\Projects\<projectId>.json` | 3 | Unterordner `Projects\` wird in Phase 3 additiv ergänzt |
| `PCT_SensorPresetFile` … `PCT_WorldStatePresetFile` | `...\Presets\Sensors.json`, `Lenses.json`, `Framings.json`, `Angles.json`, `Movements.json`, `Lights.json`, `WorldStates.json` | 2 (Sensor/Lens/Framing/Angle/Movement), 4 (Lights/WorldStates) | eine Container-Datei je Familie |
| CSV/JSON-Exporte, Screenshots | `...\Exports\` | 2+ | siehe §8 |

### 1.3 Architektur-Entscheidungen

**E1 — Referenz statt Einbettung (Sequence → Shot per `shotIds`).**
Shots werden als eigenständige Dateien gespeichert und von Sequenzen nur per ID (= Dateiname ohne `.json`) referenziert. Begründung: Shots sind wiederverwendbar (derselbe Shot in Varianten-Sequenzen), einzeln editier-/teilbar, und ein defekter Shot zerstört nicht die ganze Sequenz. Verworfen: vollständige Einbettung der Shots in die Sequenz-Datei — erzeugt Duplikate, macht Einzel-Shot-Austausch (z. B. per Discord) unmöglich und lässt Dateien unbegrenzt wachsen.

**E2 — Eine Datei pro Entität statt Monolith.**
Pro Shot/Sequenz/Projekt eine JSON-Datei (COT-Muster: eine Datei pro Loadout — direkt im COT-Quellcode verifiziert: `JMConstants.c:9/30` definiert `DIR_COT`/`DIR_LOADOUTS`, `JMLoadoutSettings.c:46/62` lädt/speichert je Loadout eine eigene Datei darunter; vgl. §4.1). Begründung: Absturz-/Korruptionsradius bleibt klein, selektives Laden ist billig, Nutzer können Dateien einzeln sichern und weitergeben. Verworfen: eine `project.json` mit allem — ein einziger Schreibfehler verliert das Gesamtwerk.

**E3 — Blocking auf Sequenz-Ebene, Welt-Override auf Shot-Ebene.**
`PCT_ActorMark[]`, `PCT_LightSetup[]` und `PCT_WorldState` liegen an der Sequenz (filmisch: das „Setup" teilt sich Blocking und Licht über mehrere Shots, vgl. Plan_A1 §11-Hierarchie). Ein Shot kann nur den Welt-Zustand überschreiben (`worldStateOverride`, z. B. Nacht-Variante eines Shots). Verworfen: Actors/Lichter pro Shot — massive Duplikation und Kontinuitätsfehler beim Editieren.

**E4 — Preset-Familien als Container-Datei je Familie.**
Presets sind klein und zahlreich; je Familie eine Datei mit `schemaVersion` + Array. Nutzer-Presets werden an dasselbe Array angehängt (`isBuiltin = false` unterscheidet). Verworfen: eine Datei pro Preset — Dutzende Kleinstdateien ohne Nutzen; und ein globales „Presets.json" — Familien hätten keinen unabhängigen Versionszyklus.

**E5 — Keyframe speichert absolute Werte, Rig speichert fotografische Absicht.**
`PCT_Keyframe.fovDegrees` ist die abspielbare Wahrheit; `PCT_CameraRig` (Sensor + Brennweite) ist der Generator, aus dem die UI den FOV berechnet (`fovV = 2 · atan(sensorHeightMm / (2 · focalLengthMm))`, Plan_A1 §5; die Engine interpretiert FOV vertikal in Radiant, Research_Vanilla_APIs.md §2). Begründung: Zoom und Dolly-Zoom animieren den FOV pro Keyframe — eine reine Ableitung aus dem Rig zur Abspielzeit könnte das nicht abbilden. Verworfen: FOV nur im Rig.

---

## 2. Datenklassen (EnScript)

**Konventionen für alle JSON-Datenklassen** (Abweichung von den allgemeinen Namensregeln, projektweit verbindlich, vgl. `.claude/CLAUDE.md` „JSON config class members — no prefix"):

- Member **ohne** Präfix, **camelCase**, exakt identisch mit den JSON-Keys (der Engine-Serializer schreibt Membernamen 1:1).
- **Jeder Member hat einen Defaultwert** — Skalare inline, Objekt-/Array-Member im Konstruktor. So erhalten fehlende JSON-Keys beim Laden deterministische Defaults (Migrationsregel M1, §5).
- `ref` **nur** auf Objekt-/Array-Membern (nie auf Parametern/Locals/Returns).
- Ablageort im Mod: `Scripts/3_Game/PsyernsCinematicTool/Data/` (Datenklassen sind Layer-3_Game-tauglich: keine Welt-Entities).
- `vector`-Member serialisieren als JSON-Array `[x, y, z]` — **verifiziert** am COT-Bestand: `JMTeleportLocation.Position` (`vector`) → `ServerProfile\CommunityOnlineTools\Teleports.json` (`"Position": [8090.49, 0, 9326.95]`), geschrieben von `JsonFileLoader<JMTeleportSerialize>` (`JMTeleportSerialize.c:25/53`).

**Entscheidung E6 — `fovDegrees` wird in GRAD gespeichert.** `Camera.SetFOV(float fov)` erwartet Radiant (vertikal, Research_Vanilla_APIs.md §1). Gespeichert wird trotzdem in Grad; die Umrechnung passiert erst beim Anwenden: `camera.SetFOV(fovDegrees * Math.DEG2RAD)` — exakt das Muster des Vanilla-CameraTools (`SetFOV(deg * Math.DEG2RAD)`, `CameraToolsMenu.c:719ff`, Research_Vanilla_APIs.md §1). Begründung: Menschenlesbarkeit der JSON-Dateien („16.1" statt „0.281") und Konsistenz mit allen anderen Winkelangaben (Orientierung in Grad). Verworfen: Speicherung in Radiant — fehleranfällig beim Handeditieren und inkonsistent zu `Object.GetOrientation()` (Grad, Research_Vanilla_APIs.md §7).

**Entscheidung E7 — Easing als Enum-String statt `easeIn`/`easeOut`-Floatpaar.** Der Keyframe trägt `easing` mit den Werten `"Linear" | "Smooth" | "Smoother"`, die 1:1 auf verifizierte Implementierungen mappen (`vector.Lerp` bzw. COT `JMCinematicCamera.SmoothStep`/`SmootherStep`, Research_COT_Infrastructure.md §1). Begründung: keine erfundene Bezier-Auswertung nötig (es gibt keinen verifizierten Bezier-Evaluator in Vanilla), lesbare Dateien, triviale Validierung. Verworfen: freie `easeIn`/`easeOut`-Gewichte — bräuchten einen eigenen Kurven-Evaluator und sind in der UI schwerer zu vermitteln. Eine spätere additive Erweiterung um Floatgewichte bleibt möglich (Migrationsregel M1).

### 2.1 Laufzeit-Enums (nicht Teil des JSON — JSON trägt Strings)

```c
enum EPCT_Easing
{
	LINEAR = 0,
	SMOOTH,
	SMOOTHER
}

enum EPCT_CurveType
{
	LINEAR = 0,
	CATMULL_ROM,
	NATURAL_CUBIC,
	UNIFORM_CUBIC
}
```

`EPCT_CurveType` (außer `LINEAR`) mappt auf den Engine-Spline-Evaluator `Math3D.Curve(ECurveType, param, points)` mit `enum ECurveType { CatmullRom, NaturalCubic, UniformCubic }` (Research_Vanilla_APIs.md §4). Das Parsen der Strings mit Fallback steht in §6.3.

### 2.2 PCT_Keyframe

Vorbild: Dabs `EditorCameraTrackData` (`float Time; vector Position; vector Orientation;`, Research_Framework_Patterns.md §8), erweitert um FOV-, Fokus-, Blur- und Easing-Kanäle.

```c
class PCT_Keyframe
{
	float time = 0.0;					// Sekunden ab Shot-Beginn, >= 0, aufsteigend
	vector position = "0 0 0";			// Weltkoordinaten (Meter)
	vector orientation = "0 0 0";		// <Yaw, Pitch, Roll> in GRAD (wie Object.GetOrientation)
	float fovDegrees = 27.0;			// vertikaler FOV in GRAD (27.0 ~= 50 mm Vollformat); Anwendung: SetFOV(fovDegrees * Math.DEG2RAD)
	float focusDistance = 2.0;			// Meter, 0..1000 (PPEDOF.PARAM_FOCUS_DIST-Range; Default = PPEDOF-Default 2.0)
	float blurStrength = 0.0;			// 0..100 (COT-Slider-Range "Blur strength"); 0 = DOF aus
	string easing = "Smooth";			// "Linear" | "Smooth" | "Smoother" — Zeitverlauf des Segments zu DIESEM Keyframe hin
}
```

Anwendung der Kanäle (Belege): `SetFOV`/`SetFocus(distance, blur)` auf `Camera` (Research_Vanilla_APIs.md §1), `PPEffects.OverrideDOF(...)` wie in COT `JMCameraModule.OnUpdate` (Research_COT_Infrastructure.md §1), Orientierung via `SetOrientation(vector)` in Grad (Research_Vanilla_APIs.md §7). Der Playback-Algorithmus selbst ist Thema von Plan_B4_Kamera_Engine.md.

### 2.3 PCT_CameraRig

```c
class PCT_CameraRig
{
	string cameraName = "CAM A";
	string sensorPresetId = "sensor_fullframe";
	float sensorWidthMm = 36.0;			// Vollformat-Default (Plan_A1 §5)
	float sensorHeightMm = 24.0;
	string lensPresetId = "";
	float focalLengthMm = 50.0;
	float aperture = 2.8;				// Metadaten; UI leitet daraus einen blurStrength-Vorschlag ab
	float minFocusDistanceM = 0.45;		// Nahgrenze; Continuity-Check-Datenbasis (Plan_A1 §10)
	float anamorphicFactor = 1.0;
	string aspectMask = "16:9";			// "2.39:1" | "16:9" | "4:3" | "9:16" | "1:1" (Overlay-Maske, Plan_A1 §9)
	float nearPlaneM = 0.05;			// Camera.SetNearPlane; engine-intern min. 0.01 m (Research_Vanilla_APIs.md §1)
}
```

`aperture` hat keinen direkten Engine-Effekt (die DOF-API kennt nur `focusDistance`/`blur`); der Wert ist fotografische Dokumentation für die ShotList und Eingang in die Blur-Heuristik der UI. Bewusst **nicht** modelliert in v1: ISO, Shutter, Weißabgleich, Bildrate (Plan_A1 §5 nennt sie, DayZ bietet keine Entsprechung) — additive Erweiterung als Metadaten bleibt jederzeit möglich (M1).

### 2.4 PCT_Shot

```c
class PCT_Shot
{
	int schemaVersion = 1;
	string id = "";						// = Dateiname ohne .json, z. B. "sh_010_cu_b"
	string name = "";
	string description = "";
	string sceneNumber = "";
	string shotNumber = "";
	string framingPresetId = "";		// Referenz in Presets\Framings.json (z. B. "framing_cu")
	string anglePresetId = "";			// Referenz in Presets\Angles.json (z. B. "angle_eye_level")
	string movementPresetId = "";		// Referenz in Presets\Movements.json (z. B. "move_dolly_in")
	string positionCurveType = "CatmullRom";	// "Linear" | "CatmullRom" | "NaturalCubic" | "UniformCubic" (Positions-Spline über alle Keyframes)
	string thumbnailFile = "";			// Dateiname in Exports\ (Screenshot, §8.3)
	int priority = 2;					// 1 = hoch, 2 = normal, 3 = optional
	string status = "planned";			// "planned" | "blocked" | "approved" | "shot"
	string notes = "";
	ref PCT_CameraRig cameraRig;
	ref array<ref PCT_Keyframe> keyframes;
	ref PCT_WorldState worldStateOverride;	// null = Sequenz-Weltzustand erben

	void PCT_Shot()
	{
		cameraRig = new PCT_CameraRig();
		keyframes = new array<ref PCT_Keyframe>();
		worldStateOverride = null;
	}
}
```

Die Shot-Dauer wird **nicht** gespeichert — sie ist die `time` des letzten Keyframes (eine Quelle der Wahrheit; verworfen: redundantes `durationSeconds`, das beim Keyframe-Editieren desynchronisieren würde). `positionCurveType` liegt am Shot (nicht am Keyframe), weil `Math3D.Curve` die **gesamte** Punktmenge auswertet (Research_Vanilla_APIs.md §4); Easing dagegen ist pro Segment sinnvoll und liegt am Keyframe.

### 2.5 PCT_Sequence

```c
class PCT_Sequence
{
	int schemaVersion = 1;
	string id = "";						// = Dateiname ohne .json
	string name = "";
	string description = "";
	string notes = "";
	ref array<string> shotIds;			// geordnete Abspiel-/Schnittreihenfolge
	ref PCT_WorldState worldState;
	ref array<ref PCT_ActorMark> actorMarks;
	ref array<ref PCT_LightSetup> lights;

	void PCT_Sequence()
	{
		shotIds = new array<string>();
		worldState = new PCT_WorldState();
		actorMarks = new array<ref PCT_ActorMark>();
		lights = new array<ref PCT_LightSetup>();
	}
}
```

### 2.6 PCT_Project (ab Phase 3)

```c
class PCT_Project
{
	int schemaVersion = 1;
	string id = "";
	string name = "";
	string description = "";
	string author = "";
	ref array<string> sequenceIds;

	void PCT_Project()
	{
		sequenceIds = new array<string>();
	}
}
```

Plan_A1 §11 nennt zusätzlich die Ebenen „Szene/Setup/Take". Entscheidung: diese Zwischenebenen werden **nicht** als eigene Dateitypen modelliert — `sceneNumber` am Shot plus Sequenz-Reihenfolge decken den Planungsnutzen ab; Take-Planung ist Namenskonvention der Screenshots (§8.3). Verworfen: fünfstufige Dateihierarchie — Verwaltungsaufwand ohne DayZ-seitigen Mehrwert.

### 2.7 PCT_WorldState

Alle Felder mappen auf verifizierte, serverautoritative APIs (Research_Vanilla_APIs.md §6): `World.SetDate(year, month, day, hour, minute)`, `World.SetTimeMultiplier(float)` (0 friert Tageszeit ein), `Weather.GetOvercast()/GetFog()/GetRain()/GetSnowfall()` je mit `WeatherPhenomenon.Set(forecast, time, minDuration)`, `Weather.SetWindSpeed(float)`, `Weather.AngleToWindDirection(float)`, `Weather.SetWeatherUpdateFreeze(bool)`. Die Anwendung läuft über die Server-RPCs des Moduls mit Permission `"CinematicTool.World"` (Phase 4, siehe §7, Plan_B1_Architektur.md §6 und Plan_B5_UI_UX.md §2.4).

```c
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
```

Volumetrischer Nebel (`SetDynVolFog*`, verifiziert ebd.) wird bewusst erst als additive Schema-Erweiterung in Phase 4/5 aufgenommen, wenn der Basis-Workflow steht.

### 2.8 PCT_ActorMark

Datenbasis für Darstellermarken (Plan_A1 §7); Anwendung über das verifizierte Vanilla-CameraTools-Muster (`PlayerBase`-Dummy spawnen, `StartCommand_Action(anim, EmoteCB, STANCEMASK_ALL)`, Attachments via `GetInventory().CreateAttachment`, Hände via `HumanInventory.CreateInHands` — Research_Vanilla_APIs.md §8). Per-Bone-Posing ist laut Research nur lesend möglich und wird nicht modelliert.

```c
class PCT_ActorMark
{
	string id = "";
	string actorName = "";				// z. B. "Person A"
	string role = "";
	string characterType = "SurvivorM_Mirek";	// DayZ-Klassenname des Dummies
	vector position = "0 0 0";
	float headingDegrees = 0.0;			// Blick-/Körperrichtung (Yaw)
	int emoteActionId = 0;				// StartCommand_Action-ID; 0 = neutral stehen
	string handsItem = "";				// Klassenname oder ""
	string note = "";
	float heightM = 1.80;				// Körpergröße — Eingang in Framing-Berechnung (Plan_A1 §5)
	float eyeHeightM = 1.68;			// Augenhöhe — Eingang in Angle-Presets (eyeLevel)
	ref array<string> attachments;

	void PCT_ActorMark()
	{
		attachments = new array<string>();
	}
}
```

### 2.9 PCT_LightSetup

Anwendung über `ScriptedLightBase.CreateLight(typename, pos, fadeIn)` mit `PointLightBase`/`SpotLightBase`-Subklassen, `SetBrightnessTo(float)`, `SetRadiusTo(float)` — alles verifiziert (Research_Vanilla_APIs.md §12). **OFFEN — ZU VERIFIZIEREN:** die exakte API für Farbe, Schattenwurf und Spot-Öffnungswinkel auf `EntityLightSource` (Research nennt sie „should be verified during implementation"). Testschritt: `1_Core`-Proto von `EntityLightSource` in `scripts - 1.29` lesen (Kandidaten `SetColorRGB`, Schatten-Setter), dann In-Game-Test mit einer `PCT_PointLight`-Subklasse. Die JSON-Felder werden trotzdem jetzt definiert (M1-sicher); bis zur Verifikation wendet der Code nur Helligkeit/Radius an.

```c
class PCT_LightSetup
{
	string id = "";
	string displayName = "";
	string lightType = "point";			// "point" | "spot"
	vector position = "0 0 0";			// Weltkoordinaten; in Presets relativ zum Preset-Anker
	vector orientation = "0 0 0";		// nur für Spot relevant
	float colorR = 1.0;					// 0..1 — Anwendung OFFEN (s. o.)
	float colorG = 1.0;
	float colorB = 1.0;
	float brightness = 1.0;				// SetBrightnessTo
	float radiusM = 10.0;				// SetRadiusTo
	bool castsShadow = true;			// Anwendung OFFEN (s. o.)
	float spotAngleDegrees = 45.0;		// Anwendung OFFEN (s. o.)
}
```

### 2.10 Preset-Familien

Gemeinsames Feld `isBuiltin`: mitgelieferte Presets werden bei jedem Start aus Code-Defaults regeneriert, Nutzer-Presets (`isBuiltin = false`) bleiben unangetastet (Entscheidung: so lassen sich Builtin-Presets per Mod-Update verbessern, ohne Nutzerdaten zu überschreiben; verworfen: alles nur in Datei — Builtin-Fixes kämen nie beim Nutzer an).

```c
class PCT_SensorPreset
{
	string id = "";
	string displayName = "";
	float widthMm = 36.0;
	float heightMm = 24.0;
	float cropFactor = 1.0;
	bool isBuiltin = true;
}

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
}

class PCT_FramingPreset					// Einstellungsgrößen, Plan_A1 §3
{
	string id = "";
	string displayName = "";
	string shortCode = "";				// "EWS", "WS", "MS", "MCU", "CU", "BCU", "ECU", ...
	float subjectScreenHeight = 0.5;	// Anteil des Motivs an der Bildhöhe (0..1)
	float headroom = 0.08;				// Anteil der Bildhöhe
	float lookRoom = 0.10;
	float focalLengthMinMm = 24.0;		// empfohlener Brennweitenbereich (Startwerte, keine Regel — Plan_A1 §3)
	float focalLengthMaxMm = 50.0;
	string description = "";
	bool isBuiltin = true;
}

class PCT_AnglePreset					// Kamerawinkel/-höhen, Plan_A1 §4
{
	string id = "";
	string displayName = "";
	string heightMode = "eyeLevel";		// "eyeLevel" | "shoulder" | "hip" | "knee" | "ground" | "absolute"
	float heightOffsetM = 0.0;			// Zusatz-Offset zur Modus-Höhe; bei "absolute": absolute Höhe über Boden
	float pitchDegrees = 0.0;			// negativ = nach unten (High Angle)
	float rollDegrees = 0.0;			// Dutch Angle: 15 / 25 / 40 als Builtin-Varianten (Plan_A1 §4)
	string description = "";
	bool isBuiltin = true;
}

class PCT_MovementPreset				// Kamerabewegungen, Plan_A1 §6
{
	string id = "";
	string displayName = "";
	string movementType = "static";		// "static" | "dollyIn" | "dollyOut" | "truck" | "pedestal" | "pan" | "tilt" | "orbit" | "craneUp" | "handheld" | "zoomIn" | "zoomOut" | "dollyZoom"
	float durationSeconds = 5.0;
	string easing = "Smooth";			// wie PCT_Keyframe.easing
	float speedMS = 1.0;				// Metern/Sekunde bzw. Grad/Sekunde bei pan/tilt/orbit
	float orbitRadiusM = 3.0;
	float orbitDegrees = 90.0;
	float handheldAmplitude = 0.0;		// Handheld-Simulation (Summierte Sinus-Drift, Research_Vanilla_APIs.md §9)
	float handheldFrequency = 0.0;
	string description = "";
	bool isBuiltin = true;
}

class PCT_LightPreset					// Licht-Presets, Plan_A1 §8 (z. B. Three-Point, Rembrandt)
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
```

Container-Klassen (je Familie eine Datei; hier exemplarisch zwei, die übrigen folgen demselben Muster):

```c
class PCT_LensPresetFile
{
	int schemaVersion = 1;
	ref array<ref PCT_LensPreset> presets;

	void PCT_LensPresetFile()
	{
		presets = new array<ref PCT_LensPreset>();
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
```

### 2.11 PCT_Settings

```c
class PCT_Settings
{
	int schemaVersion = 1;
	string lastSequenceId = "";
	string lastShotId = "";
	string defaultSensorPresetId = "sensor_fullframe";
	string defaultLensPresetId = "lens_50_f18";
	bool showCompositionGrid = true;	// Drittelraster-Overlay (Plan_A1 §9)
	string defaultAspectMask = "16:9";
	string csvDelimiter = ";";			// §8.1
}
```

---

## 3. JSON-Beispieldateien

Alle Beispiele zeigen das Zielformat, wie es `JsonFileLoader<T>.JsonSaveFile` schreibt. `vector` als 3-Element-Array ist verifiziert (§2, COT `Teleports.json`). **OFFEN — ZU VERIFIZIEREN (klein):** die exakte Repräsentation von `null`-Objekt-Membern (`worldStateOverride`) im Output des Engine-Serializers. Testschritt: Round-Trip-Test T-PER-2 (§4.5) — Shot mit `worldStateOverride = null` speichern, Datei inspizieren, laden, vergleichen.

### 3.1 Shot — `Shots\sh_030_cu_b.json`

Close-up Person B, 85 mm Vollformat (vertikaler FOV = 2·atan(24/170) ≈ 16.1°), Eye Level, langsamer Dolly-In mit Fokusnachführung (Werte gemäß Plan_A1 §3/§5: CU im Bereich 85–135 mm).

```json
{
    "schemaVersion": 1,
    "id": "sh_030_cu_b",
    "name": "CU Person B — Reaktion",
    "description": "Close-up Person B, leichter Dolly-In waehrend der Antwort, Fokus auf naeherem Auge.",
    "sceneNumber": "12",
    "shotNumber": "030",
    "framingPresetId": "framing_cu",
    "anglePresetId": "angle_eye_level",
    "movementPresetId": "move_dolly_in",
    "positionCurveType": "CatmullRom",
    "thumbnailFile": "PCT_seq_barn_dialog_010_sh_030_cu_b_T1.dds",
    "priority": 1,
    "status": "planned",
    "notes": "30-Grad-Regel gegen sh_020 pruefen (siehe Continuity-Assistent).",
    "cameraRig": {
        "cameraName": "CAM A",
        "sensorPresetId": "sensor_fullframe",
        "sensorWidthMm": 36.0,
        "sensorHeightMm": 24.0,
        "lensPresetId": "lens_85_f18",
        "focalLengthMm": 85.0,
        "aperture": 2.0,
        "minFocusDistanceM": 0.8,
        "anamorphicFactor": 1.0,
        "aspectMask": "2.39:1",
        "nearPlaneM": 0.05
    },
    "keyframes": [
        {
            "time": 0.0,
            "position": [4531.25, 339.42, 8213.77],
            "orientation": [143.5, -1.5, 0.0],
            "fovDegrees": 16.1,
            "focusDistance": 2.4,
            "blurStrength": 62.0,
            "easing": "Linear"
        },
        {
            "time": 6.0,
            "position": [4531.05, 339.42, 8213.49],
            "orientation": [143.5, -1.5, 0.0],
            "fovDegrees": 16.1,
            "focusDistance": 2.05,
            "blurStrength": 70.0,
            "easing": "Smooth"
        }
    ],
    "worldStateOverride": null
}
```

### 3.2 Sequence — `Sequences\seq_barn_dialog_010.json`

Dialogszene in einer Scheune, Golden Hour, zwei Darsteller, ein Fensterlicht — Coverage nach Plan_A1 §11 (Master, OTS, CU).

```json
{
    "schemaVersion": 1,
    "id": "seq_barn_dialog_010",
    "name": "Szene 12 — Scheunendialog",
    "description": "Konfrontation A/B in der Scheune bei Staroye, Abendlicht durchs Westfenster.",
    "notes": "Handlungsachse A-B laeuft Nord-Sued; alle Kameras auf der Westseite halten.",
    "shotIds": [
        "sh_010_ws_master",
        "sh_020_ots_a",
        "sh_030_cu_b"
    ],
    "worldState": {
        "year": 2020,
        "month": 6,
        "day": 21,
        "hour": 19,
        "minute": 45,
        "freezeTime": true,
        "timeMultiplier": 0.0,
        "overcast": 0.15,
        "fog": 0.05,
        "rain": 0.0,
        "snowfall": 0.0,
        "windMagnitude": 2.0,
        "windDirectionDegrees": 225.0,
        "freezeWeather": true
    },
    "actorMarks": [
        {
            "id": "actor_a",
            "actorName": "Person A",
            "role": "Antagonist",
            "characterType": "SurvivorM_Mirek",
            "position": [4529.80, 339.40, 8210.90],
            "headingDegrees": 320.0,
            "emoteActionId": 0,
            "handsItem": "",
            "note": "Marke 1: bleibt am Tor stehen.",
            "heightM": 1.82,
            "eyeHeightM": 1.70,
            "attachments": ["QuiltedJacket_Black", "Jeans_Black"]
        },
        {
            "id": "actor_b",
            "actorName": "Person B",
            "role": "Protagonistin",
            "characterType": "SurvivorF_Eva",
            "position": [4531.60, 339.40, 8215.10],
            "headingDegrees": 140.0,
            "emoteActionId": 0,
            "handsItem": "",
            "note": "Marke 2: weicht einen Schritt zurueck auf Cue.",
            "heightM": 1.70,
            "eyeHeightM": 1.58,
            "attachments": ["RidersJacket_Black", "SlacksPants_Beige"]
        }
    ],
    "lights": [
        {
            "id": "light_window_key",
            "displayName": "Key — Fensterlicht West",
            "lightType": "point",
            "position": [4528.40, 341.10, 8214.20],
            "orientation": [0.0, 0.0, 0.0],
            "colorR": 1.0,
            "colorG": 0.85,
            "colorB": 0.65,
            "brightness": 2.5,
            "radiusM": 12.0,
            "castsShadow": true,
            "spotAngleDegrees": 45.0
        }
    ]
}
```

### 3.3 LensPreset-Familie — `Presets\Lenses.json` (Ausschnitt)

```json
{
    "schemaVersion": 1,
    "presets": [
        {
            "id": "lens_24_f28",
            "displayName": "24 mm f/2.8 Weitwinkel",
            "focalLengthMm": 24.0,
            "focalLengthMaxMm": 0.0,
            "maxAperture": 2.8,
            "minFocusDistanceM": 0.25,
            "anamorphicFactor": 1.0,
            "isBuiltin": true
        },
        {
            "id": "lens_85_f18",
            "displayName": "85 mm f/1.8 Portrait",
            "focalLengthMm": 85.0,
            "focalLengthMaxMm": 0.0,
            "maxAperture": 1.8,
            "minFocusDistanceM": 0.8,
            "anamorphicFactor": 1.0,
            "isBuiltin": true
        },
        {
            "id": "lens_24_70_f28",
            "displayName": "24-70 mm f/2.8 Zoom",
            "focalLengthMm": 24.0,
            "focalLengthMaxMm": 70.0,
            "maxAperture": 2.8,
            "minFocusDistanceM": 0.38,
            "anamorphicFactor": 1.0,
            "isBuiltin": true
        }
    ]
}
```

### 3.4 WorldState-Preset-Familie — `Presets\WorldStates.json` (Ausschnitt)

```json
{
    "schemaVersion": 1,
    "presets": [
        {
            "id": "world_golden_hour",
            "displayName": "Golden Hour (Sommer)",
            "isBuiltin": true,
            "state": {
                "year": 2020,
                "month": 6,
                "day": 21,
                "hour": 19,
                "minute": 45,
                "freezeTime": true,
                "timeMultiplier": 0.0,
                "overcast": 0.15,
                "fog": 0.05,
                "rain": 0.0,
                "snowfall": 0.0,
                "windMagnitude": 2.0,
                "windDirectionDegrees": 225.0,
                "freezeWeather": true
            }
        },
        {
            "id": "world_moonlight",
            "displayName": "Moonlight (klar)",
            "isBuiltin": true,
            "state": {
                "year": 2020,
                "month": 9,
                "day": 3,
                "hour": 1,
                "minute": 30,
                "freezeTime": true,
                "timeMultiplier": 0.0,
                "overcast": 0.0,
                "fog": 0.1,
                "rain": 0.0,
                "snowfall": 0.0,
                "windMagnitude": 1.0,
                "windDirectionDegrees": 90.0,
                "freezeWeather": true
            }
        }
    ]
}
```

---

## 4. Persistenz

### 4.1 Pfad-Konstanten — `PCT_Constants` (3_Game)

Vorbild: COT `JMConstants` (`static const string DIR_COT = "$profile:CommunityOnlineTools\\";`, Konstanten-Konkatenation `DIR_LOADOUTS = DIR_COT + "Loadouts\\";` — verifiziert `JMConstants.c:9/30/32`). **Achtung:** In EnScript-String-Literalen ist `\\` das escapte Backslash — die Single-Backslash-Regel dieses Repos gilt nur für `config.cpp`-Pfade, nicht für Script-Strings.

```c
class PCT_Constants
{
	static const string DIR_ROOT = "$profile:PsyernsCinematicTool\\";
	static const string DIR_SHOTS = DIR_ROOT + "Shots\\";
	static const string DIR_SEQUENCES = DIR_ROOT + "Sequences\\";
	static const string DIR_PROJECTS = DIR_ROOT + "Projects\\";		// ab Phase 3
	static const string DIR_PRESETS = DIR_ROOT + "Presets\\";
	static const string DIR_EXPORTS = DIR_ROOT + "Exports\\";

	static const string FILE_SETTINGS = DIR_ROOT + "Settings.json";
	static const string FILE_PRESETS_SENSORS = DIR_PRESETS + "Sensors.json";
	static const string FILE_PRESETS_LENSES = DIR_PRESETS + "Lenses.json";
	static const string FILE_PRESETS_FRAMINGS = DIR_PRESETS + "Framings.json";
	static const string FILE_PRESETS_ANGLES = DIR_PRESETS + "Angles.json";
	static const string FILE_PRESETS_MOVEMENTS = DIR_PRESETS + "Movements.json";
	static const string FILE_PRESETS_LIGHTS = DIR_PRESETS + "Lights.json";
	static const string FILE_PRESETS_WORLDSTATES = DIR_PRESETS + "WorldStates.json";

	static const string EXT_JSON = ".json";
	static const string EXT_CSV = ".csv";

	static const int SCHEMA_VERSION_SHOT = 1;
	static const int SCHEMA_VERSION_SEQUENCE = 1;
	static const int SCHEMA_VERSION_PROJECT = 1;
	static const int SCHEMA_VERSION_PRESETS = 1;
	static const int SCHEMA_VERSION_SETTINGS = 1;
}
```

### 4.2 Verzeichnisse anlegen

`MakeDirectory(string)` ist direkt am 1.29-Quellcode verifiziert (`EnSystem.c:525`); COT ruft es idempotent bei jedem Start auf (`COTModule.c:15`, `JMItemStatsForm.c:138` u. a.) — dasselbe Muster hier:

```c
class PCT_Storage
{
	static void EnsureDirectories()
	{
		MakeDirectory(PCT_Constants.DIR_ROOT);
		MakeDirectory(PCT_Constants.DIR_SHOTS);
		MakeDirectory(PCT_Constants.DIR_SEQUENCES);
		MakeDirectory(PCT_Constants.DIR_PRESETS);
		MakeDirectory(PCT_Constants.DIR_EXPORTS);
	}
```

(`DIR_PROJECTS` kommt in Phase 3 dazu.) Aufrufort: `PCT_CinematicModule.OnMissionStart()` (siehe Plan_B1) — vor jedem ersten Save/Load.

### 4.3 Speichern & Laden mit `JsonFileLoader<T>` (Wiki-/COT-Muster)

Verifizierte API (Research_Framework_Patterns.md §3): `JsonSaveFile(filename, data)`, `JsonLoadFile(filename, out data)`, `JsonMakeData(data)`, `JsonLoadData(string, out data)`. Muster inkl. `FileExist`-Check und Default-Fallback exakt wie COT `JMTeleportSerialize.Load()` (`JMTeleportSerialize.c:19-44`):

```c
	static bool SaveShot(PCT_Shot shot)
	{
		if (!shot)
			return false;
		if (shot.id == "")
		{
			CF_Log.Warn("PCT: SaveShot ohne id verweigert.");
			return false;
		}
		if (shot.schemaVersion > PCT_Constants.SCHEMA_VERSION_SHOT)
		{
			CF_Log.Warn("PCT: SaveShot fuer '" + shot.id + "' verweigert — schemaVersion " + shot.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_SHOT + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();

		shot.schemaVersion = PCT_Constants.SCHEMA_VERSION_SHOT;
		string filePath = PCT_Constants.DIR_SHOTS + shot.id + PCT_Constants.EXT_JSON;
		JsonFileLoader<PCT_Shot>.JsonSaveFile(filePath, shot);
		return true;
	}

	static PCT_Shot LoadShot(string shotId)
	{
		string filePath = PCT_Constants.DIR_SHOTS + shotId + PCT_Constants.EXT_JSON;
		if (!FileExist(filePath))
		{
			CF_Log.Warn("PCT: Shot-Datei fehlt: " + filePath);
			return null;
		}

		PCT_Shot shot = new PCT_Shot();
		JsonFileLoader<PCT_Shot>.JsonLoadFile(filePath, shot);

		PCT_Migration.MigrateShot(shot);
		PCT_Validation.ValidateShot(shot);
		return shot;
	}

	static PCT_Settings LoadSettings()
	{
		PCT_Settings settings = new PCT_Settings();
		if (FileExist(PCT_Constants.FILE_SETTINGS))
		{
			JsonFileLoader<PCT_Settings>.JsonLoadFile(PCT_Constants.FILE_SETTINGS, settings);
		}
		else
		{
			EnsureDirectories();
			JsonFileLoader<PCT_Settings>.JsonSaveFile(PCT_Constants.FILE_SETTINGS, settings);
		}
		return settings;
	}
}
```

`FileExist` direkt am 1.29-Quellcode verifiziert (`EnSystem.c:397`); `CF_Log.Warn` verifiziert (Research_Framework_Patterns.md §6). Die Datenklassen liefern die Defaults selbst (Konstruktor + Inline-Initialisierung), daher ist „Fallback auf Defaults" schlicht: neues Objekt bauen, Datei nur laden, wenn vorhanden.

Der `schemaVersion`-Check in `SaveShot` setzt Regel M3 durch (§5.1): `MigrateShot` lässt eine zu neue `schemaVersion` unverändert am Objekt stehen (§5.2), `SaveShot` erkennt den Read-only-Fall genau daran und verweigert — erst nach diesem Check wird die Version auf den unterstützten Stand gestempelt. Ohne den Check würde ein Save die Version stillschweigend herabstempeln und (zusammen mit M2: unbekannte Keys gehen beim Speichern verloren) genau den Datenverlust erzeugen, den M3 verhindern soll.

### 4.4 Dateien auflisten (Shot-/Sequenz-Browser)

Enumeration über `FindFile`/`FindNextFile`/`CloseFindFile` (direkt am 1.29-Quellcode verifiziert, `EnSystem.c:520-522`):

```c
	static void ListJsonIds(string directory, TStringArray outIds)
	{
		if (!outIds)
			return;
		outIds.Clear();

		string fileName;
		FileAttr attributes;
		FindFileHandle handle = FindFile(directory + "*.json", fileName, attributes, FindFileFlags.DIRECTORIES);

		bool hasNext = true;
		while (hasNext)
		{
			if (fileName != "")
			{
				string idOnly = fileName;
				idOnly.Replace(".json", "");
				outIds.Insert(idOnly);
			}
			hasNext = FindNextFile(handle, fileName, attributes);
		}
		CloseFindFile(handle);
	}
```

**OFFEN — ZU VERIFIZIEREN:** Verhalten von `FindFile` bei leerem Verzeichnis (Gültigkeit des Handles, Inhalt von `fileName`). Testschritt: Aufruf gegen ein leeres `Shots\`-Verzeichnis im Offline-Spiel, Log der Rückgaben; Guard ggf. anpassen, bevor der Browser in Phase 2 kommt.

### 4.5 Fehlerbehandlung — Regeln und Testplan

| Fall | Verhalten | Beleg/Status |
|---|---|---|
| Datei fehlt | `FileExist`-Check → Defaults verwenden bzw. `null` + `CF_Log.Warn`; nie crashen | COT-Muster `JMTeleportSerialize.Load()` |
| Datei vorhanden, Keys fehlen | Klassen-Defaults bleiben stehen (Konstruktor/Inline-Init) | Muster; Test T-PER-1 |
| Datei syntaktisch defekt (Handeditierung) | **OFFEN — ZU VERIFIZIEREN:** Verhalten von `JsonLoadFile` bei kaputtem JSON (Fehlerlog? Teilbefüllung? Crash?). Testschritt T-PER-3: absichtlich beschädigte Shot-Datei laden (Offline), Verhalten protokollieren; falls nötig Schutz über vorherige Plausibilitätsprüfung des Dateiinhalts | offen |
| Werte außerhalb der Grenzen | Clamping + `CF_Log.Warn` (§6) | projektintern |
| `schemaVersion` neuer als vom Mod unterstützt | Read-only laden, Save verweigern, Notification (§5, Regel M3) | projektintern |

**Round-Trip-Tests (Phase 1, Offline-Spiel):**
- **T-PER-1:** Shot mit allen Defaults speichern → Datei um zwei Keys kürzen → laden → Feldwerte müssen den Defaults entsprechen.
- **T-PER-2:** Shot mit 2 Keyframes speichern → Datei inspizieren (`vector`-Arrays, `null`-Repräsentation) → laden → feldweiser Vergleich.
- **T-PER-3:** defekte Datei (fehlende Klammer) laden → dokumentiertes Verhalten, kein Crash im Endzustand.

---

## 5. Schema-Versionierung & Migration

Jede Wurzel-Datenklasse trägt `int schemaVersion` (Container-Dateien am Container, eingebettete Klassen erben die Version der Wurzeldatei — Entscheidung: eine Version pro Datei statt pro Klasse, weil eine Datei immer atomar geschrieben wird; verworfen: Version je Teilobjekt — Aufwand ohne Nutzen).

### 5.1 Migrationsregeln

| Regel | Situation | Verfahren |
|---|---|---|
| **M1 — Additiv** | Neuer Key kommt hinzu | Neuer Member mit Defaultwert in der Klasse. Alte Dateien laden weiter (fehlender Key → Default). `schemaVersion` wird dennoch erhöht (Nachvollziehbarkeit); beim nächsten Save schreibt der Mod die neue Version. Kein Konverter nötig. |
| **M2 — Unbekannte Keys** | Datei enthält Keys, die die Klasse nicht kennt (Datei von neuerer Mod-Version oder Fremd-Tool) | Erwartung nach Wiki-/COT-Muster: `JsonFileLoader` ignoriert unbekannte Keys. **OFFEN — ZU VERIFIZIEREN**, Testschritt T-MIG-1: Shot-Datei um einen Fantasie-Key ergänzen, laden, prüfen dass Laden fehlerfrei ist. **Wichtig:** Beim Speichern mit der älteren Klasse gehen unbekannte Keys verloren — deshalb Regel M3. |
| **M3 — Version voraus** | `schemaVersion` der Datei > höchste vom Mod unterstützte Version | Datei read-only behandeln: laden ja, **Save verweigern** (sonst Datenverlust der unbekannten Keys), `CF_Log.Warn` + CF-Notification an den Nutzer („Datei stammt aus neuerer PCT-Version"). Durchsetzung: `MigrateShot` lässt die zu neue `schemaVersion` unverändert am Objekt (§5.2); `PCT_Storage.SaveShot` prüft `shot.schemaVersion > SCHEMA_VERSION_SHOT` und lehnt ab (§4.3). |
| **M4 — Breaking Change** | Key-Umbenennung, Einheiten-/Semantikwechsel | Pro Versionssprung eine Konverterfunktion in `PCT_Migration` (Kette v1→v2→v3…). Der alte Key bleibt eine Version lang als deprecated Member in der Klasse erhalten, damit der Konverter ihn lesen kann; danach wird er entfernt. Breaking Changes sind ab Phase 5 (Release) zu vermeiden. |

### 5.2 Migration-Dispatcher (Muster)

```c
class PCT_Migration
{
	static void MigrateShot(PCT_Shot shot)
	{
		if (!shot)
			return;

		if (shot.schemaVersion <= 0)
			shot.schemaVersion = 1;

		if (shot.schemaVersion > PCT_Constants.SCHEMA_VERSION_SHOT)
		{
			CF_Log.Warn("PCT: Shot '" + shot.id + "' hat schemaVersion " + shot.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_SHOT + ") — read-only.");
			return;
		}

		// Beispiel fuer einen kuenftigen Sprung:
		// if (shot.schemaVersion == 1)
		// {
		//	MigrateShotV1toV2(shot);
		//	shot.schemaVersion = 2;
		// }
	}
}
```

(Ein separates Read-only-Flag in der Datenklasse ist nicht nötig: die unverändert belassene, zu neue `schemaVersion` am Objekt **ist** das Read-only-Kriterium — `PCT_Storage.SaveShot` prüft sie und verweigert den Save (§4.3). Die UI-Kennzeichnung „read-only" hält die Laufzeit-Verwaltung; JSON-Datenklassen bleiben frei von Laufzeitzustand.)

### 5.3 Geplante Schema-Versionen je Phase

| Phase | Datei-Typ | schemaVersion | Inhalt / Änderung |
|---|---|---|---|
| Phase 1 Kamera-Kern | `Settings.json` | 1 | Grundpräferenzen |
| Phase 1 Kamera-Kern | Shot | 1 | volle Feldmenge aus §2.4; genutzt werden zunächst nur `id`, `name`, `keyframes[]` (alle Kanäle aus §2.2), `positionCurveType`, `cameraRig` — die übrigen Felder bleiben auf Defaults |
| Phase 2 Shots & Presets | Shot | 1 (unverändert) | kein Schema-Sprung: UI/Workflow beginnen `framingPresetId`, `anglePresetId`, `movementPresetId`, `thumbnailFile`, `status`, `priority`, `sceneNumber`, `shotNumber`, `worldStateOverride` zu nutzen — die Keys sind seit v1 definiert |
| Phase 2 Shots & Presets | Presets: Sensors/Lenses/Framings/Angles/Movements | 1 | Builtin-Sets + Nutzer-Presets |
| Phase 3 Sequencer | Sequence | 1 | `shotIds[]`, `worldState`, Notizen (Actors/Lights noch leer) |
| Phase 3 Sequencer | Project | 1 | `sequenceIds[]`; Unterordner `Projects\` |
| Phase 4 Welt/Akteure/Licht & Assistenten | Presets: Lights/WorldStates | 1 | Licht-Presets, Welt-Snapshots |
| Phase 4 Welt/Akteure/Licht & Assistenten | Sequence | 2 (additiv) | + `actorMarks[]`, `lights[]` befüllt; ggf. VolFog-Felder in `PCT_WorldState` |
| Phase 5 Polish & Release | alle | eingefroren | nur noch M1-additive Fixes; Breaking Changes verboten |

---

## 6. Validierung nach dem Laden

Grundsatz: **clampen + `CF_Log.Warn`, niemals crashen, niemals Laden verweigern** (außer Datei fehlt/defekt, §4.5). Validierung läuft immer nach `PCT_Migration` und vor der ersten Verwendung (`PCT_Storage.LoadShot`, §4.3).

### 6.1 Clamping-Tabellen

**PCT_Keyframe:**

| Feld | Min | Max | Fallback bei Unsinn | Quelle der Grenze |
|---|---|---|---|---|
| `time` | 0.0 | 3600.0 | 0.0 | ≥ 0 fachlich; 1 h Obergrenze pragmatisch (Tool-seitig) |
| `fovDegrees` | 1.0 | 175.0 | 27.0 | pragmatische Tool-Grenze; Engine-Verhalten OFFEN (s. u.) |
| `focusDistance` | 0.0 | 1000.0 | 2.0 | `PPEDOF.PARAM_FOCUS_DIST`-Range 0..1000, Default 2.0 (Research_Vanilla_APIs.md §3); deckungsgleich mit COT-Slider „Focus distance" 0–1000 m |
| `blurStrength` | 0.0 | 100.0 | 0.0 | COT-Slider „Blur strength" 0–100 (Research_COT_Infrastructure.md §1) |
| `orientation` Yaw | normiert auf 0..360 | — | — | `Math.NormalizeAngle` (Research_Framework_Patterns.md §7) |
| `orientation` Pitch | -89.0 | 89.0 | 0.0 | Gimbal-Schutz (Tool-seitig) |
| `orientation` Roll | -180.0 | 180.0 | 0.0 | Dutch-Angle-Bereich vollständig abgedeckt (Plan_A1 §4: 15–45° typisch) |
| `easing` | — | — | `"Smooth"` | Enum-Parse mit Fallback (§6.3) |

**OFFEN — ZU VERIFIZIEREN (fovDegrees-Grenzen):** Die User-Option klemmt auf ~43°–75° vertikal (`OPTIONS_FIELD_OF_VIEW_MIN/MAX`), aber COTs FOV-Slider erlaubt 0.001–4.0 rad — ob `Camera.SetFOV` engine-seitig klemmt, ist laut Research_Vanilla_APIs.md §2 nicht aus dem Script ablesbar. Testschritt: In-Game `SetFOV` mit 5°, 120° und 170° aufrufen, Screenshots vergleichen; Tool-Grenzen 1..175 danach ggf. enger ziehen.

**PCT_CameraRig:**

| Feld | Min | Max | Fallback | Quelle |
|---|---|---|---|---|
| `sensorWidthMm` / `sensorHeightMm` | 1.0 | 100.0 | 36.0 / 24.0 | fachlich (Smartphone bis IMAX-ähnlich, Plan_A1 §5) |
| `focalLengthMm` | 1.0 | 1200.0 | 50.0 | fachlich (Plan_A1 §5: 14–200 mm + Makro/Tele-Reserve) |
| `aperture` | 0.7 | 32.0 | 2.8 | fachlich |
| `minFocusDistanceM` | 0.01 | 100.0 | 0.45 | fachlich |
| `anamorphicFactor` | 1.0 | 2.0 | 1.0 | fachlich |
| `nearPlaneM` | 0.01 | 10.0 | 0.05 | `SetNearPlane` intern min. 0.01 (Research_Vanilla_APIs.md §1) |

**PCT_WorldState:**

| Feld | Min | Max | Fallback | Quelle |
|---|---|---|---|---|
| `month` / `day` / `hour` / `minute` | 1/1/0/0 | 12/31/23/59 | 6/21/12/0 | Kalender |
| `timeMultiplier` | 0.0 | 64.0 | 0.0 | `SetTimeMultiplier` 0–64 (Research_Vanilla_APIs.md §6) |
| `overcast`, `fog`, `rain`, `snowfall` | 0.0 | 1.0 | 0.0 | Phänomen-Wertebereich (Forecast-Anteil) |
| `windMagnitude` | 0.0 | 30.0 | 2.0 | pragmatisch (Tool-seitig) |
| `windDirectionDegrees` | normiert 0..360 | — | — | `Math.NormalizeAngle` |

**PCT_LightSetup:** `colorR/G/B` 0..1, `brightness` 0..100 (Fallback 1.0), `radiusM` 0.1..200 (Fallback 10.0), `spotAngleDegrees` 1..179 (Fallback 45.0) — alles Tool-seitig pragmatisch, bis die Licht-API-Verifikation (§2.9) engere Grenzen liefert.

### 6.2 Strukturregeln

1. `keyframes == null` → leeres Array erzeugen (kein Crash); Playback verweigert mit CF-Notification, wenn `< 2` Keyframes (Shot bleibt aber ladbar/editierbar).
2. **Keyframes werden nach `time` aufsteigend sortiert** (stabile Insertion-Sort, §6.3). Doppelte Zeiten: der spätere Eintrag wird um 0.001 s verschoben + `CF_Log.Warn`.
3. `cameraRig == null` → `new PCT_CameraRig()` (Defaults).
4. `positionCurveType` unbekannt → `"CatmullRom"` + Warn.
5. Sequenz: `shotIds`, deren Datei fehlt, bleiben in der Liste (kein stilles Löschen von Nutzerdaten), werden aber in der UI als fehlend markiert; Playback überspringt sie mit Warn. Verworfen: automatisches Entfernen — zerstört Referenzen, wenn der Nutzer die Shot-Datei nur temporär verschoben hat.

### 6.3 Validierungscode (Muster)

```c
class PCT_Validation
{
	static float ClampWarn(float value, float min, float max, string fieldName)
	{
		if (value >= min && value <= max)
			return value;

		float clamped = Math.Clamp(value, min, max);
		CF_Log.Warn("PCT: '" + fieldName + "' = " + value + " ausserhalb [" + min + ".." + max + "], geklemmt auf " + clamped);
		return clamped;
	}

	static int ParseEasing(string easing)
	{
		if (easing == "Linear")
			return EPCT_Easing.LINEAR;
		if (easing == "Smooth")
			return EPCT_Easing.SMOOTH;
		if (easing == "Smoother")
			return EPCT_Easing.SMOOTHER;

		CF_Log.Warn("PCT: Unbekanntes easing '" + easing + "', Fallback Smooth.");
		return EPCT_Easing.SMOOTH;
	}

	static void SortKeyframes(array<ref PCT_Keyframe> keyframes)
	{
		if (!keyframes)
			return;

		int count = keyframes.Count();
		for (int i = 1; i < count; i++)
		{
			PCT_Keyframe current = keyframes[i];
			int j = i - 1;
			while (j >= 0 && keyframes[j].time > current.time)
			{
				keyframes[j + 1] = keyframes[j];
				j = j - 1;
			}
			keyframes[j + 1] = current;
		}
	}

	static void ValidateShot(PCT_Shot shot)
	{
		if (!shot)
			return;

		if (!shot.cameraRig)
			shot.cameraRig = new PCT_CameraRig();
		if (!shot.keyframes)
			shot.keyframes = new array<ref PCT_Keyframe>();

		int count = shot.keyframes.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_Keyframe keyframe = shot.keyframes[i];
			if (!keyframe)
				continue;

			keyframe.time = ClampWarn(keyframe.time, 0.0, 3600.0, "keyframe.time");
			keyframe.fovDegrees = ClampWarn(keyframe.fovDegrees, 1.0, 175.0, "keyframe.fovDegrees");
			keyframe.focusDistance = ClampWarn(keyframe.focusDistance, 0.0, 1000.0, "keyframe.focusDistance");
			keyframe.blurStrength = ClampWarn(keyframe.blurStrength, 0.0, 100.0, "keyframe.blurStrength");
		}

		SortKeyframes(shot.keyframes);
	}
}
```

(EnScript-Regeln beachtet: keine Ternaries, eine Deklaration pro Zeile, keine komplexen Ausdrücke in Array-Index-Zuweisungen — Elementkopien sind unkritisch, Zwischenvariable `current` vorhanden.)

---

## 7. Sync-Modell

### 7.1 Phase 1–3: rein clientseitig

Alle PCT-Dateien liegen im `$profile` des **erstellenden Clients**; es gibt keinen Daten-RPC. Die Kamera selbst ist client-lokal (Spectator-Kamera bzw. `staticcamera` mit `create_local = true`, Vanilla-Muster `CameraToolsMenu.c:71`, Research_Vanilla_APIs.md §1); der Kamera-Enter/Leave läuft über den PCT-eigenen Enter-/Leave-Pfad (`EPCT_RPC.Enter`/`Leave`, Spiegel von `JMCameraModule.Server_Enter` — siehe Plan_B1_Architektur.md §5.2/§6), nicht über Datei-Sync-RPCs. Im Offline-/Editor-Betrieb ist `IsMissionHost()` wahr und alles bleibt lokal (COT-Muster, Research_COT_Infrastructure.md §4). Begründung: Previs ist Einzelarbeit; Netzwerk-Komplexität vor dem Sequencer wäre reines Risiko. Verworfen: frühe Server-Ablage — erzeugt Permissions-/Sync-Aufwand ohne Phase-1-Nutzen.

Abzugrenzen: das **Anwenden** eines `PCT_WorldState` (Wetter/Zeit) ist serverautoritativ (Research_Vanilla_APIs.md §6) und läuft ab Phase 4 über Modul-RPCs mit Permission `"CinematicTool.World"` — das ist Zustands-Anwendung, kein Datei-Sync, und wird in Plan_B1_Architektur.md §6/§7 spezifiziert.

### 7.2 Phase 4+: optionaler Sequence-Broadcast

Ziel: ein Regisseur teilt eine Sequenz live mit anderen berechtigten Spielern (Director-Review). Mechanik: Serialisierung zu einem JSON-String via `JsonMakeData` (verifiziert, Research_Framework_Patterns.md §3), Versand über `ScriptRPC` im PCT-RPC-Bereich (Basis 10500, COTs höchste Basis ist 10460 — Research_COT_Infrastructure.md §5):

```c
enum EPCT_RPC
{
	INVALID = 10500,
	// ... Enter, Leave, SetWorldState u. a. — vollstaendige Belegung siehe Plan_B1_Architektur.md §6
	SequenceBroadcast,	// 10514 gemaess Plan_B1_Architektur.md §6
	COUNT
}
```

```c
// CLIENT → SERVER (PCT_CinematicModule, 5_Mission)
void SendSequenceBroadcast(PCT_Sequence sequence)
{
	if (!g_Game)
		return;
	if (!sequence)
		return;

	string json = JsonFileLoader<PCT_Sequence>.JsonMakeData(sequence);

	ScriptRPC rpc = new ScriptRPC();
	Param2<string, string> data = new Param2<string, string>(sequence.id, json);
	rpc.Write(data);
	rpc.Send(NULL, EPCT_RPC.SequenceBroadcast, true, NULL);
}

// SERVER-HANDLER (Dispatch via OnRPC-Override, Muster JMCameraModule.OnRPC)
private void RPC_SequenceBroadcast(ParamsReadContext ctx, PlayerIdentity senderRPC, Object target)
{
	if (IsMissionHost())
	{
		if (!GetPermissionsManager().HasPermission("CinematicTool.Sequencer", senderRPC))
			return;

		Param2<string, string> data = new Param2<string, string>("", "");
		if (!ctx.Read(data))
			return;

		// data.param1 = sequenceId, data.param2 = JSON
		// Weiterversand an Clients mit "CinematicTool.View" — siehe Plan_B1_Architektur.md §6.
		// Empfaenger-Seite: PCT_Sequence sequence = new PCT_Sequence();
		// JsonFileLoader<PCT_Sequence>.JsonLoadData(data.param2, sequence);
		// danach PCT_Migration + PCT_Validation wie beim Datei-Load.
	}
}
```

(Param-Objekt wird als **ein** Objekt gelesen; Permission-Check mit `senderRPC` gemäß COT-Sicherheitsmuster, Research_COT_Infrastructure.md §3/§4.)

**OFFEN — ZU VERIFIZIEREN (Größenlimit):** Das praktische Payload-Limit von `ScriptRPC` für lange Strings ist in keiner Quelle belegt; eine Sequenz mit Actors/Lichtern plus referenzierten Shots kann mehrere zehn KB JSON erreichen. Testschritt (Phase 4, Dedicated-Testserver): Diagnose-RPC, der Strings von 1/4/16/64/256 KB Client→Server→Client schickt; Ankunft + Länge loggen; daraus praktisches Limit und Sicherheitsmarge (z. B. 60 %) ableiten.

**Alternativen, falls das Limit zu klein ist** (Entscheidung erst nach dem Test):
1. **Chunked Transfer:** JSON-String in n Teilstrings splitten, je Chunk `Param4<string, int, int, string>` (`sequenceId`, `chunkIndex`, `chunkCount`, `chunkData`), Empfänger reassembliert und parst erst bei Vollständigkeit; Timeout + Warn bei Lücken.
2. **Server-Datei + Notification:** Sequenz serverseitig unter `$profile:PsyernsCinematicTool\Sequences\` ablegen (derselbe `PCT_Storage`-Code — 3_Game ist seitenneutral) und Clients per CF `NotificationSystem.Create(...)` (verifiziert, Research_Framework_Patterns.md §6) informieren; Clients ziehen die Daten dann via Chunked-Pull. Vorbild: Expansions „Server lädt JSON, pusht per RPC"-Settings-Modell (nur Muster, keine Abhängigkeit).

Shots werden beim Broadcast **mitgesendet** (die Empfänger haben die referenzierten Shot-Dateien nicht) — der Broadcast-Container ist daher perspektivisch `Param2<string, string>` je Shot plus Sequenz oder ein Sammel-JSON; Detailfestlegung nach dem Größentest (siehe Plan_B6_Roadmap_Risiken.md, O3/R4).

---

## 8. Export-Formate

Alle Exporte landen in `PCT_Constants.DIR_EXPORTS`. Datei-Schreib-API direkt am 1.29-Quellcode verifiziert: `OpenFile(string, FileMode)`, `FPrintln(FileHandle, ...)`, `CloseFile(FileHandle)` (`EnSystem.c:417/481/443`).

### 8.1 ShotList-CSV

Ausrichtung an den Shot-Inhalten aus Plan_A1 §11, reduziert auf **DayZ-machbare** Felder (bewusst ausgelassen, weil ohne Engine-Entsprechung bzw. nicht in Schema v1: ISO, Shutter Angle, Weißabgleich, Bildrate, Tonanforderung, geschätzte Drehzeit — additiv nachrüstbar per M1).

**Format-Entscheidungen:** Trennzeichen Semikolon (Default in `PCT_Settings.csvDelimiter`) — deutsches Excel öffnet Semikolon-CSV direkt; verworfen: Komma (kollidiert mit deutschem Dezimaltrennzeichen-Handling in Excel). Zahlen mit Punkt als Dezimaltrenner (JSON-konsistent). Feld-Escaping: Anführungszeichen um Textfelder, `"` im Text verdoppelt, Zeilenumbrüche in Notizen durch ` / ` ersetzt.

| # | Spalte | Quelle | Beispiel |
|---|---|---|---|
| 1 | `scene` | `PCT_Shot.sceneNumber` | `12` |
| 2 | `shot` | `PCT_Shot.shotNumber` | `030` |
| 3 | `name` | `PCT_Shot.name` | `CU Person B — Reaktion` |
| 4 | `description` | `PCT_Shot.description` | `Close-up, Dolly-In...` |
| 5 | `framing` | `framingPresetId` → `shortCode` | `CU` |
| 6 | `angle` | `anglePresetId` → `displayName` | `Eye Level` |
| 7 | `movement` | `movementPresetId` → `displayName` | `Dolly In` |
| 8 | `sensor` | `cameraRig.sensorPresetId` → `displayName` | `Full Frame 36x24` |
| 9 | `focalLengthMm` | `cameraRig.focalLengthMm` | `85` |
| 10 | `aperture` | `cameraRig.aperture` | `2.0` |
| 11 | `fovDegreesStart` | `keyframes[0].fovDegrees` | `16.1` |
| 12 | `focusDistanceStartM` | `keyframes[0].focusDistance` | `2.4` |
| 13 | `camPosX` / 14 `camPosY` / 15 `camPosZ` | `keyframes[0].position` | `4531.25` / `339.42` / `8213.77` |
| 16 | `camYaw` / 17 `camPitch` / 18 `camRoll` | `keyframes[0].orientation` | `143.5` / `-1.5` / `0.0` |
| 19 | `durationSeconds` | `time` des letzten Keyframes | `6.0` |
| 20 | `keyframeCount` | `keyframes.Count()` | `2` |
| 21 | `worldDate` | effektiver WorldState (`Override` sonst Sequenz), `YYYY-MM-DD` | `2020-06-21` |
| 22 | `worldTime` | ebd., `HH:MM` | `19:45` |
| 23 | `overcast` / 24 `fog` / 25 `rain` | ebd. | `0.15` / `0.05` / `0.0` |
| 26 | `actorCount` | `sequence.actorMarks.Count()` | `2` |
| 27 | `lightCount` | `sequence.lights.Count()` | `1` |
| 28 | `priority` | `PCT_Shot.priority` | `1` |
| 29 | `status` | `PCT_Shot.status` | `planned` |
| 30 | `thumbnail` | `PCT_Shot.thumbnailFile` | `PCT_..._T1.dds` |
| 31 | `notes` | `PCT_Shot.notes` | `30-Grad-Regel pruefen` |

Schreib-Muster (Ausschnitt; eine Zeile pro Shot der Sequenz):

```c
	static bool ExportShotListCsv(PCT_Sequence sequence, array<ref PCT_Shot> shots, string delimiter)
	{
		if (!sequence)
			return false;

		EnsureDirectories();

		string filePath = PCT_Constants.DIR_EXPORTS + "ShotList_" + sequence.id + PCT_Constants.EXT_CSV;
		FileHandle file = OpenFile(filePath, FileMode.WRITE);
		if (file == 0)
		{
			CF_Log.Warn("PCT: CSV-Export fehlgeschlagen (OpenFile): " + filePath);
			return false;
		}

		FPrintln(file, BuildCsvHeader(delimiter));

		int count = shots.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_Shot shot = shots[i];
			if (!shot)
				continue;
			string line = BuildCsvRow(sequence, shot, delimiter);
			FPrintln(file, line);
		}

		CloseFile(file);
		return true;
	}
```

**OFFEN — ZU VERIFIZIEREN (Encoding):** Umlaute/Sonderzeichen im `FPrintln`-Output (Engine-String-Encoding vs. Excel-Erwartung). Testschritt T-EXP-2: CSV mit „ÄÖÜß"-Testzeile exportieren, in deutschem Excel und in VS Code öffnen; ggf. Spaltenköpfe/Inhalte ASCII-normalisieren.

### 8.2 ShotList-JSON

Maschinenlesbares Pendant für externe Tools (Storyboard-PDF-Generator, Tagesdispo — Weiterverarbeitung außerhalb DayZ, vgl. Plan_A1 §14): flache Zeilen, identische Feldsemantik wie CSV, plus Kopfdaten. Serialisierung über denselben `JsonFileLoader`.

```c
class PCT_ShotListRow
{
	string scene = "";
	string shot = "";
	string name = "";
	string framing = "";
	string angle = "";
	string movement = "";
	string sensor = "";
	float focalLengthMm = 0.0;
	float aperture = 0.0;
	float fovDegreesStart = 0.0;
	float focusDistanceStartM = 0.0;
	vector camPos = "0 0 0";
	vector camOrientation = "0 0 0";
	float durationSeconds = 0.0;
	int keyframeCount = 0;
	string worldDate = "";
	string worldTime = "";
	int priority = 2;
	string status = "";
	string thumbnail = "";
	string notes = "";
}

class PCT_ShotListExport
{
	int schemaVersion = 1;
	string sequenceId = "";
	string sequenceName = "";
	string exportedAt = "";				// Fuellung siehe OFFEN-Punkt unten
	ref array<ref PCT_ShotListRow> rows;

	void PCT_ShotListExport()
	{
		rows = new array<ref PCT_ShotListRow>();
	}
}
```

Ablage: `Exports\ShotList_<sequenceId>.json`. **OFFEN — ZU VERIFIZIEREN (klein):** API für die Echtzeit-Systemuhr zur Füllung von `exportedAt` (Kandidaten `GetYearMonthDay`/`GetHourMinuteSecond` in `EnSystem.c` — nicht in den Research-Dokumenten verifiziert). Testschritt: `EnSystem.c` in `scripts - 1.29` auf diese Protos greppen und Testaufruf; bis dahin bleibt `exportedAt` leer.

### 8.3 Screenshots (Thumbnails / Storyboard-Frames)

API verifiziert: globales `proto native void MakeScreenshot(string name)` (`proto.c:142`, Research_Vanilla_APIs.md §11) — Ausgabe als **DDS**; beginnt `name` mit `$`, wird der volle Pfad verwendet, sonst `$profile:ScreenShotes/DATE TIME-NAME.dds`.

**Namenskonvention:** `PCT_<sequenceId>_<shotId>_T<take>` unter `Exports\`, Take-Zähler über `FileExist`-Probing (kein Uhr-API nötig):

```c
	static string NextScreenshotBaseName(string sequenceId, string shotId)
	{
		int take = 1;
		string baseName = "";
		bool exists = true;
		while (exists)
		{
			baseName = PCT_Constants.DIR_EXPORTS + "PCT_" + sequenceId + "_" + shotId + "_T" + take;
			exists = FileExist(baseName + ".dds");
			if (exists)
				take = take + 1;
		}
		return baseName;
	}
	// Aufruf: MakeScreenshot(NextScreenshotBaseName(sequence.id, shot.id));
```

**OFFEN — ZU VERIFIZIEREN (MakeScreenshot-Details):** (a) hängt die Engine `.dds` automatisch an den `$`-Pfad an, (b) wird die UI/HUD mit aufgenommen (für saubere Frames muss das PCT-Menü vorher versteckt werden), (c) Ausführungszeitpunkt (gleicher Frame oder nächster). Testschritt: Offline-Test — Aufruf mit `$`-Pfad bei offenem und verstecktem Menü, Ergebnisdateien prüfen; falls UI im Bild: Screenshot einen Frame nach `Hide()` über `g_Game.GetCallQueue()`-`CallLater` auslösen (Null-Check der CallQueue gemäß Projektregeln).

**DDS-Konvertierungs-Workflow (extern, dokumentiert für Nutzer):** DDS ist für Weitergabe/Storyboards unpraktisch (Research_Vanilla_APIs.md §11: „needs external conversion"). Empfohlener Batch-Weg im `Exports\`-Ordner: ImageMagick (`magick mogrify -format png *.dds`) oder XnConvert (GUI); alternativ Microsoft `texconv`. Das Storyboard-PDF selbst entsteht außerhalb von DayZ aus `ShotList_<id>.json` + konvertierten PNGs (Phase-5-Tooling, siehe Plan_B6); ein PDF-Export aus der Engine existiert nicht und wird nicht versucht.

---

## Querverweise

**Geschwisterdokumente der Planungsreihe:**

- `Plan_B1_Architektur.md` — Architektur, Mod-Struktur & COT-Modul-Integration (CfgPatches/CfgMods, `PCT_CinematicModule`, `modded class JMModuleConstructor`, `EPCT_RPC`-Belegung ab 10500, `CinematicTool.*`-Permission-Baum, WorldState-Anwendung)
- `Plan_B2_Feature_Mapping.md` — Feature-Mapping Plan_A1 → DayZ-Machbarkeit (Phaseneinstufung, Ersatzstrategien)
- `Plan_B3_Datenmodell_Persistenz.md` — dieses Dokument
- `Plan_B4_Kamera_Engine.md` — Kamera-Kern & Interpolation (`PCT_CinematicCamera`, `PCT_SequencePlayer`, `PCT_Math`, Playback der hier definierten Keyframe-Kanäle)
- `Plan_B5_UI_UX.md` — UI/GUI & Bedienkonzept (`PCT_CinematicForm`, Layouts, Overlays, Inputs `UAPCT_*`)
- `Plan_B6_Roadmap_Risiken.md` — Phasenplan, Tests & Release (Phase 0 Skeleton … Phase 5 Polish & Release; konsolidiert die hier genannten Tests T-PER-1..3, T-MIG-1, T-EXP-2 sowie die OFFEN-Verifikationen)

**Research-Dokumente (verifizierte Faktenbasis):**

- `Docs/Research/Research_COT_Infrastructure.md` — COT-Modul-System, RPC-Muster, Permissions, Kamera-Flow
- `Docs/Research/Research_Vanilla_APIs.md` — Vanilla-1.29-APIs: Camera/FOV, PPE/DOF, Splines, Weather/Date, Lights, Screenshots
- `Docs/Research/Research_Framework_Patterns.md` — JsonFileLoader-Idiom, Dabs `EditorCameraTrackData`-Vorbild, PCT_Math-Portierungsliste, Abhängigkeits-Entscheidung (nur CF + COT)

**Hinweis zu den Datei-APIs:** `FileExist`, `MakeDirectory`, `FindFile`/`FindNextFile`/`CloseFindFile`, `OpenFile`/`FPrintln`/`CloseFile` sind **nicht** in den Research-Dokumenten erfasst, sondern in diesem Plan direkt am 1.29-Quellcode verifiziert (`EnSystem.c:397/417/443/481/520-522/525`, §4.2–§4.4/§8.1).

**Anforderungen:**

- `Plan_A1.md` — Stoff- und Anforderungssammlung (§3 Einstellungsgrößen, §4 Winkel, §5 Kamera/Objektiv, §6 Bewegungen, §8 Licht, §11 Shot-System, §14 Export)
