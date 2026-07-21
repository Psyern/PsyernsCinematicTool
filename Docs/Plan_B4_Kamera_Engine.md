# Plan B4 — Kamera-Engine: virtuelles Kameramodell, Keyframes, Bewegungen, Assistenten

> Stand: 2026-07-21 · Status: Entwurf v1 · Mod: PsyernsCinematicTool (PCT) · Autor: Psyern

**Abstract.**
Dieses Dokument spezifiziert das technische Herzstück von PsyernsCinematicTool: die Kamera-Engine.
Es definiert das Zustandsmodell der Kamera (`PCT_CinematicCamera : JMCinematicCamera`), das virtuelle Kameramodell (`PCT_CameraRig`: Sensor, Brennweite, Blende, Fokus), die DOF-Pipeline über `Camera.SetFocus` + `PPEffects.OverrideDOF`, das Keyframe-/Playback-System (`PCT_SequencePlayer` mit Catmull-Rom-Positionsspline und Arc-Length-Zeit-Remapping), die Bewegungs-Presets (Dolly, Orbit, Follow, Handheld u. a.), den Auto-Framing-Algorithmus (Preset → Kameraposition), den geometrischen Kontinuitäts-Assistenten (180-Grad-Linie, 30-Grad-Regel) sowie PPE-Zusatzeffekte und das Performance-Budget.
Alle API-Aussagen stützen sich auf die verifizierten Research-Dokumente (siehe Querverweise); zusätzlich wurden `Math.Atan`/`Math.Tan` (`EnMath.c:386/355`) und der COT-Autofokus-Raycast (`JMCameraModule.c:111-124`) direkt im Quellcode nachgeprüft.
Nicht belegte Punkte sind ausnahmslos als **OFFEN — ZU VERIFIZIEREN** mit konkretem Testschritt markiert.
Codebeispiele sind Skizzen mit Phasen-Markierung (Phase 0 Skeleton … Phase 5 Polish & Release) und folgen den EnScript-Projektregeln.

---

## 1. Zustandsmodell der Kamera-Engine

Die Kamera-Engine lebt clientseitig in `PCT_CinematicCamera : JMCinematicCamera` (3_Game). Enter/Leave laufen über den PCT-eigenen Enter-/Leave-Pfad nach COT-Vorbild (`SelectPlayer`/`SelectSpectator`, eigene RPCs `EPCT_RPC.Enter`/`Leave` als Spiegel von `JMCameraModule.Server_Enter` — Research_COT_Infrastructure §1; Modul-/RPC-Details siehe Plan_B1_Architektur.md §5.2/§6). Innerhalb der aktiven Kamera gibt es fünf exklusive Modi plus ein orthogonales Handheld-Overlay, das auf jeden Modus aufaddiert werden kann.

```
                         COT-Enter-Flow                              COT-Leave-Flow
   [Spieler] ────────────────────────────────►  ┌─────────────┐  ◄──────────────────── [Spieler]
   (UAPCT_ToggleMenu → Modul → Enter)           │ Reset-Pflicht│  (PPEffects.ResetDOFOverride
                                                │  siehe §8    │   + ResetAll, COT-Muster)
                                                └─────────────┘
 ┌═══════════════════════════════════════════════════════════════════════════════════════════┐
 ║                     PCT_CinematicCamera aktiv (Client, EOnPostFrame → OnUpdate)            ║
 ║                                                                                           ║
 ║              ApplyFramingPreset (UI / UAPCT_ApplyFraming)                                  ║
 ║   ┌────────┐ ─────────────────────────────────────────────► ┌──────────┐                  ║
 ║   │  FREE  │                                                │  FRAMED  │                  ║
 ║   │ (Frei- │ ◄───────────────────────────────────────────── │ (Preset  │                  ║
 ║   │  flug) │      manueller Bewegungs-Input (WASD/Maus)     │ auf Ziel)│                  ║
 ║   └────────┘                                                └──────────┘                  ║
 ║     │ ▲  │ ▲                                                   │  │                       ║
 ║     │ │  │ │ UAPCT_ToggleOrbit                UAPCT_ToggleOrbit│  │UAPCT_PlayPause        ║
 ║     │ │  │ │ (Ziel gesetzt) / erneut = zurück                  │  │                       ║
 ║     │ │  │ └────────────► ┌─────────┐ ◄───────────────────────┘  │                       ║
 ║     │ │  │                │  ORBIT  │                             │                       ║
 ║     │ │  └─────────────── │ (Kreis  │                             │                       ║
 ║     │ │    Ziel verloren  │ um Ziel)│                             │                       ║
 ║     │ │                   └─────────┘                             │                       ║
 ║     │ │ UAPCT_ToggleFollow (Ziel gesetzt) / erneut = zurück       │                       ║
 ║     │ └──────────────────► ┌─────────┐                            │                       ║
 ║     │                      │ FOLLOW  │                            │                       ║
 ║     │ ◄─────────────────── │(SmoothCD│                            │                       ║
 ║     │   Ziel verloren      │ -Boom)  │                            │                       ║
 ║     │                      └─────────┘                            ▼                       ║
 ║     │ UAPCT_PlayPause      ┌──────────────────────────────────────────────┐              ║
 ║     └────────────────────► │                 PLAYBACK                     │              ║
 ║                            │ (PCT_SequencePlayer steuert alle Kanäle;     │              ║
 ║       Sequenz-Ende /       │  MoveFreeze + LookFreeze = true)             │              ║
 ║   ◄── UAPCT_Stop /         │                                              │              ║
 ║       manueller Input      └──────────────────────────────────────────────┘              ║
 ║                                                                                           ║
 ║   ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ orthogonale Region ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    ║
 ║   HANDHELD-OVERLAY:  [aus] ◄── UAPCT_ToggleHandheld ──► [an]                              ║
 ║   (additiver Yaw/Pitch/Roll-Offset auf JEDEN Modus, siehe §5 Handheld)                    ║
 └═══════════════════════════════════════════════════════════════════════════════════════════┘
```

Übergänge und Ereignisse (Input-Actions `UAPCT_*` aus `Scripts/Data/Inputs.xml`, siehe Plan_B5_UI_UX.md §7):

| Von | Nach | Ereignis | Bedingung |
|---|---|---|---|
| (Spieler) | FREE | COT-Enter-Flow abgeschlossen | Permission `CinematicTool.View` |
| FREE | FRAMED | Framing-Preset angewendet (UI-Button / `UAPCT_ApplyFraming`) | Ziel ausgewählt (§6 Schritt 1) |
| FRAMED | FREE | beliebiger manueller Bewegungs-Input | — |
| FREE / FRAMED | ORBIT | `UAPCT_ToggleOrbit` | Zielpunkt/Objekt gesetzt |
| ORBIT | FREE | `UAPCT_ToggleOrbit` erneut oder Ziel verloren | — |
| FREE / FRAMED | FOLLOW | `UAPCT_ToggleFollow` | Zielobjekt gesetzt |
| FOLLOW | FREE | `UAPCT_ToggleFollow` erneut oder Ziel verloren | — |
| jeder Modus | PLAYBACK | `UAPCT_PlayPause` | Sequenz geladen, ≥ 2 Keyframes |
| PLAYBACK | vorheriger Modus | Sequenz-Ende, `UAPCT_Stop` oder manueller Input (Abbruch) | — |
| jeder Modus | (Spieler) | Leave (Menü / `UAPCT_ToggleMenu` → Leave) | Reset-Disziplin §8 zwingend |

Entwurfsentscheidung: Handheld ist **kein** exklusiver Modus, sondern ein Overlay-Flag, weil Wackeln in jedem Modus sinnvoll kombinierbar ist (Handheld-Dolly, Handheld-Follow). Die verworfene Alternative (eigener Modus HANDHELD) hätte für jede Kombination einen Produktzustand erzwungen und die Zustandsmaschine quadratisch aufgebläht.

```c
// Phase 1 — 3_Game/PsyernsCinematicTool: Modus-Enum
enum EPCT_CameraMode
{
	FREE = 0,
	FRAMED,
	PLAYBACK,
	ORBIT,
	FOLLOW
}
```

```c
// Phase 1 (FREE/FRAMED) / Phase 3 (PLAYBACK) — 3_Game: Kamera-Ableitung
class PCT_CinematicCamera : JMCinematicCamera
{
	int m_Mode;
	bool m_HandheldEnabled;
	vector m_BaseAngles;					// unverwackelte Basis-Orientierung
	ref PCT_SequencePlayer m_SequencePlayer;
	ref PCT_HandheldNoise m_HandheldYaw;
	ref PCT_HandheldNoise m_HandheldPitch;
	ref PCT_HandheldNoise m_HandheldRoll;

	void SetMode(int newMode)
	{
		m_Mode = newMode;
		bool freeze = (newMode == EPCT_CameraMode.PLAYBACK);
		MoveFreeze = freeze;				// JMCameraBase-Member (Research_Framework_Patterns §1)
		LookFreeze = freeze;
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		if (m_Mode == EPCT_CameraMode.PLAYBACK && m_SequencePlayer)
			m_SequencePlayer.Tick(this, timeslice);
		if (m_HandheldEnabled)
			ApplyHandheldOverlay(timeslice);
	}
}
```

`OnUpdate` wird von `JMCameraBase` aus `EOnPostFrame` getrieben (`EntityEvent.FRAME|POSTFRAME`, Research_COT_Infrastructure §1) — das ist der einzige Frame-Hook der Engine-Kamera; alle per-Frame-Logik dieses Dokuments hängt daran.

---

## 2. Virtuelles Kameramodell — `PCT_CameraRig`

### 2.1 Grundformel

Die Engine erwartet an `Camera.SetFOV(float fov)` die **vertikale** FOV in **Radiant** (Research_Vanilla_APIs §1: `Camera.c:67`; §2: "The engine treats FOV values as vertical FOV in radians"). Die Umrechnung aus dem fotografischen Modell:

```
fovVertical = 2 * atan(sensorHeightMm / (2 * focalLengthMm))     [Radiant, vertikal]
```

`Math.Atan(float x)` und `Math.Tan(float angle)` sind vorhanden (verifiziert: `1_Core\DayZ\proto\EnMath.c:386` bzw. `:355`; ergänzend zur Research-Basis nachgeschlagen). `Math.DEG2RAD`/`Math.RAD2DEG` laut Research_Vanilla_APIs §4.

### 2.2 Klasse `PCT_CameraRig`

JSON-serialisierbare Datenklasse (3_Game/Data; Member ohne Präfix, Keys = JSON, Schema siehe Plan_B3_Datenmodell_Persistenz.md):

```c
// Phase 1 — 3_Game/PsyernsCinematicTool/Data: virtuelles Kameramodell (Member = JSON-Schema Plan_B3 §2.3)
class PCT_CameraRig
{
	string cameraName = "CAM A";
	string sensorPresetId = "sensor_fullframe";	// Tabelle 2.3
	float sensorWidthMm = 36.0;
	float sensorHeightMm = 24.0;
	string lensPresetId = "";
	float focalLengthMm = 50.0;
	float aperture = 2.8;				// f-Stop, z. B. 2.0
	float minFocusDistanceM = 0.45;		// Nahgrenze des Objektivs (für §7 Warnung)
	float anamorphicFactor = 1.0;		// 1.0 = sphärisch; nur Anzeige/Maske, siehe Hinweis unten
	string aspectMask = "16:9";			// Overlay-Maske (Plan_B5)
	float nearPlaneM = 0.05;			// Camera.SetNearPlane, min. 0.01 m

	float GetVerticalFovRad()
	{
		float denom = 2.0 * focalLengthMm;
		float fovRad = 2.0 * Math.Atan(sensorHeightMm / denom);
		return fovRad;
	}

	float GetVerticalFovDeg()
	{
		float fovRad = GetVerticalFovRad();
		return fovRad * Math.RAD2DEG;
	}
}
```

Hinweise zur Schema-Treue (Plan_B3): Das Rig trägt **kein** eigenes `schemaVersion` (eingebettete Klasse — die Version liegt an der Wurzeldatei, Plan_B3 §5) und **keine** `focusDistance` — die Fokusdistanz ist Keyframe-Kanal (`PCT_Keyframe.focusDistance`, Plan_B3 §2.2/E5) bzw. Live-Zustand der Kamera, nicht fotografische Rig-Absicht.

Hinweis Anamorphot: Die Engine bietet keine getrennte horizontale FOV-/Render-Kontrolle (Research_Vanilla_APIs §10: keine Aspect-Ratio-API). `anamorphicFactor` wirkt daher nur auf Overlay-Masken/Anzeigen (Plan_B5), nicht auf das Rendering.

### 2.3 Sensor-Presets

| Preset (`sensorPresetId`) | Breite mm | Höhe mm | Crop-Faktor (Diagonale vs. FF) | Bemerkung |
|---|---|---|---|---|
| `sensor_fullframe` | 36.0 | 24.0 | 1.00 | Referenzformat |
| `sensor_super35` | 24.89 | 18.66 | 1.39 | Cine-Standard |
| `sensor_apsc` | 23.6 | 15.6 | 1.53 | Foto-APS-C |
| `sensor_mft` | 17.3 | 13.0 | 2.00 | Micro Four Thirds |
| `sensor_1inch` | 13.2 | 8.8 | 2.73 | 1-Zoll |
| `sensor_smartphone` | 9.8 | 7.3 | 3.54 | Näherung (Hauptkameras variieren stark) |

Erweiterbar um `sensor_custom` (freie Breite/Höhe) gemäß Plan_A1 §5; Kameramodell-Bibliothek (Sony FX3 usw.) ist reine Datenpflege über Presets\ (Plan_B3).

### 2.4 Brennweiten-Preset-Reihe (Plan_A1 §5)

```
14 · 18 · 21 · 24 · 28 · 32 · 35 · 40 · 50 · 65 · 75 · 85 · 100 · 135 · 200 mm
```

### 2.5 Umrechnungstabelle Brennweite → vertikale FOV (FullFrame, sensorHeight = 24 mm)

| Brennweite mm | fovV (rad) | fovV (°) | im User-Options-Bereich 0.752–1.303 rad? |
|---|---|---|---|
| 14 | 1.4173 | 81.2 | nein — über Max |
| 18 | 1.1760 | 67.4 | ja |
| 21 | 1.0382 | 59.5 | ja |
| 24 | 0.9273 | 53.1 | ja |
| 28 | 0.8098 | 46.4 | ja |
| 32 | 0.7175 | 41.1 | nein — unter Min |
| 35 | 0.6606 | 37.8 | nein — unter Min |
| 40 | 0.5829 | 33.4 | nein — unter Min |
| 50 | 0.4711 | 27.0 | nein — unter Min |
| 65 | 0.3652 | 20.9 | nein — unter Min |
| 75 | 0.3173 | 18.2 | nein — unter Min |
| 85 | 0.2805 | 16.1 | nein — unter Min |
| 100 | 0.2389 | 13.7 | nein — unter Min |
| 135 | 0.1774 | 10.2 | nein — unter Min |
| 200 | 0.1199 | 6.9 | nein — unter Min |

Konsequenz: **Fast alle filmisch relevanten Brennweiten liegen außerhalb der User-Options-Grenzen** (`OPTIONS_FIELD_OF_VIEW_MIN/MAX` = 0.75242724772 / 1.30322025726, `gameplay.c:1316-1317`, Research_Vanilla_APIs §2). Diese Grenzen gelten laut Quelle für die **User-Einstellung** (`DayZGame.SetUserFOV` clampt); ob `Camera.SetFOV` ebenfalls clampt, ist im Script nicht sichtbar. COTs FOV-Slider erlaubt 0.001–4.0 rad (Research_COT_Infrastructure §1), was stark auf "nicht geclampt" hindeutet.

**OFFEN — ZU VERIFIZIEREN (V1, release-kritisch):** Ist `Camera.SetFOV` jenseits 0.75–1.30 rad frei wirksam?
*Testschritt (Phase 1):* In der PCT-Kamera nacheinander `SetFOV(0.1199)` (200 mm) und `SetFOV(1.4173)` (14 mm) setzen; pro Frame `Camera.GetCurrentFOV()` in ein Diag-HUD schreiben und Bildwirkung prüfen. Zusätzlich prüfen, ob eine parallel gesetzte User-FOV (`SetUserFOV`) den Wert überschreibt. Erwartung: Rückgabewert == gesetzter Wert, Bild entsprechend eng/weit.

```c
// Phase 1 — FOV anwenden (Engine: vertikale FOV in Radiant)
void ApplyFov(Camera cam, PCT_CameraRig rig)
{
	if (!cam)
		return;
	float fovRad = rig.GetVerticalFovRad();
	cam.SetFOV(fovRad);
}
```

Entwurfsentscheidung: Das Rig speichert `focalLengthMm` + Sensor statt direkt FOV, weil Plan_A1 §2/§5 die getrennte Editierbarkeit (Sensorwechsel bei konstanter Brennweite ändert FOV) verlangt und Presets fotografisch benannt sind. Verworfene Alternative: FOV als Primärwert — verliert die Sensor-/Crop-Semantik und macht Objektiv-Bibliotheken bedeutungslos.

---

## 3. DOF-Pipeline

### 3.1 Senken (verifizierte APIs)

Zwei parallel bediente Wege (exakt das COT-Muster, `JMCameraModule.c:130-131`, Research_COT_Infrastructure §1):

1. `Camera.SetFocus(float distance, float blur)` — kamerainterne DOF (`Camera.c:74`).
2. `PPEffects.OverrideDOF(bool enable, float focusDistance, float focusLength, float focusLengthNear, float blur, float focusDepthOffset)` — Wrapper um `g_Game.OverrideDOF(...)` (`Game.c:1354`, `PPEffects.c:503`; Research_Vanilla_APIs §3).

Parameter-Belegung aus dem Rig:

| OverrideDOF-Parameter | Quelle | Startwert |
|---|---|---|
| `enable` | DOF an, sobald `aperture` < f/22 | `true` |
| `focusDistance` | Live-Fokusdistanz der Kamera (Keyframe-Kanal `focusDistance`, Plan_B3 §2.2, bzw. Autofokus §3.3 — kein Rig-Member, siehe §2.2) | — |
| `focusLength` | `rig.focalLengthMm` | z. B. 85 |
| `focusLengthNear` | `focusLength * 0.5` | kalibrierbar |
| `blur` | Heuristik §3.2 | — |
| `focusDepthOffset` | Konstante | `10.0` (COT `CAMERA_DOFFSET`) |

```c
// Phase 1 — DOF anwenden (COT-Muster: SetFocus + OverrideDOF kombiniert)
// focusDistance = Live-Fokusdistanz (Keyframe-Kanal bzw. Autofokus §3.3), kein Rig-Member (Plan_B3 §2.3)
void ApplyDepthOfField(Camera cam, PCT_CameraRig rig, float focusDistance)
{
	if (!cam)
		return;
	float blur = PCT_Math.BlurFromAperture(rig.aperture, rig.focalLengthMm);
	float focusLength = rig.focalLengthMm;
	float focusLengthNear = focusLength * 0.5;
	cam.SetFocus(focusDistance, blur);
	PPEffects.OverrideDOF(true, focusDistance, focusLength, focusLengthNear, blur, 10.0);
}
```

### 3.2 Heuristik f-Stop → blur-Faktor

**Ausdrücklich eine kalibrierbare Näherung, keine physikalische Simulation.** Der Engine-`blur` ist ein dimensionsloser Faktor (PPEDOF `PARAM_BLUR` Default 1.0, Research_Vanilla_APIs §3); die Tabelle ist normiert 0..1 und wird über einen globalen Gain (`Settings.json`, Plan_B3) skaliert. Zusätzlich skaliert die Brennweite den Effekt (längere Brennweite = flachere Schärfentiefe):

| f-Stop | blurBase (normiert) |
|---|---|
| 1.2 | 1.00 |
| 1.4 | 0.90 |
| 2.0 | 0.75 |
| 2.8 | 0.60 |
| 4.0 | 0.45 |
| 5.6 | 0.32 |
| 8.0 | 0.20 |
| 11 | 0.12 |
| 16 | 0.06 |
| 22 | 0.00 (DOF aus) |

```c
// Phase 1 — PCT_Math: Blende → blur (kalibrierbare Heuristik)
static float BlurFromAperture(float fStop, float focalLengthMm)
{
	float blurBase = LookupBlurBase(fStop);				// Tabellen-LUT, linear interpoliert
	float focalScale = focalLengthMm / 50.0;			// 50 mm als Referenz
	float clampedScale = Math.Clamp(focalScale, 0.5, 3.0);
	float blur = blurBase * clampedScale * s_PCT_BlurGain;	// Statics immer s_PCT_*
	return Math.Clamp(blur, 0.0, 1.0);
}
```

**OFFEN — ZU VERIFIZIEREN (V2):** Wirksame Skala des `blur`-Parameters (Vanilla-Default 1.0 vs. COT-Slider "Blur strength 0–100" im Effects-Panel).
*Testschritt (Phase 1):* Feste Szene (Ziel auf 3 m, Hintergrund 50 m), `blur` in Stufen 0.25 / 0.5 / 1.0 / 5.0 / 25 / 100 setzen, Screenshots vergleichen; daraus `s_PCT_BlurGain` und die Tabellen-Normierung kalibrieren.

### 3.3 Autofokus-Raycast (COT-Muster)

Exakt aus `JMCameraModule.c:111-124` übernommen (im Quellcode nachgeprüft):

```c
// Phase 1 — Autofokus: Raycast aus der Kameramitte
float ComputeAutoFocusDistance()
{
	if (!g_Game)
		return 0.0;
	vector from = g_Game.GetCurrentCameraPosition();
	vector dir = g_Game.GetCurrentCameraDirection();
	vector to = from + dir * 9999.0;
	vector contactPos;
	DayZPhysics.RaycastRV(from, to, contactPos, NULL, NULL, NULL, NULL, NULL, false, false, ObjIntersectIFire);
	float dist = vector.Distance(from, contactPos);
	return dist;
}
```

Bei gesetztem Zielobjekt (Framed/Follow) stattdessen `vector.Distance(camPos, target.GetPosition())` (ebenfalls COT-Muster, Zeile 115-117).

### 3.4 Rack Focus

Rack Focus ist **keine** eigene Mechanik, sondern Fokus-Keyframe-Interpolation: zwei (oder mehr) Keyframes unterscheiden sich nur in `focusDistance` (+ optional `blur`), Position/Orientierung bleiben konstant; der `PCT_SequencePlayer` (§4) interpoliert skalar mit Easing. Damit erbt Rack Focus automatisch Scrubbing, Easing und Export. Verworfene Alternative: dedizierter "Fokusfahrt-Timer" außerhalb des Sequencers — doppelte Zeitlogik ohne Mehrwert.

---

## 4. Keyframe-System

### 4.1 Kanäle

| Kanal | Typ | Interpolation | API-Senke |
|---|---|---|---|
| `position` | vector (Weltkoordinaten) | `Math3D.Curve(ECurveType.CatmullRom, u, points)` + Arc-Length-Remap (§4.4) | `SetPosition` |
| `yaw/pitch/roll` | vector (Grad) | `PCT_Math.InterpolateAngles` (kürzester Weg) + Ease | `SetYawPitchRoll` |
| `fovDegrees` | float (Grad, vertikal) | skalarer Lerp + Ease; Umrechnung `* Math.DEG2RAD` beim Anwenden (Vanilla-CameraTools-Muster: `SetFOV(deg * Math.DEG2RAD)`, Research_Vanilla_APIs §1) | `SetFOV` |
| `focusDistance` | float (m) | skalarer Lerp + Ease | `SetFocus` + `OverrideDOF` |
| `blur` | float | skalarer Lerp | `SetFocus` + `OverrideDOF` |

Keyframe-Datenklasse (Form nach Dabs `EditorCameraTrackData` + FOV/Fokus/Roll, Research_Framework_Patterns §8; JSON-Schema in Plan_B3_Datenmodell_Persistenz.md):

```c
// Phase 3 — 3_Game/PsyernsCinematicTool/Data (JSON-Member ohne Präfix; Schema = Plan_B3 §2.2)
class PCT_Keyframe
{
	float time;				// Sekunden ab Shot-Beginn
	vector position;
	vector orientation;		// yaw/pitch/roll in Grad
	float fovDegrees;
	float focusDistance;
	float blurStrength;
	string easing;			// "Linear" | "Smooth" | "Smoother" — Segment ZU diesem Keyframe hin (Plan_B3 E7); Laufzeit-Parse via PCT_Validation.ParseEasing → EPCT_Easing
}
```

### 4.2 Playback — `PCT_SequencePlayer` (3_Game)

Getickt aus `PCT_CinematicCamera.OnUpdate(timeslice)` (COT-`EOnPostFrame`-Muster, §1). Globale Zeit `m_Time` läuft monoton; der Segment-Lookup nutzt deshalb einen gecachten Vorwärtszeiger statt Suche.

```c
// Phase 3 — 3_Game: Sequenz-Playback (Skizze)
class PCT_SequencePlayer
{
	ref array<ref PCT_Keyframe> m_Keyframes;
	ref array<vector> m_Positions;			// vorab befüllt: alle Keyframe-Positionen (+ ggf. duplizierte Endpunkte, §4.4 V4)
	ref array<ref array<float>> m_ArcLengths;	// pro Segment: kumulierte Distanzen (LUT, §4.4)
	ref array<ref array<float>> m_ArcParams;	// pro Segment: zugehörige Spline-Parameter u
	float m_Time;
	float m_Duration;
	float m_PlaybackSpeed;
	bool m_Playing;
	int m_CachedSegment;

	void Tick(PCT_CinematicCamera cam, float timeslice)
	{
		if (!m_Playing || !cam)
			return;
		m_Time += timeslice * m_PlaybackSpeed;
		if (m_Time >= m_Duration)
		{
			m_Time = m_Duration;
			m_Playing = false;
		}
		int seg = FindSegment(m_Time);
		PCT_Keyframe keyA = m_Keyframes.Get(seg);
		PCT_Keyframe keyB = m_Keyframes.Get(seg + 1);
		float segDuration = keyB.time - keyA.time;
		float tSeg = (m_Time - keyA.time) / segDuration;
		int easingType = PCT_Validation.ParseEasing(keyB.easing);	// beim Laden vorparsen (Skizze: inline)
		float tEased = PCT_Math.Ease(easingType, tSeg);
		float u = RemapToSplineParam(seg, tEased);				// §4.4
		vector pos = Math3D.Curve(ECurveType.CatmullRom, u, m_Positions);
		vector angles = PCT_Math.InterpolateAngles(keyA.orientation, keyB.orientation, tEased);
		float fovDeg = Math.Lerp(keyA.fovDegrees, keyB.fovDegrees, tEased);
		float focus = Math.Lerp(keyA.focusDistance, keyB.focusDistance, tEased);
		float blur = Math.Lerp(keyA.blurStrength, keyB.blurStrength, tEased);
		cam.SetPosition(pos);
		cam.SetYawPitchRoll(angles);
		float fovRad = fovDeg * Math.DEG2RAD;
		cam.SetFOV(fovRad);
		cam.SetFocus(focus, blur);
		float focusLength = 0.0;
		float focusLengthNear = 0.0;
		ComputeDofLengths(focus, focusLength, focusLengthNear);
		PPEffects.OverrideDOF(true, focus, focusLength, focusLengthNear, blur, 10.0);
	}

	int FindSegment(float time)
	{
		int lastSegment = m_Keyframes.Count() - 2;
		while (m_CachedSegment < lastSegment)
		{
			PCT_Keyframe nextKey = m_Keyframes.Get(m_CachedSegment + 1);
			if (time < nextKey.time)
				break;
			m_CachedSegment++;
		}
		return m_CachedSegment;
	}
}
```

(`SetPosition`/`SetYawPitchRoll` sind Entity-Methoden der Kamera — Research_Vanilla_APIs §1/§7; `Math.Lerp` `EnMath.c:608`, `Math3D.Curve` `EnMath3D.c:471`.)

### 4.3 Zeitparametrisierung von `Math3D.Curve` — die entscheidende Konsequenz

`Math3D.Curve(type, param, points)` nimmt **einen** Parameter 0..1 **über die gesamte Punktmenge** (Research_Vanilla_APIs §4). Das bedeutet:

1. Der Parameter ist **indexuniform** verteilt: das Intervall zwischen zwei benachbarten Punkten belegt je `1/(n-1)` des Parameterraums — unabhängig davon, wie weit die Punkte räumlich auseinanderliegen und wie lang das Segment zeitlich dauern soll.
2. Naives `u = globaleZeit / gesamtdauer` würde daher weder die Keyframe-Zeiten respektieren noch gleichmäßige Geschwindigkeit liefern: kurze Segmente würden langsam, lange Segmente schnell durchflogen.
3. Notwendig sind **zwei Abbildungen**: (a) Zeit → Segment + segmentlokales `tSeg` über die Keyframe-Zeiten (macht Segment-Dauern korrekt), (b) `tSeg` (nach Easing) → Spline-Parameter `u` über eine Arc-Length-Tabelle (macht die Geschwindigkeit **innerhalb** des Segments in Metern/Sekunde konstant, statt im Spline-Parameter).

Die naive Abbildung `u = (seg + tSeg) / (n - 1)` bleibt als Fallback (Setting `uniformParam`), die Arc-Length-Abbildung ist der Standard.

### 4.4 Zeit-Remapping über Arc-Length-Approximation (Vorab-Sampling)

Pseudocode (einmalig beim Laden/Ändern der Sequenz, nicht pro Frame — §9):

```
// Phase 3 — Pseudocode: Arc-Length-LUT bauen
S = 64                                   // Samples pro Segment
n = Anzahl Keyframes
für jedes Segment k in [0 .. n-2]:
    kumulativ = 0
    pPrev = Curve(CatmullRom, k/(n-1), positions)
    lut[k].Insert(distanz=0, u=k/(n-1))
    für i in [1 .. S]:
        u = (k + i/S) / (n-1)            // globaler Spline-Parameter
        p = Curve(CatmullRom, u, positions)
        kumulativ += |p - pPrev|
        lut[k].Insert(distanz=kumulativ, u=u)
        pPrev = p
    segmentLaenge[k] = kumulativ

// Phase 3 — Pseudocode: Laufzeit-Lookup (RemapToSplineParam)
sSoll = tEased * segmentLaenge[k]        // gewünschte Bogenlänge im Segment
finde j mit lut[k][j].distanz <= sSoll <= lut[k][j+1].distanz   // Vorwärtszeiger, da monoton
f = (sSoll - lut[k][j].distanz) / (lut[k][j+1].distanz - lut[k][j].distanz)
u = Lerp(lut[k][j].u, lut[k][j+1].u, f)
```

**OFFEN — ZU VERIFIZIEREN (V4):** Endpunktverhalten von `Math3D.Curve` bei `CatmullRom` — interpoliert die Engine durch den ersten/letzten Punkt oder dienen diese nur als Tangenten-Kontrollpunkte (klassisches Catmull-Rom braucht Phantom-Endpunkte)? Ebenso die Annahme der indexuniformen Parameterverteilung.
*Testschritt (Phase 3):* Vier bekannte Punkte übergeben; `Curve(CatmullRom, 0.0, pts)`, `Curve(...,1.0, pts)` und `Curve(..., 1.0/3.0, pts)` per `Print` ausgeben. Wenn Endpunkte nicht getroffen werden: ersten/letzten Keyframe beim Befüllen von `m_Positions` duplizieren und Parameterfenster entsprechend verschieben.

### 4.5 Orientierung: kürzester-Weg-Winkelinterpolation

Eigene `PCT_Math.InterpolateAngles` nach dem Muster von `ExpansionMath.InterpolateAngles` (Clean-Room, Research_Framework_Patterns §2/§7 — keine Expansion-Abhängigkeit). **Begründung gegen Quaternion-Slerp:** Vanilla bietet nur `Math3D.QuatLerp` (LERP, kein Slerp — Research_Vanilla_APIs §4). Ein LERP auf Quaternionen ist bei großen Winkeln nicht winkelgleichförmig und müsste zusätzlich normalisiert werden; eine eigene Slerp-Implementierung über `Acos`/`Sin` wäre möglich, bringt aber für Yaw/Pitch/Roll-Kamerafahrten keinen sichtbaren Vorteil und erschwert das Editieren (Keyframes sind als Yaw/Pitch/Roll benutzerlesbar). Kürzester-Weg-Interpolation pro Achse ist ausreichend, solange Pitch nicht über ±90° hinaus animiert wird (Gimbal-Fall dokumentieren wir als Editor-Einschränkung; senkrechte Top-Down-Fahrten werden über Position + `LookAt` gelöst).

```c
// Phase 1 — PCT_Math (3_Game/Utils): Winkel-Utilities (Clean-Room nach ExpansionMath-Vorbild)
class PCT_Math
{
	static float NormalizeAngle360(float angle)
	{
		float wrapped = angle - 360.0 * Math.Floor(angle / 360.0);
		return wrapped;
	}

	static float ShortestAngleDelta(float from, float to)
	{
		float delta = NormalizeAngle360(to - from);
		if (delta > 180.0)
			delta = delta - 360.0;
		return delta;
	}

	static vector InterpolateAngles(vector from, vector to, float t)
	{
		vector result;
		float deltaYaw = ShortestAngleDelta(from[0], to[0]);
		float yaw = from[0] + deltaYaw * t;
		result[0] = yaw;
		float deltaPitch = ShortestAngleDelta(from[1], to[1]);
		float pitch = from[1] + deltaPitch * t;
		result[1] = pitch;
		float deltaRoll = ShortestAngleDelta(from[2], to[2]);
		float roll = from[2] + deltaRoll * t;
		result[2] = roll;
		return result;
	}
}
```

### 4.6 FOV/Fokus: skalare Lerps mit Ease + Easing-Funktionen

FOV, Fokusdistanz und Blur werden mit `Math.Lerp` über das easede `tSeg` interpoliert (§4.2). Easing-Funktionen in `PCT_Math`:

| Easing (`EPCT_Easing`, Deklaration in Plan_B3 §2.1) | Formel f(t), t ∈ [0,1] |
|---|---|
| `LINEAR` | `t` |
| `SMOOTH` | `t*t*(3 - 2t)` (COT `JMCinematicCamera.SmoothStep`) |
| `SMOOTHER` | `t*t*t*(t*(6t - 15) + 10)` (COT `SmootherStep`, quintisch) |
| `EASE_IN`* | `t*t` (quadratisch) |
| `EASE_OUT`* | `1 - (1-t)*(1-t)` |

\* `EASE_IN`/`EASE_OUT` sind additive Laufzeit-Erweiterungen gegenüber dem in Plan_B3_Datenmodell_Persistenz.md §2.1/E7 festgelegten Schema-v1-Easing-Set (`"Linear"`/`"Smooth"`/`"Smoother"`); sie werden zunächst nur intern von Bewegungs-Presets (§5.9 Whip Pan) genutzt und erst nach additiver Schema-Erweiterung (JSON-Strings `"EaseIn"`/`"EaseOut"`, Migrationsregel M1) persistiert.

```c
// Phase 1 — PCT_Math: Easing
static float EaseSmoothStep(float t)
{
	return t * t * (3.0 - 2.0 * t);
}

static float EaseSmootherStep(float t)
{
	return t * t * t * (t * (6.0 * t - 15.0) + 10.0);
}

static float EaseInQuad(float t)
{
	return t * t;
}

static float EaseOutQuad(float t)
{
	float inv = 1.0 - t;
	return 1.0 - inv * inv;
}

static float Ease(int easingType, float t)
{
	switch (easingType)
	{
		case EPCT_Easing.SMOOTH:
			return EaseSmoothStep(t);
		case EPCT_Easing.SMOOTHER:
			return EaseSmootherStep(t);
		case EPCT_Easing.EASE_IN:
			return EaseInQuad(t);
		case EPCT_Easing.EASE_OUT:
			return EaseOutQuad(t);
	}
	return t;
}
```

---

## 5. Bewegungs-Presets

Jedes Preset erzeugt entweder (a) Keyframes für den Sequencer (deklarativ, exportierbar) oder (b) einen Live-Controller im jeweiligen Kameramodus (§1). Presets aus Plan_A1 §6; Parameter überall: Dauer, Easing, optional Ziel.

### 5.1 Static — *Phase 3*
Definition: Kamera vollständig statisch. Parameter: Dauer. Implementierung: ein Keyframe, `PLAYBACK` hält Position/Orientierung; alle Kanäle konstant.

### 5.2 Dolly In/Out — *Phase 3*
Definition: Fahrt entlang der Blickachse auf das Motiv zu / davon weg. Parameter: Distanz (m), Dauer, Easing. Implementierung: 2 Keyframes; Endposition = Start + `GetTransformAxis(2) * distanz` (Achse 2 = Blickrichtung; semantisch identisch mit `Object.GetDirection()`, Research_Vanilla_APIs §7 — Achsindex-Zuordnung im Phase-3-Test einmalig gegen `GetDirection/GetDirectionUp/GetDirectionAside` gegenprüfen).

### 5.3 Truck Left/Right & 5.4 Pedestal Up/Down — *Phase 3*
Definition: seitliche bzw. vertikale Fahrt **im Kamera-Raum** (nicht Welt-Achsen). Parameter: Distanz, Dauer, Easing, optional Look-at-Lock. Implementierungsskizze (gemeinsam):

```c
// Phase 3 — achsgebundene Fahrt im Kamera-Raum via GetTransformAxis (EnEntity.c:337)
vector ComputeAxisTarget(Camera cam, int axis, float distance)
{
	vector axisDir = cam.GetTransformAxis(axis);	// 0 = Aside (Truck), 1 = Up (Pedestal), 2 = Direction (Dolly)
	vector step = axisDir * distance;
	vector result = cam.GetPosition() + step;
	return result;
}
```

### 5.5 Pan / Tilt / Roll — *Phase 3*
Definition: reine Orientierungsfahrt (Yaw / Pitch / Roll), Position konstant. Parameter: Winkel (Grad), Dauer, Easing. Implementierung: 2 Keyframes mit identischer `position`, unterschiedlichen `angles`; §4.5 interpoliert kürzesten Weg. Dutch-Angle-Presets (15/25/40°, Plan_A1 §4) sind Roll-Sonderfälle mit Dauer 0.

### 5.6 Orbit / Arc — *Phase 3*
Definition: Bogenfahrt um ein Ziel bei konstantem Radius und Look-at. Parameter: Zielpunkt/-objekt, Winkelbereich (Grad), Drehrichtung, Dauer, Easing, optional Höhenänderung (Spirale). Implementierung: Live-Modus ORBIT rotiert pro Frame; alternativ werden n Keyframes (alle 15–30°) auf den Kreis gebacken (für Export/Editierbarkeit).

```c
// Phase 3 — PCT_Math: Rotation um Punkt (Y-Achse); Clean-Room nach ExpansionMath.ExRotateAroundPoint
static vector RotateAroundPointY(vector center, vector position, float angleDeg)
{
	float angleRad = angleDeg * Math.DEG2RAD;
	float sinA = Math.Sin(angleRad);
	float cosA = Math.Cos(angleRad);
	vector offset = position - center;
	float rotatedX = offset[0] * cosA - offset[2] * sinA;
	float rotatedZ = offset[0] * sinA + offset[2] * cosA;
	vector result;
	float x = center[0] + rotatedX;
	result[0] = x;
	float y = position[1];
	result[1] = y;
	float z = center[2] + rotatedZ;
	result[2] = z;
	return result;
}
```

Nach jedem Schritt `cam.LookAt(center)` (`Camera.c:80`).

### 5.7 Tracking / Follow / Lead — *Phase 3*
Definition: Kamera begleitet ein Ziel (neben / hinter / vor der Figur) mit gedämpfter Verzögerung. Parameter: Zielobjekt, optionaler Bone (z. B. `"Head"`), Wunsch-Offset im Zielraum, `smoothTime`, Look-at an/aus. Implementierung: kritisch gedämpfte Verfolgung mit `Math.SmoothCD` (`EnMath.c:678`) — das Expansion-Helikopter-Boom-Muster (`Math.SmoothCD` + `Math3D.YawPitchRollMatrix`, Research_Framework_Patterns §1). Bone-Ziel über `Human.Cast` + `GetBoneIndexByName` (`Human`-API, `3_Game\DayZ\human.c:1387`) und `GetBonePositionWS` (`Object`-API, `3_Game\DayZ\Entities\Object.c:244`; Research_Vanilla_APIs §8) — beide APIs liegen in 3_Game, `PlayerBase` (4_World) ist dafür nicht nötig. `PCT_FollowController` ist eine 3_Game-Klasse (wird aus `PCT_CinematicCamera.OnUpdate` getickt, §1/§9 — darf daher keine 4_World-Typen referenzieren).

```c
// Phase 3 — 3_Game: Follow-Controller (SmoothCD-Boom, Expansion-Muster als Vorlage)
class PCT_FollowController
{
	Object m_Target;				// Engine-Entity, keine ref nötig (Vorbild: JMCameraBase.SelectedTarget)
	string m_BoneName;
	vector m_Offset;				// Wunsch-Offset relativ zum Ziel
	float m_SmoothTime;
	float m_VelX[1];
	float m_VelY[1];
	float m_VelZ[1];

	vector ComputeCameraPosition(vector currentPos, float timeslice)
	{
		vector anchor = m_Target.GetPosition();
		Human human = Human.Cast(m_Target);
		if (human && m_BoneName != "")
		{
			int boneIndex = human.GetBoneIndexByName(m_BoneName);
			anchor = human.GetBonePositionWS(boneIndex);
		}
		vector desired = anchor + m_Offset;
		vector result;
		float x = Math.SmoothCD(currentPos[0], desired[0], m_VelX, m_SmoothTime, 1000.0, timeslice);
		result[0] = x;
		float y = Math.SmoothCD(currentPos[1], desired[1], m_VelY, m_SmoothTime, 1000.0, timeslice);
		result[1] = y;
		float z = Math.SmoothCD(currentPos[2], desired[2], m_VelZ, m_SmoothTime, 1000.0, timeslice);
		result[2] = z;
		return result;
	}
}
```

Tracking = Offset seitlich; Follow = Offset hinter Bewegungsrichtung; Lead = Offset vor Bewegungsrichtung (Richtung aus `m_Target.GetDirection()`).

### 5.8 Handheld / Shoulder — *Phase 3 (Overlay-Infrastruktur), Feintuning Phase 5*
Definition: additiver Yaw/Pitch/Roll-Offset, der menschliches Halten simuliert. **Kein Perlin-Noise in Vanilla verfügbar** (Research_Vanilla_APIs §9) — daher Summe aus 2–3 Sinusoszillatoren mit inkommensurablen Frequenzverhältnissen plus `Math.SmoothCD`-gefiltertem Random-Drift. Parameter: `amplitudeDeg` (Handheld ≈ 0.4–1.2°, Shoulder ≈ 0.15–0.5°), `frequencyHz` (≈ 0.3–2.0), Seed.

```c
// Phase 3 — Handheld-Oszillator (eine Instanz je Achse: Yaw/Pitch/Roll)
class PCT_HandheldNoise
{
	float m_Amplitude;			// Grad
	float m_Frequency;			// Hz
	float m_Time;
	float m_RandomTarget;
	float m_RandomCurrent;
	float m_RandomTimer;
	float m_RandomVel[1];

	float ComputeOffset(float timeslice)
	{
		m_Time += timeslice;
		float w1 = m_Time * m_Frequency * Math.PI2;
		float w2 = m_Time * m_Frequency * 1.73 * Math.PI2;
		float w3 = m_Time * m_Frequency * 3.11 * Math.PI2;
		float s1 = Math.Sin(w1);
		float s2 = Math.Sin(w2 + 1.3) * 0.5;
		float s3 = Math.Sin(w3 + 4.2) * 0.25;
		m_RandomTimer -= timeslice;
		if (m_RandomTimer <= 0.0)
		{
			m_RandomTarget = Math.RandomFloat(-1.0, 1.0);
			m_RandomTimer = Math.RandomFloat(0.4, 1.2);
		}
		m_RandomCurrent = Math.SmoothCD(m_RandomCurrent, m_RandomTarget, m_RandomVel, 0.5, 1000.0, timeslice);
		float sum = s1 + s2 + s3 + m_RandomCurrent;
		return sum * m_Amplitude;
	}
}
```

Anwendung als Overlay (drift-frei, weil absolut statt inkrementell): Basis-Orientierung `m_BaseAngles` merken, pro Frame `angles = basis + offset(yaw/pitch/roll)`, dann `SetYawPitchRoll(angles)`. Verworfene Alternative: Vanilla `CameraShake`/`SpawnCameraShakeProper` — hängt am Aiming-Model des **Spieler**-Kamerasystems und wirkt nicht auf eine freistehende `Camera` (Research_Vanilla_APIs §9).

### 5.9 Whip Pan — *Phase 3*
Definition: sehr schnelle Orientierungsfahrt (z. B. 90–180° Yaw in 0.2–0.4 s), klassisch als Übergang. Parameter: Winkel, Dauer (kurz), Richtung, optional Blur-Puls. Implementierung: reine Orientierungs-Keyframes mit `EASE_IN`-Beschleunigung; optional während der Fahrt ein Blur-Puls proportional zur Winkelgeschwindigkeit über `PPEffects.SetBlur(float)` (`PPEffects.c:168`) als Näherung.
**OFFEN — ZU VERIFIZIEREN (V5):** Echter rotatorischer Motion-Blur — die Materialklassen `PPERotBlur`/`PPEDynamicBlur` existieren (Research_Vanilla_APIs §3), ein verifizierter einfacher Setter fehlt.
*Testschritt (Phase 3):* Über `PPERequesterBank`/Materialparameter (`PPEManager`-Weg) einen RotBlur-Parameter setzen und Bildwirkung prüfen; falls unzugänglich, bleibt der `SetBlur`-Puls die Lösung.

### 5.10 Zoom / Dolly-Zoom — *Phase 3*
Zoom: reine `fovDegrees`-Keyframes (Brennweitenverlauf, Position konstant). Dolly-Zoom: Distanz-FOV-Kopplung — die Kamera fährt, die FOV wird pro Frame so nachgeführt, dass der Bildanteil des Motivs (`subjectScreenHeight`, §6) konstant bleibt. Herleitung aus §6-Formel: `screenFraction = subjectHeight / (2 · d · tan(fovV/2))` ⇒

```
fovV(d) = 2 * atan( subjectHeight / (2 * d * screenFraction) )
```

```c
// Phase 3 — Dolly-Zoom: FOV aus aktueller Distanz (Bildanteil konstant)
float ComputeDollyZoomFov(float subjectHeight, float screenFraction, float distance)
{
	float denom = 2.0 * distance * screenFraction;
	float fovRad = 2.0 * Math.Atan(subjectHeight / denom);
	return fovRad;
}
```

Parameter: Start-/End-Distanz, Dauer, Easing; `subjectHeight`/`screenFraction` werden beim Start eingefroren.

### 5.11 Rack Focus — *Phase 3*
Siehe §3.4 — Fokus-Keyframe-Interpolation, keine Sondermechanik. Parameter: Fokus A, Fokus B (oder Ziel A/B → Distanz via §3.3), Dauer, Easing.

---

## 6. Auto-Framing (Framing-Preset → Kameraposition)

Eingaben: Framing-Preset (`subjectScreenHeight`, `headroom`, `lookRoom`, `subjectSide` — Plan_A1 §13, z. B. Close-up `subjectScreenHeight = 0.82`), Angle-Preset (Höhenmodus), `PCT_CameraRig` (liefert `fovV`). Algorithmus (Modus FRAMED, §1):

1. **Ziel wählen.** Raycast aus der Kameramitte (identisch §3.3, `DayZPhysics.RaycastRV`-Muster) oder explizite Auswahl über die Objektliste/ESP-artige Selektion (COT-ESP-Modul als Vorbild, Research_COT_Infrastructure §6; Auswahl-UI siehe Plan_B5).
2. **Motivhöhe bestimmen.**
   - Charaktere (`Human.Cast` — 3_Game-API, kein `PlayerBase`/4_World nötig): Fußpunkt = `GetPosition()`, Kopf = `GetBonePositionWS(GetBoneIndexByName("Head"))` (`Human`-/`Object`-API in 3_Game, Research_Vanilla_APIs §8); `subjectHeight = kopfY - fussY + 0.12` (Schädeldecken-Zuschlag, kalibrierbar). Augenhöhe analog über den Kopf-Bone.
   - Objekte/Requisiten: Bounding-Box-Höhe. **OFFEN — ZU VERIFIZIEREN (V3):** exakte API — `GetCollisionBox` vs. `ClippingInfo` (keine von beiden ist im Research-Korpus verifiziert). *Testschritt (Phase 2):* An 3 Referenzobjekten (Tonne, Fahrzeug, Zaun) beide Kandidaten in einer Diag-Routine aufrufen, Min/Max ausgeben, gegen Augenmaß/Modellhöhe prüfen; die zuverlässigere API in `PCT_Math.GetObjectHeight` kapseln.
3. **Gewünschter Bildanteil** `screenFraction = subjectScreenHeight` aus dem Framing-Preset.
4. **Notwendige Distanz.** Sichtbare Bildhöhe in Distanz d ist `2 · d · tan(fovV/2)` ⇒

   ```
   d = (subjectHeight / screenFraction / 2) / tan(fovV / 2)
   ```

   ```c
   // Phase 2 — Kernrechnung Auto-Framing
   float halfFov = fovVertical * 0.5;
   float tanHalf = Math.Tan(halfFov);
   float distance = (subjectHeight / screenFraction / 2.0) / tanHalf;
   ```

5. **Kamerahöhe** gemäß Angle-Preset: `EyeLevel` = Höhe des Augen-/Kopf-Bones; `HighAngle` = Augenhöhe + Δh mit Pitch nach unten; `LowAngle`/`WormsEye` entsprechend darunter; `GroundLevel` = 0.1–0.4 m (Tabellen aus Plan_A1 §4 als Daten-Presets in Presets\, Plan_B3).
6. **Position setzen.** Richtung = horizontale Blickachse vom Ziel zur gewünschten Seite (Standard: aktuelle Kamerarichtung invertiert, alternativ Preset-Azimut). `camPos = zielAnker + richtung * d`, Höhe aus Schritt 5. `subjectSide`/`lookRoom`: laterale Verschiebung des Look-at-Punkts in Bildanteilen; Umrechnung Bildanteil → Weltmeter über die sichtbare Bildbreite `2 · d · tan(fovH/2)` mit `tan(fovH/2) = tan(fovV/2) · (sensorWidth/sensorHeight)`.
7. **Ausrichten:** `cam.LookAt(zielAnker)`; danach Headroom-Korrektur in Bildkoordinaten: vertikaler Weltversatz des Look-at-Punkts = `(headroomSoll - headroomIst) · 2 · d · tan(fovV/2)`; `headroomIst` wird über `g_Game.GetScreenPosRelative(kopfPos)` gemessen (0..1-Raum, Research_Vanilla_APIs §5).
8. **Kollisionscheck:** `g_Game.IsBoxCollidingGeometry(camPos, camOri, "0.4 0.4 0.4", ...)` (`Game.c:1344`). Bei Kollision: Alternativvorschläge durch seitliches Rotieren um das Ziel — `PCT_Math.RotateAroundPointY(zielAnker, camPos, ±15°)`, bis zu 12 Versuche (±180°); jede freie Position erneut Schritt 7. Kein freier Platz → Warnung über CF `NotificationSystem` (Research_Framework_Patterns §6) und Position trotzdem setzen (der Nutzer entscheidet — Plan_A1 §10: Fehler kennzeichnen, nicht verbieten).

Entwurfsentscheidung: Auto-Framing ist eine **reine Berechnung + einmaliges Setzen** (kein per-Frame-Constraint-Solver). Begründung: deterministisch, budgetneutral (§9), und der Nutzer kann das Ergebnis manuell verfeinern (Übergang FRAMED → FREE). Verworfene Alternative: kontinuierliches Re-Framing pro Frame — kollidiert mit manueller Kontrolle und Playback.

---

## 7. Kontinuitäts-Assistent (Geometrie)

Alle Prüfungen sind **event-getrieben** (bei Shot-Speichern, Keyframe-Änderung, Shot-Wechsel im Sequencer — §9), nicht pro Frame. Warnungen kennzeichnen, verbieten nie (Plan_A1 §10). Shot-Metadaten (Kameraposition, Ziel(e), Brennweite) kommen aus dem Datenmodell (Plan_B3).

### 7.1 180-Grad-Linie
Achse zwischen zwei Zielen A, B (XZ-Ebene). Seite der Kamera C = Vorzeichen der Y-Komponente des Kreuzprodukts:

```
s(C) = (B.x - A.x) * (C.z - A.z) - (B.z - A.z) * (C.x - A.x)
```

`s > 0` = links der Achse, `s < 0` = rechts. Warnung, wenn `sign(s)` zweier **aufeinanderfolgender** Shots derselben Szene wechselt: "Diese Kamera überschreitet die 180-Grad-Linie…" (Formulierung Plan_A1 §10).

```c
// Phase 4 — 180-Grad-Seitentest
static float SideOfAxis(vector a, vector b, vector camPos)
{
	float axisX = b[0] - a[0];
	float axisZ = b[2] - a[2];
	float camX = camPos[0] - a[0];
	float camZ = camPos[2] - a[2];
	float side = axisX * camZ - axisZ * camX;
	return side;
}
```

### 7.2 30-Grad-Regel
Für zwei aufeinanderfolgende Shots mit gemeinsamem Ziel T: Blickvektoren `v1 = norm(T - cam1)`, `v2 = norm(T - cam2)`;

```
theta = acos( v1 · v2 )          // Math.Acos, EnMath.c:379
focalRatio = max(f1, f2) / min(f1, f2)
Warnung, wenn theta < 30° UND focalRatio < 1.5
```

Begründung des Brennweiten-Terms: Ein Schnitt unter 30° wirkt nur dann als Jump Cut, wenn auch die Einstellungsgröße ähnlich ist; ein deutlicher Brennweitensprung (z. B. 35 → 85 mm ⇒ ratio 2.4) "rettet" den Schnitt. Schwellwert 1.5 ist ein kalibrierbarer Startwert (Settings.json).

### 7.3 Mindestfokus-Warnung
`focusDistance < rig.minFocusDistanceM` **oder** (im FRAMED-Modus) `distanzZumMotiv < minFocusDistanceM` → Warnung "Objektiv unterschreitet seine Mindestfokusdistanz" (Plan_A1 §10).

### 7.4 Kamera-in-Geometrie-Warnung
`g_Game.IsBoxCollidingGeometry(camPos, camOri, "0.3 0.3 0.3", ...)` beim Speichern eines Shots/Keyframes → Warnung "Kamera kollidiert mit Wand oder Requisite". Gleicher Aufruf wie §6 Schritt 8, andere Boxgröße (enger, da hier nur echte Durchdringung zählt).

### 7.5 Motiv-im-Frame-Check
Zweistufig (beide APIs verifiziert, Research_Vanilla_APIs §5):
1. **Frustum:** `g_Game.GetWorld().IsBoxVisible(mins, maxs, flags)` mit der Motiv-Bounding-Box (`flags & 1` zusätzlich PVS) — Motiv überhaupt sichtbar?
2. **Komposition:** `g_Game.GetScreenPosRelative(kopfPos)` bzw. Box-Eckpunkte → alle relevanten Punkte in `[0+rand, 1-rand]` (Safe-Margin aus dem Kompositions-Preset)? `.z` (= Distanz) > 0 sicherstellen. Zusätzlich Headroom-Ist/Soll-Vergleich wie §6 Schritt 7 → "Headroom ändert sich unbeabsichtigt"-Warnung zwischen Shots.

---

## 8. PPE-Zusatzeffekte pro Shot

Pro Shot speicherbare Look-Parameter (Datenmodell Plan_B3), angewendet beim Aktivieren des Shots, zurückgesetzt beim Verlassen:

| Effekt | API (verifiziert) | Reset |
|---|---|---|
| Vignette | `PPEffects.SetVignette(intensity, r, g, b, a)` (`PPEffects.c:434`; COT nutzt `SetVignette(x, 0, 0, 0, 0)`) | `SetVignette(0, …)` / `ResetAll` |
| Chromatische Aberration | `PPEffects.SetChromAbb(float)` (`PPEffects.c:284`) | `SetChromAbb(0)` / `ResetAll` |
| Bloom/Exposure-Look | `PPEffects.SetBloom(thres, steep, inten)` (`PPEffects.c:737`; COT-Exposure-Slider-Muster) | `ResetAll` |
| EV (Belichtung) | `g_Game.SetEVUser(float)` (−50..50, `Game.c:1352`) | `SetEVUser(0)` |
| DOF | §3 (`SetFocus` + `OverrideDOF`) | `PPEffects.ResetDOFOverride()` (`PPEffects.c:518`) |
| Film Grain | Materialklasse `PPEFilmGrain` existiert (`3_Game\DayZ\PPEManager\Materials\MatClasses\`); Legacy-Weg über Material `filmgrainNV` (Research_Vanilla_APIs §3) | Materialparameter auf Default |

**OFFEN — ZU VERIFIZIEREN (V6):** Konkreter Zugriffsweg für Film Grain (es gibt keinen verifizierten `PPEffects.SetFilmGrain`-Helper): entweder eigener `PPERequesterBase`-Requester auf die `PPEFilmGrain`-Parameter (`SetTargetValueFloat`-Muster aus `PPERCameraADS_Opt.c`, Research_Vanilla_APIs §3) oder direktes `GetMaterial("graphics/materials/postprocess/filmgrainNV")` + `SetParam`.
*Testschritt (Phase 2):* Beide Wege in einer Diag-Funktion durchprobieren, Parameternamen aus der Materialklasse ablesen, sichtbare Wirkung dokumentieren; den funktionierenden Weg in `PCT_ShotLook.Apply()` kapseln.

**Reset-Disziplin (zwingend, COT-Muster `Client_Leave` — Research_COT_Infrastructure §1):** Beim Leave der Kamera und beim Deaktivieren eines Shots **immer** `PPEffects.ResetDOFOverride()` ausführen; beim Leave zusätzlich `PPEffects.ResetAll()` (`PPEffects.c:744`) und `g_Game.SetEVUser(0)` (mit `if (!g_Game) return;`-Guard). Jeder Codepfad, der Effekte setzt, registriert sich in einer zentralen `PCT_EffectState`-Liste, die genau eine `ResetAllEffects()`-Methode besitzt — verhindert "vergessene" Effekte bei Abstürzen einzelner Pfade. Entwurfsentscheidung: Legacy-`PPEffects` statt `PPEManager`-Requester als Primärweg, weil COT (unsere Laufzeitumgebung) dieselben statischen Helper nutzt und sich beide Systeme sonst um Prioritäten streiten; der `PPEManager`-Weg bleibt Film Grain (V6) vorbehalten.

---

## 9. Performance-Budget

Zielbudget: Kamera-Engine gesamt **< 0.2 ms/Frame** auf Referenz-Hardware; Messung per Diag-Timer in Phase 5.

| Regel | Umsetzung |
|---|---|
| Ein einziger Frame-Hook | Alles läuft über `PCT_CinematicCamera.OnUpdate` (EOnPostFrame, §1); keine eigenen `CallLater`-Ketten im Frame-Takt (`g_Game.GetCallQueue()` zudem stets null-checken) |
| Playback O(1) pro Frame | Segment-Lookup über gecachten Vorwärtszeiger (`m_CachedSegment`, §4.2); Arc-Length-LUT-Lookup ebenfalls mit Vorwärtszeiger (Zeit monoton); bei Scrub-Rücksprung Zeiger zurücksetzen |
| Sampling-Tabellen cachen | Arc-Length-LUTs (§4.4) **nur** bei Sequenz-Laden/-Änderung neu bauen (64 Samples/Segment), nie pro Frame |
| Keine Allokationen im Frame-Loop | `m_Positions`, LUT-Arrays, Keyframe-Liste vorab allozieren; im `Tick` kein `new`, keine String-Konkatenation; `vector`/`float` sind Werttypen — unkritisch |
| PPE nur bei Änderung | `OverrideDOF`/`SetVignette`/… nur aufrufen, wenn sich der Zielwert seit dem letzten Frame geändert hat (Dirty-Flag); Ausnahme: aktive Fokus-/Blur-Interpolation (dann ändert sich der Wert ohnehin) |
| Projektion-Overlays gated | `GetScreenPosRelative`-basierte Overlays (Framing-Guides, Pfadanzeige — Plan_B5) nur berechnen, wenn das HUD/Overlay sichtbar ist; Widget-Updates auf 30 Hz drosselbar |
| Continuity event-getrieben | §7-Checks nur bei Shot-Speichern/-Wechsel und Keyframe-Edit, nie pro Frame; `IsBoxCollidingGeometry`/`IsBoxVisible` sind die teuersten Aufrufe und bleiben aus dem Frame-Loop draußen |
| Autofokus gedrosselt | §3.3-Raycast maximal 10 Hz (Timer), nicht jeden Frame — COT raycastet pro Frame; wir drosseln bewusst |
| GC-Regeln | `ref` nur auf Membern; Effekt-/Controller-Objekte einmalig im Konstruktor anlegen und wiederverwenden; keine temporären Arrays in `Tick` |

---

## Querverweise

Geschwisterdokumente (B-Reihe):

- `Plan_B1_Architektur.md` — Systemarchitektur, Modul-System, RPC (`EPCT_RPC`-Basis 10500), Permissions, Enter/Leave-Flow, Mod-Paketstruktur
- `Plan_B2_Feature_Mapping.md` — Feature-Mapping Plan_A1 → DayZ-Machbarkeit (Phaseneinstufung, Ersatzstrategien)
- `Plan_B3_Datenmodell_Persistenz.md` — JSON-Schemata (Shots\, Sequences\, Presets\, Settings.json, `schemaVersion`), `JsonFileLoader<T>`
- `Plan_B4_Kamera_Engine.md` — dieses Dokument
- `Plan_B5_UI_UX.md` — Formulare (`PCT_CinematicForm`), Timeline-UI, Overlays/Masken, Kompositionshilfen, Keybinds
- `Plan_B6_Roadmap_Risiken.md` — Phasenplan, Teststrategie, Risiken, konsolidierte OFFEN-Liste (Wetter/Zeit, Akteure, Licht, Export als Phase-4/5-Arbeitspakete)

Quellen (verifizierte Research-Basis):

- `Research/Research_COT_Infrastructure.md` — COT-Modul-System, RPC, Permissions, Kamera-Flow, `JMCinematicCamera`
- `Research/Research_Vanilla_APIs.md` — Camera/FOV, PPE/DOF, `Math3D.Curve`, Screen-Space, Bones, Shake/Noise, Lichter
- `Research/Research_Framework_Patterns.md` — ExpansionMath-/Dabs-Muster (Clean-Room-Vorlagen für `PCT_Math`), Persistenz- und UI-Entscheidungen
- `../Plan_A1.md` — Stoffsammlung/Anforderungen (Einstellungsgrößen, Winkel, Bewegungen, Presets, Kontinuitätsregeln)

Offene Verifikationspunkte dieses Dokuments: **V1** (SetFOV-Grenzen, §2.5), **V2** (blur-Skala, §3.2), **V3** (GetCollisionBox vs. ClippingInfo, §6), **V4** (Curve-Endpunkte/Parametrisierung, §4.4), **V5** (RotBlur-Zugriff, §5.9), **V6** (Film-Grain-Zugriff, §8).
