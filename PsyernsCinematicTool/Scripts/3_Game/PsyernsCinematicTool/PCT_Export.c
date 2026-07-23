// Task P2-6 -- Export (Shot-Liste CSV/JSON), Code so weit wie moeglich zeichengetreu aus
// Docs/Plan_B3_Datenmodell_Persistenz.md Section 8 (Export-Formate) uebernommen. ASCII-normalisierte
// Kommentare, Spaltenliste/PCT_ShotListRow/PCT_ShotListExport 1:1 aus dem Plan. COT-frei (3_Game, kein
// Community-Online-Tools-Bezug): nur PCT_Storage/PCT_Constants/Vanilla-1.29-Protos/CF_Log, exakt wie
// PCT_Storage.c/PCT_Framing.c es fuer diesen Layer bereits vormachen.
//
// === API-Fundstellen (verifiziert direkt am 1.29-Quellcode) ===
//   FileHandle OpenFile(string name, FileMode mode) -- scripts - 1.29\1_Core\DayZ\proto\EnSystem.c:417.
//     Rueckgabe 0 bei Fehlschlag (Docstring EnSystem.c:403: "\return file handle ID or 0 if fails").
//   void CloseFile(FileHandle file) -- EnSystem.c:443.
//   void FPrintln(FileHandle file, void var) -- EnSystem.c:481 (schreibt + haengt Zeilenumbruch an).
//   enum FileMode { READ, WRITE, APPEND } -- EnSystem.c:382-387.
//   bool FileExist(string name) -- EnSystem.c:397.
//   void GetYearMonthDay(out int year, out int month, out int day) -- EnSystem.c:52.
//   void GetHourMinuteSecond(out int hour, out int minute, out int second) -- EnSystem.c:28.
//     (identisches Verifikationsergebnis wie PCT_CinematicModule.GenerateShotId (Task P2-3) -- beantwortet
//     Plan_B6 Section 5 O23 ("exportedAt bleibt leer, API unverifiziert") direkt aus der Quelle: die
//     Funktionen existieren mit genau dieser Signatur, exportedAt wird daher HIER befuellt.)
//   int string.IndexOf(string sample) -- EnString.c:240 (nativ, -1 wenn nicht gefunden).
//   int string.Replace(string sample, string replace) -- EnString.c:156 (mutiert die Instanz in-place,
//     Rueckgabewert = Anzahl Ersetzungen; identische Fundstelle bereits in PCT_Storage.c dokumentiert).
//   JsonFileLoader<Class T>.SaveFile(string filename, T data, out string errorMessage): bool --
//     scripts - 1.29\3_Game\DayZ\tools\JsonFileLoader.c:42-67 (NICHT die deprecated void-Variante
//     JsonSaveFile -- identische Konvention wie PCT_Storage.c, siehe dortiger Datei-Kopfkommentar
//     "Abweichung vom woertlichen Plan-Code").
//   proto native void MakeScreenshot(string name) -- scripts - 1.29\1_Core\DayZ\proto\proto.c:142
//     ("makes screenshot and stores it into a DDS format file; if the name begins with '$' the screenshot
//     is stored in the fullpath according to the name parameter, otherwise ... '$profile:ScreenShotes/DATE
//     TIME-NAME.dds'"). Aufruf/Pfadbau liegt in PCT_CinematicModule.CaptureShotThumbnail (5_Mission,
//     Task-Brief-Vorgabe "Modul-Funktion") -- diese Datei liefert dafuer nur den Pfad-/Take-Zaehler-Helper
//     (ResolveScreenshotBasePath/SanitizeForFileName, siehe unten).
//
// === Dokumentierte Abweichungen vom woertlichen Plan-Code (Section 8) ===
// 1. Signatur ExportShotListCsv(shots, delimiter) OHNE PCT_Sequence-Parameter (Plan-Code Section 8.1:
//    "ExportShotListCsv(PCT_Sequence sequence, array<ref PCT_Shot> shots, string delimiter)"). Task-P2-6-
//    Brief gibt die Signatur bewusst ohne Sequence vor: PCT_Sequence existiert in diesem Mod-Stand noch
//    nicht (Phase 3, siehe PCT_Shot.c-Kopfkommentar "Ausgeschlossen bleiben PCT_Sequence ..."). Konsequenzen:
//      - Dateiname wird "ShotList_<Datum_Uhrzeit>.csv/json" statt "ShotList_<sequenceId>.csv" (Task-Brief-
//        Vorgabe woertlich: "-> $profile:...\Exports\ShotList_<datum>.csv").
//      - Spalten 21/22 (worldDate/worldTime) und 23-25 (overcast/fog/rain) nutzen den "effektiven WorldState"
//        AUSSCHLIESSLICH aus shot.worldStateOverride (null = PCT_WorldState()-Defaults als Platzhalter) --
//        die Plan-Regel "Override sonst SEQUENZ-Weltzustand" kann den Sequenz-Anteil mangels PCT_Sequence
//        nicht abbilden. ResolveEffectiveWorldState() dokumentiert das an Ort und Stelle.
//      - Spalten 26/27 (actorCount/lightCount) kommen aus sequence.actorMarks/lights (Plan_B3 Section 2.5)
//        -- ohne Sequence-Objekt bleiben beide fest auf 0 (BuildCsvRow-Kommentar an Ort und Stelle).
// 2. CSV-Escaping vereinfacht: Plan-Prosa (Section 8.1 "Format-Entscheidungen") verlangt Anfuehrungszeichen
//    um ALLE Textfelder; der Task-P2-6-Brief gibt stattdessen explizit die einfachere RFC4180-Minimalregel
//    vor ("Werte mit Delimiter/Quote in Anfuehrungszeichen -- einfach halten, dokumentieren") -- siehe
//    EscapeCsvField(). Zeilenumbrueche werden IMMER (nicht nur in notes) durch " / " ersetzt, weil FPrintln
//    zeilenweise schreibt und ein roher Zeilenumbruch sonst die CSV-Zeilenstruktur zerstoeren wuerde.
// 3. PCT_ShotListRow bleibt zeichengetreu bei den in Plan_B3 Section 8.2 tatsaechlich aufgefuehrten 21
//    Feldern (KEIN description/overcast/fog/rain/actorCount/lightCount -- die Plan-PROSA sagt zwar
//    "identische Feldsemantik wie CSV", der Section-8.2-CODE-BLOCK selbst listet diese 6 Felder aber nicht
//    auf). Diese Datei uebernimmt den Code-Block woertlich statt die widerspruechliche Prosa nachzubessern
//    (Praezedenzfall: PCT_Shot.c/PCT_WorldState.c folgen durchgehend "Code vor Prosa" bei Plan-Widerspruechen).
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
	string exportedAt = "";
	ref array<ref PCT_ShotListRow> rows;

	void PCT_ShotListExport()
	{
		rows = new array<ref PCT_ShotListRow>();
	}
}

class PCT_Export
{
	// ===== Oeffentliche API (Task-Brief Punkt 1) =====

	// Rueckgabe: Pfad der erzeugten CSV-Datei bei Erfolg, "" bei Fehler (ungueltiges shots-Array, OpenFile-
	// Fehlschlag). Spalten exakt Plan_B3 Section 8.1 (31 Spalten, siehe BuildCsvHeader/BuildCsvRow) --
	// Abweichungen zur woertlichen Plan-Signatur/den sequenzabhaengigen Spalten siehe Datei-Kopfkommentar.
	static string ExportShotListCsv(array<ref PCT_Shot> shots, string delimiter)
	{
		if (!shots)
		{
			CF_Log.Warn("PCT: ExportShotListCsv ohne shots-Array aufgerufen.");
			return "";
		}

		if (delimiter == "")
			delimiter = ";";

		PCT_Storage.EnsureDirectories();

		string filePath = ResolveUniqueExportPath("ShotList_", PCT_Constants.EXT_CSV);

		FileHandle file = OpenFile(filePath, FileMode.WRITE);
		if (file == 0)
		{
			CF_Log.Warn("PCT: CSV-Export fehlgeschlagen (OpenFile): " + filePath);
			return "";
		}

		FPrintln(file, BuildCsvHeader(delimiter));

		int count = shots.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_Shot shot = shots[i];
			if (!shot)
				continue;

			FPrintln(file, BuildCsvRow(shot, delimiter));
		}

		CloseFile(file);
		return filePath;
	}

	// Rueckgabe: Pfad der erzeugten JSON-Datei bei Erfolg, "" bei Fehler. PCT_ShotListRow-Feldbelegung siehe
	// BuildShotListRow (Datei-Kopfkommentar Abweichung 3 -- 21 Felder, kein description/Wetter/Sequenz-Zaehler).
	static string ExportShotListJson(array<ref PCT_Shot> shots)
	{
		if (!shots)
		{
			CF_Log.Warn("PCT: ExportShotListJson ohne shots-Array aufgerufen.");
			return "";
		}

		PCT_Storage.EnsureDirectories();

		PCT_ShotListExport exportData = new PCT_ShotListExport();
		exportData.exportedAt = BuildExportedAtDisplay();

		int count = shots.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_Shot shot = shots[i];
			if (!shot)
				continue;

			exportData.rows.Insert(BuildShotListRow(shot));
		}

		string filePath = ResolveUniqueExportPath("ShotList_", PCT_Constants.EXT_JSON);

		string errorMessage;
		bool saved = JsonFileLoader<PCT_ShotListExport>.SaveFile(filePath, exportData, errorMessage);
		if (!saved)
		{
			CF_Log.Warn("PCT: ExportShotListJson fehlgeschlagen fuer '" + filePath + "': " + errorMessage);
			return "";
		}

		return filePath;
	}

	// ===== Screenshot-Pfad-Helfer (Task-Brief Punkt 2 -- MakeScreenshot-Aufruf selbst liegt in
	// PCT_CinematicModule.CaptureShotThumbnail, siehe Datei-Kopfkommentar) =====

	// Namenskonvention/Take-Zaehler 1:1 aus Plan_B3 Section 8.3 (NextScreenshotBaseName-Muster), Praefix
	// "PCT_<name>_T<take>" -- "<name>" ist der sanitisierte Shot-Name statt "<sequenceId>_<shotId>" (kein
	// PCT_Sequence in diesem Mod-Stand, siehe Datei-Kopfkommentar). Rueckgabe OHNE ".dds"-Endung (MakeScreenshot
	// haengt sie laut proto.c:137-142 an -- Annahme identisch zum Plan-Vorbild, NICHT im Spiel gegengetestet,
	// siehe Task-Report "Bedenken").
	static string ResolveScreenshotBasePath(string shotName)
	{
		PCT_Storage.EnsureDirectories();

		string safeName = SanitizeForFileName(shotName);
		if (safeName == "")
			safeName = "shot";

		int take = 1;
		string baseName = "";
		bool exists = true;
		while (exists)
		{
			baseName = PCT_Constants.DIR_EXPORTS + "PCT_" + safeName + "_T" + take.ToString();
			exists = FileExist(baseName + ".dds");
			if (exists)
				take = take + 1;
		}

		return baseName;
	}

	// Ersetzt Dateisystem-kritische Zeichen (Leerzeichen + Windows-reservierte \/:*?"<>|) durch "_" -- ein
	// roher UI-Textfeldwert (Shot-Name) kann beliebige Zeichen enthalten, MakeScreenshot baut daraus aber
	// einen Dateipfad. Keine vollstaendige Dateinamen-Validierung (z. B. reservierte Namen wie "CON"),
	// bewusst nur die praktisch wahrscheinlichsten Problemzeichen (Task-Vorgabe "einfach halten").
	static string SanitizeForFileName(string raw)
	{
		string result = raw;
		result.Replace(" ", "_");
		result.Replace("\\", "_");
		result.Replace("/", "_");
		result.Replace(":", "_");
		result.Replace("*", "_");
		result.Replace("?", "_");
		result.Replace("\"", "_");
		result.Replace("<", "_");
		result.Replace(">", "_");
		result.Replace("|", "_");

		return result;
	}

	// ===== PCT_ShotListRow-Aufbau (JSON-Export, Abweichung 3) =====

	private static PCT_ShotListRow BuildShotListRow(PCT_Shot shot)
	{
		PCT_ShotListRow row = new PCT_ShotListRow();
		if (!shot)
			return row;

		row.scene = shot.sceneNumber;
		row.shot = shot.shotNumber;
		row.name = shot.name;
		row.framing = ResolveFramingShortCode(shot.framingPresetId);
		row.angle = ResolveAngleDisplayName(shot.anglePresetId);
		row.movement = ResolveMovementDisplayName(shot.movementPresetId);

		string sensorId = "";
		if (shot.cameraRig)
			sensorId = shot.cameraRig.sensorPresetId;
		row.sensor = ResolveSensorDisplayName(sensorId);

		if (shot.cameraRig)
		{
			row.focalLengthMm = shot.cameraRig.focalLengthMm;
			row.aperture = shot.cameraRig.aperture;
		}

		int keyframeCount = 0;
		if (shot.keyframes)
			keyframeCount = shot.keyframes.Count();
		row.keyframeCount = keyframeCount;

		if (keyframeCount > 0)
		{
			PCT_Keyframe firstKeyframe = shot.keyframes[0];
			if (firstKeyframe)
			{
				row.fovDegreesStart = firstKeyframe.fovDegrees;
				row.focusDistanceStartM = firstKeyframe.focusDistance;
				row.camPos = firstKeyframe.position;
				row.camOrientation = firstKeyframe.orientation;
			}

			PCT_Keyframe lastKeyframe = shot.keyframes[keyframeCount - 1];
			if (lastKeyframe)
				row.durationSeconds = lastKeyframe.time;
		}

		PCT_WorldState effectiveWorldState = ResolveEffectiveWorldState(shot);
		row.worldDate = FormatWorldDate(effectiveWorldState);
		row.worldTime = FormatWorldTime(effectiveWorldState);

		row.priority = shot.priority;
		row.status = shot.status;
		row.thumbnail = shot.thumbnailFile;
		row.notes = shot.notes;

		return row;
	}

	// ===== CSV-Zeilenaufbau (31 Spalten, Plan_B3 Section 8.1-Tabelle) =====

	private static string BuildCsvHeader(string delimiter)
	{
		array<string> columns = new array<string>();
		columns.Insert("scene");
		columns.Insert("shot");
		columns.Insert("name");
		columns.Insert("description");
		columns.Insert("framing");
		columns.Insert("angle");
		columns.Insert("movement");
		columns.Insert("sensor");
		columns.Insert("focalLengthMm");
		columns.Insert("aperture");
		columns.Insert("fovDegreesStart");
		columns.Insert("focusDistanceStartM");
		columns.Insert("camPosX");
		columns.Insert("camPosY");
		columns.Insert("camPosZ");
		columns.Insert("camYaw");
		columns.Insert("camPitch");
		columns.Insert("camRoll");
		columns.Insert("durationSeconds");
		columns.Insert("keyframeCount");
		columns.Insert("worldDate");
		columns.Insert("worldTime");
		columns.Insert("overcast");
		columns.Insert("fog");
		columns.Insert("rain");
		columns.Insert("actorCount");
		columns.Insert("lightCount");
		columns.Insert("priority");
		columns.Insert("status");
		columns.Insert("thumbnail");
		columns.Insert("notes");

		return JoinCsvFields(columns, delimiter, false);
	}

	private static string BuildCsvRow(PCT_Shot shot, string delimiter)
	{
		array<string> fields = new array<string>();
		if (!shot)
			return JoinCsvFields(fields, delimiter, true);

		fields.Insert(shot.sceneNumber);
		fields.Insert(shot.shotNumber);
		fields.Insert(shot.name);
		fields.Insert(shot.description);
		fields.Insert(ResolveFramingShortCode(shot.framingPresetId));
		fields.Insert(ResolveAngleDisplayName(shot.anglePresetId));
		fields.Insert(ResolveMovementDisplayName(shot.movementPresetId));

		string sensorId = "";
		float focalLengthMm = 0.0;
		float aperture = 0.0;
		if (shot.cameraRig)
		{
			sensorId = shot.cameraRig.sensorPresetId;
			focalLengthMm = shot.cameraRig.focalLengthMm;
			aperture = shot.cameraRig.aperture;
		}
		fields.Insert(ResolveSensorDisplayName(sensorId));
		fields.Insert(focalLengthMm.ToString());
		fields.Insert(aperture.ToString());

		float fovDegreesStart = 0.0;
		float focusDistanceStartM = 0.0;
		vector camPos = vector.Zero;
		vector camOrientation = vector.Zero;
		float durationSeconds = 0.0;
		int keyframeCount = 0;

		if (shot.keyframes)
			keyframeCount = shot.keyframes.Count();

		if (keyframeCount > 0)
		{
			PCT_Keyframe firstKeyframe = shot.keyframes[0];
			if (firstKeyframe)
			{
				fovDegreesStart = firstKeyframe.fovDegrees;
				focusDistanceStartM = firstKeyframe.focusDistance;
				camPos = firstKeyframe.position;
				camOrientation = firstKeyframe.orientation;
			}

			PCT_Keyframe lastKeyframe = shot.keyframes[keyframeCount - 1];
			if (lastKeyframe)
				durationSeconds = lastKeyframe.time;
		}

		fields.Insert(fovDegreesStart.ToString());
		fields.Insert(focusDistanceStartM.ToString());
		fields.Insert(camPos[0].ToString());
		fields.Insert(camPos[1].ToString());
		fields.Insert(camPos[2].ToString());
		fields.Insert(camOrientation[0].ToString());
		fields.Insert(camOrientation[1].ToString());
		fields.Insert(camOrientation[2].ToString());
		fields.Insert(durationSeconds.ToString());
		fields.Insert(keyframeCount.ToString());

		PCT_WorldState effectiveWorldState = ResolveEffectiveWorldState(shot);
		fields.Insert(FormatWorldDate(effectiveWorldState));
		fields.Insert(FormatWorldTime(effectiveWorldState));
		fields.Insert(effectiveWorldState.overcast.ToString());
		fields.Insert(effectiveWorldState.fog.ToString());
		fields.Insert(effectiveWorldState.rain.ToString());

		// Datei-Kopfkommentar Abweichung 1: actorCount/lightCount kommen aus PCT_Sequence.actorMarks/lights
		// (Plan_B3 Section 2.5) -- diese Klasse existiert in diesem Mod-Stand noch nicht (Phase 3), daher fest 0.
		fields.Insert("0");
		fields.Insert("0");

		fields.Insert(shot.priority.ToString());
		fields.Insert(shot.status);
		fields.Insert(shot.thumbnailFile);
		fields.Insert(shot.notes);

		return JoinCsvFields(fields, delimiter, true);
	}

	// escape=false fuer den Header (feste ASCII-Bezeichner, keine Delimiter-/Quote-Kollision moeglich);
	// escape=true fuer Datenzeilen (EscapeCsvField je Feld).
	private static string JoinCsvFields(array<string> fields, string delimiter, bool escape)
	{
		string line = "";
		int count = fields.Count();
		for (int i = 0; i < count; i++)
		{
			string field = fields[i];
			if (escape)
				field = EscapeCsvField(field, delimiter);

			if (i > 0)
				line = line + delimiter;

			line = line + field;
		}

		return line;
	}

	// Task-Brief-Vorgabe (vereinfachtes Escaping, Datei-Kopfkommentar Abweichung 2): nur Werte, die den
	// Delimiter oder ein Anfuehrungszeichen enthalten, werden in Anfuehrungszeichen gesetzt; enthaltene
	// Anfuehrungszeichen werden verdoppelt (RFC4180-Minimalvariante). Zeilenumbrueche werden IMMER zuerst
	// durch " / " ersetzt (FPrintln schreibt zeilenweise -- ein roher Zeilenumbruch wuerde sonst die
	// CSV-Zeilenstruktur zerstoeren, nicht nur bei notes).
	private static string EscapeCsvField(string value, string delimiter)
	{
		if (value == "")
			return value;

		string normalized = value;
		normalized.Replace("\n", " / ");

		bool needsQuoting = false;
		if (normalized.IndexOf(delimiter) != -1)
			needsQuoting = true;
		if (normalized.IndexOf("\"") != -1)
			needsQuoting = true;

		if (!needsQuoting)
			return normalized;

		string escaped = normalized;
		escaped.Replace("\"", "\"\"");

		return "\"" + escaped + "\"";
	}

	// ===== Preset-Lookups (linear, Export ist kein Hot-Path -- kein eigener Cache noetig) =====

	private static string ResolveSensorDisplayName(string id)
	{
		if (id == "")
			return "";

		PCT_SensorPresetFile file = PCT_Storage.LoadSensorPresets();
		if (!file || !file.presets)
			return "";

		int count = file.presets.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_SensorPreset preset = file.presets[i];
			if (preset && preset.id == id)
				return preset.displayName;
		}

		return "";
	}

	private static string ResolveFramingShortCode(string id)
	{
		if (id == "")
			return "";

		PCT_FramingPresetFile file = PCT_Storage.LoadFramingPresets();
		if (!file || !file.presets)
			return "";

		int count = file.presets.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_FramingPreset preset = file.presets[i];
			if (preset && preset.id == id)
				return preset.shortCode;
		}

		return "";
	}

	private static string ResolveAngleDisplayName(string id)
	{
		if (id == "")
			return "";

		PCT_AnglePresetFile file = PCT_Storage.LoadAnglePresets();
		if (!file || !file.presets)
			return "";

		int count = file.presets.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_AnglePreset preset = file.presets[i];
			if (preset && preset.id == id)
				return preset.displayName;
		}

		return "";
	}

	private static string ResolveMovementDisplayName(string id)
	{
		if (id == "")
			return "";

		PCT_MovementPresetFile file = PCT_Storage.LoadMovementPresets();
		if (!file || !file.presets)
			return "";

		int count = file.presets.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_MovementPreset preset = file.presets[i];
			if (preset && preset.id == id)
				return preset.displayName;
		}

		return "";
	}

	// Datei-Kopfkommentar Abweichung 1: kein PCT_Sequence in diesem Mod-Stand -- "Override sonst
	// Sequenz-Weltzustand" (Plan_B3 Section 8.1) kann den Sequenz-Anteil nicht abbilden. Override gewinnt,
	// wenn vorhanden; sonst PCT_WorldState()-Defaults als dokumentierter Platzhalter (KEIN "erfundener"
	// Sequenzwert).
	private static PCT_WorldState ResolveEffectiveWorldState(PCT_Shot shot)
	{
		if (shot && shot.worldStateOverride)
			return shot.worldStateOverride;

		return new PCT_WorldState();
	}

	// ===== Datum/Uhrzeit-Helfer (GetYearMonthDay/GetHourMinuteSecond, siehe Datei-Kopfkommentar) =====
	// Lokale Kopie statt Aufruf von PCT_CinematicModule.GenerateShotId/ZeroPad2 (5_Mission) -- 3_Game darf
	// 5_Mission laut Script-Layer-Regeln (CLAUDE.md) nicht referenzieren, daher eigenstaendiger, minimaler
	// Nachbau desselben bereits verifizierten Musters.

	private static void ResolveCurrentDateTimeParts(out int year, out int month, out int day, out int hour, out int minute, out int second)
	{
		GetYearMonthDay(year, month, day);
		GetHourMinuteSecond(hour, minute, second);
	}

	private static string ZeroPad2(int value)
	{
		if (value < 10)
			return "0" + value;

		return value.ToString();
	}

	// "YYYY-MM-DD" -- Kalenderdatum eines PCT_WorldState (in-game Fiktivdatum, NICHT die Systemuhr).
	private static string FormatWorldDate(PCT_WorldState state)
	{
		if (!state)
			return "";

		return state.year.ToString() + "-" + ZeroPad2(state.month) + "-" + ZeroPad2(state.day);
	}

	// "HH:MM" -- Uhrzeit eines PCT_WorldState.
	private static string FormatWorldTime(PCT_WorldState state)
	{
		if (!state)
			return "";

		return ZeroPad2(state.hour) + ":" + ZeroPad2(state.minute);
	}

	// "YYYYMMDD_HHMMSS" -- Datei-Zeitstempel (Systemuhr), identisches Format zu
	// PCT_CinematicModule.GenerateShotId ("shot_YYYYMMDD_HHMMSS").
	private static string BuildExportTimestamp()
	{
		int year;
		int month;
		int day;
		int hour;
		int minute;
		int second;
		ResolveCurrentDateTimeParts(year, month, day, hour, minute, second);

		string datePart = year.ToString() + ZeroPad2(month) + ZeroPad2(day);
		string timePart = ZeroPad2(hour) + ZeroPad2(minute) + ZeroPad2(second);
		return datePart + "_" + timePart;
	}

	// "YYYY-MM-DD HH:MM:SS" -- menschenlesbarer Zeitstempel fuer PCT_ShotListExport.exportedAt (Systemuhr).
	private static string BuildExportedAtDisplay()
	{
		int year;
		int month;
		int day;
		int hour;
		int minute;
		int second;
		ResolveCurrentDateTimeParts(year, month, day, hour, minute, second);

		string datePart = year.ToString() + "-" + ZeroPad2(month) + "-" + ZeroPad2(day);
		string timePart = ZeroPad2(hour) + ":" + ZeroPad2(minute) + ":" + ZeroPad2(second);
		return datePart + " " + timePart;
	}

	// Kollisionsschutz identisch zum Praezedenzfall PCT_CinematicModule.GenerateShotId (Task P2-3): Suffix
	// _2, _3, ... bei Kollision innerhalb derselben Sekunde, Obergrenze 99 (danach wird der letzte
	// Kandidatenpfad trotz verbleibender Kollision zurueckgegeben -- kein Endlos-Risiko).
	private static string ResolveUniqueExportPath(string prefix, string extension)
	{
		string baseTimestamp = BuildExportTimestamp();
		string candidatePath = PCT_Constants.DIR_EXPORTS + prefix + baseTimestamp + extension;

		if (!FileExist(candidatePath))
			return candidatePath;

		int suffix = 2;
		while (suffix <= 99)
		{
			candidatePath = PCT_Constants.DIR_EXPORTS + prefix + baseTimestamp + "_" + suffix.ToString() + extension;
			if (!FileExist(candidatePath))
				return candidatePath;

			suffix = suffix + 1;
		}

		return candidatePath;
	}
}
