# UI/UX-Konzept: COT-Form, Overlays, Timeline, Keybinds

Stand: 2026-07-21 · Status: Entwurf v1 · Projekt: PsyernsCinematicTool (PCT) · Autor: Psyern

**Abstract:**
Dieses Dokument definiert die komplette Benutzeroberfläche des Cinematic-Tools: das COT-integrierte Hauptfenster `PCT_CinematicForm` mit sechs Tabs, das In-Shot-HUD, die Kompositions-Overlays (Raster, Masken, Safe Areas), die Sequencer-Liste, den Shot-Browser sowie alle `UAPCT_*`-Keybinds inklusive Kollisionsprüfung gegen COT-Defaults.
Die UI-Strategie ist bewusst COT-nativ (`JMRenderableModuleBase` + `JMFormBase` + `UIActionManager`) ohne Dabs-/Expansion-MVC-Abhängigkeit.
Alle genannten Widget-Factory-Methoden und Engine-APIs sind gegen die Research-Dokumente bzw. den COT-Quellcode verifiziert; Unbelegtes ist als "OFFEN — ZU VERIFIZIEREN" markiert.
Layout-Dateien, GUI-`config.cpp`-Einbindung und Lokalisierungsansatz (#STR_PCT_*) werden phasenweise (Phase 0–5) geplant.

---

## 1. UI-Strategie-Entscheidung

**Entscheidung:** PCT baut seine gesamte UI auf dem COT-nativen Stack:

- `PCT_CinematicModule : JMRenderableModuleBase` (5_Mission) — Modul mit Sidebar-Button, Layout-Root, Toggle-Input, Permission-Gate.
- `PCT_CinematicForm : JMFormBase` (5_Mission) — Fensterinhalt, manuell verdrahtete Widgets.
- **`UIActionManager`-Widget-Factory** (verifiziert in `JM\COT\Scripts\5_Mission\CommunityOnlineTools\gui\Actions\UIActionManager.c`): `CreateButton`, `CreateButtonToggle`, `CreateNavButton`, `CreateEditableText`, `CreateEditableTextPreview`, `CreateEditableRichText`, `CreateDropdownBox`, `CreateEditableVector`, `CreateCheckbox`, `CreateText`, `CreateSelectionBox`, `CreateSlider`, `CreateScroller`, `CreateImage`, `CreateImageButton`, `CreateGridSpacer`, `CreatePanel`.

**Begründung (je 1–3 Sätze):**

1. **Nahtlose COT-Integration:** `JMRenderableModuleBase` liefert Sidebar-Button (`InitButton`), Fenster-Erzeugung aus `GetLayoutRoot()`, Toggle-Keybind-Autobinding und — entscheidend — permissiongesteuertes Ein-/Ausblenden: `HasAccess()` prüft `"CinematicTool.View"`, `OnClientPermissionsUpdated()` schließt das Fenster automatisch bei Rechteverlust (Research_COT_Infrastructure §2, JMRenderableModuleBase.c:251). Das müsste jede Fremd-UI-Lösung von Hand nachbauen.
2. **Keine zusätzlichen Abhängigkeiten:** Dabs-MVC (`ScriptView`/`ViewController`/`ObservableCollection`) und Expansion-MVC sind architektonisch eleganter (Data-Binding statt Hand-Wiring), würden aber Dabs bzw. Expansion in `requiredAddons` erzwingen — laut Projektentscheidung verboten (Research_Framework_Patterns, Kopfnotiz). **Verworfene Alternative:** Dabs-`ScriptView` mit `Binding_Name`-Autobinding; verworfen wegen Abhängigkeit, nicht wegen Qualität.
3. **Bewährte Komplexitätsreferenz:** COT baut mit demselben Stack Formulare vergleichbarer Größe (`JMESPForm`, `JMWeatherForm`, `JMCameraForm`) — der Stack ist für ein Editor-Tool nachweislich tragfähig.

**Akzeptierte Konsequenz:** Manuelles Event-Wiring. Jeder Slider/Button bekommt einen `OnChange_*`/`OnClick_*`-Handler nach dem Muster von `JMCameraForm` (`OnChange_*`-Handler, Zeilen 293–381; `OnInit`/`OnHide`-Persistenz). Kein Data-Binding, dafür null Framework-Risiko. Wo die Factory nicht reicht (Overlays, HUD, Zeilen-Templates), werden eigene `.layout`-Dateien per `g_Game.GetWorkspace().CreateWidgets(...)` geladen (Vanilla-Muster `CameraToolsMenu.c:141`, Research_Vanilla_APIs §10).

Sidebar-Eintrag: `GetTitle()` → `"#STR_PCT_MODULE_NAME"`, `GetIconName()` → eigenes Icon (`GUI/textures/modules/`, analog COT `Camera.paa`), `GetInputToggle()` → `"UAPCT_ToggleMenu"`.

---

## 2. Informationsarchitektur: `PCT_CinematicForm`

Hauptform nach dem Aufbau von `JMCameraForm` (Research_COT_Infrastructure §1): oben eine `CreateSelectionBox` als Tab-Umschalter, darunter je Tab ein Panel (`CreateGridSpacer`-Spalten in einem `CreateScroller`). Tabs:

**`{"Camera & Lens", "Shots", "Sequencer", "World", "Assist", "Settings"}`**

Die Tabellen reduzieren Plan_A1 §12 auf das in DayZ Machbare (kein 3D-Viewport-Editor, kein Vierfach-Split — es gibt genau eine aktive Engine-Kamera).

### 2.1 Tab "Camera & Lens"

| Control | UIActionManager-Typ | Gebundener Wert | Wertebereich | Phase |
|---|---|---|---|---|
| Kamera Enter/Leave | CreateButtonToggle | `EPCT_RPC.Enter`/`Leave` — PCT-eigener Enter-Pfad; bei aktiver COT-Freecam Handoff nach Koexistenz-Regel K2, kein Enter bei aktiver COT-Kamera (K1); siehe Plan_B1_Architektur.md §5.2 | — | 1 |
| Sensor-Preset | CreateSelectionBox | `m_SensorIndex` → `sensorHeightMm` | Full Frame / Super 35 / APS-C / MFT / 1" / Custom | 1 |
| Brennweite (mm) | CreateSlider | `m_FocalLengthMm` → `PCT_Math.FocalLengthToFov()` → `Camera.SetFOV` (rad!) | 8–400 mm | 1 |
| Brennweiten-Preset | CreateSelectionBox | `m_FocalLengthMm` | 14/18/21/24/28/35/50/85/135/200 mm | 2 |
| FOV direkt (rad) | CreateSlider | `m_TargetFOV` | 0.05–2.5 (COT erlaubt 0.001–4; Clamp-Verhalten von `SetFOV` oberhalb User-Limits: **OFFEN — ZU VERIFIZIEREN**, Test s. u.) | 1 |
| Fokusdistanz (m) | CreateSlider | `m_FocusDistance` → `Camera.SetFocus` + `PPEffects.OverrideDOF` | 0–1000 m | 1 |
| DOF aktiv | CreateCheckbox | `m_DofEnabled` | an/aus | 1 |
| Focal Length (DOF) | CreateSlider | `m_FocalLength` | 0–1000 (COT-Spiegel) | 1 |
| Focal Near (DOF) | CreateSlider | `m_FocalNear` | 0–1000 (COT-Spiegel) | 1 |
| Auto-Fokus (Raycast) | CreateCheckbox | `m_AutoFocus` (COT-Muster JMCameraModule.c:111–124) | an/aus | 2 |
| f-Stop-Äquivalent | CreateSelectionBox | `m_FStopIndex` → Blur-Mapping | f/1.4–f/22 (heuristisch, **OFFEN — ZU VERIFIZIEREN**) | 2 |
| Roll / Dutch (°) | CreateSlider | `m_Roll` → `SetYawPitchRoll` | −45…+45° | 1 |
| Dutch-Preset | CreateSelectionBox | `m_Roll` | 0 / 15 / 25 / 40° (Plan_A1 §4) | 2 |
| Exposure (EV) | CreateSlider | `m_Exposure` → `g_Game.SetEVUser` | −5…+5 (API-Range −50..50) | 2 |
| Vignette | CreateSlider | `m_Vignette` → `PPEffects.SetVignette` | 0–1 | 2 |
| Chrom. Aberration | CreateSlider | `m_ChromAbb` → `PPEffects.SetChromAbb` | 0–1 | 2 |
| Kamera-Speed | CreateSlider | `JMCameraBase.s_CurrentSpeed` | 0.001–10 (COT-Spiegel) | 1 |
| Kamera-Position | CreateEditableVector | `m_CameraPosition` (Set/Copy/Paste wie COT-Traveling-Panel) | Weltkoordinaten | 1 |
| Handheld aktiv | CreateCheckbox | `m_HandheldEnabled` | an/aus | 3 |
| Handheld-Amplitude | CreateSlider | `m_HandheldAmp` | 0–1 | 3 |
| Handheld-Frequenz | CreateSlider | `m_HandheldFreq` | 0.1–10 Hz | 3 |

Testschritte OFFEN-Punkte: (a) `SetFOV(2.0)` in-game aufrufen und sichtbaren Winkel gegen Berechnung prüfen; (b) f-Stop-Mapping: feste Testszene, Blur-Werte je f-Stop gegen Referenz-DOF-Verhalten tunen (reines Client-Tuning, keine API-Frage der Machbarkeit — `OverrideDOF` selbst ist verifiziert).

### 2.2 Tab "Shots"

| Control | UIActionManager-Typ | Gebundener Wert | Wertebereich | Phase |
|---|---|---|---|---|
| Shot-Name | CreateEditableText | `m_EditShotName` | Text | 2 |
| Framing-Preset | CreateSelectionBox | `m_FramingPresetIndex` | EWS/WS/FS/MS/MCU/CU/BCU/ECU (reduzierte Liste aus Plan_A1 §3) | 2 |
| Angle-Preset | CreateSelectionBox | `m_AnglePresetIndex` | Eye/High/Low/Overhead/Worm/Ground/Hip | 2 |
| Motiv wählen (Crosshair) | CreateButton | `m_TargetObject` (Raycast, COT-"Look at Object"-Muster) | — | 2 |
| Frame Target (Auto-Distanz) | CreateButton | berechnet Kameraposition aus FOV + Motivhöhe (Plan_A1 §5, `PCT_Math`) | — | 2 |
| Shot-Liste | CreateScroller + Zeilen (`pct_shot_row.layout`) | `m_Shots` (aus `Shots\*.json`) | — | 2 |
| Apply / Update / Delete / Duplicate / Export | CreateButton ×5 | selektierter Shot | Delete mit `CreateConfirmation_Two` (JMFormBase) | 2 |

### 2.3 Tab "Sequencer" (Phase 3)

| Control | UIActionManager-Typ | Gebundener Wert | Wertebereich | Phase |
|---|---|---|---|---|
| Keyframe-Liste | CreateScroller + Zeilen (`pct_keyframe_row.layout`) | `m_Sequence` (Keyframes) | — | 3 |
| Add Keyframe from Current Camera | CreateButton | hängt aktuellen Kamera-Zustand als Keyframe an | — | 3 |
| Segment-Zeit (s) | CreateEditableText (je Zeile) | `keyframe.time` | 0.1–600 s | 3 |
| Interpolation | CreateSelectionBox | `m_InterpMode` | Linear / CatmullRom / NaturalCubic / UniformCubic (`positionCurveType` via `Math3D.Curve`, Plan_B3_Datenmodell_Persistenz.md §2.4; Easing je Keyframe: Linear/Smooth/Smoother) | 3 |
| Play/Pause | CreateButton | `PCT_SequencePlayer` | — | 3 |
| Stop | CreateButton | `PCT_SequencePlayer` | — | 3 |
| Scrub | CreateSlider | `m_PlaybackNormTime` | 0–1 (normierte Sequenzzeit) | 3 |
| Loop | CreateCheckbox | `m_Loop` | an/aus | 3 |
| Sequenz-Name + Save/Load | CreateEditableText + CreateButton ×2 | `Sequences\*.json` | — | 3 |

### 2.4 Tab "World" (Phase 4; alle Wirk-Aktionen als Server-RPC, Permission `"CinematicTool.World"` bzw. `.Actors`/`.Lights`/`.Props`; siehe Plan_B1_Architektur.md §6–8)

| Control | UIActionManager-Typ | Gebundener Wert | Wertebereich | Phase |
|---|---|---|---|---|
| Datum/Uhrzeit (J/M/T/h/min) | CreateEditableText ×5 + Apply-Button | RPC → `World.SetDate` | gültige Datumswerte | 4 |
| Zeit einfrieren | CreateCheckbox | RPC → `World.SetTimeMultiplier(0)` | an/aus | 4 |
| Overcast / Fog / Rain / Snow | CreateSlider ×4 | RPC → `Weather`-Phänomene `Set(v, 0, langeDauer)` | 0–1 | 4 |
| Wind-Stärke | CreateSlider | RPC → `SetWindSpeed` | 0–max | 4 |
| Wetter einfrieren | CreateCheckbox | RPC → `SetWeatherUpdateFreeze(true)` | an/aus | 4 |
| Akteur spawnen | CreateButton (+ SelectionBox Typ) | RPC, `CinematicTool.Actors` (CTActor-Muster; nur client-lokal verifiziert — Server-Spawn: **OFFEN — ZU VERIFIZIEREN**, Test s. u.) | — | 4 |
| Akteur-Emote/Pose | CreateSelectionBox + Button | RPC → `StartCommand_Action` (Replikation bei serverseitigem Aufruf: **OFFEN — ZU VERIFIZIEREN**, Test s. u.) | Emote-Liste | 4 |
| Licht spawnen (Point/Spot) | CreateSelectionBox + Slider Brightness/Radius + Button | `CinematicTool.Lights`, `ScriptedLightBase.CreateLight` | Brightness/Radius je API | 4 |
| Prop spawnen (Classname) | CreateEditableText + Button | `CinematicTool.Props`, `CreateObjectEx` | — | 4 |

Testschritt OFFEN-Punkt Akteure: Research_Vanilla_APIs.md §8 verifiziert `StartCommand_Action` und das CTActor-Muster nur im **client-lokalen** CameraTools-Kontext (`CreateObject` local, `EmoteCB` auf dem eigenen Client); ob ein serverseitiger Aufruf auf einer server-gespawnten `PlayerBase` die Animation zu allen Clients repliziert, ist unbelegt. Test: dedizierter Testserver mit 2 Clients — Server spawnt `PlayerBase` via `CreateObject`, ruft `StartCommand_Action(emoteId, EmoteCB, DayZPlayerConstants.STANCEMASK_ALL)` auf; prüfen, ob beide Clients Spawn **und** Animation sehen. Fallback: Server broadcastet `ActorCommand` an alle Clients, jeder Client führt das Kommando lokal auf der replizierten Akteur-Entität aus (analog Licht-Modell, Plan_B1_Architektur.md §7).

### 2.5 Tab "Assist"

| Control | UIActionManager-Typ | Gebundener Wert | Wertebereich | Phase |
|---|---|---|---|---|
| Overlays anzeigen (Master) | CreateCheckbox | `m_OverlayVisible` | an/aus | 2 |
| Drittelraster / Goldener Schnitt / Fadenkreuz / Diagonalen / Symmetrieachse | CreateCheckbox ×5 | `m_OverlayFlags` (Toggle-Gruppe) | an/aus | 2 |
| Safe Areas (Action/Title) | CreateCheckbox ×2 | `m_SafeAction`, `m_SafeTitle` | 90 % / 80 % Rahmen | 2 |
| Seitenverhältnis-Maske | CreateSelectionBox | `m_AspectMaskIndex` | Aus / 2.39:1 / 1.85:1 / 16:9 / 4:3 / 9:16 / 1:1 | 2 |
| Masken-Deckkraft | CreateSlider | `m_MaskAlpha` → `SetAlpha` | 0–1 | 2 |
| Horizont-Level | CreateCheckbox | `m_OverlayHorizon` | an/aus | 2 |
| Augenlinien-Marker + Ziel wählen | CreateCheckbox + CreateButton | `m_EyelineTarget` | — | 4 |
| Coverage: Akteur A/B setzen | CreateButton ×2 | `m_CoverageActorA/B` | — | 4 |
| Coverage generieren | CreateButton | erzeugt Vorschlagsliste (§6) | — | 4 |
| Vorschlagsliste | CreateScroller + Zeilen (`pct_coverage_row.layout`) | `m_CoverageSuggestions` | — | 4 |

### 2.6 Tab "Settings"

| Control | UIActionManager-Typ | Gebundener Wert | Wertebereich | Phase |
|---|---|---|---|---|
| HUD automatisch bei Freecam | CreateCheckbox | `settings.hudAutoShow` | an/aus | 1 |
| Standard-Sensor | CreateSelectionBox | `settings.defaultSensor` | Sensor-Liste | 2 |
| Standard-Maske | CreateSelectionBox | `settings.defaultAspectMask` | Masken-Liste | 2 |
| Autosave Sequenzen | CreateCheckbox | `settings.autosaveSequences` | an/aus | 3 |
| Overlay-Farbe/Deckkraft-Defaults | CreateSlider | `settings.overlayAlpha` | 0–1 | 5 |

Persistenz aller Settings: `Settings.json` (`$profile:PsyernsCinematicTool\`, mit `schemaVersion`) via `JsonFileLoader<T>`; Schreiben in `OnHide()` (COT-Muster), Details siehe Plan_B3_Datenmodell_Persistenz.md. JSON-Datenklassen-Member ohne Präfix.

---

## 3. Kamera-HUD (In-Shot-Overlay)

Eigenes **Fullscreen-Layout ohne Fenster-Chrome**: `pct_hud.layout`, geladen per `g_Game.GetWorkspace().CreateWidgets("PsyernsCinematicTool/GUI/layouts/pct_hud.layout")` (Muster `CameraToolsMenu.c:141`). Nur `TextWidget`/`ImageWidget`/`PanelWidget` — keine interaktiven Widgets, damit kein Input-Fokus gestohlen wird (Fokus-/Input-Transparenz im Freecam trotzdem testen: **OFFEN — ZU VERIFIZIEREN**, Test: HUD an, WASD/Maus/Scroll im Freecam prüfen).

**Statuszeile** (unterer Bildrand, ein `GridSpacer` mit Textblöcken), pro Frame in `PCT_CinematicModule.OnUpdate()` via `SetText` aktualisiert:

| Feld | Quelle | Beispiel |
|---|---|---|
| Brennweite | `m_FocalLengthMm` | `85mm` |
| Sensor | `m_SensorIndex`-Kürzel | `FF` / `S35` |
| f-Stop-Äquivalent | `m_FStopIndex` (Phase 2, sonst Blur-Wert) | `f/2.0` |
| Fokusdistanz | `m_FocusDistance` | `3.4m` |
| FOV | `m_TargetFOV * Math.RAD2DEG` (vertikal) | `27°` |
| Roll | `GetOrientation()`-Roll | `15°` |
| Shot-Name | aktiver/letzter Shot | `SC01_SH040_CU` |
| REC-Indikator | `PCT_SequencePlayer.IsPlaying()` | rot blinkend (Alpha-Toggle im `OnUpdate`, akkumulierte Zeit — kein `CallLater` nötig) |

Sichtbarkeit: Toggle per `UAPCT_ToggleHUD` (§7); optional Auto-Einblenden beim Freecam-Enter (`settings.hudAutoShow`). HUD ist rein clientseitig, Phase 1.

---

## 4. Kompositions-Overlays

Eigenes Fullscreen-Layout `pct_overlay.layout` (Name gemäß Paketstruktur Plan_B1_Architektur.md §3; getrennt vom HUD, damit HUD und Overlays unabhängig toggeln). Rein clientseitige Widgets, keine RPCs, keine Permissions über `CinematicTool.View` hinaus. Phase 2.

| Overlay | Umsetzung | Technik |
|---|---|---|
| Drittelraster | `ImageWidget`, Fullscreen | .edds-Textur mit Alpha |
| Goldener Schnitt | `ImageWidget`, Fullscreen | eigene .edds (Phi-Linien) |
| Fadenkreuz/Zentrum | `ImageWidget`, zentriert | kleine .edds |
| Diagonalen | `ImageWidget` (Linien-Textur) + `SetRotation` (EnWidgets, verifiziert §5/§10) | 2 rotierte Linien-Widgets |
| Symmetrieachse | schmaler `PanelWidget` (1–2 px), vertikal zentriert | texturlos, `CreatePanel`-artig im Layout |
| Safe Areas (Action 90 % / Title 80 %) | 4 Rahmen-`PanelWidget`s je Area, per `SetScreenPos/SetScreenSize` aus `GetScreenSize` berechnet | texturlos |
| Seitenverhältnis-Masken 2.39:1 / 1.85:1 / 16:9 / 4:3 / 9:16 / 1:1 | 4 schwarze `PanelWidget`s (oben/unten/links/rechts) — Letterbox oder Pillarbox | Code unten |
| Horizont-Level | mittige Linien-`ImageWidget`, per `SetRotation` mit negativem Kamera-Roll gedreht + Grad-Text | Roll aus `GetOrientation()` |
| Augenlinien-Marker | `ImageWidget`-Marker auf projizierter Kopfposition des Ziel-Akteurs | `GetBonePositionWS(GetBoneIndexByName("Head"))` → `g_Game.GetScreenPosRelative(worldPos)` (beide verifiziert, Research_Vanilla_APIs §5/§8); Verhalten bei Punkt **hinter** der Kamera: **OFFEN — ZU VERIFIZIEREN** (Test: Marker-Ziel hinter Kamera bringen, `.z` und x/y loggen; Marker bei `.z <= 0` bzw. Ausreißer-Koordinaten verstecken) |

**Masken-Berechnung** (Letterbox/Pillarbox; `GetScreenSize` verifiziert EnWidgets.c:154, `SetScreenPos/SetScreenSize` 142–143):

```c
void UpdateAspectMask(float targetAspect)
{
	float screenW;
	float screenH;
	m_OverlayRoot.GetScreenSize(screenW, screenH);
	float screenAspect = screenW / screenH;

	if (targetAspect >= screenAspect)
	{
		float contentH = screenW / targetAspect;
		float barH = (screenH - contentH) * 0.5;
		m_MaskTop.SetScreenSize(screenW, barH);
		m_MaskTop.SetScreenPos(0, 0);
		m_MaskBottom.SetScreenSize(screenW, barH);
		float bottomY = screenH - barH;
		m_MaskBottom.SetScreenPos(0, bottomY);
		m_MaskLeft.Show(false);
		m_MaskRight.Show(false);
		m_MaskTop.Show(true);
		m_MaskBottom.Show(true);
	}
	else
	{
		float contentW = screenH * targetAspect;
		float barW = (screenW - contentW) * 0.5;
		m_MaskLeft.SetScreenSize(barW, screenH);
		m_MaskLeft.SetScreenPos(0, 0);
		m_MaskRight.SetScreenSize(barW, screenH);
		float rightX = screenW - barW;
		m_MaskRight.SetScreenPos(rightX, 0);
		m_MaskTop.Show(false);
		m_MaskBottom.Show(false);
		m_MaskLeft.Show(true);
		m_MaskRight.Show(true);
	}
}
```

**Technik-Hinweise:**

- **POT-Texturen sind Pflicht:** alle Overlay-Texturen als `.edds` in Zweierpotenz-Größen (512/1024/2048) unter `GUI/textures/`; Nicht-POT-Texturen verursachen Fehler/Artefakte.
- Raster-Overlays werden für das *native* Bildformat gezeichnet; bei aktiver Seitenverhältnis-Maske beziehen sich Drittel/Golden Ratio auf den maskierten Bildbereich → Raster-Widgets werden auf die Content-Fläche der Maske skaliert (gleiche Rechnung wie oben).
- **Toggle-Gruppen:** ein Bitfeld `m_OverlayFlags` steuert alle Einzel-Overlays; `UAPCT_ToggleOverlay` schaltet den Master-Root (`Show`), die Checkboxen im Assist-Tab die Einzel-Widgets. `UAPCT_CycleAspectMask` rotiert `m_AspectMaskIndex`.
- Alternative kreisförmige Vignette: nativer NDC-Mask `g_Game.AddPPMask(...)` / `ResetPPMask()` (verifiziert §10) — kein Widget, Phase 5 optional.

---

## 5. Sequencer-UI (Phase 3)

**Keyframe-Liste statt Canvas-Timeline.** Aufbau:

- `CreateScroller` im Sequencer-Tab; pro Keyframe eine Zeile aus `pct_keyframe_row.layout` (per `CreateWidgets(path, parentWidget)` instanziert) mit:
  - **Index + Zeit** (`EditableText`, Sekunden des Segments),
  - **Kanalwerte-Kurztext** (`TextWidget`): `pos`, `FOV°`, `roll°`, `focus m`,
  - **Buttons:** `Set` (Keyframe mit aktuellem Kamera-Zustand überschreiben), `Goto` (Kamera hart auf Keyframe setzen), `Del`, `▲`/`▼` (Reorder).
- **Transport-Leiste** unter der Liste: `Play/Pause`- und `Stop`-Button, `Scrub`-Slider (0–1, treibt `PCT_SequencePlayer` auf normierte Sequenzzeit; beim Scrubben pausiert Playback), `Loop`-Checkbox, Interpolations-SelectBox.
- **Kern-Interaktion:** Button **"Add Keyframe from Current Camera"** (zusätzlich Keybind `UAPCT_AddKeyframe`) — liest Position/Orientierung/FOV/Fokus der aktiven Kamera und hängt einen Keyframe an. Das ist der 90-%-Workflow; alles andere ist Korrektur.

**Begründung gegen eine Canvas-Timeline in Phase 3:** Die `UIActionManager`-Factory bietet kein Timeline-Primitive; eine gezeichnete Timeline bräuchte pixelgenaues Zeichnen, eigenes Hit-Testing für draggable Keyframe-Marker, Zoom/Pan und Zeitlineal — mehrere Wochen Custom-Widget-Arbeit mit unverifizierten Widget-APIs, während die Scroller-Liste denselben Funktionsumfang (Zeiten numerisch editieren, Reorder, Scrub) mit verifizierten Bausteinen liefert. Die Dabs-`EditorCameraTrackData`-Präzedenz (Research_Framework_Patterns §8) zeigt: das Datenmodell "geordnete Keyframe-Liste" braucht keine grafische Timeline. **Canvas-Timeline ist Phase-5-Stretch** und als solcher **OFFEN — ZU VERIFIZIEREN** (Test: Minimal-Layout mit `CanvasWidget`, Linien/Rechtecke zeichnen, Maus-Events prüfen — die Canvas-API ist in keinem Research-Dokument belegt).

---

## 6. Shot-Browser (Phase 2)

Liste gespeicherter Shots (aus `$profile:PsyernsCinematicTool\Shots\*.json`, siehe Plan_B3_Datenmodell_Persistenz.md) im Shots-Tab:

- **Zeile** (`pct_shot_row.layout`): Shot-Name · Preset-Kürzel (z. B. `CU/Eye`) · Brennweite (`85mm`); Klick selektiert.
- **Buttons:** `Apply` (Kamera + Optik-Zustand setzen), `Update` (Shot mit aktuellem Zustand überschreiben), `Delete` (mit `CreateConfirmation_Two`-Dialog, JMFormBase-Helper verifiziert), `Duplicate`, `Export` (Kopie nach `Exports\` inkl. technischer Daten; optional `MakeScreenshot` für ein DDS-Standbild — **Anzeige** solcher Thumbnails im UI ist **OFFEN — ZU VERIFIZIEREN**, Test: `ImageWidget.LoadImageFile` mit `$profile:`-DDS-Pfad probieren; bis dahin Thumbnails nur als Datei-Export, UI-Vorschau Phase 5).
- Sortierung: zuletzt geändert oben; `QuickSaveShot`-Keybind (§7) legt ohne Formular einen Shot mit Auto-Namen (`Shot_JJJJMMTT_HHMMSS`) an.

**Coverage-Assistent-Panel (Phase 4, im Assist-Tab):** Nach Setzen von Akteur A/B generiert `PCT_CinematicModule` die Standard-Abdeckung einer Dialogszene (Plan_A1 §11: Master, Two Shot, OTS A, OTS B, CU A, CU B, Insert) als **Vorschlagsliste**: je Zeile Vorschlags-Name + Framing-Kürzel + Buttons `Preview` (Kamera hinfliegen) und `Als Shot speichern`. Geometrieberechnung (Achse A–B, 180°-Seite, Distanz aus FOV) in `PCT_Math`; UI ist reine Liste, kein Automatik-Zwang.

---

## 7. Keybind-Tabelle (`UAPCT_*`, eigenes `Scripts/Data/Inputs.xml`)

**COT-Belegung (verifiziert aus `JM\COT\Scripts\Data\Inputs.xml`, Preset-Block Z. 95–229):** `kEnd`, `kY`, `kInsert`, `kH`, `kL`, `kU`, `kDelete`, `kNumpad1`, `mWheelUp/Down` (nackt und mit `kLControl`) sowie die Chords `kRMenu`+`kT/kY/kU/kI/kB/kP/kM/kJ/kK/kL/kO/kN`. Diese sind sämtlich **tabu**. PCT nutzt deshalb den freien `kRMenu`+`kC`-Chord, `kRControl`-Chords und den ansonsten unbelegten Numpad-Block als Transport-Cluster:

| Action | Funktion | Default | Kollisionfrei weil | Phase |
|---|---|---|---|---|
| `UAPCT_ToggleMenu` | PCT-Fenster öffnen/schließen (via `GetInputToggle()`) | `kRMenu` + `kC` | `kC` im COT-Chord-Namensraum frei | 0 |
| `UAPCT_ToggleCamera` | PCT-Kamera Enter/Leave (eigener Pfad, `EPCT_RPC.Enter`/`Leave`; Handoff nach Regel K2 bei aktiver COT-Freecam — Plan_B1_Architektur.md §5.2) | `kRControl` + `kC` | `kC` von COT nicht belegt; `kRControl`-Chord frei | 1 |
| `UAPCT_ToggleHUD` | Kamera-HUD ein/aus | `kRControl` + `kH` | `kH` nur nackt von COT belegt; `kRControl`-Chord frei | 1 |
| `UAPCT_ToggleOverlay` | Overlay-Master ein/aus | `kRControl` + `kG` | frei | 2 |
| `UAPCT_CycleAspectMask` | Maske durchschalten (Aus→2.39→…→1:1) | `kRControl` + `kB` | frei | 2 |
| `UAPCT_QuickSaveShot` | Shot ohne Dialog speichern | `kRControl` + `kS` | frei | 2 |
| `UAPCT_AddKeyframe` | Keyframe aus aktueller Kamera | `kNumpad8` | Numpad von COT nur `kNumpad1` belegt | 3 |
| `UAPCT_PlayPause` | Sequenz Play/Pause | `kNumpad5` | frei | 3 |
| `UAPCT_Stop` | Sequenz Stop (zurück zu Keyframe 0) | `kNumpad0` | frei | 3 |
| `UAPCT_PrevShot` | vorherigen Shot anwenden (A/B) | `kNumpad4` | frei | 2 |
| `UAPCT_NextShot` | nächsten Shot anwenden (A/B) | `kNumpad6` | frei | 2 |
| `UAPCT_ApplyFraming` | Framing-Preset auf Ziel anwenden (FRAMED-Modus, Plan_B4_Kamera_Engine.md §1/§6) | ohne Default (über Optionen belegbar) | kein Default-Bind → keine Kollision | 2 |
| `UAPCT_ToggleOrbit` | Orbit-Modus ein/aus (Plan_B4_Kamera_Engine.md §1/§5.6) | ohne Default (über Optionen belegbar) | kein Default-Bind → keine Kollision | 3 |
| `UAPCT_ToggleFollow` | Follow-Modus ein/aus (Plan_B4_Kamera_Engine.md §1/§5.7) | ohne Default (über Optionen belegbar) | kein Default-Bind → keine Kollision | 3 |
| `UAPCT_ToggleHandheld` | Handheld-Overlay ein/aus (Plan_B4_Kamera_Engine.md §1/§5.8) | ohne Default (über Optionen belegbar) | kein Default-Bind → keine Kollision | 3 |

Hinweise:

- Key-Identifier `kNumpad*`, `kInsert`, `kRMenu`, `kLControl` sind durch die COT-Datei belegt/verifiziert; **`kRControl` als Identifier ist plausibel, aber nicht in den Research-Dokumenten belegt: OFFEN — ZU VERIFIZIEREN** (Test: Preset-Bind in `Inputs.xml` eintragen, in-game unter Optionen→Steuerung prüfen, Tastendruck testen; Fallback: `kLControl`-Chords, die kollidieren nur mit COT-Zoom auf `mWheel`, nicht auf Buchstaben).
- Registrierung im Modul (Muster JMCameraModule.c:203; `super` zuerst — `JMRenderableModuleBase` bindet dort bereits `GetInputToggle()`):

```c
override void RegisterKeyMouseBindings()
{
	super.RegisterKeyMouseBindings();

	Bind(new JMModuleBinding("Input_ToggleCamera", "UAPCT_ToggleCamera", true));
	Bind(new JMModuleBinding("Input_ToggleHud", "UAPCT_ToggleHUD", true));
	Bind(new JMModuleBinding("Input_ToggleOverlay", "UAPCT_ToggleOverlay", true));
	Bind(new JMModuleBinding("Input_CycleAspectMask", "UAPCT_CycleAspectMask", true));
	Bind(new JMModuleBinding("Input_QuickSaveShot", "UAPCT_QuickSaveShot", true));
	Bind(new JMModuleBinding("Input_AddKeyframe", "UAPCT_AddKeyframe", true));
	Bind(new JMModuleBinding("Input_PlayPause", "UAPCT_PlayPause", true));
	Bind(new JMModuleBinding("Input_Stop", "UAPCT_Stop", true));
	Bind(new JMModuleBinding("Input_PrevShot", "UAPCT_PrevShot", true));
	Bind(new JMModuleBinding("Input_NextShot", "UAPCT_NextShot", true));
	Bind(new JMModuleBinding("Input_ApplyFraming", "UAPCT_ApplyFraming", true));
	Bind(new JMModuleBinding("Input_ToggleOrbit", "UAPCT_ToggleOrbit", true));
	Bind(new JMModuleBinding("Input_ToggleFollow", "UAPCT_ToggleFollow", true));
	Bind(new JMModuleBinding("Input_ToggleHandheld", "UAPCT_ToggleHandheld", true));
}
```

- Deklaration der Actions in `PsyernsCinematicTool/Scripts/Data/Inputs.xml`, registriert über `inputs = "PsyernsCinematicTool/Scripts/Data/Inputs.xml"` im `CfgMods`-Eintrag `PsyernsCinematicTool` (COT-Muster, Scripts/config.cpp:33).
- Keybind-Wirkungen sind clientlokal (HUD/Overlays/Sequencer/Shot-Apply) — keine Permission über `CinematicTool.View` hinaus nötig. Ausnahme: `UAPCT_ToggleCamera` sendet `EPCT_RPC.Enter`/`Leave`; der Server prüft `CinematicTool.View` im Handler (Plan_B1_Architektur.md §6/§8). Weitere Wirk-Aktionen laufen ohnehin über Formular + RPC.

---

## 8. UX-Flüsse

### (a) "Ersten Shot bauen"

1. COT aktivieren; PCT-Fenster über Sidebar-Button oder `kRMenu`+`kC` öffnen (Permission `CinematicTool.View` nötig, sonst existiert der Button nicht).
2. PCT-Kamera über den **PCT-eigenen Enter-Pfad** betreten: Button "Kamera Enter/Leave" im Tab "Camera & Lens" oder `kRControl`+`kC` (`UAPCT_ToggleCamera`) — der Server spawnt `PCT_CinematicCamera` via `EPCT_RPC.Enter` (Plan_B1_Architektur.md §5.2/§6). Ist bereits die COT-Freecam (`kInsert`) aktiv, wird **kein** PCT-Enter gesendet (Koexistenz-Regel K1); stattdessen sequenzieller Handoff nach Regel K2: Position merken → COT-Leave → PCT-Enter an gemerkter Position.
3. Motiv anfliegen; optional Tab "Shots" → "Motiv wählen (Crosshair)" für Ziel-Lock.
4. Framing-Preset wählen (z. B. `CU` + `Eye Level` + 85 mm) → "Frame Target" berechnet Distanz/Höhe (Phase 2; in Phase 1 manuell fliegen).
5. Feintuning im Tab "Camera & Lens": Brennweite, Fokusdistanz, Roll, Exposure.
6. Overlays einschalten (`kRControl`+`kG`), Maske durchschalten (`kRControl`+`kB`), Headroom/Drittel prüfen.
7. `kRControl`+`kS` (QuickSave) oder Tab "Shots" → Name eintragen → `Save`. Bestätigung als CF-Notification.

### (b) "Sequenz aufnehmen und abspielen"

1. Ersten Kamera-Stand einrichten (Fluss a, Schritte 2–6).
2. `kNumpad8` — Keyframe 0 gesetzt (HUD zeigt Keyframe-Zähler).
3. Kamera zum nächsten Stand fliegen, Brennweite/Fokus anpassen, erneut `kNumpad8`; beliebig wiederholen.
4. Tab "Sequencer": Segment-Zeiten eintragen, Interpolation wählen (CatmullRom für weiche Fahrt).
5. `kNumpad5` — Playback; HUD zeigt REC-Indikator; `kNumpad5` erneut = Pause.
6. Mit Scrub-Slider kritische Stellen prüfen, Keyframes per `Set`/`Goto` korrigieren.
7. `kNumpad0` — Stop, Kamera zurück auf Keyframe 0.
8. Sequenz-Name eintragen → `Save` (`Sequences\*.json`).

### (c) "A/B-Vergleich zweier Shots" (sequentieller Switch)

Es gibt genau eine aktive Engine-Kamera und kein verifiziertes Render-to-Texture-API — Splitscreen ist daher verworfen; der Vergleich läuft als harter Cut, was dem filmischen Schnitt-Eindruck ohnehin näher ist.

1. Tab "Shots": Shot A selektieren → `Apply`.
2. Bild beurteilen (Overlays an, HUD zeigt Shot-Namen).
3. `kNumpad6` (`NextShot`) — Shot B wird hart geschnitten angewendet.
4. Mit `kNumpad4`/`kNumpad6` zwischen A und B pendeln; auf Anschluss, Framing-Differenz und 30°-Regel achten (automatische 30°-Warnung: Phase 4, Assist).
5. Gewinner-Shot behalten, Verlierer per `Delete` (Bestätigungsdialog) entfernen oder für Coverage aufheben.

### (d) "Welt-Look einstellen" (Zeit/Wetter + Freeze)

1. Tab "World" öffnen (Controls nur aktiv mit Permission `CinematicTool.World`; sonst ausgegraut/versteckt).
2. Datum/Uhrzeit setzen (z. B. Golden Hour) → `Apply` — Server-RPC, `World.SetDate`.
3. "Zeit einfrieren" aktivieren → `SetTimeMultiplier(0)` serverseitig.
4. Overcast/Fog/Rain per Slider setzen — Phänomene mit `Set(wert, 0, sehr lange Dauer)`.
5. "Wetter einfrieren" aktivieren → `SetWeatherUpdateFreeze(true)`.
6. Look im Kamerabild prüfen; danach Shot speichern — der Welt-Zustand wird als optionaler Block im Shot-JSON abgelegt (siehe Plan_B3_Datenmodell_Persistenz.md).
7. Beim Beenden der Session: Freezes lösen (Checkboxen aus) — UI erinnert per Notification, wenn Freezes beim Schließen aktiv sind.

---

## 9. Layout-Datei-Plan

Alle Layouts unter `PsyernsCinematicTool/GUI/layouts/`, Texturen unter `PsyernsCinematicTool/GUI/textures/` (.edds, POT-Pflicht):

| Datei | Zweck | Geladen von | Phase |
|---|---|---|---|
| `pct_cinematic_form.layout` | Hauptfenster-Inhalt (Tab-SelectBox + Panels); Root-Script wird via `GetLayoutRoot()`/`widgets.GetScript(m_Form)` an `PCT_CinematicForm` gebunden (COT-Show-Flow, Research_COT_Infrastructure §2); Name gemäß Plan_B1_Architektur.md §3/§5.1 | `JMRenderableModuleBase.Show()` | 0–1 |
| `pct_hud.layout` | Kamera-HUD-Statuszeile (§3) | `PCT_CinematicModule` (Workspace) | 1 |
| `pct_overlay.layout` | Kompositions-Overlays: Raster, Masken-Panels, Safe Areas, Horizont, Marker (§4) | `PCT_CinematicModule` (Workspace) | 2 |
| `pct_shot_row.layout` | Zeilen-Template Shot-Browser (§6) | `PCT_CinematicForm` (pro Shot instanziert) | 2 |
| `pct_keyframe_row.layout` | Zeilen-Template Keyframe-Liste (§5) | `PCT_CinematicForm` | 3 |
| `pct_coverage_row.layout` | Zeilen-Template Coverage-Vorschläge (§6) | `PCT_CinematicForm` | 4 |

**GUI/config.cpp-Einbindung:** eigener Addon-Teil `PCT_GUI` (CfgPatches), `requiredVersion = 0.1`, `requiredAddons[] = {"DZ_Data"}` (Layouts/Texturen brauchen keine Script-Abhängigkeit). Pfade in `config.cpp` mit **einfachen** Backslashes (Projektregel); Layout-Pfade im EnScript-Code dagegen mit Forward-Slashes (`"PsyernsCinematicTool/GUI/layouts/pct_cinematic_form.layout"`, COT-Konvention). Modul-Icon nach COT-Vorbild unter `GUI/textures/modules/`. Das Scripts-Addon `PCT_Scripts` referenziert `JM_COT_GUI` bereits über seine `requiredAddons` (Konvention), sodass COT-Basis-Layouts (`CF_Window`-Chrome) garantiert geladen sind.

Zeilen-Templates werden mit `g_Game.GetWorkspace().CreateWidgets(pfad, parent)` in den Scroller gehängt; wo eine Zeile nur Standard-Controls braucht, ist alternativ die reine `UIActionManager`-Komposition (GridSpacer + Buttons) zulässig — Template-Layouts nur dort, wo dichteres Layout nötig ist (Keyframe-Zeile).

---

## 10. Lokalisierung

- **Ansatz:** alle sichtbaren Strings als `#STR_PCT_*`-Keys (z. B. `#STR_PCT_MODULE_NAME`, `#STR_PCT_TAB_CAMERA`, `#STR_PCT_BTN_ADD_KEYFRAME`), sowohl in Layouts als auch in `CreateButton/CreateText/...`-Aufrufen — COT verwendet dasselbe Muster (`"#STR_COT_CAMERA_MODULE_NAME"`).
- **Phasen:** Ab Phase 2 werden neue UI-Strings direkt als Keys angelegt (englischer Text als einzige Spalte); **DE+EN vollständig ab Phase 5** (Polish & Release). So entsteht kein Umbau-Aufwand kurz vor Release.
- **Ablage:** `stringtable.csv` im `PCT_GUI`-Addon-Root. **OFFEN — ZU VERIFIZIEREN:** ob die Stringtable aus dem GUI-PBO geladen wird (Test: einen `#STR_PCT_*`-Key im HUD anzeigen; erscheint der Key-Name statt des Textes, Fallback: eigenes Mini-Addon `PCT_LanguageCore` nach COT-Vorbild `JM_COT_LanguageCore` mit eigener `CfgPatches`).
- Format: DayZ-Standard-CSV (`"language","original","german",...`); Umlaute UTF-8.

---

## Querverweise

- `Plan_A1.md` — Stoffsammlung/Anforderungen (v. a. §3 Einstellungsgrößen, §4 Winkel, §9 Kompositionshilfen, §11 Coverage, §12 UI-Wunschbild).
- `Plan_B1_Architektur.md` — Mod-Paket, CfgPatches/CfgMods, Modul-Registrierung (`JMModuleConstructor`); `EPCT_RPC` (Basis 10500), Server-Handler, Permission-Baum `CinematicTool.*` (§6–8); Enter-/Leave-Pfad und Koexistenz-Regeln K1–K5 (§5.2).
- `Plan_B2_Feature_Mapping.md` — Feature-Mapping Plan_A1 → DayZ-Machbarkeit (Phaseneinstufung, Ersatzstrategien).
- `Plan_B3_Datenmodell_Persistenz.md` — Shot-/Sequenz-/Settings-JSON-Schemata, `schemaVersion`, `$profile:`-Ordnerstruktur.
- `Plan_B4_Kamera_Engine.md` — Kamera-Kern, FOV-/Brennweiten-Mathematik (`PCT_Math`), DOF-Pipeline, Sequencer-Playback.
- `Plan_B6_Roadmap_Risiken.md` — Phasenplan (Phase 0 Skeleton … Phase 5 Polish & Release), Teststrategie, Risiko-Register.
- `Docs/Research/Research_COT_Infrastructure.md` — COT-Modul-/Form-/RPC-/Permission-Fakten, Keybind-Belegung.
- `Docs/Research/Research_Vanilla_APIs.md` — Camera/PPE/Widget/Weather/Screenshot-APIs (1.29, verifiziert).
- `Docs/Research/Research_Framework_Patterns.md` — UI-Stack-Entscheidung, Dabs/Expansion als Pattern-Referenz.
