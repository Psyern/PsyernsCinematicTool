# PsyernsCinematicTool (PCT) — Planungsübersicht

Stand: 2026-07-21 · Status: Planungsphase abgeschlossen (Entwurf v1) · Repo: https://github.com/Psyern/PsyernsCinematicTool

PsyernsCinematicTool ist ein Cinematic-/Previsualisierungs-Tool für DayZ auf Basis der Kamera- und Modul-Infrastruktur von **Community Online Tools (COT)** und **Community Framework (CF)**. Es erweitert die COT-Freecam zu einer filmreifen Kinokamera: Keyframe-Sequencer, fotografisches Kameramodell (Sensor/Brennweite/Blende), Shot-Presets, Kompositions-Overlays, Welt-Steuerung (Zeit/Wetter), Akteure, Licht und Continuity-Assistenten.

## Dokumentensatz (Lesereihenfolge)

| Dokument | Inhalt |
|---|---|
| [Plan_A1.md](Plan_A1.md) | Stoff- und Anforderungssammlung (fachliches Anforderungsprofil, filmisches Vokabular) |
| [Docs/Plan_B1_Architektur.md](Docs/Plan_B1_Architektur.md) | Systemarchitektur: Paketstruktur, config.cpp, Schichten, COT-Integration, RPC-Design, Autorität/Replikation, Sicherheitsmodell |
| [Docs/Plan_B2_Feature_Mapping.md](Docs/Plan_B2_Feature_Mapping.md) | Mapping aller 17 Plan_A1-Abschnitte auf DayZ-Machbarkeit (VOLL/TEILWEISE/ERSATZ/NICHT) mit APIs und Phasen |
| [Docs/Plan_B3_Datenmodell_Persistenz.md](Docs/Plan_B3_Datenmodell_Persistenz.md) | Datenklassen, JSON-Schemata, `$profile:`-Persistenz, Schema-Versionierung, Validierung, Export-Formate |
| [Docs/Plan_B4_Kamera_Engine.md](Docs/Plan_B4_Kamera_Engine.md) | Kamera-Engine: Zustandsmodell, Sensor→FOV-Mathe, DOF-Pipeline, Keyframes/Catmull-Rom, Bewegungs-Presets, Auto-Framing, Continuity-Geometrie |
| [Docs/Plan_B5_UI_UX.md](Docs/Plan_B5_UI_UX.md) | UI/UX: COT-Form (Tabs), Kamera-HUD, Kompositions-Overlays, Sequencer-UI, Keybinds, UX-Flüsse, Layout-Dateien |
| [Docs/Plan_B6_Roadmap_Risiken.md](Docs/Plan_B6_Roadmap_Risiken.md) | Phasenplan 0–5 mit DoD, Teststrategie, Paketierung/Release, Risiko-Register, konsolidierte OFFEN-Punkte |

## Verifizierte Referenz-Recherche (Grundlage aller API-Aussagen)

| Dokument | Inhalt |
|---|---|
| [Docs/Research/Research_COT_Infrastructure.md](Docs/Research/Research_COT_Infrastructure.md) | COT intern: Kamera-Flow (Enter/Leave/SelectSpectator), Modul-System, Permissions, RPC-Muster, Inputs, Ordnerlayout |
| [Docs/Research/Research_Vanilla_APIs.md](Docs/Research/Research_Vanilla_APIs.md) | Vanilla 1.29: `Camera`-Klasse, PPE/DOF, `Math3D.Curve`, Screen-Projektion, Weather/Zeit, Spawning, Actor-Kommandos, Lights, `MakeScreenshot` |
| [Docs/Research/Research_Framework_Patterns.md](Docs/Research/Research_Framework_Patterns.md) | Wiederverwendbare Patterns aus Expansion/CF/Dabs (nur als Vorlage — keine Abhängigkeit) |

## Kernentscheidungen (Kurzfassung)

1. **Abhängigkeiten:** nur `DZ_Data`, `JM_CF_Scripts`, `JM_COT_Scripts`, `JM_COT_GUI`. Expansion/Dabs sind reine Pattern-Referenzen; benötigte Utilities werden in `PCT_Math` nachimplementiert.
2. **Eigener Enter-/Leave-Pfad** mit `PCT_CinematicCamera : JMCinematicCamera`, gespawnt über `g_Game.SelectSpectator` — die aktive COT-Freecam wird nicht ferngesteuert (COTs `OnUpdate` überschreibt FOV/DOF jeden Frame). Koexistenz-Regeln K1–K5 in Plan_B1 §5.
3. **RPC-Basis 10500** (`enum EPCT_RPC { INVALID = 10500, … COUNT }`), jeder Server-Handler prüft COT-Permissions (`CinematicTool.*`) erneut.
4. **Fotografisches Kameramodell:** Sensor + Brennweite sind Primärwerte, `fovV = 2·atan(sensorHeight/(2·f))` (vertikal, Radiant); Speicherung in Grad, Umrechnung erst bei `SetFOV`.
5. **Persistenz:** `JsonFileLoader<T>` nach `$profile:PsyernsCinematicTool\` (Shots/Sequences/Presets/Exports + Settings.json), jede Datei mit `schemaVersion`.
6. **UI:** COT-nativ (`JMRenderableModuleBase` + `JMFormBase` + `UIActionManager`); Overlays als Client-Widgets, runde Vignette via `AddPPMask`.
7. **Phasen:** 0 Skeleton · 1 Kamera-Kern · 2 Shots & Presets · 3 Sequencer · 4 Welt/Akteure/Licht & Assistenten · 5 Polish & Release.

## Qualitätssicherung des Plans

Die sechs Planungsdokumente wurden in einem Multi-Agent-Workflow erstellt: parallele Autoren → adversarialer Review je Dokument (erfundene APIs, EnScript-Regelverstöße, Konventions-/Architektur-Widersprüche; 38 Befunde, alle gegen Quellcode verifiziert und eingearbeitet) → Cross-Dokument-Konsistenz-Pass. Nicht belegte Annahmen sind durchgängig als **OFFEN — ZU VERIFIZIEREN** markiert und in Plan_B6 §5 konsolidiert.

## Nächster Schritt

Phase 0 gemäß [Docs/Plan_B6_Roadmap_Risiken.md](Docs/Plan_B6_Roadmap_Risiken.md): Mod-Skeleton unter `PsyernsCinematicTool/` (Scripts/config.cpp, GUI/config.cpp, leeres Modul im COT-Menü, Permission registriert) und Erst-Test im Offline-Modus.
