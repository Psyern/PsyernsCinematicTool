# Phase 0 — DoD-Protokoll (Skeleton)

**Stand:** 2026-07-21 · **Status:** Implementierung abgeschlossen, In-Game-Abnahme ausstehend · **Bezug:** Plan_B6_Roadmap_Risiken.md §1 Phase 0

## 1. Deliverables — Status

| Deliverable | Pfad | Status |
|---|---|---|
| Paket-Skeleton | `PsyernsCinematicTool/PsyernsCinematicTool/` mit `Scripts/` + `GUI/` | ✅ erstellt |
| Scripts-config | `Scripts/config.cpp` (CfgPatches `PCT_Scripts`, CfgMods `PsyernsCinematicTool`, defs 3_Game/4_World/5_Mission, inputs) | ✅ wörtlich nach Plan_B1 §3.1 |
| GUI-config | `GUI/config.cpp` (CfgPatches `PCT_GUI`) | ✅ wörtlich nach Plan_B1 §3.2 |
| Input-Stub | `Scripts/Data/Inputs.xml` — `UAPCT_ToggleMenu` ohne Default-Bind (R6) | ✅ Struktur nach COT-Vorbild verifiziert |
| RPC-Enum | `Scripts/3_Game/PsyernsCinematicTool/PCT_RPC.c` — `EPCT_RPC { INVALID = 10500, COUNT }` | ✅ |
| Konstanten | `Scripts/3_Game/PsyernsCinematicTool/PCT_Constants.c` — `PCT_VERSION` "0.1.0", `$profile`-Pfade | ✅ Muster nach COT `JMConstants.c` |
| Leeres Modul | `Scripts/5_Mission/PsyernsCinematicTool/Modules/PCT_CinematicModule.c` — alle 7 Permissions im Konstruktor, HasAccess/GetLayoutRoot/GetInputToggle/GetTitle/GetIconName/ImageIsIcon/GetWebhookTitle/GetRPCMin/GetRPCMax | ✅ Signaturen gegen COT-Quelle verifiziert |
| Modul-Registrierung | `Scripts/5_Mission/PsyernsCinematicTool/PCT_ModuleRegistration.c` — `modded class JMModuleConstructor` | ✅ (Ort nach B1 §3, nicht B6-Tabelle) |
| Form-Stub | `Scripts/5_Mission/PsyernsCinematicTool/GUI/PCT_CinematicForm.c` + `GUI/layouts/pct_cinematic_form.layout` | ✅ nach COT `JMExampleForm`/`Example_form.layout` |
| 4_World-Slot | `Scripts/4_World/PsyernsCinematicTool/PCT_World.c` (Platzhalter) | ✅ (Final-Review-Fix: worldScriptModule-Pfad muss im PBO existieren) |
| Build-Skript | `Tools/Build_PCT.ps1` + `Tools/Build_PCT.cmd` (Doppelklick) | ✅ Pack (AddonBuilder `-packonly`), Sign (DSSignFile, Auto-Keygen `%USERPROFILE%\DayZKeys\Psyern`), Deploy `Build/@PsyernsCinematicTool/` |

## 2. Definition of Done — Prüfstand

| # | DoD-Kriterium (B6 §1 Phase 0) | Ergebnis |
|---|---|---|
| 1 | `Build_PCT.ps1` erzeugt fehlerfrei `PCT_Scripts.pbo` + `PCT_GUI.pbo` inkl. `.bisign` | ✅ **BESTANDEN** — Build mehrfach (fresh + idempotent + nach Fix-Wave) mit Exit-Code 0; PBO-Prefixe (`PsyernsCinematicTool\Scripts` / `...\GUI`) und Inhalte (config.cpp, Inputs.xml, Layout, PCT_World.c) byte-verifiziert; Signaturen mit `DSCheckSignatures.exe` geprüft: OK |
| 2 | Offline-Mission mit CF+COT+PCT: `script.log` leer | ⬜ **AUSSTEHEND** — DayZ ist auf der Build-Maschine nicht installiert (nur DayZ Tools); Test auf Spiel-Maschine nötig |
| 3 | COT-Sidebar zeigt "Psyerns Cinematic Tool"; Klick öffnet leeres Fenster | ⬜ AUSSTEHEND (wie 2) |
| 4 | `CinematicTool.View` im COT-Permissions-Editor; Entzug blendet Modul aus | ⬜ AUSSTEHEND (wie 2) |
| 5 | Dedizierter Server + 1 Client: Modul-Sichtbarkeit mit/ohne Permission | ⬜ AUSSTEHEND (T3-Minimalprobe) |

**Anleitung für DoD 2–4 (Spiel-Maschine):** `Build/@PsyernsCinematicTool` neben `@CF` und `@COT` installieren, `Psyern.bikey` in Server-`keys\`, Start mit `-mod=@CF;@COT;@PsyernsCinematicTool` (PCT ist `-mod`, NICHT `-serverMod` — B6 §3). Offline-Mission nach COT-Vorlage (`Missions\COT.ChernarusPlus`). Danach `script.log` auf Compile-Errors/`FIX-ME`-Warnungen aus PCT-Dateien prüfen.

## 3. Verifizierte Fakten aus der Implementierung (relevant für Phase 1)

- **Form-Bindung (Korrektur zu Research_COT_Infrastructure.md):** Der aktive Pfad ist `JMRenderableModuleBase.Show()` → `GetCOTWindowManager().Create()` → `JMWindowBase.SetModule()` → `CreateWidgets(GetLayoutRoot())` + `menu.GetScript(m_Form)`; die Bindung erfolgt über `scriptclass "PCT_CinematicForm"` am Root-Widget des Layouts. Der `CF_WINDOWS`-Zweig ist tot (nirgends defined).
- **RPC-Dispatch:** CF routet `min <= id < max` (`CF_ModuleGameEvent.c:16`); PCT belegt exakt ID 10500. Basis-`OnRPC` hat keinen Body — der Phase-1-`OnRPC`-Override braucht keinen `super`-Erwartungswert.
- **Permissions:** Registrierung MUSS im Modul-Konstruktor bleiben (`JMPermissionManager.RegisterPermission` wirft nach Mission-Load). `HasPermission` liefert offline `true` → Offline-Entwicklungszyklus (T2) funktioniert ohne Permission-Setup.
- **Build ohne `P:\`-Workdrive:** `-packonly` packt direkt aus dem Staging-Ordner — B6 §6 Schritt 2 (Workdrive-Setup) ist für den reinen Pack/Sign/Deploy-Zyklus nicht erforderlich; erst für Workbench-/Diag-Workflows relevant.

## 4. Dokumentierte Abweichungen vom Plan

1. Datei-Orte nach Plan_B1 §3 wo Plan_B6-Tabelle abweicht: Registrierung als `PCT_ModuleRegistration.c` direkt unter `5_Mission\PsyernsCinematicTool\`, Form unter `...\GUI\`.
2. Kein `scripts`→`Scripts`-Rename: Der Paketordner war leer; `Scripts\` wurde direkt angelegt.
3. Modul implementiert zusätzlich `GetIconName`/`ImageIsIcon`/`GetWebhookTitle` — Spiegelung des COT-`JMExampleModule`-Minimalsatzes (vom Brief gefordert), nicht Scope-Creep.
4. Klassen-/Methoden-Brace-Stil: COT-Konvention (next line) vs. CLAUDE.md (same line) ist projektweit unentschieden — Task-2-Dateien nutzen same-line für Methoden, Plan-verbatim-Code next-line. **Offene Stilentscheidung, kein Funktionsthema.**

## 5. Offene Punkte / Nächste Schritte

1. **DoD 2–5 abnehmen** (Spiel-Maschine, Anleitung oben) — dabei fällt Offener-Punkt O7 aus Plan_B1 §10 (#7, `UAPCT_ToggleMenu`-Bindung externer Mod-Inputs) direkt mit an.
2. **O1 vorziehen** (B6 §6 Schritt 6): SetFOV-Grenztest als erstes Phase-1-Arbeitspaket — Ergebnis bestimmt die Rig-Clamp-Konstanten.
3. Deploy für echten Testserver: `-DeployDir` des Build-Skripts auf den Server-Mod-Pfad zeigen lassen.
4. Git: Stand ist bewusst uncommitted (Projektregel: Commit nur auf Anfrage). `.gitignore` (`Build/`, `.superpowers/`) liegt bereit für den ersten Commit.

## 6. Review-Historie

- Task 1 (Skeleton/Configs): Spec ✅, Approved — Inputs.xml-Struktur + `$profile`-Konvention gegen COT-Quelle belegt.
- Task 2 (Modul/Form/Layout): Spec ✅, Approved — alle Override-Signaturen + Form-Bindungsmechanismus unabhängig in COT/CF-Quelle nachverifiziert.
- Task 3 (Build): Spec ✅, Approved — Prefixe/PBO-Inhalte byte-verifiziert, Key-Guard getestet, PS-5.1-Syntax-Check bestanden.
- Final-Review (ganzer Stand): „Ready to merge — with fixes"; beide Important-Findings (`.gitignore`, 4_World-Platzhalter) plus zwei Quick-Wins (Deploy-Cleanup, `Build_PCT.cmd`) umgesetzt und per erneutem Build verifiziert.
