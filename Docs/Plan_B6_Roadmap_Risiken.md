# Plan B6 — Roadmap, Teststrategie, Risiken & offene Fragen

**Stand:** 2026-07-21 · **Status:** Entwurf v1 · **Projekt:** PsyernsCinematicTool (PCT) · **Autor:** Psyern

**Abstract:**
Dieses Dokument legt den Umsetzungsfahrplan für PsyernsCinematicTool in sechs Phasen (Phase 0 Skeleton bis Phase 5 Polish & Release) fest,
definiert je Phase Deliverables, Abhängigkeiten und nachprüfbare Definition-of-Done-Kriterien und ordnet grobe Aufwandsklassen zu.
Ergänzend beschreibt es die Teststrategie (Offline-Mission mit COT, dedizierter Testserver mit zwei Clients), die Paketierung und Signierung
für die Workshop-Distribution sowie ein Risiko-Register. Alle in B1–B5 verbliebenen "OFFEN — ZU VERIFIZIEREN"-Punkte werden hier konsolidiert,
mit konkretem Verifikationsschritt, blockierten Features und Zielphase. Grundlage sind ausschließlich die verifizierten Research-Dokumente.

---

## 1. Phasenplan

Phasen-Nomenklatur (projektweit verbindlich): **Phase 0 Skeleton · Phase 1 Kamera-Kern · Phase 2 Shots & Presets · Phase 3 Sequencer · Phase 4 Welt/Akteure/Licht & Assistenten · Phase 5 Polish & Release**.

Begründung der Reihenfolge: Die Kamera ist der Kern des Tools — ohne stabilen Enter/Leave-Flow und Rig-Mathematik (Phase 1) sind Shots (Phase 2) und Sequenzen (Phase 3) nicht sinnvoll testbar. Welt-/Akteur-Features (Phase 4) sind die einzigen stark server-authoritativen Systeme und werden bewusst nach hinten gezogen, damit die RPC-/Permission-Infrastruktur bis dahin im Feld erprobt ist. Verworfene Alternative: Sequencer vor Shots — verworfen, weil der Sequencer Keyframes aus dem Shot-Datenmodell (`PCT_Shot`, siehe Plan_B3_Datenmodell_Persistenz.md) ableitet und sonst ein Wegwerf-Datenmodell entstünde.

### Phase 0 — Skeleton (Aufwandsklasse: S)

**Ziel:** Ein lauffähiges, signierbares Mod-Paket, dessen leeres Modul im COT-Menü sichtbar ist und dessen Permission im COT-Permission-Baum registriert wird.

**Deliverables:**

| Deliverable | Pfad / Inhalt |
|---|---|
| Paket-Skeleton | `PsyernsCinematicTool/PsyernsCinematicTool/` mit `Scripts/` und `GUI/` (vorhandener leerer Ordner `scripts` wird zu `Scripts` umbenannt) |
| Scripts-config | `Scripts/config.cpp` — CfgPatches `PCT_Scripts`, `requiredVersion = 0.1`, `requiredAddons[] = {"DZ_Data","JM_CF_Scripts","JM_COT_Scripts","JM_COT_GUI"}`; CfgMods `PsyernsCinematicTool` mit `gameScriptModule`/`worldScriptModule`/`missionScriptModule` auf `PsyernsCinematicTool/Scripts/3_Game|4_World|5_Mission` und `inputs = "PsyernsCinematicTool/Scripts/Data/Inputs.xml"` (Muster: COT `Scripts/config.cpp`, siehe Research_COT_Infrastructure.md §5/§7) |
| GUI-config | `GUI/config.cpp` — CfgPatches `PCT_GUI` |
| Input-Stub | `Scripts/Data/Inputs.xml` mit `UAPCT_ToggleMenu` (ohne Default-Bind-Konflikt, siehe Risiko R6) |
| RPC-Enum | `Scripts/3_Game/PsyernsCinematicTool/PCT_RPC.c` — `enum EPCT_RPC { INVALID = 10500, COUNT }` (Basis 10500; COTs höchste Basis ist 10460, Research_COT_Infrastructure.md §4/§5; vollständige Belegung Plan_B1_Architektur.md §6) |
| Konstanten | `Scripts/3_Game/PsyernsCinematicTool/PCT_Constants.c` — `PCT_VERSION`, Pfad-Konstanten (`$profile:PsyernsCinematicTool\`) |
| Leeres Modul | `Scripts/5_Mission/PsyernsCinematicTool/Modules/PCT_CinematicModule.c` — `PCT_CinematicModule : JMRenderableModuleBase`; Konstruktor registriert alle 7 `CinematicTool.*`-Permissions (Plan_B1_Architektur.md §5.1); `HasAccess()` prüft `"CinematicTool.View"`, `GetLayoutRoot()` → `"PsyernsCinematicTool/GUI/layouts/pct_cinematic_form.layout"`, `GetTitle()`, `GetRPCMin()/GetRPCMax()` (Muster: `JMExampleModule`) |
| Modul-Registrierung | `Scripts/5_Mission/PsyernsCinematicTool/PCT_ModuleRegistration.c` — `modded class JMModuleConstructor`, `override RegisterModules(...)` mit `super.RegisterModules(modules);` zuerst (Datei gemäß Plan_B1_Architektur.md §3/§5.1) |
| Form-Stub | `Scripts/5_Mission/PsyernsCinematicTool/GUI/PCT_CinematicForm.c` (`PCT_CinematicForm : JMFormBase`) + Platzhalter-Layout `GUI/layouts/pct_cinematic_form.layout` |
| Build-Skript ("CI des Packens") | `Tools/Build_PCT.ps1` — packt `Scripts/` und `GUI/` zu PBOs, signiert beide, kopiert nach `P:\`-freiem Testpfad; reproduzierbar per Doppelklick/Kommandozeile (Details §3) |

**Abhängigkeiten:** keine (Startphase). Voraussetzung: CF + COT lokal installiert.

**Definition of Done (nachprüfbar):**
1. `Build_PCT.ps1` erzeugt fehlerfrei `PCT_Scripts.pbo` + `PCT_GUI.pbo` inkl. `.bisign`.
2. Offline-Mission mit CF+COT+PCT startet ohne Einträge in `script.log` (keine Compile-Errors, keine `FIX-ME`-Override-Warnungen aus PCT-Dateien).
3. Der COT-Sidebar zeigt den Eintrag "Cinematic Tool"; Klick öffnet das (leere) Fenster.
4. `"CinematicTool.View"` erscheint im COT-Permissions-Editor; Entzug der Permission blendet das Modul aus (`OnClientPermissionsUpdated`-Verhalten der Basisklasse).
5. Dedizierter Server + 1 Client: Client ohne Permission sieht das Modul nicht; mit Permission ja.

### Phase 1 — Kamera-Kern (Aufwandsklasse: M)

**Ziel:** Eine eigene Cinematic-Kamera mit sauberem Enter/Leave über den COT-Spectator-Flow, filmisches Rig (Sensor/Brennweite→FOV), DOF-Steuerung und Kompositions-HUD.

**Deliverables:**

| Deliverable | Pfad / Inhalt |
|---|---|
| Kamera-Klasse | `Scripts/3_Game/PsyernsCinematicTool/Cameras/PCT_CinematicCamera.c` — `PCT_CinematicCamera : JMCinematicCamera` (erbt Fly-Steuerung (`OnUpdate`) und Traveling (`SetupTraveling`); die Easing-Helper `SmoothStep`/`SmootherStep` sind in `JMCinematicCamera.c:243/248` `private` und aus der Subklasse nicht aufrufbar — sie werden in `PCT_Math` nachimplementiert (siehe "PCT_Math (Teil 1)"); Research_COT_Infrastructure.md §1) |
| Enter/Leave | RPCs `EPCT_RPC.Enter`/`Leave`/`Leave_Finish` (Belegung Plan_B1_Architektur.md §6) in `PCT_CinematicModule`; Server-Pfad spiegelt `JMCameraModule.Server_Enter`: `g_Game.SelectPlayer(sender, NULL)` + `g_Game.SelectSpectator(sender, "PCT_CinematicCamera", position)`; Offline-Pfad `g_Game.CreateObject("PCT_CinematicCamera", position, false)`; jeder Server-Handler prüft `GetPermissionsManager().HasPermission("CinematicTool.View", senderRPC)` |
| Kamera-Rig | `Scripts/3_Game/PsyernsCinematicTool/Data/PCT_CameraRig.c` (Plan_B3_Datenmodell_Persistenz.md §2.3) — Sensor-Presets (Full Frame 36×24, Super 35, APS-C, MFT, 1", Custom), Brennweite→FOV: `fovVertical = 2 * Math.Atan2(sensorHeightMm, 2 * focalLengthMm)`; Anwendung via `Camera.SetFOV(radians)` (FOV ist vertikal, in Radiant — Research_Vanilla_APIs.md §1/§2; Clamp-Verhalten OFFEN, siehe O1) |
| PCT_Math (Teil 1) | `Scripts/3_Game/PsyernsCinematicTool/Utils/PCT_Math.c` — FOV/Brennweiten-Konvertierung, `SmoothStep`/`SmootherStep`/S-Curve-Easing, Winkel-Wrap-Interpolation (Clean-Room nach `ExpansionMath.InterpolateAngles`-Muster, Research_Framework_Patterns.md §7) |
| DOF-Steuerung | Blende/Fokusdistanz im Rig → `PPEffects.OverrideDOF(...)` bzw. `g_Game.OverrideDOF(enable, focusDistance, focusLength, focusLengthNear, blur, focusDepthOffset)`; Reset bei Leave via `PPEffects.ResetDOFOverride()` (Research_Vanilla_APIs.md §3) |
| Basis-HUD | `GUI/layouts/pct_hud.layout` — Kamera-HUD-Statuszeile (Brennweite, Sensor, Fokus, FOV, Roll — Plan_B5_UI_UX.md §3); die Kompositions-Overlays (Raster, Aspekt-Masken) folgen in Phase 2 (`pct_overlay.layout` + `PCT_OverlayHud`, Plan_B5_UI_UX.md §4) |
| Form-Panel "Rig" | Erweiterung `PCT_CinematicForm` mit `UIActionManager`-Widgets: Sensor-SelectBox, Brennweiten-Slider/Buttons, Blende, Fokusdistanz, FOV-Anzeige (Grad) |
| Inputs | `UAPCT_ToggleCamera` in `Inputs.xml`; Bindung via `RegisterKeyMouseBindings()`/`JMModuleBinding` |

**Abhängigkeiten:** Phase 0.

**Definition of Done (nachprüfbar):**
1. Offline: Kamera-Enter/Leave über Menü-Button und `UAPCT_ToggleCamera` funktioniert 10× in Folge ohne hängenden Input (Player-Input nach Leave wieder aktiv).
2. Dedizierter Server, 2 Clients: Client A (mit Permission) betritt/verlässt die Kamera; Client B sieht As Körper stehen bleiben; Client ohne Permission erhält beim RPC-Versuch keine Kamera (Server-Gate greift).
3. Brennweiten-Preset 24/50/85 mm auf Full Frame erzeugt sichtbar korrekte, monoton fallende FOV-Werte; angezeigter Grad-Wert entspricht der Formel (Stichprobe von Hand nachgerechnet).
4. DOF: Fokusdistanz-Änderung erzeugt sichtbare Schärfeebene; nach Leave ist das Bild wieder neutral (kein DOF-Rest).
5. HUD-Statuszeile per `UAPCT_ToggleHUD` ein-/ausblendbar; angezeigte Werte (Brennweite, FOV, Fokus) stimmen mit dem Rig überein.

### Phase 2 — Shots & Presets (Aufwandsklasse: L)

**Ziel:** Shots (kompletter Kamera-/Rig-Zustand) speichern, wieder anwenden, über Framing-/Angle-Presets automatisch positionieren und als JSON/CSV/Screenshot exportieren.

**Deliverables:**

| Deliverable | Pfad / Inhalt |
|---|---|
| Datenmodell | `Scripts/3_Game/PsyernsCinematicTool/Data/PCT_Shot.c`, `PCT_Presets.c` (Preset-Familien `PCT_SensorPreset` … `PCT_WorldStatePreset` + Container), `PCT_Settings.c` — JSON-Datenklassen, Member OHNE Präfix, jede Datei mit `int schemaVersion` (Details siehe Plan_B3_Datenmodell_Persistenz.md §2) |
| Persistenz | `Scripts/3_Game/PsyernsCinematicTool/Persistence/PCT_Storage.c` (Plan_B1_Architektur.md §3, Plan_B3 §4) — `JsonFileLoader<T>` nach `$profile:PsyernsCinematicTool\Shots\` bzw. `Presets\`; `Settings.json` im Wurzelordner |
| Kompositions-Overlays | `GUI/layouts/pct_overlay.layout` + `Scripts/5_Mission/PsyernsCinematicTool/GUI/PCT_OverlayHud.c` — Drittelraster, Goldener Schnitt, Fadenkreuz, Safe Areas, Aspekt-Masken (2.39:1, 16:9, 4:3, 9:16, 1:1) als Letterbox-Panels über `SetScreenSize` (kein Engine-Aspect-API vorhanden — Research_Vanilla_APIs.md §10; Plan_B5_UI_UX.md §4) |
| Auto-Framing | `Scripts/3_Game/PsyernsCinematicTool/PCT_Framing.c` — Einstellungsgrößen-Presets (EWS…ECU, Plan_A1.md §3) + Angle-Presets (Eye/High/Low/Dutch…, §4): aus Motivhöhe, gewünschtem Bildanteil und vertikalem FOV wird die Kameradistanz berechnet; Motivhöhen-Messung per BoundingBox-API OFFEN (O2) — Fallback: konfigurierte Standardhöhe 1.8 m |
| Shot-Browser | Form-Panel mit Scroller-Liste (Muster `JMCameraForm`-Positionsliste), Anwenden/Löschen/Umbenennen |
| Export | `Scripts/3_Game/PsyernsCinematicTool/PCT_Export.c` — Shot-Liste als JSON (`JsonFileLoader`) und CSV nach `$profile:PsyernsCinematicTool\Exports\` (CSV über Engine-Datei-API `OpenFile`/`FPrintln`/`CloseFile` — Signaturen am 1.29-Quellcode verifiziert, `EnSystem.c:417/481/443`, Plan_B3_Datenmodell_Persistenz.md §8) |
| Screenshots | Shot-Thumbnail via `MakeScreenshot(name)` — Ausgabe ist DDS (`$profile:ScreenShotes\...` bzw. `$`-Pfad, Research_Vanilla_APIs.md §11); Workflow-Hinweis in UI ("DDS, Konvertierung extern"), siehe Risiko R8 |
| Permission | `"CinematicTool.Export"` gate für Export-Aktionen (Registrierung bereits im Modul-Konstruktor ab Phase 0) |

**Abhängigkeiten:** Phase 1 (Rig-Zustand ist der Shot-Inhalt).

**Definition of Done (nachprüfbar):**
1. Shot speichern → Spiel neu starten → Shot laden stellt Position, Orientierung, Sensor, Brennweite, Blende, Fokus, Maske exakt wieder her (Vergleich über angezeigte Werte + Screenshot).
2. `Shots\*.json` enthält `schemaVersion` und ist von Hand lesbar/editierbar; manipulierte Datei mit falscher Version wird abgewiesen und geloggt statt zu crashen.
3. Framing-Preset "MS · Eye Level · 50 mm" positioniert die Kamera vor einem Test-PlayerBase so, dass die Hüft-Linie im unteren Bilddrittel liegt (Sichtprüfung mit Drittelraster).
4. CSV-Export öffnet fehlerfrei in Excel/LibreOffice; Spalten entsprechen dem in Plan_B3_Datenmodell_Persistenz.md definierten Schema.
5. Screenshot-Button erzeugt eine DDS-Datei mit Shot-Namen im Dateinamen.
6. Alle Kompositions-Overlays einzeln zu- und abschaltbar; Aspekt-Masken decken bei 16:9-Auflösung pixelgenau ab (Screenshot-Kontrolle).

### Phase 3 — Sequencer (Aufwandsklasse: L)

**Ziel:** Keyframe-basierte Kamerafahrten aufnehmen, mit Catmull-Rom und Easing abspielen, scrubben und über Bewegungs-Presets generieren.

**Deliverables:**

| Deliverable | Pfad / Inhalt |
|---|---|
| Sequencer | `Scripts/3_Game/PsyernsCinematicTool/Sequencer/PCT_SequencePlayer.c` (Plan_B1_Architektur.md §3) — Kanäle Position/Orientierung/FOV/Fokus/Roll; Position via `Math3D.Curve(ECurveType.CatmullRom, t, points)` (Research_Vanilla_APIs.md §4); Orientierung via PCT_Math-Winkel-Interpolation (kein `QuatSlerp` in Vanilla — nur `QuatLerp`); Easing pro Segment; Scrub = direkte Auswertung bei beliebigem t; Pause/Resume |
| Datenmodell | `Scripts/3_Game/PsyernsCinematicTool/Data/PCT_Sequence.c` (+ `PCT_Keyframe`, Plan_B3_Datenmodell_Persistenz.md §2.2/§2.5) — Form nach Dabs `EditorCameraTrackData` (Time/Position/Orientation) + FOV/Fokus/Roll (Research_Framework_Patterns.md §8); Persistenz nach `Sequences\` |
| Aufnahme | "Add Keyframe" übernimmt aktuellen Rig-Zustand; Keyframe-Liste im Form-Panel (Reihenfolge ändern, löschen, Zeit editieren) |
| Bewegungs-Presets | `Scripts/3_Game/PsyernsCinematicTool/PCT_MotionPresets.c` — Dolly (In/Out/Truck), Orbit (PCT_Math-Port von `ExRotateAroundPoint`-Muster), Follow (Ziel-Objekt + `Math.SmoothCD`-Dämpfung), Handheld (summierte Sinus-Störungen + `Math.SmoothCD`; kein Perlin-Noise in Vanilla — Research_Vanilla_APIs.md §9), Dolly-Zoom (Distanz- und FOV-Kanal gegenläufig, Motivgröße konstant) |
| Inputs | `UAPCT_AddKeyframe`, `UAPCT_PlayPause`, `UAPCT_Stop` in `Inputs.xml` (Belegung siehe Keybind-Tabelle Plan_B5_UI_UX.md §7; Scrubben läuft über den Scrub-Slider im Sequencer-Tab) |
| Pfad-Visualisierung | Optionales Overlay: Keyframe-Marker via `g_Game.GetScreenPosRelative(worldPos)` ins HUD projiziert (Research_Vanilla_APIs.md §5) |

**Abhängigkeiten:** Phase 1 (Kamera), Phase 2 (Datenmodell/Persistenz-Infrastruktur).

**Definition of Done (nachprüfbar):**
1. 5-Keyframe-Fahrt: Playback läuft ruckelfrei durch alle Keyframes; an jedem Keyframe stimmen Position/FOV mit den gespeicherten Werten überein (Anzeige-Vergleich).
2. Catmull-Rom sichtbar: Bei 3 nicht-kollinearen Keyframes fährt die Kamera eine Kurve, keine Polylinie (Vergleich mit COT-Traveling als Referenz).
3. Scrubbing auf beliebige Zeit setzt Kamera deterministisch (zweimal auf t=0.5 scrubben liefert identische Position).
4. Dolly-Zoom-Preset hält die Motivgröße eines Test-Objekts über die Fahrt sichtbar konstant, Hintergrund-FOV ändert sich.
5. Sequenz speichern/laden über Neustart hinweg verlustfrei (`Sequences\*.json`, `schemaVersion` vorhanden).
6. Handheld-Preset erzeugt einstellbare Amplitude/Frequenz; Amplitude 0 = statisches Bild.

### Phase 4 — Welt/Akteure/Licht & Assistenten (Aufwandsklasse: L)

**Ziel:** Server-authoritative Szenen-Kontrolle: Zeit/Wetter setzen und einfrieren, Darsteller spawnen und posieren, Lichter setzen, Props anbinden sowie Continuity-/Coverage-Assistenten.

**Deliverables:**

| Deliverable | Pfad / Inhalt |
|---|---|
| WorldState | RPC `EPCT_RPC.SetWorldState` (ein RPC, Payload `PCT_WorldState` — Plan_B1_Architektur.md §6) gated mit `"CinematicTool.World"`; Server-seitig: `World.SetDate(...)`, `World.SetTimeMultiplier(0)` (0 = Zeit eingefroren), `Weather.GetOvercast()/GetFog()/GetRain()/... .Set(forecast, 0, langeDauer)` + `Weather.SetWeatherUpdateFreeze(true)` (Research_Vanilla_APIs.md §6). **Entscheidung: eigene RPCs, NICHT Aufruf des COT-Weather-Moduls.** Begründung: eigener Permission-Ast (`CinematicTool.World` statt COT-Weather-Permissions), keine Kopplung an COT-interne Modul-API (nicht als stabile öffentliche Schnittstelle dokumentiert), Snapshot/Restore des Vorzustands bleibt in PCT-Hand. Verworfene Alternative: `GetModuleManager().GetModule(JMWeatherModule)` + `SetFreezeTime`/`SetFog`/`SetStorm` — funktional vorhanden (Research_COT_Infrastructure.md §6), aber doppelte Permission-Pflege und Update-Bruchrisiko |
| WorldState-Restore | Vorzustand (Datum, Multiplier, Phänomene) wird beim Aktivieren gesnapshottet und bei Deaktivierung/Disconnect wiederhergestellt |
| Actors | `Scripts/4_World/PsyernsCinematicTool/Actors/PCT_ActorService.c` (Plan_B1_Architektur.md §3/§4) + Datenklasse `PCT_ActorMark` (3_Game/Data, Plan_B3_Datenmodell_Persistenz.md §2.8) — Spawn/Despawn per Server-RPC (`EPCT_RPC.SpawnActor`/`RemoveActor`/`ActorCommand`) gated `"CinematicTool.Actors"`; Emotes/Posen via `StartCommand_Action(anim, EmoteCB, DayZPlayerConstants.STANCEMASK_ALL)` (verifiziertes CameraTools-Muster, Research_Vanilla_APIs.md §8); Loadout via `GetInventory().CreateAttachment(...)`/`CreateInHands(...)` (CTActor-Muster); Gehen via `OverrideMovementAngle/Speed`. **Spawn-Methode für Dummy-PlayerBase ohne AI/Identity ist OFFEN (O4) — Feature startet hinter einem Testplan, nicht als Release-Zusage** |
| Lights | `Scripts/4_World/PsyernsCinematicTool/Lights/PCT_PointLight.c`/`PCT_SpotLight.c` (Subklassen von `PointLightBase`/`SpotLightBase`). **Architektur:** `ScriptedLightBase`-Lichter sind keine replizierten Netzwerk-Entities — Vanilla 1.29 erzeugt sie ausschließlich client-lokal (`Flashlight.c:15-17`, gated `!g_Game.IsServer() || !g_Game.IsMultiplayer()`); ein rein server-seitig erzeugtes Licht rendert auf Clients nicht. Deshalb: Server validiert `"CinematicTool.Lights"` und broadcastet die Licht-Daten (Typ, Position, Helligkeit, Radius) per RPC an alle Clients; jeder Client erzeugt/aktualisiert/entfernt das Licht lokal via `ScriptedLightBase.CreateLight(...)` (Vanilla-Muster Flashlight.c). Verifizierte API: `CreateLight`, `SetBrightnessTo`, `SetRadiusTo`, `SetFadeOutTime` (Research_Vanilla_APIs.md §12); Farb-API-Surface OFFEN (O5); direkte Server-Spawn-Replikation zusätzlich OFFEN (O10 — das Broadcast-Modell ist unabhängig davon der Default, Plan_B1_Architektur.md §7) |
| Props | `Scripts/4_World/PsyernsCinematicTool/Props/PCT_PropService.c` — eigene Server-RPCs `EPCT_RPC.SpawnProp`/`RemoveProp` gated `"CinematicTool.Props"`; Spawn via `CreateObjectEx` (`ECE_PLACE_ON_SURFACE` u. a.), Registry + Entfernen, Spawn-Typen gegen server-seitige Whitelist (Plan_B1_Architektur.md §3/§6–8 — dort als Sollarchitektur festgelegt). Die vorhandenen COT-Module `JMObjectSpawnerModule`/`JMESPModule` (Research_COT_Infrastructure.md §6) bleiben parallel nutzbar; PCT speichert Platzierungsdaten im Shot |
| Continuity-Assistent | `Scripts/3_Game/PsyernsCinematicTool/PCT_ContinuityChecker.c` (Name gemäß Plan_B2_Feature_Mapping.md §1) — 180°-Linie zwischen zwei markierten Motiven, 30°-Regel zwischen aufeinanderfolgenden Shots (Winkelvergleich der gespeicherten Kamerapositionen), Warnungen als HUD-/Form-Hinweis (Regeln aus Plan_A1.md §10) |
| Coverage-Assistent | `Scripts/3_Game/PsyernsCinematicTool/PCT_Coverage.c` — Vorschlagsliste (Master, Two Shot, OTS A/B, CU A/B, Insert, Cutaway; Plan_A1.md §11) als generierte Shot-Stubs im Shot-Browser |

**Abhängigkeiten:** Phase 0 (RPC/Permissions), Phase 1 (Kamera), Phase 2 (Shot-Datenmodell für Continuity/Coverage). Unabhängig von Phase 3 — kann bei Bedarf parallel zum Sequencer beginnen.

**Definition of Done (nachprüfbar):**
1. Dedizierter Server, 2 Clients: Zeit auf 06:30 setzen + Freeze → beide Clients zeigen dieselbe eingefrorene Zeit; nach Unfreeze läuft die Zeit weiter; Vorzustand wird beim Beenden wiederhergestellt.
2. Wetter (Overcast/Fog/Rain) über PCT gesetzt bleibt mindestens 10 Minuten stabil (kein Forecast-Drift, `SetWeatherUpdateFreeze` wirksam).
3. Client ohne `"CinematicTool.World"` kann Zeit/Wetter nicht ändern (Server-Log zeigt abgewiesenen RPC).
4. Gespawnter Akteur ist auf beiden Clients sichtbar, spielt ein Emote ab und läuft eine Strecke; Ergebnis des O4-Testplans dokumentiert (welche Spawn-Variante stabil ist).
5. Licht: Punktlicht mit Helligkeit/Radius auf beiden Clients sichtbar (je Client lokal aus dem Server-Broadcast erzeugt); Despawn-Broadcast entfernt es auf allen Clients rückstandsfrei.
6. Continuity: Kamera bewusst über die 180°-Linie bewegt → Warnung erscheint; <30°-Differenz zweier Shots → Jump-Cut-Hinweis erscheint.

### Phase 5 — Polish & Release (Aufwandsklasse: M)

**Ziel:** Release-Reife: Lokalisierung, Icons, Dokumentation, Workshop-Veröffentlichung und optionales Sequence-Sharing.

**Deliverables:**

| Deliverable | Pfad / Inhalt |
|---|---|
| Lokalisierung | `#STR_PCT_*`-Keys für alle UI-Texte + Stringtable (EN/DE); Muster: COT `languagecore` |
| Icons | Modul-Icon (`GUI/textures/modules/`, .edds/.paa) + Workshop-Preview-Bild |
| Polish | UI-Feinschliff (Slider-Ranges, Tooltips), Fehlertoleranz beim JSON-Laden, CF-`NotificationSystem`-/COT-Toast-Feedback für alle Aktionen |
| Doku | `README.md` (Installation, Permissions-Liste, Keybinds), `Docs/Permissions.md`, Changelog |
| Workshop-Release | `mod.cpp` (Name, Autor Psyern, Version, Bilder), signierte PBOs, `keys\`-Ordner mit `.bikey`; Versionsschema SemVer (§3) |
| Optional: Sequence-Sharing | Server-seitige Sequenz-Ablage + Broadcast an berechtigte Clients (Expansion-Settings-Muster "load on server, push via RPC", Research_Framework_Patterns.md §3) — **nur wenn O3 (Payload-Limit) positiv verifiziert; sonst Chunking oder Streichung** |

**Abhängigkeiten:** Phasen 0–4 (Feature-Freeze vor Polish).

**Definition of Done (nachprüfbar):**
1. Frische Workshop-Installation (ohne Dev-Umgebung) auf einem Testserver: Mod lädt, alle Phase-1–4-DoD-Stichproben bestehen.
2. Keine untranslatierten Roh-Keys im UI (Sichtprüfung aller Panels in EN und DE).
3. Signaturprüfung: Client mit verifySignatures=2 verbindet erfolgreich; manipulierte PBO wird abgewiesen.
4. README deckt Installation, alle 7 Permissions und alle `UAPCT_*`-Actions ab.

---

## 2. Teststrategie

### 2.1 Test-Stufen

| Stufe | Umgebung | Was wird geprüft |
|---|---|---|
| T1 Compile/Smoke | Workbench bzw. Spielstart mit `-mod` und Diag-/Script-Log-Kontrolle | Compile-Fehler, Override-Warnungen, Modul-Registrierung, Layout-Ladbarkeit |
| T2 Offline-Funktional | Offline-Mission mit CF+COT+PCT (COT-Mission-Vorlage `Missions\COT.ChernarusPlus` als Muster) | Gesamte Client-Funktionalität: Kamera, Rig, HUD, Shots, Sequencer, Exporte |
| T3 Multiplayer | Dedizierter Testserver + 2 Clients | Permission-Gates, Replikation, Spectator-Body-Verhalten, WorldState, Actors, Lights |
| T4 Release-Kandidat | Frischer Server, Workshop-Paket, verifySignatures=2 | Signierung, Distribution, Clean-Install-Verhalten |

**Wichtiger Beschleuniger — COT-Offline-Pfad:** Im Offline-/Host-Betrieb gilt `IsMissionHost()` auf dem Client; COTs `Enter()` ruft dann `Server_Enter(...)` **direkt** auf (ohne RPC-Roundtrip) und `HasPermission(...)` liefert offline `true` (Research_COT_Infrastructure.md §1/§3). Dadurch lassen sich Kamera, Rig, HUD, Shots und Sequencer vollständig ohne Server testen — der schnelle innere Entwicklungszyklus für die Phasen 1–3. Permission- und Replikationstests sind offline prinzipbedingt wirkungslos und gehören zwingend in T3.

### 2.2 Multiplayer-Pflichtprüfungen (T3, ab Phase 1 wiederkehrend)

1. **Permission-Gates:** Jede neue Server-RPC einmal mit und einmal ohne zugehörige Permission auslösen (zweiter Client ohne Permission); Erwartung: kommentarloses Abweisen serverseitig, kein Effekt beim Client.
2. **Replikation:** Zustandsänderungen (Wetter, Zeit, Akteure, Lichter) auf beiden Clients gegeneinander verifizieren.
3. **Spectator-Body:** Beim Kamera-Enter bleibt der Spielerkörper in der Welt (COT-Verhalten: `SelectPlayer(sender, NULL)` + `SelectSpectator`); Godmode-Automatik und Positions-Streaming (`UpdatePosition`-Mechanik) beobachten; nach Leave volle Kontrolle und korrekte Rückposition.
4. **Disconnect-Härte:** Client trennt während aktiver Kamera/Sequenz → Server räumt auf (Body-Reattach, WorldState-Restore).

### 2.3 Checkliste je Phase

| Phase | Pflicht-Checks vor Abschluss |
|---|---|
| 0 | T1 vollständig; T3-Minimalprobe (Modul-Sichtbarkeit mit/ohne Permission) |
| 1 | T2: Enter/Leave-Zyklen, Rig-Mathe-Stichprobe, DOF-Reset; T3: Punkte 1, 3, 4 aus §2.2 |
| 2 | T2: Save/Load-Roundtrip über Neustart, Export-Dateien öffnen; Schema-Fehlerfall (defekte JSON) |
| 3 | T2: Playback-Determinismus, Scrub, alle 5 Bewegungs-Presets; Langzeit-Playback 5 min (Leak-/GC-Beobachtung) |
| 4 | T3 vollständig (§2.2 Punkte 1–4) für WorldState/Actors/Lights; Continuity-Warnungen provozieren |
| 5 | T4 vollständig; Regressions-Stichprobe aller vorherigen Phasen-DoDs |

---

## 3. Paketierung & Release

**Ordner → PBO:** Das Paket `PsyernsCinematicTool/PsyernsCinematicTool/` wird als zwei PBOs gebaut — `Scripts/` → `PCT_Scripts.pbo`, `GUI/` → `PCT_GUI.pbo` (je ein `config.cpp` pro PBO; Muster: COTs Aufteilung `JM_COT_Scripts`/`JM_COT_GUI`). Werkzeuge: **Addon Builder** (DayZ Tools, CLI-fähig → Basis für `Tools/Build_PCT.ps1`), alternativ **pboProject** oder **Workbench**-Packen; das Build-Skript aus Phase 0 kapselt den gewählten Packer, damit Builds reproduzierbar sind.

**Prefix:** PBO-Prefix = Pfad im Spiel, also `PsyernsCinematicTool\Scripts` bzw. `PsyernsCinematicTool\GUI` — muss exakt zu den Pfaden in `CfgMods`-`defs` und `GetLayoutRoot()` passen; Workdrive-Layout (`P:\PsyernsCinematicTool\...`) spiegelt das Repo-Paket.

**Signieren:** Pro PBO eine `.bisign` (erzeugt mit DSUtils/DSSignFile gegen den privaten Psyern-Key), die neben der PBO in `addons\` liegt. Der öffentliche Key (`.bikey`) wird im Mod-Ordner `keys\` mitgeliefert, damit Serverbetreiber ihn installieren können. Der private Key verlässt niemals den Build-Rechner und liegt nicht im Repo.

**Distribution:** **`-mod`, NICHT `-serverMod`.** Begründung: PCT enthält Client-Content (Layouts, HUD, Kamera-Klasse im Client-Kontext, `Inputs.xml`) — `-serverMod` würde diesen Content den Clients vorenthalten; Kamera und UI liefen nicht. Clients und Server laden das identische Paket; die Autorität liegt trotzdem serverseitig über die Permission-gegateten RPCs.

**Workshop-Metadaten:** `mod.cpp` mit `name = "Psyerns Cinematic Tool"`, Autor Psyern, Versions-String, Logo/Overview-Bild; Workshop-Beschreibung listet Abhängigkeiten (CF, COT) und verweist auf die Permissions-Doku. Abhängigkeiten werden im Workshop als "Required Items" (CF, COT) verlinkt.

**Versionsschema:** SemVer `MAJOR.MINOR.PATCH` — MAJOR: Bruch an JSON-Schema oder Permissions; MINOR: neue Features/Phasen; PATCH: Fixes. Die Version steht dreifach synchron: `mod.cpp`, `PCT_Constants.c` (`PCT_VERSION`) und im Changelog; JSON-Dateien tragen unabhängig davon ihr eigenes `schemaVersion` (Migrationspfad siehe Plan_B3_Datenmodell_Persistenz.md).

---

## 4. Risiko-Register

| # | Risiko | Wahrscheinlichkeit | Auswirkung | Gegenmaßnahme |
|---|---|---|---|---|
| R1 | `Camera.SetFOV`-Grenzen unklar: User-Option-Clamp (0.752–1.303 rad) gilt nur fürs Setting; ob `SetFOV` selbst clamped, ist aus Script nicht ablesbar (COT-Slider erlaubt 0.001–4.0) | mittel | mittel — extreme Brennweiten (14 mm / 200 mm) evtl. nicht darstellbar; Rig-Mathe liefe ins Leere | O1 früh in Phase 1 testen; Rig clamped selbst auf empirisch verifizierten Bereich; UI zeigt "außerhalb darstellbar"-Hinweis |
| R2 | COT-Basisklassen stammen aus CF-`Deprecated`-Ordner (`JMModuleBase` via `Module\Deprecated\`) und `PermissionsOld` — ein CF-/COT-Update kann die Vererbungskette brechen | mittel | hoch — Modul-, RPC- und Permission-System betroffen | Alle COT-Berührungspunkte in wenigen Klassen kapseln (`PCT_CinematicModule`, `PCT_CinematicCamera`, Form); COT-Version im Workshop pinnen/dokumentieren; nach jedem COT-Release T1+T3-Smoke-Test; Abstraktionsschicht erst bauen, wenn ein Bruch real eintritt (YAGNI) |
| R3 | RPC-Basis-Kollision mit anderen COT-Addons, die ebenfalls >10460 wählen | mittel | hoch — stille Fehlzustellung oder Fremd-Dispatch (Module filtern nur über Min/Max-Range) | Basis 10500 im README und Workshop dokumentieren; Enum kompakt halten (kleine Range); optional ab Phase 5: Basis über `Settings.json` konfigurierbar machen (Offset zur Enum-Basis beim `GetRPCMin/Max`-Report) — Entscheidung nach Feldlage |
| R4 | JSON-Größe beim Sequence-Broadcast: lange Sequenzen (viele Keyframes) könnten `ScriptRPC`-Payload-Grenzen überschreiten (Limit nicht dokumentiert) | mittel | mittel — Sequence-Sharing (Phase 5, optional) scheitert oder korrumpiert | O3 vor Phase-5-Beginn messen; Chunked-Transfer (Sequenz in Keyframe-Blöcken) als Design-Fallback; Feature ist als "optional" deklariert und kann gestrichen werden |
| R5 | Performance: PPE-Overrides (DOF, Vignette) + mehrere HUD-Overlays + Sequencer-Update pro Frame drücken die Client-FPS | mittel | mittel — Tool unbrauchbar auf schwachen Clients, Ruckeln in Aufnahmen | Overlays einzeln abschaltbar (Phase 1 DoD); keine Allokationen im `OnUpdate`-Pfad (Projektregel); PPE nur bei aktiver Kamera; Langzeit-Playback-Messung in Phase-3-Checkliste |
| R6 | Input-Konflikte: `UAPCT_*`-Defaults kollidieren mit anderen Mods oder COT-Binds (COT belegt u. a. `kInsert`, `kRMenu+kY`, `kU`, `kH`, `kDelete`) | hoch | niedrig — Doppelbelegung nervt, ist aber vom User umbindbar | Defaults bewusst auf unbelegte Kombinationen legen bzw. Actions teilweise ohne Default ausliefern; alle Binds im README; alles über Spiel-Optionen umbindbar |
| R7 | `MakeScreenshot` liefert DDS — Workflow-Reibung für Storyboards/Sharing (kein PNG/JPG aus Engine) | hoch (sicher) | niedrig — Zusatzschritt, kein Funktionsverlust | UI-Hinweis + README-Abschnitt mit Konverter-Empfehlung; Export-Ordnerstruktur so benennen, dass Batch-Konvertierung trivial ist; keine eigene Konverter-Entwicklung |
| R8 | Multiplayer-Sichtbarkeit gespawnter Akteure: Darsteller müssen server-seitig gespawnt werden; wie eine Dummy-`PlayerBase` ohne AI/Identity server-seitig **stabil** existiert (kein Despawn, kein AI-Verhalten, keine CE-Interferenz), ist unverifiziert — Vanilla-CameraTools arbeitet lokal/Debug | hoch | hoch — Actors-Feature (Phase 4) im MP evtl. nur eingeschränkt möglich | O4 als dedizierter Testplan **vor** Implementierungsbeginn Phase 4 (Spike); Fallback-Stufen: (a) Akteure nur host-/offline (Previz-Solo-Workflow), (b) statische Mannequin-Objekte statt animierter PlayerBase, (c) Feature-Cut mit Doku |
| R9 | DayZ-1.29+-Breaking-Changes (Deprecation von `GetGame()`, unzuverlässige `IsClient/IsServer`) und künftige Engine-Updates | niedrig | mittel | Projektregeln bereits konform (`g_Game`, nur `IsDedicatedServer()` für Authority); `DayZGame`-Breaking-Change-Notizen (`DayZGame/DayZ-1.29.161219.md`) bei jedem Gameupdate prüfen; API-Aufrufe zentralisiert in PCT_Math/Rig/Module statt verstreut |

---

## 5. Offene Fragen (konsolidiert aus B1–B5)

Alle Punkte sind "OFFEN — ZU VERIFIZIEREN". Kein Feature, das von einem offenen Punkt abhängt, gilt als zugesagt, bevor der Verifikationsschritt dokumentiert bestanden ist.

| ID | Frage | Verifikationsschritt | Blockierte Features | Zielphase |
|---|---|---|---|---|
| O1 | Clamped `Camera.SetFOV()` intern auf die User-Option-Grenzen (0.752–1.303 rad) oder akzeptiert es den vollen COT-Slider-Bereich (0.001–4.0)? | Offline-Test in Phase 1: FOV-Werte 0.1/0.3/1.5/2.5 rad setzen, `GetCurrentFOV()` zurücklesen und Bild vergleichen; Grenzwerte protokollieren | Extreme Brennweiten (14 mm, 135–200 mm) im Rig; Dolly-Zoom-Spannweite | 1 |
| O2 | Existiert eine nutzbare BoundingBox-API auf `Object`/`EntityAI` (z. B. `GetCollisionBox`/ClippingInfo) zur Messung der Motivhöhe für Auto-Framing, und liefert sie für `PlayerBase` plausible Werte? | Quellcheck in `scripts - 1.29` (`Object.c`/`EnEntity.c`) auf exakte Signatur; danach Offline-Test: Höhe eines PlayerBase in stehend/hockend messen und loggen | Framing-Auto-Positionierung mit echter Motivhöhe (Phase 2) — Fallback: konfigurierte Standardhöhe | 2 |
| O3 | Wie groß darf ein `ScriptRPC`-Payload praktisch werden (JSON-String einer langen Sequenz, z. B. 100 Keyframes)? | T3-Messreihe vor Phase 5: Sequenz-JSON in Stufen 8/32/64/128 KB als String senden, Empfang integritätsgeprüft (Länge + Hash-Feld) | Sequence-Sharing (Phase 5, optional); Chunking-Design | 5 (Messung spätestens Ende Phase 4) |
| O4 | Wie lässt sich eine Dummy-`PlayerBase` (Darsteller) server-seitig stabil ohne AI/Identity spawnen — Klasse (`SurvivorM_*`?), `CreateObject`-Variante/ECE-Flags, Verhalten von CE/Cleanup, Unterdrückung von AI-Logik? | Dedizierter Spike vor Phase 4 (Testplan): (1) `g_Game.CreateObject("SurvivorM_Mirek", pos, false)` serverseitig, (2) `CreateObjectEx` mit `ECE_LOCAL`-freien Flag-Kombinationen (`ECE_PLACE_ON_SURFACE`, ohne `ECE_INITAI`), je: Sichtbarkeit auf 2 Clients, Lebensdauer 10 min, Emote via `StartCommand_Action`, Serverlog auf Fehler | Gesamtes Actors-Feature im Multiplayer (Phase 4); Continuity-Checks mit echten Akteuren | 4 (Spike als erstes Arbeitspaket der Phase) |
| O5 | Welche Farb-/Schatten-API bietet `EntityLightSource` konkret aus Script (SetColorRGB/SetCastShadow o. ä. — Namen und Signaturen unbestätigt)? | Quellcheck `1_Core`-Proto (`EnEntity.c`/EntityLightSource) + Gegenprobe an Vanilla-Subklassen (z. B. `FlashlightLight`); danach Offline-Test: Farbe/Schatten an `PCT_PointLight` setzen | Licht-Farbsteuerung, Gels/Farbtemperatur-Presets (Phase 4) — Helligkeit/Radius sind bereits verifiziert | 4 |
| O6 | Koexistenz `staticcamera` vs. `SelectSpectator`: Kann der Sequencer (Vanilla-Muster: zwei lokale `staticcamera`-Objekte + `InterpolateTo`) genutzt werden, während der COT-Spectator-Flow (`SelectSpectator` mit `PCT_CinematicCamera`) aktiv ist — oder kollidieren `Camera.GetCurrentCamera()`/Aktivierungs-Zustände? | Offline-Test in Phase 3: bei aktiver PCT-Kamera zusätzlich `staticcamera` lokal erzeugen, `SetActive`-Wechsel und `InterpolateTo` fahren, danach sauber zur PCT-Kamera zurück; Zustände von `CurrentActiveCamera` loggen | Entscheidung Playback-Backend des Sequencers: eigene Kanal-Auswertung auf `PCT_CinematicCamera` (Plan A) vs. `InterpolateTo`-Nutzung (Plan B). Plan A ist unabhängig von O6 umsetzbar und daher Default | 3 |
| O7 | Funktioniert `g_Game.SelectSpectator(sender, "PCT_CinematicCamera", position)` mit **eigener** Kameraklasse? COT belegt den Aufruf nur mit `"JMCinematicCamera"` (Plan_B1 #1) | Phase-0/1-Skeleton auf Dedicated-Server: Enter-RPC senden, auf dem Client `Camera.GetCurrentCamera()` casten und `ClassName()` loggen; Steuerbarkeit der Kamera prüfen | Gesamter eigener Enter-/Leave-Pfad (Phase 1) | 0/1 |
| O8 | Zweiter `SelectSpectator`-Aufruf während aktiver Spectator-Session — direkte Übernahme COT→PCT ohne vorheriges `Leave` (Plan_B1 #2) | Testserver: COT-Freecam aktivieren, dann PCT-`Server_Enter` ohne Handoff ausführen; Verhalten/Fehler loggen. Bis zur Verifikation gilt Koexistenz-Regel K2 (sequenzieller Handoff) | Vereinfachter Kamera-Handoff (K2 bleibt Default) | 1 |
| O9 | Auto-Serialisierung verschachtelter Datenklassen über `ScriptRPC`/`ctx.Read(Param1<PCT_WorldState>)` (Plan_B1 #4) | Phase-0-Test-RPC mit Dummy-Datenklasse Server↔Client (alle Feldtypen); bei Fehlschlag feldweises `OnSend/OnRecieve` nach Expansion-Muster | Payload-Design aller Daten-RPCs (`SetWorldState`, `SpawnActor`, `SpawnLight`, …) | 0 |
| O10 | Repliziert ein rein server-seitiger `ScriptedLightBase`-Spawn zu den Clients, oder ist Broadcast + lokaler Client-Spawn zwingend? (Plan_B1 #5) | Testserver mit 2 Clients: Server-Spawn vs. Broadcast-Spawn vergleichen (Sichtbarkeit beim zweiten Client) | Bestätigung der Licht-Architektur (Broadcast-Modell ist Default, Phase 4) | 4 |
| O11 | `MakeScreenshot`-Details: exakter DDS-Pfad/`$`-Präfix, automatische `.dds`-Endung, UI/HUD im Bild, Ausführungszeitpunkt (Plan_B1 #6, Plan_B3 §8.3) | Offline: Aufruf mit `$`-Pfad bei offenem und verstecktem Menü, Ergebnisdateien prüfen; falls UI im Bild: Screenshot einen Frame nach `Hide()` via `CallLater` (CallQueue-Null-Check) | Shot-Thumbnails, Storyboard-Frames (Phase 2) | 2 |
| O12 | Externe Mod-Inputs (`UAPCT_*` aus eigener `Inputs.xml`) in Kombination mit CF-`JMModuleBinding` — COT bindet nur eigene UA-Namen (Plan_B1 #7) | Phase 0: `UAPCT_ToggleMenu` deklarieren, binden, Log im Handler; Taste im Spiel drücken | Alle Keybinds (Phase 0+) | 0 |
| O13 | Wechselwirkung PCT↔COT auf den `PlayerBase`-COT-Feldern (`m_JM_CameraPosition`, Godmode-Restore, one-shot-Flag) bei schnellen Wechseln PCT-Kamera ↔ COT-Freecam (Plan_B1 #8) | Testserver: Zyklen PCT-Enter→Leave→COT-Enter→Leave in beiden Reihenfolgen; Godmode-/Positionszustand und `OnSelectPlayer`-Verhalten nach jedem Schritt loggen | Koexistenz-Regeln K1–K5 (Phase 1) | 1 |
| O14 | Nutzbarkeit anderer COT-Module (ESP-Objektmanipulation, Teleport-an-Cursor) während aktiver PCT-Kamera bei **nicht** gesetztem `CurrentActiveCamera` (Plan_B1 #9) | Testserver: PCT-Kamera aktiv, ESP-Selektion und `UATeleportModuleTeleportCursor` testen; ggf. dokumentierte Einschränkung statt Workaround | Props-Anbindung über COT-Module, Teleport-Komfort (Phase 4) | 4 |
| O15 | `PPEColorGrading`-PARAM_*-Parameter für Weißabgleich-/Tint-Näherung (Plan_B2 O4) | `PPEManager\Materials\MatClasses\PPEColorGrading.c` PARAM_*-Liste auslesen; Test-Requester bauen | Weißabgleich-/Tint-Ersatz (Phase 4) | 4 |
| O16 | 3D-Linien-/Debug-Shape-API für Pfad-/Frustum-Visualisierung (Plan_B2 O5) | `1_Core`-Protos (Debug/Shape) sichten; falls vorhanden Testlinie zeichnen, sonst Widget-Marker-Fallback festschreiben | Sichtbarer Kamerapfad, Frustum-Anzeige (Phase 3/4) | 3 |
| O17 | Wertebereich/Optik von `PPEGlow` `PARAM_LENSDISTORT` (Plan_B2 O7) | Parameter über Requester in Stufen setzen, Bildwirkung dokumentieren | Lens-Distortion-Preview (Phase 4) | 4 |
| O18 | In-Game-Anzeige von DDS-Screenshots im `ImageWidget` (Plan_B2 O8, Plan_B5 §6) | `ImageWidget.LoadImageFile` mit `$profile:`-DDS-Pfad probieren | Thumbnail-Vorschau im Shot-Browser (Phase 5; bis dahin nur Datei-Export) | 5 |
| O19 | Far-Plane-Steuerung: wirkt `World.SetCameraFarPlane(slot, ...)` auf die aktive Script-Kamera? (Plan_B2 O9) | In aktiver Freecam Far Plane über Slot-API ändern, Sichtweite prüfen | Far-Clipping-Steuerung im Rig (Phase 2) | 2 |
| O20 | `JsonFileLoader`-Robustheit: Verhalten bei defektem JSON, unbekannten Keys und `null`-Objekt-Membern im Output (Plan_B3 §3/§4.5/§5.1) | Offline-Round-Trip-Tests T-PER-1..3 und T-MIG-1 (Plan_B3 §4.5): gekürzte, beschädigte und um Fantasie-Keys ergänzte Shot-Dateien laden, Verhalten protokollieren | Persistenz-Fehlerbehandlung, Migrationsregeln M2/M3 (Phase 1/2) | 1 |
| O21 | Verhalten von `FindFile` bei leerem Verzeichnis — Handle-Gültigkeit, Inhalt von `fileName` (Plan_B3 §4.4) | Aufruf gegen ein leeres `Shots\`-Verzeichnis im Offline-Spiel, Rückgaben loggen; Guard ggf. anpassen | Shot-/Sequenz-Browser (Phase 2) | 2 |
| O22 | CSV-Encoding: Umlaute/Sonderzeichen im `FPrintln`-Output vs. Excel-Erwartung (Plan_B3 §8.1, Test T-EXP-2) | CSV mit „ÄÖÜß"-Testzeile exportieren, in deutschem Excel und VS Code öffnen; ggf. ASCII-normalisieren | CSV-Export (Phase 2/5) | 2 |
| O23 | Echtzeit-Systemuhr-API für `exportedAt` (`GetYearMonthDay`/`GetHourMinuteSecond` in `EnSystem.c` unverifiziert) (Plan_B3 §8.2) | `EnSystem.c` in `scripts - 1.29` auf die Protos greppen, Testaufruf; bis dahin bleibt `exportedAt` leer | `exportedAt`-Feld im ShotList-Export (Phase 3/5) | 3 |
| O24 | Wirksame Skala des DOF-`blur`-Parameters (Vanilla-Default 1.0 vs. COT-Slider 0–100) und Kalibrierung des f-Stop-Mappings (Plan_B4 V2, Plan_B5 §2.1) | Feste Szene (Ziel 3 m, Hintergrund 50 m), `blur` in Stufen 0.25–100 setzen, Screenshots vergleichen; `s_PCT_BlurGain` und Tabellen-Normierung kalibrieren | DOF-Heuristik Blende→blur (Phase 1/2) | 1 |
| O25 | `Math3D.Curve`-Endpunktverhalten bei `CatmullRom` (Phantom-Endpunkte?) und indexuniforme Parameterverteilung (Plan_B4 V4) | Vier bekannte Punkte übergeben; `Curve(..., 0.0/1.0/⅓, pts)` per `Print` ausgeben; ggf. ersten/letzten Keyframe duplizieren | Positions-Spline des Sequencer-Playbacks (Phase 3) | 3 |
| O26 | Zugriffsweg für rotatorischen Motion-Blur (`PPERotBlur`/`PPEDynamicBlur` — kein verifizierter Setter) (Plan_B4 V5) | Über `PPERequesterBank`/Materialparameter einen RotBlur-Parameter setzen, Bildwirkung prüfen; sonst `SetBlur`-Puls als Lösung | Whip-Pan-Blur (Phase 3) | 3 |
| O27 | Film-Grain-Zugriff: eigener `PPERequesterBase`-Requester auf `PPEFilmGrain` vs. `GetMaterial("…/filmgrainNV")` + `SetParam` (Plan_B4 V6) | Beide Wege in einer Diag-Funktion durchprobieren, Parameternamen aus der Materialklasse ablesen, Wirkung dokumentieren | Film-Grain-Look pro Shot (Phase 2+) | 2 |
| O28 | HUD-/Overlay-Layouts: Input-Transparenz im Freecam-Betrieb (stiehlt das Workspace-Layout Fokus?) (Plan_B5 §3) | HUD einblenden, WASD/Maus/Scroll in der Freecam prüfen | Kamera-HUD (Phase 1) | 1 |
| O29 | Verhalten von `GetScreenPosRelative` bei Punkt **hinter** der Kamera (Marker-Ausreißer) (Plan_B5 §4) | Marker-Ziel hinter die Kamera bringen, `.z` und x/y loggen; Marker bei `.z <= 0` verstecken | Augenlinien-/Pfad-/Keyframe-Marker (Phase 2–4) | 2 |
| O30 | `CanvasWidget`-API (Zeichnen, Maus-Events) für die Canvas-Timeline — in keinem Research-Dokument belegt (Plan_B5 §5) | Minimal-Layout mit `CanvasWidget`, Linien/Rechtecke zeichnen, Maus-Events prüfen | Canvas-Timeline (Phase 5, Stretch; Keyframe-Liste ist der Default) | 5 |
| O31 | Key-Identifier `kRControl` in `Inputs.xml` (plausibel, aber unbelegt) (Plan_B5 §7) | Preset-Bind eintragen, in-game unter Optionen→Steuerung prüfen, Tastendruck testen; Fallback `kLControl`-Chords | Default-Keybinds `kRControl`+X (Phase 1+) | 1 |
| O32 | Wird die `stringtable.csv` aus dem `PCT_GUI`-PBO geladen? (Plan_B5 §10) | `#STR_PCT_*`-Key im HUD anzeigen; erscheint der Key-Name, Fallback eigenes Mini-Addon `PCT_LanguageCore` nach COT-Vorbild | Lokalisierung (Phase 5) | 5 |

---

## 6. Nächste konkrete Schritte (unmittelbar nach Plan-Freigabe)

1. **Skeleton anlegen:** Ordner `PsyernsCinematicTool/PsyernsCinematicTool/` mit `Scripts/{config.cpp, Data/Inputs.xml, 3_Game/PsyernsCinematicTool/, 4_World/PsyernsCinematicTool/, 5_Mission/PsyernsCinematicTool/}` und `GUI/{config.cpp, layouts/, textures/}` erstellen; vorhandenen leeren Ordner `scripts` zu `Scripts` umbenennen.
2. **Workdrive-Setup:** `P:\`-Workdrive einrichten bzw. prüfen; CF- und COT-Pakete verfügbar machen (lokale Repos `DayZ-CommunityFramework-production`, `DayZ-CommunityOnlineTools-production` als Quellen); Junction/Copy des PCT-Pakets nach `P:\PsyernsCinematicTool`.
3. **Phase-0-Implementierung:** `config.cpp` (beide), `PCT_RPC.c`, `PCT_Constants.c`, `PCT_CinematicModule` (leer, Permission `"CinematicTool.View"` + Registrierung aller 7 Permissions im Konstruktor), `modded class JMModuleConstructor`, Form- und Layout-Stub gemäß §1/Phase 0.
4. **Build-Skript:** `Tools/Build_PCT.ps1` gegen Addon Builder CLI aufsetzen (Pack + Sign + Deploy in Test-Mod-Ordner); ersten Build erzeugen.
5. **Erst-Test im Offline-Modus:** Offline-Mission mit CF+COT+PCT starten (T1/T2-Minimal): `script.log` leer, COT-Menüeintrag "Cinematic Tool" sichtbar, Fenster öffnet; Ergebnis als Phase-0-DoD-Protokoll in `Docs/` ablegen.
6. **O1 vorziehen:** Direkt nach Phase-0-Abschluss den SetFOV-Grenztest (§5, O1) als erstes Phase-1-Arbeitspaket durchführen — das Ergebnis bestimmt die Rig-Clamp-Konstanten.

---

## Querverweise

- **Stoffsammlung / Anforderungen:** Plan_A1.md
- **Geschwisterdokumente (Planung):**
  - Plan_B1_Architektur.md — Paketstruktur, Modul-/RPC-/Permission-Architektur, Enter/Leave, WorldState/Actors/Lights/Props-Autoritätsmodell
  - Plan_B2_Feature_Mapping.md — Feature-Mapping Plan_A1 → DayZ-Machbarkeit (Phaseneinstufung, Ersatzstrategien)
  - Plan_B3_Datenmodell_Persistenz.md — JSON-Datenklassen, schemaVersion, Ordnerlayout `$profile:PsyernsCinematicTool\`
  - Plan_B4_Kamera_Engine.md — Kamera-Kern, Rig-Mathematik, DOF, Keyframe-Modell, Playback, Bewegungs-Presets
  - Plan_B5_UI_UX.md — Formulare, HUD/Overlays, Sequencer-UI, Keybinds
- **Research (verifizierte Fakten):**
  - Docs/Research/Research_COT_Infrastructure.md — COT-Modul-System, RPC, Permissions, Kamera-Flow
  - Docs/Research/Research_Vanilla_APIs.md — Vanilla-1.29-APIs (Camera, PPE/DOF, Splines, Weather, Lights, Screenshots)
  - Docs/Research/Research_Framework_Patterns.md — Framework-Patterns, Abhängigkeits-Entscheidung (nur CF+COT)
