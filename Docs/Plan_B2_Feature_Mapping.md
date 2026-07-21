# Feature-Mapping Plan_A1 → DayZ-Machbarkeit (Plan_B2)

> Stand: 2026-07-21 · Status: Entwurf v1 · Projekt: PsyernsCinematicTool (PCT) · Autor: Psyern

**Abstract:**
Dieses Dokument mappt alle 17 Abschnitte der Stoffsammlung `Plan_A1.md` auf die verifizierte DayZ/Enfusion-Machbarkeit.
Grundlage sind ausschließlich die drei Research-Dokumente (COT-Infrastruktur, Vanilla-1.29-APIs, Framework-Patterns); nicht belegte APIs sind als „OFFEN — ZU VERIFIZIEREN" markiert.
Kernergebnis: Kamera, Framing-Presets, Sequencer, Kompositionshilfen und Kontinuitäts-Checks sind voll umsetzbar; Objektiv-Physik, Licht und Exporte teilweise; Raum-Editor, Multi-Viewport und Web-Stack werden durch DayZ-native Ersatzstrategien ersetzt.
Jede Zeile nennt die konkrete API bzw. Systematik und die PCT-Phase (0 Skeleton, 1 Kamera-Kern, 2 Shots & Presets, 3 Sequencer, 4 Welt/Akteure/Licht & Assistenten, 5 Polish & Release).
Abschluss: bewusste Nicht-Ziele, Ersatzstrategien und die konsolidierte OFFEN-Liste mit Testschritten.

---

## Legende

| Einstufung | Bedeutung |
|---|---|
| **VOLL** | Feature ist mit verifizierten APIs vollständig abbildbar. |
| **TEILWEISE** | Kernnutzen abbildbar, aber mit Einschränkungen oder Näherungen. |
| **ERSATZ** | Originalfeature nicht abbildbar; ein funktional gleichwertiger DayZ-nativer Ersatz ist definiert. |
| **NICHT** | Nicht abbildbar; bewusstes Nicht-Ziel (siehe Schlusslisten). |

Phasen: **0** Skeleton · **1** Kamera-Kern · **2** Shots & Presets · **3** Sequencer · **4** Welt/Akteure/Licht & Assistenten · **5** Polish & Release.

Quellenkürzel: **[COT]** = Research_COT_Infrastructure.md · **[VAN]** = Research_Vanilla_APIs.md · **[FRW]** = Research_Framework_Patterns.md.

---

## 1. Grundidee (A1 §1) — Previsualisierung in der DayZ-Welt

Die Grundidee trägt vollständig, mit einer bewussten Umdeutung: PCT baut keine Drehorte nach — die DayZ-Welt (Chernarus, Livonia, Sakhal, Custom-Maps) **ist** der Drehort. PCT ist damit ein In-Engine-Previs-Tool für Machinima-, Trailer- und Server-Content-Produktion, kein generisches Drehvorbereitungstool für reale Filmsets.

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| „Wo muss die Kamera stehen" | VOLL | Freecam `PCT_CinematicCamera : JMCinematicCamera`, Positions-/Orientierungsdaten pro Shot [COT §1] | 1 | Kamera-Enter/Leave über COT-Flow (`SelectPlayer`/`SelectSpectator`) |
| „Welches Objektiv" | TEILWEISE | Sensor+Brennweite → FOV-Umrechnung in `PCT_Math` (Engine hat keine Brennweiten-API) [VAN §2] | 1 | Details siehe §5 |
| „Wie groß ist der Bildausschnitt" | VOLL | `Camera.SetFOV(rad)` + Auto-Framing-Mathematik [VAN §1, §2] | 2 | Details siehe §3 |
| „Wie werden Darsteller positioniert" | ERSATZ | Gespawnte `PlayerBase`-Dummies, CTActor-Muster [VAN §8] | 4 | Details siehe §7 |
| „Wie bewegen sich Kamera und Darsteller" | VOLL / TEILWEISE | Keyframes + `Math3D.Curve`; Darsteller via `OverrideMovementSpeed/Angle` [VAN §4, §8] | 3–4 | Details siehe §6 / §7 |
| „Funktionieren Blickrichtungen und Anschluss" | VOLL | Geometrie-Checks (180°, 30°, Screen Direction) in `PCT_ContinuityChecker` | 4 | Details siehe §10 |
| „Welche Wirkung erzeugt eine Einstellung" | VOLL | Dokumentarisch: Wirkungs-Metadaten in Preset-JSONs (Tags, Beschreibung) | 2 | Keine „Wirkungs-Engine", nur kuratierte Daten |
| „Ist die Einstellung räumlich machbar" | TEILWEISE | `IsBoxCollidingGeometry`-Warnung am Kamerastandort/Pfad [VAN §5] | 4 | Nur Warnung, kein Auto-Ausweichen |

---

## 2. Begriffliche Trennung (A1 §2) — Fünf unabhängige Ebenen

**Machbarkeit: VOLL.** Die fünf Ebenen werden 1:1 als getrennte Blöcke im Shot-Datenmodell abgebildet (siehe Plan_B3_Datenmodell_Persistenz.md). Begründung: Die Trennung ist eine reine Datenmodell-Entscheidung und engine-unabhängig; verworfene Alternative wäre ein monolithisches „Preset = fertige Kamera-Pose"-Modell, das die von A1 geforderte Kombinierbarkeit (Close-up mit 35 mm vs. 135 mm vergleichen) unmöglich machte.

| Feature (Ebene) | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Einstellungsgröße | VOLL | Datenklasse Framing-Block (subjectScreenHeight, headroom, lookRoom) → Auto-Framing | 2 | siehe §3 |
| Kameraperspektive | VOLL | Datenklasse Angle-Block (heightMode, pitch, roll, Relativposition zum Motiv) | 2 | siehe §4 |
| Objektiv | TEILWEISE | Datenklasse Lens-Block (Sensor, Brennweite, Blende) → FOV/DOF-Mapping in `PCT_Math` | 1–2 | siehe §5 |
| Kamerabewegung | VOLL | Datenklasse Movement-Block → Keyframe-Kanäle im Sequencer | 3 | siehe §6 |
| Bildkomposition | VOLL | Datenklasse Composition-Block → Overlay-Konfiguration + Marker | 2 | siehe §9 |

Alle fünf Blöcke sind unabhängig editierbar und werden erst zur Laufzeit zu einer Kamera-Pose aufgelöst (Auflösungsreihenfolge in Plan_B4).

---

## 3. Einstellungsgrößen als Presets (A1 §3)

**Machbarkeit: VOLL** (Pflicht-Einordnung). Auto-Framing über Motivhöhe + FOV-Mathematik; die vollständige Herleitung steht in Plan_B4. Kernformeln (eigene Implementierung in `PCT_Math`, da die Engine keine Brennweiten-API besitzt [VAN §2]):

```
fovVertical = 2 * atan( sensorHeight / (2 * focalLength) )
distance    = ( subjectHeight / subjectScreenHeight ) / ( 2 * tan( fovVertical / 2 ) )
```

`Camera.SetFOV(float fov)` erwartet **vertikale FOV in Radiant** [VAN §1].

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Größen-Presets EWS/ES/WS/FS/MLS/CS/MS/MCU/CU/BCU/ECU | VOLL | Preset-JSON (subjectScreenHeight-Zielwert + Brennweiten-Startwert) → Distanzberechnung → `SetPosition`/`LookAt`/`SetFOV` [VAN §1] | 2 | Startbrennweiten aus A1-Tabelle als Default-Daten |
| Insert / Cut-in / Cutaway | VOLL | Wie oben, Motiv = beliebiges Objekt statt Person | 2 | Motivmaß für Nicht-Player: **OFFEN — ZU VERIFIZIEREN** (O2) |
| Motivhöhe messen (Person) | VOLL | `GetBoneIndexByName` + `GetBonePositionWS` (Kopf-/Fuß-Bones) [VAN §8] | 2 | Vanilla-CameraTools nutzt exakt dieses Muster |
| Single / Two / Three / Group Shot | VOLL | Framing auf gemeinsame Bounding-Sphäre mehrerer Motive (Mittelpunkt + max. Ausdehnung) | 2 | reine Geometrie in `PCT_Math` |
| OTS Dirty / Clean / Wide OTS | VOLL | Kameraposition relativ zu Schulter-Bone der Vordergrundfigur, `LookAt` auf Kopf-Bone der Zielfigur [VAN §8] | 2 | „Dirty/Clean" = Offset-Varianten im Preset |
| POV / Subjective | VOLL | Kameraposition auf Augen-/Kopf-Bone (`GetBonePositionWS`), Orientierung aus `GetBoneRotationWS` [VAN §8] | 2 | |
| Profile / Reaction / Master / Symmetrical / Top-down Detail | VOLL | Winkel-/Positionslogik relativ zum Motiv (siehe §4) | 2 | |
| Silhouette Shot / Product Hero | VOLL | Framing-Preset + verknüpftes Licht-/Zeit-Preset (Gegenlicht via `SetDate`, Lichter §8) | 4 | Licht-Anteil erst Phase 4 |
| Automatisches Framing (Headroom-Feinjustage) | VOLL | Nach Distanzlösung: Pitch-/Höhen-Korrektur, bis projizierter Kopf-Bone (`GetScreenPosRelative`) den Headroom-Zielwert trifft [VAN §5] | 2 | iterative Feinjustage, Plan_B4 |

---

## 4. Kamerawinkel und Kamerahöhen (A1 §4)

**Machbarkeit: VOLL** (Pflicht-Einordnung). Alle Winkel-Presets sind Positions-/Orientierungslogik relativ zum Motiv: Höhenmodus (absolut oder relativ zu Bone-Höhen), Pitch, Roll, Azimut. Umsetzung über `SetPosition`, `SetOrientation` (Yaw/Pitch/Roll in **Grad** [VAN §7]) bzw. `Camera.LookAt(vector)` [VAN §1].

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Eye / Shoulder / Hip / Knee / Ground Level | VOLL | Kamerahöhe aus Bone-Höhen der Zielfigur (`GetBonePositionWS`) bzw. festen Offsets [VAN §8] | 2 | Höhenmodi als Enum im Angle-Block |
| Slight High / High Angle / Low / Worm's Eye | VOLL | Höhen-Offset + Pitch aus `LookAt` auf Motiv | 2 | |
| Overhead / Bird's Eye / Top-down | VOLL | Position senkrecht über Motiv, Pitch −90°, Distanz aus Framing (§3) | 2 | |
| Dutch Angle (15/25/40°-Presets) | VOLL | Roll-Komponente via `SetYawPitchRoll` / `SetOrientation` [VAN §7] | 2 | Roll als eigener Kanal, kompatibel mit Sequencer |
| Aerial / Drone | VOLL | Freie Freecam-Position (`PCT_CinematicCamera`) [COT §1] | 1 | ist der Grundzustand der Kamera |
| POV / OTS / Profile | VOLL | Bone-relative Positionierung (siehe §3) | 2 | |

---

## 5. Virtuelle Kamera und Objektivsystem (A1 §5)

**Machbarkeit: TEILWEISE** (Pflicht-Einordnung). Sensor+Brennweite→FOV ist als eigene Mathematik voll abbildbar (die Engine hat **keine** Brennweiten-API [VAN §2]); Blende→DOF wird heuristisch auf `OverrideDOF` gemappt; ISO/Shutter/Weißabgleich sind nur näherungsweise (EV + PPE-Farbeffekte), anamorph/Lens Shift/echte Verzeichnungsprofile sind nicht abbildbar.

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Kameraname/Nummer/Modell, Gewicht, Rig-Typ | VOLL | Reine Metadaten im JSON-Datenmodell (Plan_B3) | 2 | keine Simulation, nur Doku |
| Sensorformat/-breite/-höhe, Crop-Faktor, Sensor-Presets, Kameramodell-Bibliothek | VOLL | Eigene Datenklassen + `PCT_Math`-Umrechnung; Presets als JSON unter `$profile:PsyernsCinematicTool\Presets\` [FRW §3] | 1–2 | Full Frame, S35, APS-C, MFT, 1", Smartphone, Custom |
| Brennweite → horizontales/vertikales FOV | VOLL | `fov = 2*atan(sensor/(2*f))` in `PCT_Math`; Anwendung via `Camera.SetFOV` (vertikal, Radiant) [VAN §1, §2] | 1 | horizontales FOV nur rechnerisch (Anzeige) |
| FOV-Grenzen | TEILWEISE | User-Option klemmt 0.75–1.30 rad; COT-Slider erlaubt 0.001–4.0 rad auf `Camera.SetFOV` [VAN §2; COT §1] | 1 | Clamping von `SetFOV`: **OFFEN — ZU VERIFIZIEREN** (O1) |
| Seitenverhältnis | ERSATZ | Keine Renderziel-API [VAN §10]; Masken-Panels (Letterbox/Pillarbox) als Widgets | 2 | Bild bleibt nativ, Maske simuliert Format |
| Auflösung, Bildrate | TEILWEISE | Nur Metadaten (Shotlist/Export); Engine rendert nativ | 2 | keine Render-Kontrolle |
| Blende → Schärfentiefe | TEILWEISE | Heuristisches Mapping Blende→`blur`/`focusLength`-Parameter von `g_Game.OverrideDOF(enable, focusDist, focusLen, focusLenNear, blur, offset)` [VAN §3] | 1 | Mapping-Tabelle in Plan_B4; keine physikalische DOF-Simulation |
| Fokusdistanz | VOLL | `Camera.SetFocus(distance, blur)` + `OverrideDOF` (COT-Muster: beides parallel) [VAN §1; COT §1] | 1 | Auto-Fokus-Raycast wie COT möglich [COT §1] |
| Nahgrenze des Objektivs | VOLL | Datenregel im Lens-Preset → Kontinuitäts-Warnung bei Unterschreitung (§10) | 4 | keine optische Simulation nötig |
| ISO / Shutter / ND | ERSATZ | Belichtungs-Näherung via `g_Game.SetEVUser(float)` (−50..50) [VAN §3]; kombinierter „Exposure"-Regler | 2 | physikalisch korrekt: NICHT |
| Weißabgleich / Tint | ERSATZ | Farb-Näherung via PPE-Color-Grading-Material (`PPEColorGrading`) [VAN §3] | 4 | exakte PARAM_*: **OFFEN — ZU VERIFIZIEREN** (O4) |
| Near/Far Clipping Plane | TEILWEISE | Near: `Camera.SetNearPlane` (min 0.01 m) [VAN §1]; Far: nur Engine-Slot-API `SetCameraFarPlane` [VAN §1] | 2 | Wirkung der Slot-API auf `staticcamera`/COT-Cam: **OFFEN** (O9) |
| Anamorph-Faktor, Lens Shift | NICHT | Keine Engine-Unterstützung [VAN §10] | — | Nicht-Ziel |
| Verzeichnung / Vignettierung (Objektivcharakter) | ERSATZ | PPE-Näherung: `PPEGlow` `PARAM_LENSDISTORT`, `PPEffects.SetVignette`, `SetChromAbb`, `SetLensEffect` [VAN §3] | 4 | keine echten Objektivprofile |
| Focus Breathing, Bokeh-Charakter | NICHT | Kein Zugriff auf Bokeh-Form/Breathing im PPE-System [VAN §3] | — | Nicht-Ziel |
| Zoomobjektive / Lens Kits | VOLL | Datenmodell: Brennweitenbereich; Sequencer-FOV-Kanal für Zoomfahrten (§6) | 2–3 | |
| Automatische Kameradistanz | VOLL | Distanzformel aus Motivhöhe/Bildanteil/FOV (§3, Plan_B4) | 2 | Pflicht-Einordnung |

**Architektur-Entscheidung:** Alle Objektiv-Physik läuft durch eine einzige Übersetzungsschicht `PCT_Math` (Lens-Block → FOV/DOF/EV-Werte). Begründung: nur eine Stelle, an der Heuristiken kalibriert werden; verworfene Alternative: verstreute Umrechnungen in Form/Modul, die bei Kalibrier-Iterationen (Phase 5) unwartbar würden.

---

## 6. Kamerabewegungen (A1 §6)

**Machbarkeit: GRÖSSTENTEILS VOLL** (Pflicht-Einordnung). Playback-Kern: Keyframes + Catmull-Rom (`Math3D.Curve(ECurveType.CatmullRom, t, points)` [VAN §4]) im `PCT_SequencePlayer`, aufgesetzt auf das verifizierte COT-Traveling-Muster (`JMCinematicCamera.SetupTraveling` — linear, wird um Splines/Kanäle erweitert [COT §1; FRW §8]).

**Architektur-Entscheidung:** `PCT_CinematicCamera : JMCinematicCamera` erweitert die COT-Kamera um Kanäle (Orientierung, FOV, Fokus, Roll) statt das Vanilla-Muster „zwei `staticcamera` + `InterpolateTo`" zu verwenden. Begründung: COT liefert Enter/Leave, Spectator-Sync, Input und Permissions gratis; `InterpolateTo` bietet nur 3 fixe Easing-Typen und keine eigenen Kanäle. Das `staticcamera`-Paar [VAN §1] bleibt Fallback-Referenz.

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Static | VOLL | Ein Keyframe, keine Interpolation | 3 | |
| Pan / Tilt / Roll | VOLL | Orientierungs-Keyframes; winkelbewusste Interpolation (Port von `InterpolateAngles` nach `PCT_Math`) [FRW §2, §7] | 3 | `Math3D.QuatLerp` existiert, **kein QuatSlerp** in Vanilla [VAN §4] |
| Dolly / Truck / Pedestal / Crane / Drone | VOLL | Positions-Keyframes + `Math3D.Curve(CatmullRom, ...)` [VAN §4] | 3 | Pfadverdichtung nach `PathInterpolated`-Muster (Port) [FRW §2] |
| Tracking / Follow / Lead | VOLL | `Math.SmoothCD` (kritisch gedämpft) + Bone-Follow via `GetBonePositionWS` [VAN §4, §8] | 3 | Muster: Expansion-Helikopter-Boom-Cam [FRW §1] |
| Orbit / Arc | VOLL | Eigener Orbit in `PCT_Math` (Port von `ExRotateAroundPoint`) [FRW §2, §7] | 3 | |
| Handheld / Shoulder / Gimbal / Steadicam | VOLL | Eigenes Rauschen: summierte Sinuswellen + `Math.RandomFloat` + `Math.SmoothCD`-Filterung — **kein Perlin-Noise in der Engine** [VAN §9] | 3 | Amplitude/Frequenz als Preset-Parameter; `CameraShake` ist an Player-Kamera gebunden, wird nicht genutzt [VAN §9] |
| Whip Pan | VOLL | Schnelle Orientierungs-Interpolation mit steiler Ease-Kurve; optional `PPEffects.SetBlur` während des Swish [VAN §3] | 3 | |
| Zoom In/Out | VOLL | FOV-Kanal im Sequencer (Brennweiten-Keyframes → `SetFOV`) | 3 | |
| Dolly Zoom | VOLL | FOV-Distanz-Kopplung: `2*d*tan(fov/2) = konstant` — Distanz-Keyframes treiben FOV gegenläufig (`PCT_Math`) | 3 | Pflicht-Einordnung |
| Rack Focus | VOLL | Fokus-Keyframes → `SetFocus`/`OverrideDOF` pro Frame [VAN §1, §3] | 3 | |
| Ease In/Out, Kurventyp | VOLL | `SmoothStep`/`SmootherStep` als Ein-Zeilen-Ports in `PCT_Math` (`t*t*(3-2*t)` bzw. quintisch; COT-Originale sind `private` in `JMCinematicCamera` und aus Subklassen nicht aufrufbar) + eigene Kurven [COT §1; FRW §2, §7] | 3 | wie die übrigen `PCT_Math`-Ports (`InterpolateAngles`, `ExRotateAroundPoint`) |
| Look-at-Verhalten / Zielobjekt | VOLL | `Camera.LookAt(vector)` bzw. `SelectedTarget`-Muster aus `JMCameraBase` [VAN §1; FRW §1] | 3 | |
| Position Lock / Horizon Lock | VOLL | Kanal-Sperren im Player (Roll = 0 erzwingen bzw. Position fixieren) | 3 | reine Logik |
| Handheld-Amplitude/-Frequenz, Follow-Verzögerung | VOLL | Parameter des Rausch-/SmoothCD-Modells | 3 | |
| Fokus-/Brennweitenverlauf | VOLL | Eigene Kanäle im Keyframe-Modell (Plan_B3) | 3 | |
| Collision Avoidance | TEILWEISE | Nur **Warnung** via `IsBoxCollidingGeometry` entlang des abgetasteten Pfads [VAN §5] | 4 | Pflicht-Einordnung: kein automatisches Ausweichen in frühen Phasen |
| Sichtbarer Pfad im Raum | TEILWEISE | Projizierte Widget-Marker via `GetScreenPosRelative` [VAN §5]; echte 3D-Linien: **OFFEN — ZU VERIFIZIEREN** (O5) | 3 | Fallback: Marker-Punkte statt durchgezogener Linie |

---

## 7. Szenenaufbau (A1 §7)

**Machbarkeit: ERSATZ** (Pflicht-Einordnung). Kein Raum-Editor, kein glTF-Import — die DayZ-Welt **ist** die Location. Requisiten kommen über COT Object Spawner/ESP, Akteure sind gespawnte `PlayerBase`-Dummies mit Emote-/Command-Steuerung. Begründung: Ein eigener Geometrie-Editor wäre in Enfusion weder mit vertretbarem Aufwand noch mit Asset-Import (proprietäres P3D-Format) realisierbar; verworfene Alternative „primitive Wand-Objekte spawnen" bringt gegenüber der real vorhandenen Map-Architektur keinen Previs-Mehrwert.

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Raum-Editor (Wände/Böden/Decken, Türen, Fenster) | ERSATZ | DayZ-Map-Gebäude als Locations nutzen; Location-Bookmarks im Datenmodell (Teleport via COT-Teleport-Muster [COT §6]) | 2 | |
| glTF/GLB-Import, 3D-Scan, Photogrammetrie, LiDAR, GPS-Daten, Grundriss-Import | NICHT | Kein Runtime-Modell-Import in Enfusion | — | Nicht-Ziel |
| Requisiten (Möbel, Fahrzeuge, Pflanzen, Straßenelemente) | ERSATZ | COT Object Spawner (`JMObjectSpawnerModule`, RPC-Basis 10220) + ESP-Modul (`SetPosition`/`SetOrientation`/`Delete`) [COT §6]; eigene Props via `CreateObjectEx` mit `ECE_PLACE_ON_SURFACE` bzw. client-lokal `ECE_LOCAL\|ECE_NOSURFACEALIGN\|ECE_KEEPHEIGHT` [VAN §7] | 4 | Permission `"CinematicTool.Props"`; nur vorhandene DayZ-Assets |
| Filmtechnik-Objekte (Stative, Dollys, Kräne, Mikrofone, Flags) | TEILWEISE | Platzhalter aus DayZ-Assets + Metadaten im Shot („benötigte Technik") | 4 | keine eigenen 3D-Modelle in v1 |
| Akteure spawnen | ERSATZ | `PlayerBase`-Dummies nach CTActor-Muster (`CTObjectFollower.CreateFollowedObject`-Analogon); Ausrüstung via `GetInventory().CreateAttachment` / `CreateInHands` [VAN §8] | 4 | Permission `"CinematicTool.Actors"`; MP-Dedicated-Verhalten: **OFFEN — ZU VERIFIZIEREN** (O6) |
| Posen & Aktionen | TEILWEISE | `StartCommand_Action(anim, EmoteCB, DayZPlayerConstants.STANCEMASK_ALL)` + `HumanCommandActionCallback.Cancel()` [VAN §8] | 4 | Grenze: **kein Per-Bone-Posing** (Bones nur lesbar) |
| Gehen/Laufen (Bewegungswege) | TEILWEISE | `GetInputController().OverrideMovementAngle/OverrideMovementSpeed` (CTEvent-Muster) [VAN §8]; Wegpunkte im Datenmodell | 4 | einfache Wege; keine Navigation um Hindernisse |
| Darsteller-Metadaten (Name, Rolle, Körpergröße, Cues, Marken) | VOLL | JSON-Datenmodell (Plan_B3); Augen-/Schulterhöhe zur Laufzeit aus Bones [VAN §8] | 4 | |
| Darstellermarken im Grundriss | ERSATZ | Karten-Ansicht mit Markern (COT-Map-Modul-Anleihe, §12) | 4 | |

---

## 8. Lichtplanung (A1 §8)

**Machbarkeit: TEILWEISE** (Pflicht-Einordnung). Point-/Spotlights sind via `ScriptedLightBase.CreateLight` voll verfügbar; die Sonne ist nur über Datum/Uhrzeit (`SetDate`) steuerbar — es gibt **keinen Sonnenrichtungs-Getter** [VAN §6]. IES/Softbox/Area-Lights sind nicht abbildbar; Ersatz sind kuratierte Licht-Presets aus Point/Spot-Kombinationen.

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Point Light / Spot Light | VOLL | `ScriptedLightBase.CreateLight(PointLightBase/SpotLightBase, pos, fadeIn)`; `SetBrightnessTo`, `SetRadiusTo`, `SetFadeOutTime` [VAN §12] | 4 | eigene Klassen `PCT_PointLight`/`PCT_SpotLight`; Permission `"CinematicTool.Lights"` |
| Lichtfarbe / Farbtemperatur / Gel | TEILWEISE | Kelvin→RGB-Tabelle in `PCT_Math`; Farb-Setter auf `EntityLightSource`: **OFFEN — ZU VERIFIZIEREN** (O3) [VAN §12] | 4 | |
| Abstrahlwinkel (Spot), Schattenwurf | TEILWEISE | `SpotLightBase`-/`EntityLightSource`-Surface: **OFFEN — ZU VERIFIZIEREN** (O3) | 4 | |
| Dimmer / Flicker | VOLL | Skriptgesteuerte `SetBrightnessTo`-Modulation pro Frame (Rausch-Modell aus §6) | 4 | |
| Sonne & Himmel / Sonnenstand / Tageszeit | TEILWEISE | Nur indirekt: `g_Game.GetWorld().SetDate(y,m,d,h,min)`; Zeit einfrieren via `SetTimeMultiplier(0)` [VAN §6] | 4 | kein Sonnenrichtungs-Getter → keine Sonnenstand-Anzeige; Golden Hour = Datums-/Zeit-Preset |
| Wetter | VOLL | `Weather`-Phänomene: `GetOvercast()/GetFog()/GetRain()/... .Set(forecast, time, minDuration)`; Look halten via `SetWeatherUpdateFreeze(true)`; `SetStorm(...)`, Volumetrik-Fog-Setter [VAN §6] | 4 | server-authoritativ → RPC + Permission `"CinematicTool.World"` [COT §6 Weather-Modul als Muster] |
| Directional / Area / Softbox / LED-Panel / Tube / Fresnel / IES-Profile | NICHT | Keine Engine-Entsprechung [VAN §12] | — | Nicht-Ziel; Ersatz: Presets unten |
| Practicals (Kerze, Feuer, Neon, Fensterlicht) | TEILWEISE | Vorhandene DayZ-Licht-Entities (Fackel-/Feuer-/Lampen-Muster der Vanilla-Subklassen) spawnen [VAN §12] | 4 | Fensterlicht nur real durch Sonne+Gebäude |
| Reflektor / Negativ-Fill / Diffusion / Gobos / Flags | NICHT | Kein Licht-Bounce-/Blocker-System im Scripting | — | Nicht-Ziel |
| Licht-Presets (Three-Point, Rembrandt, Low Key, Moonlight, ...) | ERSATZ | Kuratierte Presets: n Point/Spot-Lichter relativ zum Motiv platziert + Zeit-/Wetter-Anteil; JSON unter `Presets\` | 4 | Pflicht-Einordnung: Ersatzstrategie statt IES/Softbox |

---

## 9. Kompositionshilfen (A1 §9)

**Machbarkeit: VOLL** (Pflicht-Einordnung). Alles sind Client-UI-Overlays: Widget-Layouts über `g_Game.GetWorkspace().CreateWidgets(...)`, pixelgenau via `SetScreenPos/SetScreenSize`, projektionsbasierte Marker via `g_Game.GetScreenPosRelative(worldPos)` (liefert 0..1-relative Koordinaten + Distanz in `.z`) [VAN §5, §10].

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Drittelraster / Fadenkreuz / Zentrum / Golden Ratio / Diagonalen / Symmetrieachse | VOLL | Statische Overlay-Layouts (ImageWidgets/Panels), umschaltbar | 2 | ein Layout pro Raster, per Checkbox |
| Formatmasken 2.39:1 / 16:9 / 4:3 / 9:16 / 1:1 + Letterbox | VOLL | Zwei bzw. vier Panels, Größe aus `GetScreenSize` + Zielformat berechnet [VAN §10] | 2 | ersetzt Seitenverhältnis-Rendering (§5) |
| Action Safe / Title Safe / Social Safe Areas | VOLL | Rahmen-Panels mit prozentualen Insets | 2 | |
| Runde Vignette | VOLL | Nativer NDC-Mask-Pfad: `g_Game.AddPPMask(x, y, radius, blur)` / `ResetPPMask()` [VAN §3, §10] | 2 | Pflicht-Einordnung |
| Headroom / Look Room / Lead Room / Augenlinie | VOLL | Bone-Positionen (`GetBonePositionWS`) → `GetScreenPosRelative` → Markerlinien [VAN §5, §8] | 2 | Basis für Checks in §10 |
| Horizon Level | VOLL | Roll-Wert der Kameraorientierung als Anzeige (Wasserwaage-Widget) | 2 | |
| Fokusbereich / Tiefenschärfe | TEILWEISE | Numerische Anzeige der DOF-Parameter + Distanz-Marker; realer Look direkt via `OverrideDOF` sichtbar [VAN §3] | 3 | keine berechnete Schärfentiefe-Zone (heuristisches DOF) |
| Kamera-Frustum | TEILWEISE | In freier Ansicht: projizierte Eckstrahlen als Marker; 3D-Linien: **OFFEN** (O5) | 4 | Stretch |
| Sichtbare/verdeckte Objekte | TEILWEISE | `IsBoxVisible(mins, maxs, flags)` (Frustum+PVS) und `P2PVisibilityEx(from, to, flags)` (Sichtlinie) [VAN §5] | 4 | Pflicht-Einordnung: TEILWEISE |
| Lens Distortion Preview | TEILWEISE | **Nur** PPE-Näherung: `PPEGlow` `PARAM_LENSDISTORT` (+ `PARAM_LENSCENTERX/Y`) [VAN §3] | 4 | Wertebereich/Optik: **OFFEN** (O7); keine echten Profile |

---

## 10. Kontinuitäts- und Regieassistent (A1 §10)

**Machbarkeit: VOLL für Geometrie-Checks** (180°-Linie, 30°-Regel, Achsen/Screen-Direction), **TEILWEISE für Sichtbarkeits-Checks**; Headroom-/Anschnitt-Checks via Bone-Projektion sind möglich, aber Phase-4+-Stretch (Pflicht-Einordnung). Alle Checks warnen nur, sie verbieten nichts (A1-Vorgabe).

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| 180-Grad-Linie | VOLL | Handlungsachse zwischen zwei Motiven; Seitentest der Kameraposition (2D-Kreuzprodukt, `Side`-Port in `PCT_Math` [FRW §2]) | 4 | Warntext wie A1; Achse in Karten-Ansicht zeichnen |
| 30-Grad-Regel | VOLL | Winkeldifferenz der Kamera-Azimute zweier aufeinanderfolgender Shots relativ zum Motiv (`Math3D.AngleFromPosition` bzw. eigene atan2-Mathe [VAN §4]) | 4 | |
| Screen Direction / Achsen-Checks | VOLL | Bewegungs-/Blickvektor des Motivs → Vorzeichen der Screen-X-Komponente via `GetScreenPosRelative` über zwei Shots vergleichen [VAN §5] | 4 | reine Geometrie |
| Eyeline Match / Blickrichtung im Gegenschuss | TEILWEISE | Kopf-Bone-Orientierung (`GetBoneRotationWS`) als Blickvektor; Plausibilitätsheuristik über Shot-Paare [VAN §8] | 4 | Heuristik, keine echte Blickanalyse |
| Kamera kollidiert mit Wand/Requisite / Kamera außerhalb des Raums | TEILWEISE | `IsBoxCollidingGeometry(center, orient, edge, ...)` an Kameraposition und Pfad-Samples [VAN §5] | 4 | nur Warnung (§6) |
| Mindestfokusdistanz unterschritten | VOLL | Vergleich Fokusdistanz vs. Lens-Preset-Nahgrenze (Datenregel, §5) | 4 | |
| Mikrofon/Lichtstativ im Bild | TEILWEISE | `IsBoxVisible` auf Bounding-Box der Technik-Props [VAN §5] | 4 | Prop-Maße: siehe O2 |
| Darsteller verlässt Frame / Headroom ändert sich / Anschnitt an Gelenken | TEILWEISE | Bone-Projektion (`GetBonePositionWS` → `GetScreenPosRelative`) gegen Frame-/Headroom-Grenzen und „verbotene Schnittzonen" (Knie, Knöchel) | 4+ | **Stretch** (Pflicht-Einordnung) |
| Horizont leicht schief | VOLL | Roll-Wert-Toleranz-Check | 4 | |
| Zwei Einstellungen zu ähnlich (Jump-Cut-Gefahr) | VOLL | Kombination aus 30°-Check + FOV-/Distanz-Differenz | 4 | |
| Requisitenposition differiert zwischen Shots | VOLL | Vergleich gespeicherter Prop-Transforms pro Shot (Datenmodell) | 4 | |
| Bewegung verlässt Fokusbereich | TEILWEISE | Distanz Motiv↔Kamera über Pfad-Samples vs. Fokusdistanz-Kanal | 4+ | Stretch |
| Kamerapfad für gewählten Dolly nicht möglich | NICHT | Kein physisches Dolly-Modell | — | Ersatz: Kollisions-/Steigungswarnung am Pfad (Stretch) |

---

## 11. Shot- und Sequenzsystem (A1 §11)

**Machbarkeit: VOLL** (Pflicht-Einordnung). Das vollständige Datenmodell (Hierarchie Projekt→Sequenz→Szene→Setup→Shot→Take) steht in Plan_B3_Datenmodell_Persistenz.md; Vorbild für die Keyframe-Form ist Dabs `EditorCameraTrackData` (Time/Position/Orientation), erweitert um FOV/Roll/Fokus [FRW §8].

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Hierarchie Projekt/Sequenz/Szene/Setup/Shot/Take | VOLL | Datenklassen in `3_Game/Data`, Persistenz via `JsonFileLoader<T>` nach `$profile:PsyernsCinematicTool\Shots\` bzw. `Sequences\` [FRW §3] | 2–3 | jede Datei mit `schemaVersion` (int) |
| Shot-Felder (alle A1-Attribute) | VOLL | Wirksame Felder (Sensor, Brennweite, Blende, Fokus, Position, Bewegung) mappen auf Kamera-Parameter; Rest (Tonanforderung, Priorität, Drehzeit, Regiehinweis, Status) als Metadaten | 2 | ISO/Shutter/WB nur Metadaten + EV-Näherung (§5) |
| Thumbnail pro Shot | TEILWEISE | `MakeScreenshot(name)` → DDS unter `$profile:` [VAN §11] | 2 | In-Game-Anzeige der DDS: **OFFEN — ZU VERIFIZIEREN** (O8) |
| Storyboard-Referenz | TEILWEISE | Screenshot-Serie + Shot-Metadaten (Export §14) | 5 | |
| Coverage-Assistent (Dialog-Abdeckung) | VOLL | Regelbasierter Generator: erzeugt aus einem Motiv-Paar die Shot-Datensätze (Master, Two Shot, OTS A/B, CU A/B, Reaction, Insert, Cutaway, Safety) als Preset-Instanzen der §3-Logik | 4 | Pflicht-Einordnung: regelbasiert, keine KI |
| Take-Planung / Status-Workflow | VOLL | Statusfeld + Listen-UI (Plan_B5) | 5 | |

---

## 12. Benutzeroberfläche (A1 §12)

**Machbarkeit: ERSATZ** (Pflicht-Einordnung). Kein Multi-Viewport, kein Splitscreen — die Engine hat **eine** aktive Kamera (`Camera.GetCurrentCamera`, `SetActive`) [VAN §1]. Ersatz: Bedienmodi + schneller Kamera-Switch (A/B-Vergleich **sequentiell**, harter Cut oder `Camera.InterpolateTo`). UI-Stack ist der COT-Stack: `PCT_CinematicForm : JMFormBase`, `UIActionManager`-Widget-Fabrik, eigene Layouts wo nötig [COT §2; FRW §5]. Details in Plan_B5.

**Architektur-Entscheidung:** COT-UI-Stack statt Dabs/Expansion-MVC. Begründung: keine zusätzliche Abhängigkeit, identisches Look-and-Feel zum COT-Menü, Formular-Lifecycle (Show/Hide/Permissions) geschenkt; verworfene Alternative Dabs `ScriptView`/`ObservableCollection` — sauberer für Listen, aber erzwingt eine Dabs-Dependency, die die Projekt-Konvention ausschließt.

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Fenster/Formulare, Seitenleisten, Inspector-Register | ERSATZ | `JMRenderableModuleBase.Show()` + `CF_Window` + `JMFormBase`-Panels; Register als SelectBox/Tabs (COT-Muster `JMCameraForm`) [COT §1, §2] | 0–2 | Layout `PsyernsCinematicTool/GUI/layouts/pct_cinematic_form.layout` (Pfad folgt der Paketstruktur, COT-Muster `JM/COT/GUI/layouts/camera_form.layout` [COT §2]) |
| Bibliothek (Presets, Kameras, Objektive, Lichter, Setups) | VOLL | Scroller-Listen aus JSON-Preset-Ordnern (`UIActionManager`-Rows) | 2 | |
| 3D-Viewport frei / aktive Kamera | VOLL | Freecam = freie Ansicht; „aktive Kamera" = dieselbe Kamera mit angewendetem Shot | 1 | ein Viewport, Modus-Umschaltung |
| Vierfachansicht / Splitscreen / A-B parallel | NICHT | Engine-Limit: eine aktive Kamera [VAN §1] | — | Ersatz: sequentieller A/B-Switch (Hotkey), Cut oder `InterpolateTo` |
| Grundriss von oben | ERSATZ | COT-Map-Modul-Anleihe (`COTMap`): Kartenansicht mit Kamera-/Darsteller-/Licht-Markern [COT §6] | 4 | „Plan View"-Modus |
| Timeline unten (Spuren: Kamera, Fokus, Darsteller, Brennweite; Shot Cards; Playback) | TEILWEISE | Eigene Widget-Leiste (Slider/Buttons) über `PCT_SequencePlayer`-API; Spuren vereinfacht als Kanalliste | 3 | vollwertige Spur-UI ist Phase-5-Polish; Plan_B5 |
| Bedienmodi (Build/Blocking/Camera/Lighting/Director/Sequence/Plan/Presentation) | ERSATZ | Modi als Form-Zustände: Build→Props(§7), Blocking→Akteure(§7), Camera(§3–6), Lighting(§8), Director→Shot-Bewertung(§11), Sequence(§6/§11), Plan→Karte, Presentation→alle Overlays/UI aus | 2–5 | Presentation-Modus = HUD-Hide + Overlays aus |

---

## 13. Preset-System (A1 §13)

**Machbarkeit: VOLL** (Pflicht-Einordnung). JSON-Presets via `JsonFileLoader<T>` nach `$profile:PsyernsCinematicTool\Presets\` [FRW §3]; Datenklassen-Member **ohne** Präfix (JSON-Schlüssel-Treue, Projekt-Konvention), jede Datei mit `schemaVersion`.

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Preset-Arten (ShotSize, Angle, Lens, Body, Movement, Focus, Lighting, CompleteShot, Coverage, Genre, User) | VOLL | Eine Preset-Datenklasse pro Art in `3_Game/Data`; Ablage in Unterordnern von `Presets\` | 2 | Lighting-Presets erst Phase 4 |
| A1-Beispiel-JSON (`cu_emotional_85_eye`) | VOLL | Struktur wird nahezu 1:1 übernommen (framing/camera/movement/composition/tags) + `schemaVersion` | 2 | Referenzschema in Plan_B3 |
| Preset-Anwendung (Motiv messen → Position rechnen → Fokus setzen → ausrichten) | VOLL | Pipeline aus §3/§4/§5: Bone-Maße → Distanzformel → `SetPosition`/`LookAt`/`SetFOV`/`SetFocus` | 2 | |
| Kollisionsprüfung + Alternativvorschlag | TEILWEISE | `IsBoxCollidingGeometry` an Zielposition; bei Treffer: Azimut-Varianten testen und beste vorschlagen [VAN §5] | 4 | Vorschlag = einfache Azimut-Rotation, kein Solver |

---

## 14. Exportfunktionen (A1 §14)

**Machbarkeit: TEILWEISE** (Pflicht-Einordnung). JSON/CSV-Shotlisten nach `$profile:PsyernsCinematicTool\Exports\`; Screenshots via `MakeScreenshot` (**DDS** — externe Konvertierung nötig [VAN §11]). NICHT: PDF, EDL/OTIO, GLB, MP4 — Ersatz ist In-Game-Animatic-Playback + externes Capture (OBS o. ä.).

| Feature | Machbarkeit | Umsetzung in DayZ | Phase | Anmerkung |
|---|---|---|---|---|
| Projekt-JSON | VOLL | `JsonFileLoader<T>.JsonSaveFile` (komplettes Projekt inkl. Sequenzen) [FRW §3] | 2–3 | zugleich das Austauschformat für externe Konverter |
| Shot List JSON | VOLL | Export-Serialisierung des Shot-Datenmodells nach `Exports\` | 3 | Permission `"CinematicTool.Export"` |
| Shot List CSV/Excel | TEILWEISE | Eigener CSV-Writer über Datei-API (`OpenFile`/`FPrintln`/`CloseFile` — inzwischen direkt am 1.29-Quellcode verifiziert, siehe Plan_B3_Datenmodell_Persistenz.md §8) | 5 | Excel = CSV-Import |
| Screenshots mit technischen Daten | TEILWEISE | `MakeScreenshot("$profile:PsyernsCinematicTool\\Exports\\<shot>")` → DDS; Metadaten als Sidecar-JSON [VAN §11] | 2 | DDS-Konvertierung extern (Batch-Hinweis in Doku) |
| Storyboard-PDF / Shot-List-PDF / Projektbericht / QR-Codes | NICHT | Kein PDF-/QR-Rendering in Enfusion | — | Ersatz: JSON+Screenshots → externes Skript (Nicht-Ziel im Mod) |
| Floor Plan / Camera Plot / Lighting Plot | ERSATZ | Karten-Ansicht (§12) als Screenshot + Positions-JSON (Kameras/Lichter/Marken) | 4–5 | |
| Equipment-/Objektivliste, Tagesdispo-Grundlage | VOLL | Aggregation aus Shot-Metadaten → JSON/CSV | 5 | |
| MP4-Animatic / Bildsequenz | ERSATZ | In-Game-Animatic-Playback (`PCT_SequencePlayer`, Presentation-Modus) + externes Capture | 3 | Pflicht-Einordnung |
| EDL / OpenTimelineIO / GLB / Blender-/Unreal-Kameraexport | NICHT | Keine Formatschreiber im Mod | — | Projekt-JSON ist der dokumentierte Konverter-Einstiegspunkt |

---

## 15. Technische Umsetzung (A1 §15)

**Machbarkeit: ENTFÄLLT** (Pflicht-Einordnung). Der gesamte in A1 §15 empfohlene Web-Stack (React/TypeScript/Three.js, Node.js, PostgreSQL, Objektspeicher, Benutzerkonten, Teamfreigaben) entfällt ersatzlos, weil PCT als DayZ-Mod in der Enfusion-Engine läuft: Rendering, Szene, Kamera und Persistenz kommen aus Engine + CF/COT, gespeichert wird lokal als JSON unter `$profile:` — es gibt kein Backend, keine Konten und keine Web-Anteile. Die A1-Architekturliste (Scene Graph, Preset Engine, Shot Manager, ...) lebt konzeptionell in den PCT-Klassen weiter (`PCT_CinematicModule`, `PCT_CinematicCamera`, `PCT_SequencePlayer`, `PCT_Math`, Datenmodell-Klassen; siehe Plan_B1).

---

## 16. Entwicklungsstufen (A1 §16) — Übersetzung auf PCT-Phasen 0–5

A1 kennt vier produktorientierte Phasen; PCT nutzt die verbindliche Nomenklatur **Phase 0 Skeleton · 1 Kamera-Kern · 2 Shots & Presets · 3 Sequencer · 4 Welt/Akteure/Licht & Assistenten · 5 Polish & Release**.

| A1-Phase | A1-Inhalt (Auszug) | PCT-Phase(n) | Anmerkung |
|---|---|---|---|
| — (implizit) | Mod-Gerüst, Modul, Permissions, Inputs | **0 Skeleton** | config.cpp (`PCT_Scripts`/`PCT_GUI`), `modded class JMModuleConstructor`, `EPCT_RPC` (Basis 10500), Permission-Baum, `Inputs.xml` (`UAPCT_*`) |
| Phase 1 MVP: Kamera, freie Positionierung, Brennweitenwahl | Kamera + Sensor/Brennweite | **1 Kamera-Kern** | Freecam-Anbindung, FOV-/DOF-Steuerung, Lens-Mathe |
| Phase 1 MVP: Framing-/Winkel-Presets, Shot speichern, Screenshot, Shot List | Presets + Persistenz | **2 Shots & Presets** | Auto-Framing, Winkel-Presets, JSON-Speicherung, Screenshot, Overlays-Basis |
| Phase 2: Bewegungswege, Timeline, Fokusverlauf | Timeline/Keyframes | **3 Sequencer** | Keyframes, Catmull-Rom, Movement-Presets, Animatic-Playback |
| Phase 2: mehrere Figuren, Blocking, OTS/POV, 180-Grad-Linie | Akteure + Checks | **4 Welt/Akteure/Licht & Assistenten** | OTS/POV bereits in Phase 2 (bone-basiert); Dummies + 180° in Phase 4 |
| Phase 2: Raumeditor, PDF-Storyboard | — | **Nicht-Ziel / Ersatz** | DayZ-Welt statt Raumeditor; JSON+Screenshots statt PDF |
| Phase 3: Lichtplanung, Figuranimation, Continuity, Coverage, Animatic | Welt & Assistenten | **4** (+ Playback aus **3**) | Multi-Camera nur sequentiell (A/B-Switch) |
| Phase 3: Teamkommentare, Versionen/Freigaben | — | **Nicht-Ziel** | kein Backend (§15) |
| Phase 4: Kameramodell-/Objektivdatenbanken | Bibliotheken | **5 Polish & Release** | als JSON-Preset-Bibliotheken |
| Phase 4: Focus Breathing, LiDAR, Sonnenstandsberechnung, Unreal-Export, AR, Tablet, KI-Vorschläge | — | **Nicht-Ziel** | siehe Schlussliste |

---

## 17. Kompakte Projektbeschreibung (A1 §17) — DayZ-Fassung

PCT ist ein In-Engine-Previsualisierungswerkzeug für DayZ: Auf Basis der COT-Kamera (`JMCinematicCamera`) plant der Nutzer in der realen Spielwelt einzelne Kameraeinstellungen und ganze Sequenzen. Das Kamerasystem rechnet mit realen Sensorgrößen und Brennweiten (eigene FOV-Mathematik), filmische Presets positionieren die Kamera automatisch relativ zum Motiv, Einstellungsgröße/Winkel/Objektiv/Bewegung/Komposition bleiben getrennt editierbar. Kameras werden auf einer Keyframe-Timeline animiert (Catmull-Rom, Orbit, Follow, Handheld, Dolly-Zoom, Rack Focus); ein Kontinuitäts-Assistent prüft 180°-Linie, 30°-Regel, Screen Direction und Kollisionen. Shots werden mit Screenshot und technischen Daten als JSON gespeichert; Shotlisten und ein In-Game-Animatic ersetzen die Datei-Exporte der Web-Vision. Der A1-Kern („Wo steht die Kamera, welches Objektiv, funktioniert der Anschluss?") bleibt damit vollständig erhalten — nur der Drehort ist jetzt immer eine DayZ-Map.

---

## Bewusste Nicht-Ziele

1. **Multi-Viewport / Vierfachansicht / paralleler Splitscreen** — Engine-Limit: eine aktive Kamera.
2. **Raum-Editor und Modell-Import** (glTF/GLB, 3D-Scan, Photogrammetrie, LiDAR, Grundriss-Import, GPS-Daten).
3. **Physikalisch korrekte Belichtung**: ISO, Shutter/Shutter Angle, Weißabgleich, ND-Filter (nur EV-/PPE-Näherung).
4. **Anamorph-Faktor, Lens Shift, echte Verzeichnungs-/Bokeh-Profile, Focus Breathing.**
5. **IES-Profile, Area-/Softbox-/Tube-/LED-Lichter, Reflektoren, Diffusion, Gobos/Flags.**
6. **Per-Bone-Posing von Darstellern** (Bones sind aus Script nur lesbar).
7. **Datei-Exporte PDF, EDL, OpenTimelineIO, GLB, MP4, QR-Codes.**
8. **Web-Backend**: Benutzerkonten, Teamfreigaben, Kommentare, Projektversionierung, Cloud-Speicher.
9. **Automatisches Collision Avoidance / Pfad-Umplanung** (in frühen Phasen; nur Warnungen).
10. **KI-gestützte Shot-Vorschläge, AR-Ansicht, Tablet-Modus, Shot-Abgleich mit realem Kamerabild.**
11. **Änderung von Renderauflösung/Bildrate/Seitenverhältnis des Backbuffers.**
12. **Physisches Dolly-/Rig-Modell** (Machbarkeitsprüfung realer Dolly-Fahrten).

## Ersatzstrategien

| Entfallenes Original | Ersatz in PCT |
|---|---|
| Raum-Editor / Location-Import | DayZ-Welt als Drehort + Location-Bookmarks (Teleport) |
| Requisiten-/Möbel-Bibliothek (glTF) | COT Object Spawner + ESP-Manipulation vorhandener DayZ-Assets |
| Posierbare Charaktere | `PlayerBase`-Dummies mit Emote-/Action-Commands und Bewegungs-Override |
| Multi-Viewport / A-B-Vergleich parallel | Bedienmodi + sequentieller A/B-Kamera-Switch (Hotkey, Cut/Interpolation) |
| Grundriss-/Plan-Ansicht | Karten-Ansicht nach COT-Map-Muster mit Kamera-/Darsteller-/Licht-Markern |
| Sonnen-/Lichtstudio (IES, Softbox, Area) | Licht-Presets aus Point/Spot-Kombinationen + Datum/Uhrzeit/Wetter-Presets |
| Seitenverhältnis-Rendering | Formatmasken/Letterbox als Widget-Panels |
| ISO/Shutter/WB | Ein „Exposure"-Regler (`SetEVUser`) + PPE-Farb-Näherung |
| Lens-Distortion-Profile | PPE-Näherung (`PPEGlow` `PARAM_LENSDISTORT`) |
| MP4-Animatic / EDL / OTIO | In-Game-Animatic-Playback + externes Capture; Projekt-JSON als Konverter-Schnittstelle |
| PDF-Shotlist/Storyboard | JSON/CSV + Screenshot-Serie (externe Weiterverarbeitung) |
| Web-Backend/Kollaboration | Lokale JSON-Dateien unter `$profile:PsyernsCinematicTool\` (Weitergabe per Datei) |

## OFFEN — ZU VERIFIZIEREN (konsolidiert)

| Nr. | Punkt | Konkreter Testschritt |
|---|---|---|
| O1 | Clamping von `Camera.SetFOV` außerhalb 0.75–1.30 rad | `staticcamera` spawnen, `SetFOV(0.2)` und `SetFOV(2.0)` setzen, Screenshots vergleichen (COT-Slider legt 0.001–4.0 nahe) |
| O2 | Motivmaße für Nicht-Player-Objekte (Bounding-Box-API, z. B. `ClippingInfo`/`GetCollisionBox`) | Signaturen in `scripts - 1.29\3_Game\...\Object.c` prüfen; Diag-Ausgabe der Box eines Fahrzeugs/Items |
| O3 | `EntityLightSource`-Setter-Surface (Farbe, Schattenwurf, Spot-Winkel) | Proto in `1_Core` prüfen; `PointLightBase`/`SpotLightBase` spawnen, Setter aufrufen, visuell prüfen |
| O4 | `PPEColorGrading`-Parameter für Weißabgleich-/Tint-Näherung | `PPEManager\Materials\MatClasses\PPEColorGrading.c` PARAM_*-Liste auslesen; Test-Requester bauen |
| O5 | 3D-Linien-/Debug-Shape-API für Pfad-/Frustum-Visualisierung | `1_Core`-Protos (Debug/Shape) sichten; falls vorhanden Testlinie zeichnen, sonst Widget-Marker-Fallback festschreiben |
| O6 | `PlayerBase`-Dummy auf Dedicated Server: Spawn + `StartCommand_Action` via RPC (CTActor ist ein lokales Debug-Tool-Muster) | Serverseitig `SurvivorM_*` spawnen, Emote-Command per PCT-RPC auslösen, Client-Sync und Verhalten beobachten |
| O7 | Wertebereich/Optik von `PPEGlow` `PARAM_LENSDISTORT` | Parameter über Requester in Stufen setzen, Bildwirkung dokumentieren |
| O8 | In-Game-Anzeige von DDS-Screenshots (Thumbnail im Widget) | ImageWidget-Lade-API in `EnWidgets.c` prüfen; DDS aus `$profile:` in ein ImageWidget laden |
| O9 | Far-Plane-Steuerung: wirkt `World.SetCameraFarPlane(slot, ...)` auf die aktive Script-Kamera? | In aktiver Freecam Far Plane über Slot-API ändern, Sichtweite prüfen |
| O10 | Text-/CSV-Datei-Schreib-API (`OpenFile`/`FPrintln`-Signaturen) | **erledigt** — direkt am 1.29-Quellcode verifiziert (`EnSystem.c:417/481/443`, siehe Plan_B3_Datenmodell_Persistenz.md §8); Rest-Testschritt nur noch Encoding (T-EXP-2, Plan_B3 §8.1) |

---

## Querverweise

- `Plan_A1.md` — Stoff- und Anforderungssammlung (Quelle dieses Mappings)
- `Plan_B1_Architektur.md` — Sollarchitektur, Mod-Struktur, config.cpp, Modul-Registrierung (Phase 0)
- `Plan_B3_Datenmodell_Persistenz.md` — Shot-/Sequenz-/Preset-Datenmodell, JSON-Schema, `schemaVersion`
- `Plan_B4_Kamera_Engine.md` — FOV-/Distanz-/Dolly-Zoom-Herleitungen, Blende→DOF-Mapping, `PCT_Math`
- `Plan_B5_UI_UX.md` — Formulare, Overlays, Timeline, Bedienmodi, Keybinds
- `Plan_B6_Roadmap_Risiken.md` — Phasenplan 0–5, Meilensteine, Verifikation der OFFEN-Punkte
- `Docs/Research/Research_COT_Infrastructure.md` — COT-Modul-/RPC-/Permission-/Kamera-Fakten
- `Docs/Research/Research_Vanilla_APIs.md` — Vanilla-1.29-APIs (Camera, PPE, Splines, Weather, Lights, Screenshots)
- `Docs/Research/Research_Framework_Patterns.md` — Framework-Patterns + Abhängigkeits-Entscheidung (nur CF+COT)
