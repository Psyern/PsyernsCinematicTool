// Task P2-2 -- Persistenz (PCT_Storage / PCT_Migration / PCT_Validation), Code so weit wie moeglich
// zeichengetreu aus Docs/Plan_B3_Datenmodell_Persistenz.md Section 4 (Storage), Section 5 (Migration) und
// Section 6 (Validation) uebernommen. ASCII-normalisierte Kommentare (ae/oe/ue/ss, "--", "Section n").
//
// Datei-Layout-Entscheidung (Task-Brief: "wenn B1 nur PCT_Storage.c listet: alle drei Klassen duerfen in
// EINER Datei liegen, entscheide nach Plan und dokumentiere"): Docs/Plan_B1_Architektur.md Section 3 listet
// unter Scripts\3_Game\PsyernsCinematicTool\Persistence\ ausschliesslich "PCT_Storage.c" -- kein separater
// Eintrag fuer PCT_Migration.c/PCT_Validation.c. Nach der bereits in PCT_Presets.c (Task P2-1) etablierten
// Regel "B1-Baum gewinnt" liegen daher alle drei Klassen (PCT_Storage, PCT_Migration, PCT_Validation) in
// dieser einen Datei.
//
// Dependency-Reihenfolge in dieser Datei: PCT_Validation zuerst (reine Helfer, keine Abhaengigkeit auf die
// anderen beiden), dann PCT_Migration (nur Datenklassen + PCT_Constants), zuletzt PCT_Storage (nutzt beide).
// EnScript kennt keine Vorwaertsdeklaration fuer Klassen -- die Reihenfolge ist hier bewusst so gewaehlt,
// dass jede Klasse nur auf bereits oberhalb definierte Klassen verweist (PCT_Storage.LoadShot ruft
// PCT_Migration.MigrateShot/PCT_Validation.ValidateShot -- beide muessen vorher bekannt sein).
//
// === API-Fundstellen (verifiziert, siehe Task-Report fuer die volle Tabelle) ===
//   JsonFileLoader<Class T>.LoadFile(string filename, out T data, out string errorMessage): bool
//     -- scripts - 1.29\3_Game\DayZ\tools\JsonFileLoader.c:7-40 (bool-Rueckgabe + Fehlertext, NICHT die
//        deprecated void-Variante JsonLoadFile/JsonSaveFile aus Plan_B3 Section 4.3 -- siehe Abweichung
//        unten).
//   JsonFileLoader<Class T>.SaveFile(string filename, T data, out string errorMessage): bool
//     -- JsonFileLoader.c:42-67.
//   FileExist(string name): bool -- scripts - 1.29\1_Core\DayZ\proto\EnSystem.c:397.
//   MakeDirectory(string name): bool -- EnSystem.c:525.
//   DeleteFile(string name): bool -- EnSystem.c:528 ("Works only on \"$profile:\" and \"$saves:\"
//     locations" -- exakt unser Fall, DIR_ROOT ist $profile:-basiert). Das ist die Datei-Loesch-API, NICHT
//     das verbotene EnScript-`delete`-Keyword (CLAUDE.md).
//   FindFile(string pattern, out string fileName, out FileAttr fileAttributes, FindFileFlags flags):
//     FindFileHandle -- EnSystem.c:520.
//   FindNextFile(FindFileHandle handle, out string fileName, out FileAttr fileAttributes): bool --
//     EnSystem.c:521.
//   CloseFindFile(FindFileHandle handle): void -- EnSystem.c:522.
//   TStringArray -- typedef array<string> TStringArray; EnSystem.c:712 (bzw. EnScript.c, je nach
//     Compile-Unit-Zaehlung; Fundstelle im 1_Core/DayZ/proto-Ordner).
//   CF_Log.Warn(string message, ...): void -- DayZ-CommunityFramework-production\JM\CF\Scripts\1_Core\
//     CommunityFramework\Logging\CF_Log.c:61-66 (optionale %1..%9-Parameter; ein einzelner, bereits
//     verketteter String als `message` funktioniert unveraendert, string.Format ohne Platzhalter gibt die
//     Eingabe unveraendert zurueck).
//   string.Replace(string sample, string replace): int -- scripts - 1.29\1_Core\DayZ\proto\EnString.c:156
//     (mutiert die Instanz in-place, Rueckgabewert = Anzahl Ersetzungen).
//   Math.Clamp(float value, float min, float max): float -- EnMath.c:540.
//   Math.NormalizeAngle(float ang): float -- EnMath.c:155 ("Normalizes the angle (0...360)").
//   array<ref T>.Insert(T value): int / .Remove(int index): void -- scripts - 1.29\1_Core\DayZ\proto\
//     EnScript.c:407/463.
//
// === Abweichung vom woertlichen Plan-Code (Section 4.3), begruendet ===
// Plan_B3 Section 4.3 zeigt SaveShot/LoadShot/LoadSettings mit der DEPRECATED void-API
// (JsonFileLoader<T>.JsonSaveFile/JsonLoadFile -- JsonFileLoader.c:99-172, dort als "!use ... instead"
// markiert). Diese void-Variante gibt bei kaputtem JSON KEIN Erfolgs-/Fehlersignal an den Aufrufer zurueck
// (nur ein interner ErrorEx-Log-Eintrag, JsonFileLoader.c:128-129) -- genau das, was Plan_B3 Section 4.5/7
// als "OFFEN -- ZU VERIFIZIEREN: JsonFileLoader-Fehlersemantik (wirft es? liefert es false? Param-out?)"
// offen laesst. Am 1.29-Quellcode direkt verifiziert: JsonFileLoader.c definiert zusaetzlich die NICHT
// deprecated `static bool LoadFile(string filename, out T data, out string errorMessage)` (Zeile 7) und
// `static bool SaveFile(string filename, T data, out string errorMessage)` (Zeile 42) -- beide rufen
// denselben `m_Serializer.ReadFromString`/`WriteToString` wie die deprecated Varianten auf, liefern aber
// deterministisch `false` + Klartext-Fehlermeldung bei Miss-erfolg (fehlende Datei, kaputtes JSON, nicht
// oeffenbare Datei), OHNE zu crashen. Diese Datei verwendet durchgehend LoadFile/SaveFile statt
// JsonLoadFile/JsonSaveFile -- das beantwortet die im Plan offene Frage direkt aus der Quelle (kein
// Laufzeittest noetig) und ermoeglicht die vom Task-Brief geforderte Fehlerbehandlung ("defekte JSON --
// loggen, Datei NICHT ueberschreiben, Skip", Section 4.5-Tabelle) ueberhaupt erst zuverlaessig. Verhalten
// bei bestehender/fehlender Datei ist mit dem Plan-Muster identisch (eigener FileExist-Check VOR dem Load,
// getrennte Log-Meldung fuer "Datei fehlt" vs. "Datei defekt" -- deckungsgleich mit der Section-4.5-Tabelle,
// die genau diese zwei Faelle unterscheidet).
//
// === Easing-/Kurventyp-Validierung: String-Form (Stand P3-2) ===
// Seit Task P3-1/P3-2 existieren EPCT_Easing/EPCT_PathType/EPCT_PointBehavior/EPCT_LookMode/
// EPCT_TrackChannel als Laufzeit-Enums (Data/PCT_Enums.c). Das JSON traegt weiterhin STRINGS
// (Plan_B7 Section 10.3 "Strings im JSON -- analog EPCT_Easing"); die Validierung prueft/normalisiert
// deshalb weiterhin die gespeicherte String-Form (ValidateEasingString/ValidatePathTypeString/...);
// das String->Enum-Parsing fuer den Playback-Pfad liegt beim Player (Plan_B7 Section 14), nicht hier.
class PCT_Validation
{
	// ===== Skalare Clamp-Helfer (Plan_B3 Section 6.3, ClampWarn zeichengetreu uebernommen) =====

	static float ClampWarn(float value, float min, float max, string fieldName)
	{
		if (value >= min && value <= max)
			return value;

		float clamped = Math.Clamp(value, min, max);
		CF_Log.Warn("PCT: '" + fieldName + "' = " + value + " ausserhalb [" + min + ".." + max + "], geklemmt auf " + clamped);
		return clamped;
	}

	// Int-Pendant zu ClampWarn (Plan_B3 Section 6.1 PCT_WorldState-Tabelle: month/day/hour/minute sind int).
	// Math.Clamp (EnMath.c:540) ist nur als float-Overload verifiziert -- eigene int-Grenzwertlogik statt
	// impliziter float-Konvertierung, um keine unbelegte Overload-Aufloesung anzunehmen.
	static int ClampWarnInt(int value, int min, int max, string fieldName)
	{
		if (value >= min && value <= max)
			return value;

		int clamped = value;
		if (clamped < min)
			clamped = min;
		if (clamped > max)
			clamped = max;

		CF_Log.Warn("PCT: '" + fieldName + "' = " + value + " ausserhalb [" + min + ".." + max + "], geklemmt auf " + clamped);
		return clamped;
	}

	// Prueft/normalisiert die gespeicherte Easing-String-Form (Plan_B3 Section 6.1: Fallback "Smooth").
	// Task P3-2: um den vollstaendigen B7-Katalog erweitert (Plan_B7 Section 9.1; String-Formen wie im
	// v2-JSON-Beispiel Section 13.2: "EaseIn", "EaseInOut", "FastTransition", "SmoothTransition", ...).
	static string ValidateEasingString(string easing)
	{
		if (easing == "Linear")
			return easing;
		if (easing == "Smooth")
			return easing;
		if (easing == "Smoother")
			return easing;
		if (easing == "EaseIn")
			return easing;
		if (easing == "EaseOut")
			return easing;
		if (easing == "EaseInOut")
			return easing;
		if (easing == "HardStart")
			return easing;
		if (easing == "HardStop")
			return easing;
		if (easing == "FastTransition")
			return easing;
		if (easing == "SmoothTransition")
			return easing;
		if (easing == "CustomCurve")
			return easing;

		CF_Log.Warn("PCT: Unbekanntes easing '" + easing + "', Fallback Smooth.");
		return "Smooth";
	}

	// ===== Task P3-3: String -> Enum-Parser fuer den Playback-Pfad (Plan_B7 Section 14.3 referenziert
	// PCT_Validation.ParseEasing/ParsePathType/ParseLookMode; Plan_B3 Section 6.3-Muster). Fallbacks
	// identisch zu den String-Normalisierern unten (Smooth / SmoothCurve / LookAt). =====

	static int ParseEasing(string easing)
	{
		if (easing == "Linear")
			return EPCT_Easing.LINEAR;
		if (easing == "Smooth")
			return EPCT_Easing.SMOOTH;
		if (easing == "Smoother")
			return EPCT_Easing.SMOOTHER;
		if (easing == "EaseIn")
			return EPCT_Easing.EASE_IN;
		if (easing == "EaseOut")
			return EPCT_Easing.EASE_OUT;
		if (easing == "EaseInOut")
			return EPCT_Easing.EASE_IN_OUT;
		if (easing == "HardStart")
			return EPCT_Easing.HARD_START;
		if (easing == "HardStop")
			return EPCT_Easing.HARD_STOP;
		if (easing == "FastTransition")
			return EPCT_Easing.FAST_TRANSITION;
		if (easing == "SmoothTransition")
			return EPCT_Easing.SMOOTH_TRANSITION;
		if (easing == "CustomCurve")
			return EPCT_Easing.CUSTOM_CURVE;
		return EPCT_Easing.SMOOTH;
	}

	static int ParsePathType(string pathType)
	{
		if (pathType == "Linear")
			return EPCT_PathType.LINEAR;
		if (pathType == "SmoothCurve")
			return EPCT_PathType.SMOOTH_CURVE;
		if (pathType == "Bezier")
			return EPCT_PathType.BEZIER;
		if (pathType == "Arc")
			return EPCT_PathType.ARC;
		if (pathType == "Orbit")
			return EPCT_PathType.ORBIT;
		if (pathType == "Rail")
			return EPCT_PathType.RAIL;
		if (pathType == "HandheldPath")
			return EPCT_PathType.HANDHELD_PATH;
		return EPCT_PathType.SMOOTH_CURVE;
	}

	static int ParseLookMode(string lookMode)
	{
		if (lookMode == "LookAt")
			return EPCT_LookMode.LOOK_AT;
		if (lookMode == "FollowTarget")
			return EPCT_LookMode.FOLLOW_TARGET;
		if (lookMode == "LeadTarget")
			return EPCT_LookMode.LEAD_TARGET;
		if (lookMode == "OffsetTarget")
			return EPCT_LookMode.OFFSET_TARGET;
		if (lookMode == "FreeRotation")
			return EPCT_LookMode.FREE_ROTATION;
		if (lookMode == "HorizonLock")
			return EPCT_LookMode.HORIZON_LOCK;
		if (lookMode == "TargetLock")
			return EPCT_LookMode.TARGET_LOCK;
		return EPCT_LookMode.LOOK_AT;
	}

	// ===== Task P3-2: String-Normalisierer der Dual-Path-Enums (Plan_B7 Section 3.3/4/5.2/10.3;
	// strukturelle Checks laut Task-Vorgabe -- keine erfundenen Wertebereiche) =====

	static string ValidatePathTypeString(string pathType, string fallback, string fieldName)
	{
		if (pathType == "Linear")
			return pathType;
		if (pathType == "SmoothCurve")
			return pathType;
		if (pathType == "Bezier")
			return pathType;
		if (pathType == "Arc")
			return pathType;
		if (pathType == "Orbit")
			return pathType;
		if (pathType == "Rail")
			return pathType;
		if (pathType == "HandheldPath")
			return pathType;

		CF_Log.Warn("PCT: Unbekannter " + fieldName + " '" + pathType + "', Fallback " + fallback + ".");
		return fallback;
	}

	static string ValidatePointBehaviorString(string behavior)
	{
		if (behavior == "ExactThrough")
			return behavior;
		if (behavior == "DirectionMarkerOnly")
			return behavior;
		if (behavior == "StopAt")
			return behavior;
		if (behavior == "WaitHold")
			return behavior;
		if (behavior == "ContinuePass")
			return behavior;

		CF_Log.Warn("PCT: Unbekanntes behavior '" + behavior + "', Fallback ExactThrough.");
		return "ExactThrough";
	}

	static string ValidateLookModeString(string lookMode)
	{
		if (lookMode == "LookAt")
			return lookMode;
		if (lookMode == "FollowTarget")
			return lookMode;
		if (lookMode == "LeadTarget")
			return lookMode;
		if (lookMode == "OffsetTarget")
			return lookMode;
		if (lookMode == "FreeRotation")
			return lookMode;
		if (lookMode == "HorizonLock")
			return lookMode;
		if (lookMode == "TargetLock")
			return lookMode;

		CF_Log.Warn("PCT: Unbekannter lookMode '" + lookMode + "', Fallback LookAt.");
		return "LookAt";
	}

	static bool IsKnownTrackChannel(string channel)
	{
		if (channel == "focalLength")
			return true;
		if (channel == "focusDistance")
			return true;
		if (channel == "aperture")
			return true;
		if (channel == "height")
			return true;
		if (channel == "pan")
			return true;
		if (channel == "tilt")
			return true;
		if (channel == "roll")
			return true;
		if (channel == "handheld")
			return true;
		if (channel == "stabilize")
			return true;
		if (channel == "subjectScreenX")
			return true;
		if (channel == "subjectScreenY")
			return true;
		if (channel == "speed")
			return true;
		return false;
	}

	// Strukturelle Speed-Profile-Pruefung (Plan_B7 Section 8.1: mode "duration"|"speed"; negative
	// Zeiten/Geschwindigkeiten sind bedeutungslos -> 0-Klemme; min<=max; distanceM ist abgeleiteter
	// Cache und wird nicht geprueft).
	static void ValidateSpeedProfile(PCT_SpeedProfile speed, string context)
	{
		if (!speed)
			return;

		if (speed.mode != "duration" && speed.mode != "speed")
		{
			CF_Log.Warn("PCT: Unbekannter " + context + ".mode '" + speed.mode + "', Fallback duration.");
			speed.mode = "duration";
		}

		if (speed.durationSeconds < 0.0)
		{
			CF_Log.Warn("PCT: " + context + ".durationSeconds negativ, auf 0 gesetzt.");
			speed.durationSeconds = 0.0;
		}
		if (speed.speedMS < 0.0)
		{
			CF_Log.Warn("PCT: " + context + ".speedMS negativ, auf 0 gesetzt.");
			speed.speedMS = 0.0;
		}
		if (speed.accelMS2 < 0.0)
			speed.accelMS2 = 0.0;
		if (speed.decelMS2 < 0.0)
			speed.decelMS2 = 0.0;
		if (speed.minSpeedMS < 0.0)
			speed.minSpeedMS = 0.0;
		if (speed.maxSpeedMS < speed.minSpeedMS)
		{
			CF_Log.Warn("PCT: " + context + ".maxSpeedMS < minSpeedMS, auf minSpeedMS angehoben.");
			speed.maxSpeedMS = speed.minSpeedMS;
		}
	}

	static void ValidatePathPoint(PCT_PathPoint point, string curveDefault)
	{
		if (!point)
			return;

		if (!point.speed)
			point.speed = new PCT_SpeedProfile();

		point.pathType = ValidatePathTypeString(point.pathType, curveDefault, "pathPoint.pathType");
		point.behavior = ValidatePointBehaviorString(point.behavior);
		point.easing = ValidateEasingString(point.easing);
		ValidateSpeedProfile(point.speed, "pathPoint.speed");

		if (point.holdSeconds < 0.0)
		{
			CF_Log.Warn("PCT: pathPoint.holdSeconds negativ, auf 0 gesetzt.");
			point.holdSeconds = 0.0;
		}

		// Review-Fix-Wave (Minor-Punkt 6): strukturell gueltig, kein Abbruch -- ohne Handles zieht die
		// Kurve aber Richtung Weltursprung (fehlende Auto-Tangenten, V11 noch offen), daher nur Warnung.
		if (point.pathType == "Bezier" && point.bezierOut == vector.Zero && point.bezierIn == vector.Zero)
			CF_Log.Warn("PCT: Bezier-Segment ohne Handles -- Kurve zieht Richtung Weltursprung (Auto-Tangenten V11 offen).");
	}

	static void ValidateLookPoint(PCT_LookPoint lookPoint)
	{
		if (!lookPoint)
			return;

		if (!lookPoint.timing)
			lookPoint.timing = new PCT_SpeedProfile();

		lookPoint.lookMode = ValidateLookModeString(lookPoint.lookMode);
		lookPoint.easing = ValidateEasingString(lookPoint.easing);
		ValidateSpeedProfile(lookPoint.timing, "lookPoint.timing");

		if (lookPoint.smoothTime < 0.0)
			lookPoint.smoothTime = 0.0;
		if (lookPoint.leadSeconds < 0.0)
			lookPoint.leadSeconds = 0.0;
	}

	// Unbekannter Kanal wird deaktiviert statt geloescht (kein Datenverlust, aber Player ignoriert ihn);
	// Key-Zeiten: negative Zeiten auf 0, unsortierte Keys nur gewarnt (Sortierung ist Editor-Sache).
	static void ValidateTrack(PCT_Track track)
	{
		if (!track)
			return;

		if (!track.keys)
			track.keys = new array<ref PCT_TrackKey>();

		if (!IsKnownTrackChannel(track.channel))
		{
			CF_Log.Warn("PCT: Unbekannter track.channel '" + track.channel + "', Track deaktiviert.");
			track.enabled = false;
		}

		float lastTime = 0.0;
		bool warnedUnsorted = false;
		int keyCount = track.keys.Count();
		for (int i = 0; i < keyCount; i++)
		{
			PCT_TrackKey key = track.keys.Get(i);
			if (!key)
				continue;

			if (key.time < 0.0)
			{
				CF_Log.Warn("PCT: track '" + track.channel + "' Key-Zeit negativ, auf 0 gesetzt.");
				key.time = 0.0;
			}
			if (key.time < lastTime && !warnedUnsorted)
			{
				CF_Log.Warn("PCT: track '" + track.channel + "' Keys nicht zeitlich sortiert.");
				warnedUnsorted = true;
			}
			lastTime = key.time;

			key.easing = ValidateEasingString(key.easing);
		}
	}

	// Plan_B3 Section 2.4: positionCurveType ist "Linear" | "CatmullRom" | "NaturalCubic" | "UniformCubic"
	// (Section 6.2 Regel 4: unbekannt -> Fallback "CatmullRom" + Warn).
	static void ValidatePositionCurveType(PCT_Shot shot)
	{
		if (!shot)
			return;

		if (shot.positionCurveType == "Linear")
			return;
		if (shot.positionCurveType == "CatmullRom")
			return;
		if (shot.positionCurveType == "NaturalCubic")
			return;
		if (shot.positionCurveType == "UniformCubic")
			return;

		CF_Log.Warn("PCT: Shot '" + shot.id + "' hat unbekannten positionCurveType '" + shot.positionCurveType + "', Fallback CatmullRom.");
		shot.positionCurveType = "CatmullRom";
	}

	// ===== Eingebettete-Klassen-Validierung (Plan_B3 Section 6.1 Clamp-Tabellen, vollstaendig -- das
	// Section-6.3-"Muster" clampt nur einen Teil der Keyframe-Felder und keine der uebrigen drei Tabellen;
	// hier werden ALLE in Section 6.1 gelisteten Felder abgedeckt, siehe Task-Brief "Clamping-Tabelle aus
	// dem Plan uebernehmen") =====

	static void ValidateKeyframe(PCT_Keyframe keyframe)
	{
		if (!keyframe)
			return;

		keyframe.time = ClampWarn(keyframe.time, 0.0, 3600.0, "keyframe.time");
		keyframe.fovDegrees = ClampWarn(keyframe.fovDegrees, 1.0, 175.0, "keyframe.fovDegrees");
		keyframe.focusDistance = ClampWarn(keyframe.focusDistance, 0.0, 1000.0, "keyframe.focusDistance");
		keyframe.blurStrength = ClampWarn(keyframe.blurStrength, 0.0, 100.0, "keyframe.blurStrength");

		// orientation = <Yaw, Pitch, Roll> in Grad (Plan_B3 Section 2.2-Kommentar, wie Object.GetOrientation).
		// vector-Komponentenzugriff per Index (scripts - 1.29 ObjectSpawner.c:141/145 verifiziertes Muster
		// "obj.pos[0] = position[0];"), Zwischenvariablen gemaess CLAUDE.md ("komplexe Ausdruecke in
		// Array-Index-Zuweisungen -> Segfault").
		float normalizedYaw = Math.NormalizeAngle(keyframe.orientation[0]);
		keyframe.orientation[0] = normalizedYaw;

		float clampedPitch = ClampWarn(keyframe.orientation[1], -89.0, 89.0, "keyframe.orientation.pitch");
		keyframe.orientation[1] = clampedPitch;

		float clampedRoll = ClampWarn(keyframe.orientation[2], -180.0, 180.0, "keyframe.orientation.roll");
		keyframe.orientation[2] = clampedRoll;

		keyframe.easing = ValidateEasingString(keyframe.easing);
	}

	static void ValidateCameraRig(PCT_CameraRig rig)
	{
		if (!rig)
			return;

		rig.sensorWidthMm = ClampWarn(rig.sensorWidthMm, 1.0, 100.0, "cameraRig.sensorWidthMm");
		rig.sensorHeightMm = ClampWarn(rig.sensorHeightMm, 1.0, 100.0, "cameraRig.sensorHeightMm");
		rig.focalLengthMm = ClampWarn(rig.focalLengthMm, 1.0, 1200.0, "cameraRig.focalLengthMm");
		rig.aperture = ClampWarn(rig.aperture, 0.7, 32.0, "cameraRig.aperture");
		rig.minFocusDistanceM = ClampWarn(rig.minFocusDistanceM, 0.01, 100.0, "cameraRig.minFocusDistanceM");
		rig.anamorphicFactor = ClampWarn(rig.anamorphicFactor, 1.0, 2.0, "cameraRig.anamorphicFactor");
		rig.nearPlaneM = ClampWarn(rig.nearPlaneM, 0.01, 10.0, "cameraRig.nearPlaneM");
	}

	static void ValidateWorldState(PCT_WorldState state)
	{
		if (!state)
			return;

		// "year" hat KEINEN Eintrag in Plan_B3 Section 6.1 -- bewusst ungeklemmt gelassen (keine erfundenen
		// Wertebereiche, CLAUDE.md/Task-Brief).
		state.month = ClampWarnInt(state.month, 1, 12, "worldState.month");
		state.day = ClampWarnInt(state.day, 1, 31, "worldState.day");
		state.hour = ClampWarnInt(state.hour, 0, 23, "worldState.hour");
		state.minute = ClampWarnInt(state.minute, 0, 59, "worldState.minute");
		state.timeMultiplier = ClampWarn(state.timeMultiplier, 0.0, 64.0, "worldState.timeMultiplier");
		state.overcast = ClampWarn(state.overcast, 0.0, 1.0, "worldState.overcast");
		state.fog = ClampWarn(state.fog, 0.0, 1.0, "worldState.fog");
		state.rain = ClampWarn(state.rain, 0.0, 1.0, "worldState.rain");
		state.snowfall = ClampWarn(state.snowfall, 0.0, 1.0, "worldState.snowfall");
		state.windMagnitude = ClampWarn(state.windMagnitude, 0.0, 30.0, "worldState.windMagnitude");

		float normalizedWindDir = Math.NormalizeAngle(state.windDirectionDegrees);
		state.windDirectionDegrees = normalizedWindDir;
	}

	static void ValidateLightSetup(PCT_LightSetup light)
	{
		if (!light)
			return;

		light.colorR = ClampWarn(light.colorR, 0.0, 1.0, "lightSetup.colorR");
		light.colorG = ClampWarn(light.colorG, 0.0, 1.0, "lightSetup.colorG");
		light.colorB = ClampWarn(light.colorB, 0.0, 1.0, "lightSetup.colorB");
		light.brightness = ClampWarn(light.brightness, 0.0, 100.0, "lightSetup.brightness");
		light.radiusM = ClampWarn(light.radiusM, 0.1, 200.0, "lightSetup.radiusM");
		light.spotAngleDegrees = ClampWarn(light.spotAngleDegrees, 1.0, 179.0, "lightSetup.spotAngleDegrees");
	}

	// Fix-Wave (Task P2-4-Review, Minor-Punkt): `defaultSubjectHeightM` (PCT_Settings.c, additiv seit P2-4)
	// hat KEINEN Section-6.1-Tabelleneintrag (das Feld existierte in Plan_B3 noch nicht) -- Wertebereich
	// 0.5..3.0 m (realistische Motivhoehen, Kind bis sehr grosser Erwachsener), geclampt analog den uebrigen
	// Feldern ueber ClampWarn.
	static void ValidateSettings(PCT_Settings settings)
	{
		if (!settings)
			return;

		settings.defaultSubjectHeightM = ClampWarn(settings.defaultSubjectHeightM, 0.5, 3.0, "settings.defaultSubjectHeightM");

		// Task P2-5: "allowCloseFocusOverride" (PCT_Settings.c, additiv seit P2-5) ist bool -- kein
		// numerischer Wertebereich, daher bewusst KEIN ClampWarn-Aufruf (wie "year" in ValidateWorldState
		// oben: keine erfundenen Wertebereiche fuer Felder ohne Tabelleneintrag).
	}

	// ===== Struktur (Plan_B3 Section 6.2) =====

	// Regel 2 Teil 1 (stabile Insertion-Sort ueber .time), zeichengetreu aus Plan_B3 Section 6.3.
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

	// Regel 2 Teil 2 (im Plan-"Muster" Section 6.3 NICHT ausprogrammiert, aber in Section 6.2 explizit
	// gefordert): "Doppelte Zeiten: der spaetere Eintrag wird um 0.001 s verschoben + CF_Log.Warn." Laeuft
	// NACH SortKeyframes (Array ist dann aufsteigend sortiert) -- sequenziell von links, damit eine
	// Verschiebung nicht erneut mit dem naechsten Element kollidiert.
	static void FixDuplicateKeyframeTimes(array<ref PCT_Keyframe> keyframes)
	{
		if (!keyframes)
			return;

		int count = keyframes.Count();
		for (int i = 1; i < count; i++)
		{
			PCT_Keyframe previous = keyframes[i - 1];
			PCT_Keyframe current = keyframes[i];
			if (!previous || !current)
				continue;

			if (current.time <= previous.time)
			{
				float shiftedTime = previous.time + 0.001;
				CF_Log.Warn("PCT: Doppelte Keyframe-Zeit " + current.time + " gefunden, auf " + shiftedTime + " verschoben.");
				current.time = shiftedTime;
			}
		}
	}

	// Wurzel-Validierung fuer PCT_Shot (Plan_B3 Section 4.3: "PCT_Migration.MigrateShot(shot);
	// PCT_Validation.ValidateShot(shot);" -- Reihenfolge/Aufrufort bindend). Deckt Section 6.2 Regeln 1, 3,
	// 4 (2 ueber SortKeyframes/FixDuplicateKeyframeTimes) ab; Regel 5 (fehlende shotIds-Dateien) betrifft
	// PCT_Sequence, die in diesem Mod noch nicht existiert (Phase 3, siehe PCT_Shot.c-Kopfkommentar).
	static void ValidateShot(PCT_Shot shot)
	{
		if (!shot)
			return;

		if (!shot.cameraRig)
			shot.cameraRig = new PCT_CameraRig();
		if (!shot.keyframes)
			shot.keyframes = new array<ref PCT_Keyframe>();

		ValidateCameraRig(shot.cameraRig);

		int count = shot.keyframes.Count();
		for (int i = 0; i < count; i++)
		{
			ValidateKeyframe(shot.keyframes[i]);
		}

		SortKeyframes(shot.keyframes);
		FixDuplicateKeyframeTimes(shot.keyframes);

		ValidatePositionCurveType(shot);

		// worldStateOverride == null ist ein gueltiger Zustand ("Sequenz-Weltzustand erben", Plan_B3
		// Section 2.4) -- NICHT wie cameraRig/keyframes zu einem Default-Objekt aufloesen, nur validieren,
		// falls vorhanden.
		if (shot.worldStateOverride)
			ValidateWorldState(shot.worldStateOverride);

		// ===== Task P3-2: Dual-Path-Strukturen (Plan_B7 Section 13.1; lookPath == null ist gueltig =
		// "kein separater Blickpfad") =====
		if (!shot.cameraPath)
			shot.cameraPath = new PCT_CameraPath();
		if (!shot.tracks)
			shot.tracks = new array<ref PCT_Track>();

		shot.cameraPath.curveDefault = ValidatePathTypeString(shot.cameraPath.curveDefault, "SmoothCurve", "cameraPath.curveDefault");
		if (!shot.cameraPath.points)
			shot.cameraPath.points = new array<ref PCT_PathPoint>();

		int pathPointCount = shot.cameraPath.points.Count();
		for (int p = 0; p < pathPointCount; p++)
		{
			ValidatePathPoint(shot.cameraPath.points.Get(p), shot.cameraPath.curveDefault);
		}

		if (shot.lookPath)
		{
			if (!shot.lookPath.points)
				shot.lookPath.points = new array<ref PCT_LookPoint>();

			int lookPointCount = shot.lookPath.points.Count();
			for (int l = 0; l < lookPointCount; l++)
			{
				ValidateLookPoint(shot.lookPath.points.Get(l));
			}
		}

		int trackCount = shot.tracks.Count();
		for (int t = 0; t < trackCount; t++)
		{
			ValidateTrack(shot.tracks.Get(t));
		}
	}

	// ===== Preset-Container: reine Struktur-Sicherheit (kein Clamp-Tabelleneintrag in Section 6.1 fuer
	// Sensor/Lens/Framing/Angle/Movement-Felder -- keine erfundenen Wertebereiche) plus, wo vorhanden,
	// Validierung der eingebetteten Section-6.1-Klassen (Light/WorldState-Familie referenzieren
	// PCT_LightSetup bzw. PCT_WorldState). =====

	static void ValidateSensorPresetFile(PCT_SensorPresetFile file)
	{
		if (!file)
			return;
		if (!file.presets)
			file.presets = new array<ref PCT_SensorPreset>();
	}

	static void ValidateLensPresetFile(PCT_LensPresetFile file)
	{
		if (!file)
			return;
		if (!file.presets)
			file.presets = new array<ref PCT_LensPreset>();
	}

	// Task P2-5 (Plan_B8 Section 3.2/3.3) -- reine Struktur-Sicherheit wie die uebrigen Preset-Container
	// (kein Clamp-Tabelleneintrag fuer Kit-Felder, keine erfundenen Wertebereiche): stellt nur sicher,
	// dass file.kits und jede eingebettete lensPresetIds-Liste nicht null sind.
	static void ValidateLensKitFile(PCT_LensKitFile file)
	{
		if (!file)
			return;
		if (!file.kits)
			file.kits = new array<ref PCT_LensKit>();

		int count = file.kits.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_LensKit kit = file.kits[i];
			if (!kit)
				continue;
			if (!kit.lensPresetIds)
				kit.lensPresetIds = new array<string>();
		}
	}

	static void ValidateFramingPresetFile(PCT_FramingPresetFile file)
	{
		if (!file)
			return;
		if (!file.presets)
			file.presets = new array<ref PCT_FramingPreset>();
	}

	static void ValidateAnglePresetFile(PCT_AnglePresetFile file)
	{
		if (!file)
			return;
		if (!file.presets)
			file.presets = new array<ref PCT_AnglePreset>();
	}

	static void ValidateMovementPresetFile(PCT_MovementPresetFile file)
	{
		if (!file)
			return;
		if (!file.presets)
			file.presets = new array<ref PCT_MovementPreset>();
	}

	static void ValidateLightPresetFile(PCT_LightPresetFile file)
	{
		if (!file)
			return;
		if (!file.presets)
			file.presets = new array<ref PCT_LightPreset>();

		int count = file.presets.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_LightPreset preset = file.presets[i];
			if (!preset)
				continue;
			if (!preset.lights)
				preset.lights = new array<ref PCT_LightSetup>();

			int lightCount = preset.lights.Count();
			for (int j = 0; j < lightCount; j++)
			{
				ValidateLightSetup(preset.lights[j]);
			}
		}
	}

	static void ValidateWorldStatePresetFile(PCT_WorldStatePresetFile file)
	{
		if (!file)
			return;
		if (!file.presets)
			file.presets = new array<ref PCT_WorldStatePreset>();

		int count = file.presets.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_WorldStatePreset preset = file.presets[i];
			if (!preset)
				continue;
			if (!preset.state)
				preset.state = new PCT_WorldState();

			ValidateWorldState(preset.state);
		}
	}
}

// Migrations-Dispatcher (Plan_B3 Section 5.2-Muster), einmal je Wurzel-Dateityp. MigrateShot ist
// zeichengetreu aus dem Plan uebernommen; die uebrigen Dispatcher folgen exakt demselben Muster
// (schemaVersion <= 0 -> auf 1 anheben, schemaVersion > unterstuetzt -> Warn + read-only/unveraendert
// zurueck, auskommentierte Beispielkette fuer kuenftige Versionsspruenge). Ein Dispatcher pro Typ statt
// eines generischen Helpers -- CLAUDE.md "Avoid unnecessary abstraction"/"Prefer minimal, targeted fixes":
// EnScript-Template-Klassen (wie JsonFileLoader<Class T>) koennten schemaVersion nur ueber eine gemeinsame
// Basisklasse generisch anfassen, die fuer die bestehenden (P2-1) Datenklassen nicht existiert und hier
// nicht nachtraeglich eingezogen wird.
class PCT_Migration
{
	// Zeichengetreu aus Plan_B3 Section 5.2.
	static void MigrateShot(PCT_Shot shot)
	{
		if (!shot)
			return;

		if (shot.schemaVersion <= 0)
			shot.schemaVersion = 1;

		if (shot.schemaVersion > PCT_Constants.SCHEMA_VERSION_SHOT)
		{
			CF_Log.Warn("PCT: Shot '" + shot.id + "' hat schemaVersion " + shot.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_SHOT + ") -- read-only.");
			return;
		}

		// Task P3-2: v1 -> v2 (Dual-Path, Plan_B7 Section 13.3 / Plan_B3 Section 5.2-Kette).
		if (shot.schemaVersion == 1)
		{
			MigrateShotV1toV2(shot);
			shot.schemaVersion = 2;
		}
	}

	// Konvertierung EPCT_CurveType-String (v1 positionCurveType) -> EPCT_PathType-String, exakt nach der
	// Plan_B7-Section-13.3-Tabelle. KEINE direkte Zuweisung: die Wertemengen sind inkompatibel
	// ("CatmullRom"/"NaturalCubic"/"UniformCubic" sind keine gueltigen EPCT_PathType-Strings).
	static string MapCurveTypeToPathType(string curveType)
	{
		if (curveType == "Linear")
			return "Linear";
		if (curveType == "CatmullRom")
			return "SmoothCurve";
		if (curveType == "NaturalCubic")
		{
			CF_Log.Warn("PCT: positionCurveType 'NaturalCubic' hat kein exaktes EPCT_PathType-Gegenstueck, Fallback SmoothCurve (V13).");
			return "SmoothCurve";
		}
		if (curveType == "UniformCubic")
		{
			CF_Log.Warn("PCT: positionCurveType 'UniformCubic' hat kein exaktes EPCT_PathType-Gegenstueck, Fallback SmoothCurve (V13).");
			return "SmoothCurve";
		}

		CF_Log.Warn("PCT: Unbekannter positionCurveType '" + curveType + "', Fallback SmoothCurve.");
		return "SmoothCurve";
	}

	static void AddTrackKey(PCT_Track track, float time, float value, string easing)
	{
		PCT_TrackKey key = new PCT_TrackKey();
		key.time = time;
		key.value = value;
		key.easing = easing;
		track.keys.Insert(key);
	}

	// Plan_B7 Section 13.3: alter Single-Stream-Shot (nur keyframes[]) laedt als Kamerapfad OHNE separaten
	// Blickpfad. Schritt 1: je Keyframe ein PCT_PathPoint (behavior ExactThrough, Dauer = Zeitdifferenz zum
	// naechsten Keyframe, pathType = MapCurveTypeToPathType). Schritt 2: Tracks aus den Keyframe-Kanaelen
	// (focalLength via Rig-Rueckrechnung, focusDistance, pan/tilt/roll aus orientation). Schritt 3:
	// lookPath = null (Orientierung aus den Tracks, Modus FreeRotation -- exakt wie B4 interpolierte).
	// Schritt 4: keyframes[] bleibt als Kompat-/Export-Sicht erhalten.
	// KEIN aperture-Track: das v1-Keyframe traegt blurStrength (0..100, dimensionslos), keinen f-Stop --
	// eine Rueckrechnung waere erfunden (Section 13.3 nennt "aperture/blur" nur als Kanal-Kandidaten).
	static void MigrateShotV1toV2(PCT_Shot shot)
	{
		if (!shot)
			return;

		if (!shot.cameraPath)
			shot.cameraPath = new PCT_CameraPath();
		if (!shot.tracks)
			shot.tracks = new array<ref PCT_Track>();

		string mappedType = MapCurveTypeToPathType(shot.positionCurveType);
		shot.cameraPath.curveDefault = mappedType;

		int keyframeCount = 0;
		if (shot.keyframes)
			keyframeCount = shot.keyframes.Count();

		// Idempotenz: bereits vorhandene Dual-Path-Daten (oder leere v1-Shots) nicht ueberschreiben.
		if (keyframeCount == 0)
			return;
		if (shot.cameraPath.points.Count() > 0)
			return;

		for (int i = 0; i < keyframeCount; i++)
		{
			PCT_Keyframe kf = shot.keyframes.Get(i);
			if (!kf)
				continue;

			PCT_PathPoint point = new PCT_PathPoint();
			point.position = kf.position;
			point.pathType = mappedType;
			point.easing = kf.easing;
			point.behavior = "ExactThrough";

			float segmentDuration = 0.0;
			int nextIndex = i + 1;
			if (nextIndex < keyframeCount)
			{
				PCT_Keyframe nextKf = shot.keyframes.Get(nextIndex);
				if (nextKf)
					segmentDuration = nextKf.time - kf.time;
			}
			if (segmentDuration < 0.0)
				segmentDuration = 0.0;

			point.speed.mode = "duration";
			point.speed.durationSeconds = segmentDuration;

			shot.cameraPath.points.Insert(point);
		}

		if (shot.tracks.Count() == 0)
		{
			PCT_Track focalTrack = new PCT_Track();
			focalTrack.channel = "focalLength";
			PCT_Track focusTrack = new PCT_Track();
			focusTrack.channel = "focusDistance";
			PCT_Track panTrack = new PCT_Track();
			panTrack.channel = "pan";
			PCT_Track tiltTrack = new PCT_Track();
			tiltTrack.channel = "tilt";
			PCT_Track rollTrack = new PCT_Track();
			rollTrack.channel = "roll";

			float sensorHeight = 24.0;
			if (shot.cameraRig)
				sensorHeight = shot.cameraRig.sensorHeightMm;

			for (int k = 0; k < keyframeCount; k++)
			{
				PCT_Keyframe trackKf = shot.keyframes.Get(k);
				if (!trackKf)
					continue;

				float fovRad = trackKf.fovDegrees * Math.DEG2RAD;
				float focalMm = PCT_Math.FocalFromFovV(sensorHeight, fovRad);
				AddTrackKey(focalTrack, trackKf.time, focalMm, trackKf.easing);
				AddTrackKey(focusTrack, trackKf.time, trackKf.focusDistance, trackKf.easing);

				float yawValue = trackKf.orientation[0];
				AddTrackKey(panTrack, trackKf.time, yawValue, trackKf.easing);
				float pitchValue = trackKf.orientation[1];
				AddTrackKey(tiltTrack, trackKf.time, pitchValue, trackKf.easing);
				float rollValue = trackKf.orientation[2];
				AddTrackKey(rollTrack, trackKf.time, rollValue, trackKf.easing);
			}

			shot.tracks.Insert(focalTrack);
			shot.tracks.Insert(focusTrack);
			shot.tracks.Insert(panTrack);
			shot.tracks.Insert(tiltTrack);
			shot.tracks.Insert(rollTrack);
		}

		shot.lookPath = null;
	}

	static void MigrateSettings(PCT_Settings settings)
	{
		if (!settings)
			return;

		if (settings.schemaVersion <= 0)
			settings.schemaVersion = 1;

		if (settings.schemaVersion > PCT_Constants.SCHEMA_VERSION_SETTINGS)
		{
			CF_Log.Warn("PCT: Settings.json hat schemaVersion " + settings.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_SETTINGS + ") -- read-only.");
			return;
		}
	}

	static void MigrateSensorPresetFile(PCT_SensorPresetFile file)
	{
		if (!file)
			return;

		if (file.schemaVersion <= 0)
			file.schemaVersion = 1;

		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: Presets/Sensors.json hat schemaVersion " + file.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_PRESETS + ") -- read-only.");
			return;
		}
	}

	static void MigrateLensPresetFile(PCT_LensPresetFile file)
	{
		if (!file)
			return;

		if (file.schemaVersion <= 0)
			file.schemaVersion = 1;

		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: Presets/Lenses.json hat schemaVersion " + file.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_PRESETS + ") -- read-only.");
			return;
		}
	}

	// Task P2-5 -- mirrors MigrateLensPresetFile above, siehe dortigen Dispatcher-Klassenkommentar.
	static void MigrateLensKitFile(PCT_LensKitFile file)
	{
		if (!file)
			return;

		if (file.schemaVersion <= 0)
			file.schemaVersion = 1;

		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: Presets/LensKits.json hat schemaVersion " + file.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_PRESETS + ") -- read-only.");
			return;
		}
	}

	static void MigrateFramingPresetFile(PCT_FramingPresetFile file)
	{
		if (!file)
			return;

		if (file.schemaVersion <= 0)
			file.schemaVersion = 1;

		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: Presets/Framings.json hat schemaVersion " + file.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_PRESETS + ") -- read-only.");
			return;
		}
	}

	static void MigrateAnglePresetFile(PCT_AnglePresetFile file)
	{
		if (!file)
			return;

		if (file.schemaVersion <= 0)
			file.schemaVersion = 1;

		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: Presets/Angles.json hat schemaVersion " + file.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_PRESETS + ") -- read-only.");
			return;
		}
	}

	static void MigrateMovementPresetFile(PCT_MovementPresetFile file)
	{
		if (!file)
			return;

		if (file.schemaVersion <= 0)
			file.schemaVersion = 1;

		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: Presets/Movements.json hat schemaVersion " + file.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_PRESETS + ") -- read-only.");
			return;
		}
	}

	static void MigrateLightPresetFile(PCT_LightPresetFile file)
	{
		if (!file)
			return;

		if (file.schemaVersion <= 0)
			file.schemaVersion = 1;

		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: Presets/Lights.json hat schemaVersion " + file.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_PRESETS + ") -- read-only.");
			return;
		}
	}

	static void MigrateWorldStatePresetFile(PCT_WorldStatePresetFile file)
	{
		if (!file)
			return;

		if (file.schemaVersion <= 0)
			file.schemaVersion = 1;

		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: Presets/WorldStates.json hat schemaVersion " + file.schemaVersion + " (unterstuetzt: " + PCT_Constants.SCHEMA_VERSION_PRESETS + ") -- read-only.");
			return;
		}
	}
}

// PCT_Storage -- JsonFileLoader-Wrapper, $profile-Ordner-Anlage, Laden/Speichern je Typ (Plan_B3 Section 4).
// Rein client-seitig: Persistenz ist laut Plan_B1_Architektur.md Section 7 (Server-Autoritaet &
// Replikationsmodell) sowie Plan_B3 Section 7.1 ("Phase 1-3: rein clientseitig ... alle PCT-Dateien liegen
// im $profile des erstellenden Clients") ein reiner Client-Mechanismus -- diese Klasse enthaelt daher
// keine IsDedicatedServer()-Verzweigung (ein dedizierter Server hat i.d.R. kein sinnvolles $profile fuer
// Kreativ-Daten und ruft diese Klasse in Phase 1-3 nicht auf; Aufrufer sind Form/Modul-Code in 5_Mission,
// der bereits client-seitig gated ist, siehe PCT_CinematicModule.c EnsureOverlayHud-Muster).
class PCT_Storage
{
	// ===== Init (Plan_B3 Section 4.2) =====

	// Lazy: jede oeffentliche Load/Save-Methode ruft dies zuerst auf (idempotent, wie COT es bei jedem
	// Start tut -- Plan_B3 Section 4.2 "COT ruft es idempotent bei jedem Start auf"). Kein Aufruf aus einem
	// Modul-Konstruktor noetig; das erspart eine Abhaengigkeit von 5_Mission-Lifecycle-Details, die nicht
	// Teil dieses Tasks sind. DIR_PROJECTS wird bewusst NICHT angelegt (Phase 3, exakt wie Plan_B3
	// Section 4.2 Kommentar "(DIR_PROJECTS kommt in Phase 3 dazu.)").
	static void EnsureDirectories()
	{
		MakeDirectory(PCT_Constants.DIR_ROOT);
		MakeDirectory(PCT_Constants.DIR_SHOTS);
		MakeDirectory(PCT_Constants.DIR_SEQUENCES);
		MakeDirectory(PCT_Constants.DIR_PRESETS);
		MakeDirectory(PCT_Constants.DIR_EXPORTS);
	}

	// ===== Settings (Plan_B3 Section 4.3, Task-Brief Punkt 2) =====

	static PCT_Settings LoadSettings()
	{
		EnsureDirectories();

		PCT_Settings settings = new PCT_Settings();

		if (FileExist(PCT_Constants.FILE_SETTINGS))
		{
			string errorMessage;
			bool loadedOk = JsonFileLoader<PCT_Settings>.LoadFile(PCT_Constants.FILE_SETTINGS, settings, errorMessage);
			if (!loadedOk)
			{
				// Section 4.5: defekte Datei -> loggen, NICHT ueberschreiben, Skip (kein SaveSettings-Aufruf
				// hier) -- der Nutzer kann die defekte Datei so noch von Hand reparieren/inspizieren.
				CF_Log.Warn("PCT: Settings.json ist defekt, Defaults werden nur im Speicher verwendet (Datei bleibt unveraendert): " + errorMessage);
				settings = new PCT_Settings();
				PCT_Migration.MigrateSettings(settings);
				PCT_Validation.ValidateSettings(settings);
				return settings;
			}

			PCT_Migration.MigrateSettings(settings);
			PCT_Validation.ValidateSettings(settings);
			return settings;
		}

		// Datei fehlt -> Defaults + sofort speichern (Plan_B3 Section 4.3 LoadSettings-Muster).
		PCT_Migration.MigrateSettings(settings);
		PCT_Validation.ValidateSettings(settings);
		SaveSettings(settings);
		return settings;
	}

	static bool SaveSettings(PCT_Settings settings)
	{
		if (!settings)
			return false;

		if (settings.schemaVersion > PCT_Constants.SCHEMA_VERSION_SETTINGS)
		{
			CF_Log.Warn("PCT: SaveSettings verweigert -- schemaVersion " + settings.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_SETTINGS + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();
		settings.schemaVersion = PCT_Constants.SCHEMA_VERSION_SETTINGS;

		string errorMessage;
		bool saved = JsonFileLoader<PCT_Settings>.SaveFile(PCT_Constants.FILE_SETTINGS, settings, errorMessage);
		if (!saved)
			CF_Log.Warn("PCT: SaveSettings fehlgeschlagen: " + errorMessage);

		return saved;
	}

	// ===== Shots (Plan_B3 Section 4.3/4.4, Task-Brief Punkt 3) =====

	// Zeichengetreu an Plan_B3 Section 4.3 orientiert, mit LoadFile/SaveFile statt JsonLoadFile/JsonSaveFile
	// (siehe Datei-Kopfkommentar "Abweichung vom woertlichen Plan-Code").
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
			CF_Log.Warn("PCT: SaveShot fuer '" + shot.id + "' verweigert -- schemaVersion " + shot.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_SHOT + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();

		shot.schemaVersion = PCT_Constants.SCHEMA_VERSION_SHOT;
		string filePath = PCT_Constants.DIR_SHOTS + shot.id + PCT_Constants.EXT_JSON;

		string errorMessage;
		bool saved = JsonFileLoader<PCT_Shot>.SaveFile(filePath, shot, errorMessage);
		if (!saved)
			CF_Log.Warn("PCT: SaveShot fehlgeschlagen fuer '" + filePath + "': " + errorMessage);

		return saved;
	}

	static PCT_Shot LoadShot(string shotId)
	{
		EnsureDirectories();

		string filePath = PCT_Constants.DIR_SHOTS + shotId + PCT_Constants.EXT_JSON;
		if (!FileExist(filePath))
		{
			CF_Log.Warn("PCT: Shot-Datei fehlt: " + filePath);
			return null;
		}

		PCT_Shot shot = new PCT_Shot();
		string errorMessage;
		bool loadedOk = JsonFileLoader<PCT_Shot>.LoadFile(filePath, shot, errorMessage);
		if (!loadedOk)
		{
			// Section 4.5: defekte Datei -> loggen, Datei NICHT ueberschreiben (kein Save hier), Skip (null).
			CF_Log.Warn("PCT: Shot-Datei '" + filePath + "' ist defekt, wird uebersprungen (Datei bleibt unveraendert): " + errorMessage);
			return null;
		}

		PCT_Migration.MigrateShot(shot);
		PCT_Validation.ValidateShot(shot);
		return shot;
	}

	// Task-Brief Punkt 3: "LoadAllShots() (Verzeichnis-Listing)". Defekte/verschwundene Einzeldateien werden
	// stillschweigend uebersprungen (LoadShot liefert dafuer bereits null + eigenes Warn), der Browser sieht
	// so nur ladbare Shots.
	static void LoadAllShots(array<ref PCT_Shot> outShots)
	{
		if (!outShots)
			return;
		outShots.Clear();

		EnsureDirectories();

		TStringArray shotIds = new TStringArray();
		ListJsonIds(PCT_Constants.DIR_SHOTS, shotIds);

		int count = shotIds.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_Shot shot = LoadShot(shotIds[i]);
			if (shot)
				outShots.Insert(shot);
		}
	}

	// Task-Brief Punkt 3: "DeleteShot(id) (Datei loeschen -- API nachschlagen; KEIN EnScript-delete-Keyword)".
	// DeleteFile (EnSystem.c:528) ist die Datei-API, siehe Datei-Kopfkommentar-Tabelle.
	static bool DeleteShot(string shotId)
	{
		if (shotId == "")
			return false;

		string filePath = PCT_Constants.DIR_SHOTS + shotId + PCT_Constants.EXT_JSON;
		if (!FileExist(filePath))
		{
			CF_Log.Warn("PCT: DeleteShot -- Datei nicht gefunden: " + filePath);
			return false;
		}

		bool deleted = DeleteFile(filePath);
		if (!deleted)
			CF_Log.Warn("PCT: DeleteShot fehlgeschlagen (DeleteFile): " + filePath);

		return deleted;
	}

	// Task-Brief Punkt 3: "RenameShot". Kein Signatur-Vorbild im Plan -- Entscheidung: id ist der
	// Dateiname (Plan_B3 Section 2.4 "id = Dateiname ohne .json"), Rename = unter neuer id speichern, dann
	// alte Datei loeschen. Ziel-id darf nicht bereits existieren (kein stilles Ueberschreiben fremder
	// Shots); die alte Datei wird erst NACH erfolgreichem Save der neuen geloescht (kein Datenverlust bei
	// Fehlschlag).
	static bool RenameShot(string oldId, string newId)
	{
		if (oldId == "" || newId == "")
			return false;
		if (oldId == newId)
			return true;

		string newFilePath = PCT_Constants.DIR_SHOTS + newId + PCT_Constants.EXT_JSON;
		if (FileExist(newFilePath))
		{
			CF_Log.Warn("PCT: RenameShot -- Ziel-id '" + newId + "' existiert bereits, abgebrochen.");
			return false;
		}

		PCT_Shot shot = LoadShot(oldId);
		if (!shot)
		{
			CF_Log.Warn("PCT: RenameShot -- Quell-Shot '" + oldId + "' konnte nicht geladen werden.");
			return false;
		}

		shot.id = newId;
		bool saved = SaveShot(shot);
		if (!saved)
			return false;

		string oldFilePath = PCT_Constants.DIR_SHOTS + oldId + PCT_Constants.EXT_JSON;
		DeleteFile(oldFilePath);
		return true;
	}

	// ===== Datei-Enumeration (Plan_B3 Section 4.4) =====

	// Gegenueber dem Plan-"Muster" (Section 4.4) um einen Guard NACH FindFile ergaenzt: `if (!handle)
	// return;` VOR der Schleife. Beantwortet die dort selbst als "OFFEN -- ZU VERIFIZIEREN: Verhalten von
	// FindFile bei leerem Verzeichnis" markierte Frage anhand des verifizierten CF-Referenzcodes
	// (DayZ-CommunityFramework-production\JM\CF\Scripts\1_Core\CommunityFramework\Files\CF_Directory.c:25-29,
	// `CF_Directory.GetFiles`: "FindFileHandle handle = FindFile(...); if (!handle) return false;" -- CF ist
	// eine Pflichtabhaengigkeit dieses Mods und laut CLAUDE.md Rang 2 der autoritativen Quellen fuer
	// Framework-Konventionen). Ohne den Guard wuerde ein leeres Shots\-Verzeichnis FindNextFile/
	// CloseFindFile mit einem potenziell ungueltigen Handle aufrufen.
	static void ListJsonIds(string directory, TStringArray outIds)
	{
		if (!outIds)
			return;
		outIds.Clear();

		string fileName;
		FileAttr attributes;
		FindFileHandle handle = FindFile(directory + "*" + PCT_Constants.EXT_JSON, fileName, attributes, FindFileFlags.DIRECTORIES);
		if (!handle)
			return;

		bool hasNext = true;
		while (hasNext)
		{
			if (fileName != "")
			{
				string idOnly = fileName;
				idOnly.Replace(PCT_Constants.EXT_JSON, "");
				outIds.Insert(idOnly);
			}
			hasNext = FindNextFile(handle, fileName, attributes);
		}

		CloseFindFile(handle);
	}

	// ===== Preset-Familien (Plan_B3 Section 2.10/4.1, Task-Brief Punkt 4) =====
	//
	// Builtin-Regenerations-/Merge-Regel (Plan_B3 Section 2.10): "mitgelieferte Presets werden bei jedem
	// Start aus Code-Defaults regeneriert, Nutzer-Presets (isBuiltin = false) bleiben unangetastet."
	// Umsetzung je Familie identisch: alle geladenen Eintraege mit isBuiltin == true werden aus dem Array
	// entfernt, dann wird der vollstaendige aktuelle Code-Default-Satz (CreateBuiltin<Familie>Presets)
	// angehaengt. Ergebnis-Tabelle (Task-Brief Selbstpruefung Punkt 3):
	//   | Fall                                   | Ergebnis                                            |
	//   |-----------------------------------------|------------------------------------------------------|
	//   | builtin-id in Datei vorhanden            | alter Eintrag entfernt, durch aktuellen Code-Default  |
	//   |                                           | mit gleicher id ersetzt (Werte ggf. aktualisiert)     |
	//   | builtin-id in Datei NICHT vorhanden      | Code-Default wird neu hinzugefuegt                    |
	//   |   (neu im Code seit letztem Speichern)   |                                                        |
	//   | Nutzer-id (isBuiltin = false)             | unveraendert erhalten, vom Merge nicht angefasst      |
	// Wie bei den Presets-Load-Methoden gilt: eine defekte Datei wird NICHT ueberschrieben (kein
	// Save-Aufruf), ein frisches In-Memory-Objekt mit nur den Code-Builtins wird trotzdem zurueckgegeben
	// (Tool bleibt bedienbar, Grundsatz "niemals Laden verweigern", Section 4.5/6).

	// Hinweis Duplikat-ids: eine Nutzer-Preset mit isBuiltin = false, deren id zufaellig mit einer
	// builtin-id kollidiert, wird HIER bewusst nicht dedupliziert (der Merge entfernt nur isBuiltin ==
	// true-Eintraege, siehe Schleife unten) -- Plan_B3 Section 6 schreibt keine id-Eindeutigkeit ueber
	// Nutzer-/Builtin-Grenzen hinweg vor. Ein spaeterer Lookup-by-id (z.B. Dropdown-Anzeige) nimmt in
	// diesem Fall den ERSTEN Treffer im Array (Builtins zuerst, s.u.).
	static void MergeBuiltinSensorPresets(array<ref PCT_SensorPreset> presets)
	{
		if (!presets)
			return;

		int i = presets.Count() - 1;
		while (i >= 0)
		{
			PCT_SensorPreset entry = presets[i];
			if (entry && entry.isBuiltin)
				presets.RemoveOrdered(i);
			i = i - 1;
		}

		array<ref PCT_SensorPreset> builtins = new array<ref PCT_SensorPreset>();
		CreateBuiltinSensorPresets(builtins);

		int count = builtins.Count();
		for (int b = 0; b < count; b++)
		{
			presets.Insert(builtins[b]);
		}
	}

	// Phase-2-Minimalsatz (Task-Brief): Sensor-Familie aus Docs/Plan_B4_Kamera_Engine.md Section 2.3
	// ("Sensor-Presets"-Tabelle), Werte zeichengetreu uebernommen (Breite/Hoehe mm, Crop-Faktor). displayName
	// ist im Plan nicht woertlich vorgegeben (nur die "Bemerkung"-Spalte) -- fuer sensor_fullframe exakt aus
	// dem in Plan_B3 Section 8.1 (ShotList-CSV-Beispiel, Zeile "sensor") genannten Anzeigetext "Full Frame
	// 36x24" uebernommen; die uebrigen fuenf analog aus der Bemerkung/dem Preset-Namen abgeleitet (keine
	// erfundenen Zahlenwerte, nur die Anzeigebezeichnung).
	static void CreateBuiltinSensorPresets(array<ref PCT_SensorPreset> outPresets)
	{
		if (!outPresets)
			return;

		PCT_SensorPreset fullFrame = new PCT_SensorPreset();
		fullFrame.id = "sensor_fullframe";
		fullFrame.displayName = "Full Frame 36x24";
		fullFrame.widthMm = 36.0;
		fullFrame.heightMm = 24.0;
		fullFrame.cropFactor = 1.00;
		fullFrame.isBuiltin = true;
		outPresets.Insert(fullFrame);

		PCT_SensorPreset super35 = new PCT_SensorPreset();
		super35.id = "sensor_super35";
		super35.displayName = "Super 35 (Cine)";
		super35.widthMm = 24.89;
		super35.heightMm = 18.66;
		super35.cropFactor = 1.39;
		super35.isBuiltin = true;
		outPresets.Insert(super35);

		PCT_SensorPreset apsc = new PCT_SensorPreset();
		apsc.id = "sensor_apsc";
		apsc.displayName = "APS-C";
		apsc.widthMm = 23.6;
		apsc.heightMm = 15.6;
		apsc.cropFactor = 1.53;
		apsc.isBuiltin = true;
		outPresets.Insert(apsc);

		PCT_SensorPreset mft = new PCT_SensorPreset();
		mft.id = "sensor_mft";
		mft.displayName = "Micro Four Thirds";
		mft.widthMm = 17.3;
		mft.heightMm = 13.0;
		mft.cropFactor = 2.00;
		mft.isBuiltin = true;
		outPresets.Insert(mft);

		PCT_SensorPreset oneInch = new PCT_SensorPreset();
		oneInch.id = "sensor_1inch";
		oneInch.displayName = "1 Zoll";
		oneInch.widthMm = 13.2;
		oneInch.heightMm = 8.8;
		oneInch.cropFactor = 2.73;
		oneInch.isBuiltin = true;
		outPresets.Insert(oneInch);

		PCT_SensorPreset smartphone = new PCT_SensorPreset();
		smartphone.id = "sensor_smartphone";
		smartphone.displayName = "Smartphone";
		smartphone.widthMm = 9.8;
		smartphone.heightMm = 7.3;
		smartphone.cropFactor = 3.54;
		smartphone.isBuiltin = true;
		outPresets.Insert(smartphone);
	}

	static PCT_SensorPresetFile LoadSensorPresets()
	{
		EnsureDirectories();

		PCT_SensorPresetFile file = new PCT_SensorPresetFile();
		bool skipAutoSave = false;

		if (FileExist(PCT_Constants.FILE_PRESETS_SENSORS))
		{
			string errorMessage;
			bool loadedOk = JsonFileLoader<PCT_SensorPresetFile>.LoadFile(PCT_Constants.FILE_PRESETS_SENSORS, file, errorMessage);
			if (!loadedOk)
			{
				CF_Log.Warn("PCT: Presets/Sensors.json ist defekt, wird uebersprungen (Datei bleibt unveraendert): " + errorMessage);
				file = new PCT_SensorPresetFile();
				skipAutoSave = true;
			}
		}

		PCT_Migration.MigrateSensorPresetFile(file);
		PCT_Validation.ValidateSensorPresetFile(file);
		MergeBuiltinSensorPresets(file.presets);

		if (!skipAutoSave)
			SaveSensorPresets(file);

		return file;
	}

	static bool SaveSensorPresets(PCT_SensorPresetFile file)
	{
		if (!file)
			return false;
		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: SaveSensorPresets verweigert -- schemaVersion " + file.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_PRESETS + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();
		file.schemaVersion = PCT_Constants.SCHEMA_VERSION_PRESETS;

		string errorMessage;
		bool saved = JsonFileLoader<PCT_SensorPresetFile>.SaveFile(PCT_Constants.FILE_PRESETS_SENSORS, file, errorMessage);
		if (!saved)
			CF_Log.Warn("PCT: SaveSensorPresets fehlgeschlagen: " + errorMessage);

		return saved;
	}

	// ---- Lens-Familie: Builtins folgen in P2-5 (Task-Brief) -- Hook bewusst leer, keine erfundenen
	// Objektiv-Presets (Plan_B3 Section 2.10 "KEINE erfundenen Preset-Inhalte"). ----

	static void MergeBuiltinLensPresets(array<ref PCT_LensPreset> presets)
	{
		if (!presets)
			return;

		int i = presets.Count() - 1;
		while (i >= 0)
		{
			PCT_LensPreset entry = presets[i];
			if (entry && entry.isBuiltin)
				presets.RemoveOrdered(i);
			i = i - 1;
		}

		array<ref PCT_LensPreset> builtins = new array<ref PCT_LensPreset>();
		CreateBuiltinLensPresets(builtins);

		int count = builtins.Count();
		for (int b = 0; b < count; b++)
		{
			presets.Insert(builtins[b]);
		}
	}

	// Task P2-5 (Docs/Plan_B8_Objektivsystem_LensKits.md Section 4.1, Quelle Docs/ZEISS Supreme Prime
	// Radiance.md Tabelle + 65-mm-Absatz): 8 Builtin-Linsen des ZEISS-Supreme-Prime-Radiance-Sets, Werte
	// EXAKT aus der Plan-Tabelle uebernommen -- keine erfundenen Werte. Gemeinsame Felder (Plan_B8
	// Section 4.1-Kopfzeile): apertureScale="T", maxAperture=1.5, minAperture=22.0, frontDiameterMm=95.0,
	// anamorphicFactor=1.0, focalLengthMaxMm=0.0 (Festbrennweiten), isBuiltin=true, kitId="kit_zsr_7_metric"
	// (ausser 65 mm: kitId="" -- Einzellinse, nicht Teil des 7er-Kits, Entscheidung Plan_B8 Section 2.5).
	// 65 mm: weightKg/lengthMm bleiben 0.0 ("nicht dokumentiert" -- in der Quelle nicht genannt, Plan_B8
	// Section 4.1 Fussnote 2). displayName-Muster "ZEISS Radiance <f> mm T1.5" (Plan_B8 Section 4.1).
	static void CreateBuiltinLensPresets(array<ref PCT_LensPreset> outPresets)
	{
		if (!outPresets)
			return;

		PCT_LensPreset lens21 = new PCT_LensPreset();
		lens21.id = "lens_zsr_21_t15";
		lens21.displayName = "ZEISS Radiance 21 mm T1.5";
		lens21.focalLengthMm = 21.0;
		lens21.focalLengthMaxMm = 0.0;
		lens21.maxAperture = 1.5;
		lens21.minFocusDistanceM = 0.35;
		lens21.anamorphicFactor = 1.0;
		lens21.isBuiltin = true;
		lens21.minAperture = 22.0;
		lens21.apertureScale = "T";
		lens21.weightKg = 1.50;
		lens21.lengthMm = 119.0;
		lens21.frontDiameterMm = 95.0;
		lens21.kitId = "kit_zsr_7_metric";
		lens21.hFovFullFrameDeg = 79.5;
		lens21.hFovSuper35Deg = 59.8;
		outPresets.Insert(lens21);

		PCT_LensPreset lens25 = new PCT_LensPreset();
		lens25.id = "lens_zsr_25_t15";
		lens25.displayName = "ZEISS Radiance 25 mm T1.5";
		lens25.focalLengthMm = 25.0;
		lens25.focalLengthMaxMm = 0.0;
		lens25.maxAperture = 1.5;
		lens25.minFocusDistanceM = 0.26;
		lens25.anamorphicFactor = 1.0;
		lens25.isBuiltin = true;
		lens25.minAperture = 22.0;
		lens25.apertureScale = "T";
		lens25.weightKg = 1.42;
		lens25.lengthMm = 119.0;
		lens25.frontDiameterMm = 95.0;
		lens25.kitId = "kit_zsr_7_metric";
		lens25.hFovFullFrameDeg = 70.8;
		lens25.hFovSuper35Deg = 52.3;
		outPresets.Insert(lens25);

		PCT_LensPreset lens29 = new PCT_LensPreset();
		lens29.id = "lens_zsr_29_t15";
		lens29.displayName = "ZEISS Radiance 29 mm T1.5";
		lens29.focalLengthMm = 29.0;
		lens29.focalLengthMaxMm = 0.0;
		lens29.maxAperture = 1.5;
		lens29.minFocusDistanceM = 0.33;
		lens29.anamorphicFactor = 1.0;
		lens29.isBuiltin = true;
		lens29.minAperture = 22.0;
		lens29.apertureScale = "T";
		lens29.weightKg = 1.61;
		lens29.lengthMm = 119.0;
		lens29.frontDiameterMm = 95.0;
		lens29.kitId = "kit_zsr_7_metric";
		lens29.hFovFullFrameDeg = 64.0;
		lens29.hFovSuper35Deg = 46.8;
		outPresets.Insert(lens29);

		PCT_LensPreset lens35 = new PCT_LensPreset();
		lens35.id = "lens_zsr_35_t15";
		lens35.displayName = "ZEISS Radiance 35 mm T1.5";
		lens35.focalLengthMm = 35.0;
		lens35.focalLengthMaxMm = 0.0;
		lens35.maxAperture = 1.5;
		lens35.minFocusDistanceM = 0.32;
		lens35.anamorphicFactor = 1.0;
		lens35.isBuiltin = true;
		lens35.minAperture = 22.0;
		lens35.apertureScale = "T";
		lens35.weightKg = 1.40;
		lens35.lengthMm = 119.0;
		lens35.frontDiameterMm = 95.0;
		lens35.kitId = "kit_zsr_7_metric";
		lens35.hFovFullFrameDeg = 55.0;
		lens35.hFovSuper35Deg = 39.6;
		outPresets.Insert(lens35);

		PCT_LensPreset lens50 = new PCT_LensPreset();
		lens50.id = "lens_zsr_50_t15";
		lens50.displayName = "ZEISS Radiance 50 mm T1.5";
		lens50.focalLengthMm = 50.0;
		lens50.focalLengthMaxMm = 0.0;
		lens50.maxAperture = 1.5;
		lens50.minFocusDistanceM = 0.45;
		lens50.anamorphicFactor = 1.0;
		lens50.isBuiltin = true;
		lens50.minAperture = 22.0;
		lens50.apertureScale = "T";
		lens50.weightKg = 1.22;
		lens50.lengthMm = 119.0;
		lens50.frontDiameterMm = 95.0;
		lens50.kitId = "kit_zsr_7_metric";
		lens50.hFovFullFrameDeg = 39.0;
		lens50.hFovSuper35Deg = 27.5;
		outPresets.Insert(lens50);

		// 65 mm: Einzellinse (kitId="" -- Plan_B8 Section 2.5, NICHT Teil des 7er-Kits, "logischste
		// Ergaenzung" laut Quelle). weightKg/lengthMm bleiben 0.0 -- in der Quelle nicht dokumentiert
		// (Plan_B8 Section 4.1 Fussnote 2), keine erfundenen Werte.
		PCT_LensPreset lens65 = new PCT_LensPreset();
		lens65.id = "lens_zsr_65_t15";
		lens65.displayName = "ZEISS Radiance 65 mm T1.5";
		lens65.focalLengthMm = 65.0;
		lens65.focalLengthMaxMm = 0.0;
		lens65.maxAperture = 1.5;
		lens65.minFocusDistanceM = 0.60;
		lens65.anamorphicFactor = 1.0;
		lens65.isBuiltin = true;
		lens65.minAperture = 22.0;
		lens65.apertureScale = "T";
		lens65.weightKg = 0.0;
		lens65.lengthMm = 0.0;
		lens65.frontDiameterMm = 95.0;
		lens65.kitId = "";
		lens65.hFovFullFrameDeg = 30.5;
		lens65.hFovSuper35Deg = 21.3;
		outPresets.Insert(lens65);

		PCT_LensPreset lens85 = new PCT_LensPreset();
		lens85.id = "lens_zsr_85_t15";
		lens85.displayName = "ZEISS Radiance 85 mm T1.5";
		lens85.focalLengthMm = 85.0;
		lens85.focalLengthMaxMm = 0.0;
		lens85.maxAperture = 1.5;
		lens85.minFocusDistanceM = 0.84;
		lens85.anamorphicFactor = 1.0;
		lens85.isBuiltin = true;
		lens85.minAperture = 22.0;
		lens85.apertureScale = "T";
		lens85.weightKg = 1.42;
		lens85.lengthMm = 119.0;
		lens85.frontDiameterMm = 95.0;
		lens85.kitId = "kit_zsr_7_metric";
		lens85.hFovFullFrameDeg = 24.0;
		lens85.hFovSuper35Deg = 16.7;
		outPresets.Insert(lens85);

		PCT_LensPreset lens100 = new PCT_LensPreset();
		lens100.id = "lens_zsr_100_t15";
		lens100.displayName = "ZEISS Radiance 100 mm T1.5";
		lens100.focalLengthMm = 100.0;
		lens100.focalLengthMaxMm = 0.0;
		lens100.maxAperture = 1.5;
		lens100.minFocusDistanceM = 1.10;
		lens100.anamorphicFactor = 1.0;
		lens100.isBuiltin = true;
		lens100.minAperture = 22.0;
		lens100.apertureScale = "T";
		lens100.weightKg = 1.70;
		lens100.lengthMm = 119.0;
		lens100.frontDiameterMm = 95.0;
		lens100.kitId = "kit_zsr_7_metric";
		lens100.hFovFullFrameDeg = 20.4;
		lens100.hFovSuper35Deg = 14.2;
		outPresets.Insert(lens100);
	}

	static PCT_LensPresetFile LoadLensPresets()
	{
		EnsureDirectories();

		PCT_LensPresetFile file = new PCT_LensPresetFile();
		bool skipAutoSave = false;

		if (FileExist(PCT_Constants.FILE_PRESETS_LENSES))
		{
			string errorMessage;
			bool loadedOk = JsonFileLoader<PCT_LensPresetFile>.LoadFile(PCT_Constants.FILE_PRESETS_LENSES, file, errorMessage);
			if (!loadedOk)
			{
				CF_Log.Warn("PCT: Presets/Lenses.json ist defekt, wird uebersprungen (Datei bleibt unveraendert): " + errorMessage);
				file = new PCT_LensPresetFile();
				skipAutoSave = true;
			}
		}

		PCT_Migration.MigrateLensPresetFile(file);
		PCT_Validation.ValidateLensPresetFile(file);
		MergeBuiltinLensPresets(file.presets);

		if (!skipAutoSave)
			SaveLensPresets(file);

		return file;
	}

	static bool SaveLensPresets(PCT_LensPresetFile file)
	{
		if (!file)
			return false;
		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: SaveLensPresets verweigert -- schemaVersion " + file.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_PRESETS + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();
		file.schemaVersion = PCT_Constants.SCHEMA_VERSION_PRESETS;

		string errorMessage;
		bool saved = JsonFileLoader<PCT_LensPresetFile>.SaveFile(PCT_Constants.FILE_PRESETS_LENSES, file, errorMessage);
		if (!saved)
			CF_Log.Warn("PCT: SaveLensPresets fehlgeschlagen: " + errorMessage);

		return saved;
	}

	// Task P2-5: Einzel-Lookup nach id ueber die Lens-Preset-Familie -- Konsumenten laut Plan_B8
	// Section 4/8 Task 2 "Interfaces" (PCT_Storage.GetLensPresetById fuer Task 3/4-UI). Laedt/regeneriert
	// die Familie wie LoadLensPresets() (gleiches Auto-Save-Verhalten) und sucht linear nach id. Wird vom
	// Formular NICHT im Button-Bau-Hot-Path verwendet (dort haelt das Modul m_LensPresets bereits einmalig
	// geladen, siehe PCT_CinematicModule.OnMissionLoaded/Task-P2-5-Report "Abweichung Andockpunkte") --
	// bleibt aber als allgemeiner Einzel-Lookup nutzbar (Muster identisch zu LoadShot() bei Button-Klicks).
	static PCT_LensPreset GetLensPresetById(string id)
	{
		if (id == "")
			return null;

		PCT_LensPresetFile file = LoadLensPresets();
		if (!file || !file.presets)
			return null;

		int count = file.presets.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_LensPreset preset = file.presets[i];
			if (preset && preset.id == id)
				return preset;
		}

		return null;
	}

	// ---- LensKit-Familie (Task P2-5, Docs/Plan_B8_Objektivsystem_LensKits.md Section 3.2/4.2): folgt
	// exakt demselben Familien-Muster wie Lens/Sensor oben (Merge/Create/Load/Save). ----

	static void MergeBuiltinLensKits(array<ref PCT_LensKit> kits)
	{
		if (!kits)
			return;

		int i = kits.Count() - 1;
		while (i >= 0)
		{
			PCT_LensKit entry = kits[i];
			if (entry && entry.isBuiltin)
				kits.RemoveOrdered(i);
			i = i - 1;
		}

		array<ref PCT_LensKit> builtins = new array<ref PCT_LensKit>();
		CreateBuiltinLensKits(builtins);

		int count = builtins.Count();
		for (int b = 0; b < count; b++)
		{
			kits.Insert(builtins[b]);
		}
	}

	// Task P2-5 (Plan_B8 Section 4.2, Quellen-ID "zeiss_supreme_prime_radiance_7_metric" -- PCT-id folgt
	// der Korpus-Konvention kit_*, Plan_B3 Section 3.3): das ZEISS-Supreme-Prime-Radiance-7er-Set, Werte
	// EXAKT aus der Plan-JSON uebernommen (Gegenprobe: Task-P2-5-Report). 65 mm ist bewusst NICHT in
	// lensPresetIds (eigenstaendige Einzellinse, siehe CreateBuiltinLensPresets-Kommentar oben).
	static void CreateBuiltinLensKits(array<ref PCT_LensKit> outKits)
	{
		if (!outKits)
			return;

		PCT_LensKit kit = new PCT_LensKit();
		kit.id = "kit_zsr_7_metric";
		kit.displayName = "ZEISS Supreme Prime Radiance (7er-Set)";
		kit.manufacturer = "ZEISS";
		kit.family = "Supreme Prime Radiance";
		kit.mount = "PL";
		kit.focusScale = "metric";
		kit.coverage = "fullFrame";
		kit.opticalCharacter = "Modern · Warm · Controlled Flare · Gentle Sharpness";
		kit.coating = "ZEISS T* blue";
		kit.frontDiameterMm = 95.0;
		kit.hasExtendedData = true;
		kit.flareProfileId = "flare_radiance";
		kit.isBuiltin = true;
		kit.lensPresetIds.Insert("lens_zsr_21_t15");
		kit.lensPresetIds.Insert("lens_zsr_25_t15");
		kit.lensPresetIds.Insert("lens_zsr_29_t15");
		kit.lensPresetIds.Insert("lens_zsr_35_t15");
		kit.lensPresetIds.Insert("lens_zsr_50_t15");
		kit.lensPresetIds.Insert("lens_zsr_85_t15");
		kit.lensPresetIds.Insert("lens_zsr_100_t15");
		outKits.Insert(kit);
	}

	static PCT_LensKitFile LoadLensKits()
	{
		EnsureDirectories();

		PCT_LensKitFile file = new PCT_LensKitFile();
		bool skipAutoSave = false;

		if (FileExist(PCT_Constants.FILE_PRESETS_LENSKITS))
		{
			string errorMessage;
			bool loadedOk = JsonFileLoader<PCT_LensKitFile>.LoadFile(PCT_Constants.FILE_PRESETS_LENSKITS, file, errorMessage);
			if (!loadedOk)
			{
				CF_Log.Warn("PCT: Presets/LensKits.json ist defekt, wird uebersprungen (Datei bleibt unveraendert): " + errorMessage);
				file = new PCT_LensKitFile();
				skipAutoSave = true;
			}
		}

		PCT_Migration.MigrateLensKitFile(file);
		PCT_Validation.ValidateLensKitFile(file);
		MergeBuiltinLensKits(file.kits);

		if (!skipAutoSave)
			SaveLensKits(file);

		return file;
	}

	static bool SaveLensKits(PCT_LensKitFile file)
	{
		if (!file)
			return false;
		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: SaveLensKits verweigert -- schemaVersion " + file.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_PRESETS + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();
		file.schemaVersion = PCT_Constants.SCHEMA_VERSION_PRESETS;

		string errorMessage;
		bool saved = JsonFileLoader<PCT_LensKitFile>.SaveFile(PCT_Constants.FILE_PRESETS_LENSKITS, file, errorMessage);
		if (!saved)
			CF_Log.Warn("PCT: SaveLensKits fehlgeschlagen: " + errorMessage);

		return saved;
	}

	// Task P2-5: Convenience-Wrapper laut Plan_B8 Section 8 Task 2 "Interfaces" (PCT_Storage.GetLensKits()
	// fuer Task 3/4-UI) -- laedt/regeneriert die Familie wie LoadLensKits() und liefert direkt das Array.
	static array<ref PCT_LensKit> GetLensKits()
	{
		PCT_LensKitFile file = LoadLensKits();
		if (!file)
			return new array<ref PCT_LensKit>();

		return file.kits;
	}

	// ---- Framing-Familie: Builtins folgen in P2-4 (Task-Brief). ----

	static void MergeBuiltinFramingPresets(array<ref PCT_FramingPreset> presets)
	{
		if (!presets)
			return;

		int i = presets.Count() - 1;
		while (i >= 0)
		{
			PCT_FramingPreset entry = presets[i];
			if (entry && entry.isBuiltin)
				presets.RemoveOrdered(i);
			i = i - 1;
		}

		array<ref PCT_FramingPreset> builtins = new array<ref PCT_FramingPreset>();
		CreateBuiltinFramingPresets(builtins);

		int count = builtins.Count();
		for (int b = 0; b < count; b++)
		{
			presets.Insert(builtins[b]);
		}
	}

	// Task P2-4 -- Einstellungsgroessen-Presets (Plan_A1 Section 3, "mindestens" EWS/WS/FS/MLS/CS/MS/MCU/
	// CU/BCU/ECU). Werte-Quellen je Spalte (vollstaendige Tabelle + Handrechnungen im Task-P2-4-Report):
	//   - shortCode/displayName: Plan_A1 Section 3 Spalte "Kuerzel"/Preset-Name, woertlich.
	//   - focalLengthMinMm/MaxMm: Plan_A1 Section 3 Spalte "Moeglicher Startwert", woertlich.
	//   - headroom/lookRoom: NICHT tabelliert; CU = 0.06/0.12 woertlich aus Plan_A1 Section 13 (derselbe
	//     Beispielpreset); die uebrigen 9 Werte sind hergeleitete, monoton fallende (headroom) bzw. an CU
	//     angenaeherte (lookRoom) Startwerte (kalibrierbar). Unveraendert durch den Fix-Wave-Review.
	//
	// === Fix-Wave (Review Task P2-4) -- bodyCutRatio + subjectScreenHeight neu gesetzt ===
	// Das Review hat nachgewiesen, dass die alte Distanzformel `subjectScreenHeight` auf die VOLLE
	// Motivhoehe bezog -- fuer enge Einstellungen (MCU..ECU) musste das Feld dadurch > 1.0 sein (ausserhalb
	// des in Plan_B3 Section 2.10 dokumentierten Bereichs 0..1) und ergab unplausible Distanzen (z. B. CU/
	// 85mm vorher ~7.8 m). Design-Entscheidung "Visible-Height-Reinterpretation" (siehe PCT_Framing.c-
	// Kopfkommentar "Fix-Wave"): `subjectScreenHeight` bezieht sich jetzt auf die SICHTBARE Motivspanne
	// (Kopfoberkante..Schnittlinie), das neue additive Feld `bodyCutRatio` (siehe PCT_Presets.c) legt die
	// Schnittlinie fest (Anteil der Koerperhoehe UNTERHALB der Schnittlinie, aus den bereits vorhandenen
	// Anthropometrie-Ratios in PCT_Framing -- eyeLevel 0.933, shoulder 0.82, hip 0.53, knee 0.285).
	//
	// Ganzkoerper-Presets (EWS/WS/FS): bodyCutRatio = 0.0 (Ganzkoerper sichtbar), subjectScreenHeight
	// bleibt der bisherige Wert -- die Fix-Wave-Formel ist fuer bodyCutRatio = 0 algebraisch identisch mit
	// der alten Formel (visibleHeightM = subjectHeightM), diese drei Presets sind vom Fix nicht betroffen.
	//
	// Schnitt-Presets (MLS..ECU): bodyCutRatio aus den PCT_Framing-Anthropometrie-Ratios (MLS = Knie 0.285,
	// CS/MS = Huefte 0.53 -- beide schneiden an derselben Koerperstelle, CS ist durch ein NIEDRIGERES
	// subjectScreenHeight bewusst weiter/lockerer gerahmt als MS, klassische Cowboy-Shot-Wirkung; MCU/CU =
	// hergeleiteter Rumpf-/Brustschnitt zwischen Huefte und Schulter, CU = 0.75 per Task-Brief-Vorgabe; BCU =
	// Schulter 0.82; ECU = hergeleiteter Schnitt oberhalb der Schulter Richtung Kinn/Hals, 0.90).
	// subjectScreenHeight je Schnitt-Preset ist so gewaehlt, dass die Schnittlinie deutlich im unteren
	// Bilddrittel liegt (Nachweis ueber die in PCT_Framing.c hergeleitete Identitaet
	// fraction(Schnittlinie) = headroom + subjectScreenHeight, alle Werte > 0.667):
	//   | id  | bodyCutRatio | subjectScreenHeight | headroom | fraction(Schnittlinie) = headroom+ssH |
	//   |-----|--------------|----------------------|----------|-----------------------------------------|
	//   | MLS | 0.285 (Knie) | 0.65                 | 0.10     | 0.75                                     |
	//   | CS  | 0.53 (Huefte)| 0.71                 | 0.09     | 0.80                                     |
	//   | MS  | 0.53 (Huefte)| 0.84 (Task-Brief-Anker)| 0.08   | 0.92 (Task-Brief-Handrechnung)            |
	//   | MCU | 0.65 (herg.) | 0.83                 | 0.07     | 0.90                                     |
	//   | CU  | 0.75 (Task-Brief-Anker ~0.75)| 0.82  | 0.06     | 0.88                                     |
	//   | BCU | 0.82 (Schulter)| 0.84                | 0.04     | 0.88                                     |
	//   | ECU | 0.90 (herg.) | 0.85                 | 0.02     | 0.87                                     |
	// MS/CU sind exakt die im Task-Brief vorgegebenen Ankerwerte (siehe Task-P2-4-Report "Verifikation");
	// MLS/CS/MCU/BCU/ECU sind fachlich hergeleitete, kalibrierbare Startwerte (wie zuvor bei allen 8 nicht-
	// verankerten Werten -- kalibrierbar heisst hier: Distanz/Framing sind mit der neuen Formel bereits
	// physikalisch plausibel, siehe Handrechnungen im Task-P2-4-Report, aber nicht In-Game verifiziert).
	static void CreateBuiltinFramingPresets(array<ref PCT_FramingPreset> outPresets)
	{
		if (!outPresets)
			return;

		PCT_FramingPreset ews = new PCT_FramingPreset();
		ews.id = "framing_ews";
		ews.displayName = "Extreme Wide Shot";
		ews.shortCode = "EWS";
		ews.subjectScreenHeight = 0.12;
		ews.headroom = 0.18;
		ews.lookRoom = 0.20;
		ews.bodyCutRatio = 0.0;		// Ganzkoerper (Fix-Wave, unveraendert -- siehe Kommentar oben)
		ews.focalLengthMinMm = 14.0;
		ews.focalLengthMaxMm = 24.0;
		ews.description = "Umgebung dominiert, Figur sehr klein (Plan_A1 Section 3).";
		ews.isBuiltin = true;
		outPresets.Insert(ews);

		PCT_FramingPreset ws = new PCT_FramingPreset();
		ws.id = "framing_ws";
		ws.displayName = "Wide/Long Shot";
		ws.shortCode = "WS";
		ws.subjectScreenHeight = 0.30;
		ws.headroom = 0.14;
		ws.lookRoom = 0.17;
		ws.bodyCutRatio = 0.0;		// Ganzkoerper (Fix-Wave, unveraendert)
		ws.focalLengthMinMm = 24.0;
		ws.focalLengthMaxMm = 35.0;
		ws.description = "Figur vollstaendig mit viel Raum (Plan_A1 Section 3).";
		ws.isBuiltin = true;
		outPresets.Insert(ws);

		PCT_FramingPreset fs = new PCT_FramingPreset();
		fs.id = "framing_fs";
		fs.displayName = "Full Shot";
		fs.shortCode = "FS";
		fs.subjectScreenHeight = 0.50;
		fs.headroom = 0.11;
		fs.lookRoom = 0.14;
		fs.bodyCutRatio = 0.0;		// Ganzkoerper (Fix-Wave, unveraendert)
		fs.focalLengthMinMm = 35.0;
		fs.focalLengthMaxMm = 50.0;
		fs.description = "Person vollstaendig von Kopf bis Fuss (Plan_A1 Section 3).";
		fs.isBuiltin = true;
		outPresets.Insert(fs);

		// MLS: bodyCutRatio = PCT_Framing.PCT_KNEE_HEIGHT_RATIO (0.285, Schnitt am Knie). subjectScreenHeight
		// neu gesetzt (Fix-Wave, siehe Tabelle oben): fraction(Schnittlinie) = 0.10+0.65 = 0.75 (unteres Drittel).
		PCT_FramingPreset mls = new PCT_FramingPreset();
		mls.id = "framing_mls";
		mls.displayName = "Medium Long Shot";
		mls.shortCode = "MLS";
		mls.subjectScreenHeight = 0.65;
		mls.headroom = 0.10;
		mls.lookRoom = 0.13;
		mls.bodyCutRatio = 0.285;
		mls.focalLengthMinMm = 40.0;
		mls.focalLengthMaxMm = 65.0;
		mls.description = "Ungefaehr ab Knie oder Oberschenkel (Plan_A1 Section 3).";
		mls.isBuiltin = true;
		outPresets.Insert(mls);

		// CS: bodyCutRatio = PCT_Framing.PCT_HIP_HEIGHT_RATIO (0.53, dieselbe Schnitthoehe wie MS) --
		// niedrigeres subjectScreenHeight als MS haelt CS bewusst weiter/lockerer gerahmt (klassische
		// Cowboy-Shot-Wirkung, Hueften/Haende mit etwas Umgebung). fraction(Schnittlinie) = 0.09+0.71 = 0.80.
		PCT_FramingPreset cs = new PCT_FramingPreset();
		cs.id = "framing_cs";
		cs.displayName = "Cowboy Shot";
		cs.shortCode = "CS";
		cs.subjectScreenHeight = 0.71;
		cs.headroom = 0.09;
		cs.lookRoom = 0.12;
		cs.bodyCutRatio = 0.53;
		cs.focalLengthMinMm = 40.0;
		cs.focalLengthMaxMm = 65.0;
		cs.description = "Ungefaehr ab Mitte Oberschenkel (Plan_A1 Section 3).";
		cs.isBuiltin = true;
		outPresets.Insert(cs);

		// MS: subjectScreenHeight = 0.84 / bodyCutRatio = 0.53 (Huefte) sind die woertlichen Task-Brief-
		// Fix-Wave-Ankerwerte (siehe Task-P2-4-Report "Verifikation"). fraction(Schnittlinie/Huefte) =
		// 0.08+0.84 = 0.92 (unteres Drittel, DoD B6-Phase-2-Nr.3 jetzt EXAKT erfuellt statt nur tendenziell).
		PCT_FramingPreset ms = new PCT_FramingPreset();
		ms.id = "framing_ms";
		ms.displayName = "Medium Shot";
		ms.shortCode = "MS";
		ms.subjectScreenHeight = 0.84;
		ms.headroom = 0.08;
		ms.lookRoom = 0.10;
		ms.bodyCutRatio = 0.53;
		ms.focalLengthMinMm = 50.0;
		ms.focalLengthMaxMm = 85.0;
		ms.description = "Ungefaehr ab Huefte (Plan_A1 Section 3).";
		ms.isBuiltin = true;
		outPresets.Insert(ms);

		// MCU: bodyCutRatio hergeleitet zwischen Huefte (0.53) und CU (0.75) -- Rumpf-/Brustschnitt.
		// fraction(Schnittlinie) = 0.07+0.83 = 0.90.
		PCT_FramingPreset mcu = new PCT_FramingPreset();
		mcu.id = "framing_mcu";
		mcu.displayName = "Medium Close-up";
		mcu.shortCode = "MCU";
		mcu.subjectScreenHeight = 0.83;
		mcu.headroom = 0.07;
		mcu.lookRoom = 0.11;
		mcu.bodyCutRatio = 0.65;
		mcu.focalLengthMinMm = 65.0;
		mcu.focalLengthMaxMm = 100.0;
		mcu.description = "Brust oder Schulter bis Kopf (Plan_A1 Section 3).";
		mcu.isBuiltin = true;
		outPresets.Insert(mcu);

		// CU: headroom = 0.06 / lookRoom = 0.12 woertlich aus Plan_A1 Section 13 ("cu_emotional_85_eye"-
		// Beispielpreset). bodyCutRatio = 0.75 (Task-Brief-Fix-Wave-Vorgabe, Schnitt Brust/Schulter) und
		// subjectScreenHeight = 0.82 sind die Fix-Wave-Ankerwerte (Task-P2-4-Report "Verifikation").
		// fraction(Schnittlinie) = 0.06+0.82 = 0.88 (unteres Drittel).
		PCT_FramingPreset cu = new PCT_FramingPreset();
		cu.id = "framing_cu";
		cu.displayName = "Close-up";
		cu.shortCode = "CU";
		cu.subjectScreenHeight = 0.82;
		cu.headroom = 0.06;
		cu.lookRoom = 0.12;
		cu.bodyCutRatio = 0.75;
		cu.focalLengthMinMm = 85.0;
		cu.focalLengthMaxMm = 135.0;
		cu.description = "Gesicht beziehungsweise wichtiges Objekt (Plan_A1 Section 3/13).";
		cu.isBuiltin = true;
		outPresets.Insert(cu);

		// BCU: bodyCutRatio = PCT_Framing.PCT_SHOULDER_HEIGHT_RATIO (0.82, Schnitt an Schulter).
		// fraction(Schnittlinie) = 0.04+0.84 = 0.88.
		PCT_FramingPreset bcu = new PCT_FramingPreset();
		bcu.id = "framing_bcu";
		bcu.displayName = "Big Close-up";
		bcu.shortCode = "BCU";
		bcu.subjectScreenHeight = 0.84;
		bcu.headroom = 0.04;
		bcu.lookRoom = 0.08;
		bcu.bodyCutRatio = 0.82;
		bcu.focalLengthMinMm = 100.0;
		bcu.focalLengthMaxMm = 150.0;
		bcu.description = "Gesicht fuellt das Bild staerker (Plan_A1 Section 3).";
		bcu.isBuiltin = true;
		outPresets.Insert(bcu);

		// ECU: bodyCutRatio hergeleitet oberhalb der Schulter (0.82) Richtung Kinn/Hals -- Detail-Schnitt
		// (Augen/Mund/Haende). fraction(Schnittlinie) = 0.02+0.85 = 0.87.
		PCT_FramingPreset ecu = new PCT_FramingPreset();
		ecu.id = "framing_ecu";
		ecu.displayName = "Extreme Close-up";
		ecu.shortCode = "ECU";
		ecu.subjectScreenHeight = 0.85;
		ecu.headroom = 0.02;
		ecu.lookRoom = 0.05;
		ecu.bodyCutRatio = 0.90;
		ecu.focalLengthMinMm = 100.0;
		ecu.focalLengthMaxMm = 200.0;
		ecu.description = "Augen, Mund, Haende oder Detail (Plan_A1 Section 3).";
		ecu.isBuiltin = true;
		outPresets.Insert(ecu);
	}

	static PCT_FramingPresetFile LoadFramingPresets()
	{
		EnsureDirectories();

		PCT_FramingPresetFile file = new PCT_FramingPresetFile();
		bool skipAutoSave = false;

		if (FileExist(PCT_Constants.FILE_PRESETS_FRAMINGS))
		{
			string errorMessage;
			bool loadedOk = JsonFileLoader<PCT_FramingPresetFile>.LoadFile(PCT_Constants.FILE_PRESETS_FRAMINGS, file, errorMessage);
			if (!loadedOk)
			{
				CF_Log.Warn("PCT: Presets/Framings.json ist defekt, wird uebersprungen (Datei bleibt unveraendert): " + errorMessage);
				file = new PCT_FramingPresetFile();
				skipAutoSave = true;
			}
		}

		PCT_Migration.MigrateFramingPresetFile(file);
		PCT_Validation.ValidateFramingPresetFile(file);
		MergeBuiltinFramingPresets(file.presets);

		if (!skipAutoSave)
			SaveFramingPresets(file);

		return file;
	}

	static bool SaveFramingPresets(PCT_FramingPresetFile file)
	{
		if (!file)
			return false;
		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: SaveFramingPresets verweigert -- schemaVersion " + file.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_PRESETS + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();
		file.schemaVersion = PCT_Constants.SCHEMA_VERSION_PRESETS;

		string errorMessage;
		bool saved = JsonFileLoader<PCT_FramingPresetFile>.SaveFile(PCT_Constants.FILE_PRESETS_FRAMINGS, file, errorMessage);
		if (!saved)
			CF_Log.Warn("PCT: SaveFramingPresets fehlgeschlagen: " + errorMessage);

		return saved;
	}

	// ---- Angle-Familie: Builtins folgen in P2-4 (Task-Brief). ----

	static void MergeBuiltinAnglePresets(array<ref PCT_AnglePreset> presets)
	{
		if (!presets)
			return;

		int i = presets.Count() - 1;
		while (i >= 0)
		{
			PCT_AnglePreset entry = presets[i];
			if (entry && entry.isBuiltin)
				presets.RemoveOrdered(i);
			i = i - 1;
		}

		array<ref PCT_AnglePreset> builtins = new array<ref PCT_AnglePreset>();
		CreateBuiltinAnglePresets(builtins);

		int count = builtins.Count();
		for (int b = 0; b < count; b++)
		{
			presets.Insert(builtins[b]);
		}
	}

	// Task P2-4 -- Kamerawinkel/-hoehen-Presets (Plan_A1 Section 4: Eye Level, High, Low, Shoulder, Hip,
	// Knee, Ground, Overhead, Dutch 15/25/40). heightMode/heightOffsetM/pitchDegrees-Zusammenspiel und die
	// PCT_Framing.HeightModeBaseHeight-Anthropometrie-Naeherungen (eyeLevel/shoulder/hip/knee-Verhaeltnisse)
	// sind in PCT_Framing.c dokumentiert (dort auch die vollstaendige Handrechnung im Report). Vorzeichen-
	// konvention pitchDegrees "negativ = nach unten" ist Plan_B3 Section 2.10-Feldkommentar, woertlich
	// zitiert in jeder Preset-Zeile unten. Dutch 15/25/40 sind Plan_A1 Section 4 woertlich ("kann der Mod
	// beispielsweise feste Ausgangswerte von 15, 25 und 40 Grad anbieten").
	static void CreateBuiltinAnglePresets(array<ref PCT_AnglePreset> outPresets)
	{
		if (!outPresets)
			return;

		PCT_AnglePreset eyeLevel = new PCT_AnglePreset();
		eyeLevel.id = "angle_eye_level";
		eyeLevel.displayName = "Eye Level";
		eyeLevel.heightMode = "eyeLevel";
		eyeLevel.heightOffsetM = 0.0;
		eyeLevel.pitchDegrees = 0.0;
		eyeLevel.rollDegrees = 0.0;
		eyeLevel.description = "Kamera auf Augenhoehe -- neutral, direkt, menschlich (Plan_A1 Section 4).";
		eyeLevel.isBuiltin = true;
		outPresets.Insert(eyeLevel);

		// High Angle: heightOffsetM hebt die Kamera ueber die Augenhoehe -- der resultierende Blick nach
		// unten ergibt sich rein geometrisch aus PCT_Framing (Look-at-Richtung), pitchDegrees bleibt 0 (kein
		// zusaetzlicher manueller Korrekturwinkel noetig, siehe PCT_Framing.c-Kommentar zum Pitch-Modell).
		PCT_AnglePreset high = new PCT_AnglePreset();
		high.id = "angle_high";
		high.displayName = "High Angle";
		high.heightMode = "eyeLevel";
		high.heightOffsetM = 1.2;
		high.pitchDegrees = 0.0;
		high.rollDegrees = 0.0;
		high.description = "Deutlich ueber dem Motiv -- klein, verletzlich, unterlegen (Plan_A1 Section 4). Pitch (negativ = nach unten) ergibt sich geometrisch aus der Kamerahoehe.";
		high.isBuiltin = true;
		outPresets.Insert(high);

		// Low Angle: heightMode=hip statt eyeLevel+negativem Offset -- vermeidet, dass ein grosser
		// negativer Offset die Kamera bei kleinen Motiven/kurzer Distanz unter den Boden zieht.
		PCT_AnglePreset low = new PCT_AnglePreset();
		low.id = "angle_low";
		low.displayName = "Low Angle";
		low.heightMode = "hip";
		low.heightOffsetM = 0.0;
		low.pitchDegrees = 0.0;
		low.rollDegrees = 0.0;
		low.description = "Kamera unterhalb der Augen -- Staerke, Macht, Dominanz (Plan_A1 Section 4). Pitch (positiv = nach oben) ergibt sich geometrisch.";
		low.isBuiltin = true;
		outPresets.Insert(low);

		PCT_AnglePreset shoulder = new PCT_AnglePreset();
		shoulder.id = "angle_shoulder";
		shoulder.displayName = "Shoulder Level";
		shoulder.heightMode = "shoulder";
		shoulder.heightOffsetM = 0.0;
		shoulder.pitchDegrees = 0.0;
		shoulder.rollDegrees = 0.0;
		shoulder.description = "Schulterhoehe -- leicht beobachtend (Plan_A1 Section 4).";
		shoulder.isBuiltin = true;
		outPresets.Insert(shoulder);

		PCT_AnglePreset hip = new PCT_AnglePreset();
		hip.id = "angle_hip";
		hip.displayName = "Hip Level";
		hip.heightMode = "hip";
		hip.heightOffsetM = 0.0;
		hip.pitchDegrees = 0.0;
		hip.rollDegrees = 0.0;
		hip.description = "Etwa Hueft-hoehe -- Bewegung, Haende, Western-Wirkung (Plan_A1 Section 4).";
		hip.isBuiltin = true;
		outPresets.Insert(hip);

		PCT_AnglePreset knee = new PCT_AnglePreset();
		knee.id = "angle_knee";
		knee.displayName = "Knee Level";
		knee.heightMode = "knee";
		knee.heightOffsetM = 0.0;
		knee.pitchDegrees = 0.0;
		knee.rollDegrees = 0.0;
		knee.description = "Kniehoehe -- ungewoehnliche, dynamische Perspektive (Plan_A1 Section 4).";
		knee.isBuiltin = true;
		outPresets.Insert(knee);

		// Ground: pitchDegrees-Korrekturwert auf 0.0 zurueckgesetzt (Fix-Wave/Task P2-4-Review) -- der
		// vorherige Ad-hoc-Wert (+5 Grad) war gegen die inzwischen als fehlerhaft nachgewiesene Distanz-/
		// Aim-Geometrie kalibriert (siehe PCT_Framing.c-Kopfkommentar "Fix-Wave") und damit hinfaellig.
		// Kalibrierung nach Geometrie-Fix ausstehend (In-Game).
		PCT_AnglePreset ground = new PCT_AnglePreset();
		ground.id = "angle_ground";
		ground.displayName = "Ground Level";
		ground.heightMode = "ground";
		ground.heightOffsetM = 0.0;
		ground.pitchDegrees = 0.0;
		ground.rollDegrees = 0.0;
		ground.description = "Etwa 10-40 cm Hoehe -- Naehe zum Boden, Dynamik (Plan_A1 Section 4).";
		ground.isBuiltin = true;
		outPresets.Insert(ground);

		// Overhead: pitchDegrees-Korrekturwert auf 0.0 zurueckgesetzt (Fix-Wave/Task P2-4-Review) -- der
		// vorherige Ad-hoc-Wert (-55 Grad) war gegen die inzwischen als fehlerhaft nachgewiesene Distanz-/
		// Aim-Geometrie kalibriert (siehe PCT_Framing.c-Kopfkommentar "Fix-Wave") und damit hinfaellig.
		// Kalibrierung nach Geometrie-Fix ausstehend (In-Game).
		PCT_AnglePreset overhead = new PCT_AnglePreset();
		overhead.id = "angle_overhead";
		overhead.displayName = "Overhead";
		overhead.heightMode = "eyeLevel";
		overhead.heightOffsetM = 3.0;
		overhead.pitchDegrees = 0.0;
		overhead.rollDegrees = 0.0;
		overhead.description = "Fast oder vollstaendig senkrecht von oben -- Uebersicht, Muster, Kontrolle (Plan_A1 Section 4).";
		overhead.isBuiltin = true;
		outPresets.Insert(overhead);

		PCT_AnglePreset dutch15 = new PCT_AnglePreset();
		dutch15.id = "angle_dutch_15";
		dutch15.displayName = "Dutch Angle 15";
		dutch15.heightMode = "eyeLevel";
		dutch15.heightOffsetM = 0.0;
		dutch15.pitchDegrees = 0.0;
		dutch15.rollDegrees = 15.0;
		dutch15.description = "Roll-Achse 15 Grad gekippt -- Instabilitaet, Spannung (Plan_A1 Section 4).";
		dutch15.isBuiltin = true;
		outPresets.Insert(dutch15);

		PCT_AnglePreset dutch25 = new PCT_AnglePreset();
		dutch25.id = "angle_dutch_25";
		dutch25.displayName = "Dutch Angle 25";
		dutch25.heightMode = "eyeLevel";
		dutch25.heightOffsetM = 0.0;
		dutch25.pitchDegrees = 0.0;
		dutch25.rollDegrees = 25.0;
		dutch25.description = "Roll-Achse 25 Grad gekippt -- Instabilitaet, Spannung (Plan_A1 Section 4).";
		dutch25.isBuiltin = true;
		outPresets.Insert(dutch25);

		PCT_AnglePreset dutch40 = new PCT_AnglePreset();
		dutch40.id = "angle_dutch_40";
		dutch40.displayName = "Dutch Angle 40";
		dutch40.heightMode = "eyeLevel";
		dutch40.heightOffsetM = 0.0;
		dutch40.pitchDegrees = 0.0;
		dutch40.rollDegrees = 40.0;
		dutch40.description = "Roll-Achse 40 Grad gekippt -- Instabilitaet, Spannung, Verwirrung (Plan_A1 Section 4).";
		dutch40.isBuiltin = true;
		outPresets.Insert(dutch40);
	}

	static PCT_AnglePresetFile LoadAnglePresets()
	{
		EnsureDirectories();

		PCT_AnglePresetFile file = new PCT_AnglePresetFile();
		bool skipAutoSave = false;

		if (FileExist(PCT_Constants.FILE_PRESETS_ANGLES))
		{
			string errorMessage;
			bool loadedOk = JsonFileLoader<PCT_AnglePresetFile>.LoadFile(PCT_Constants.FILE_PRESETS_ANGLES, file, errorMessage);
			if (!loadedOk)
			{
				CF_Log.Warn("PCT: Presets/Angles.json ist defekt, wird uebersprungen (Datei bleibt unveraendert): " + errorMessage);
				file = new PCT_AnglePresetFile();
				skipAutoSave = true;
			}
		}

		PCT_Migration.MigrateAnglePresetFile(file);
		PCT_Validation.ValidateAnglePresetFile(file);
		MergeBuiltinAnglePresets(file.presets);

		if (!skipAutoSave)
			SaveAnglePresets(file);

		return file;
	}

	static bool SaveAnglePresets(PCT_AnglePresetFile file)
	{
		if (!file)
			return false;
		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: SaveAnglePresets verweigert -- schemaVersion " + file.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_PRESETS + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();
		file.schemaVersion = PCT_Constants.SCHEMA_VERSION_PRESETS;

		string errorMessage;
		bool saved = JsonFileLoader<PCT_AnglePresetFile>.SaveFile(PCT_Constants.FILE_PRESETS_ANGLES, file, errorMessage);
		if (!saved)
			CF_Log.Warn("PCT: SaveAnglePresets fehlgeschlagen: " + errorMessage);

		return saved;
	}

	// ---- Movement-Familie: kein eigener P2-x-Task im Task-Brief genannt -- bleibt bis dahin analog zu
	// Lens/Framing/Angle ein leerer Hook (Plan_B3 Section 2.10 "KEINE erfundenen Preset-Inhalte"). ----

	static void MergeBuiltinMovementPresets(array<ref PCT_MovementPreset> presets)
	{
		if (!presets)
			return;

		int i = presets.Count() - 1;
		while (i >= 0)
		{
			PCT_MovementPreset entry = presets[i];
			if (entry && entry.isBuiltin)
				presets.RemoveOrdered(i);
			i = i - 1;
		}

		array<ref PCT_MovementPreset> builtins = new array<ref PCT_MovementPreset>();
		CreateBuiltinMovementPresets(builtins);

		int count = builtins.Count();
		for (int b = 0; b < count; b++)
		{
			presets.Insert(builtins[b]);
		}
	}

	static void CreateBuiltinMovementPresets(array<ref PCT_MovementPreset> outPresets)
	{
		// Hook fuer eine kuenftige Bewegungs-Presets-Task -- bewusst leer, siehe Kommentar oben.
	}

	static PCT_MovementPresetFile LoadMovementPresets()
	{
		EnsureDirectories();

		PCT_MovementPresetFile file = new PCT_MovementPresetFile();
		bool skipAutoSave = false;

		if (FileExist(PCT_Constants.FILE_PRESETS_MOVEMENTS))
		{
			string errorMessage;
			bool loadedOk = JsonFileLoader<PCT_MovementPresetFile>.LoadFile(PCT_Constants.FILE_PRESETS_MOVEMENTS, file, errorMessage);
			if (!loadedOk)
			{
				CF_Log.Warn("PCT: Presets/Movements.json ist defekt, wird uebersprungen (Datei bleibt unveraendert): " + errorMessage);
				file = new PCT_MovementPresetFile();
				skipAutoSave = true;
			}
		}

		PCT_Migration.MigrateMovementPresetFile(file);
		PCT_Validation.ValidateMovementPresetFile(file);
		MergeBuiltinMovementPresets(file.presets);

		if (!skipAutoSave)
			SaveMovementPresets(file);

		return file;
	}

	static bool SaveMovementPresets(PCT_MovementPresetFile file)
	{
		if (!file)
			return false;
		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: SaveMovementPresets verweigert -- schemaVersion " + file.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_PRESETS + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();
		file.schemaVersion = PCT_Constants.SCHEMA_VERSION_PRESETS;

		string errorMessage;
		bool saved = JsonFileLoader<PCT_MovementPresetFile>.SaveFile(PCT_Constants.FILE_PRESETS_MOVEMENTS, file, errorMessage);
		if (!saved)
			CF_Log.Warn("PCT: SaveMovementPresets fehlgeschlagen: " + errorMessage);

		return saved;
	}

	// ---- Light-Familie: Phase 4 (Plan_B3 Section 5.3) -- Hook bewusst leer, Merge/Load/Save-Infrastruktur
	// aber bereits vollstaendig (inkl. eingebetteter PCT_LightSetup-Validierung, siehe
	// PCT_Validation.ValidateLightPresetFile). ----

	static void MergeBuiltinLightPresets(array<ref PCT_LightPreset> presets)
	{
		if (!presets)
			return;

		int i = presets.Count() - 1;
		while (i >= 0)
		{
			PCT_LightPreset entry = presets[i];
			if (entry && entry.isBuiltin)
				presets.RemoveOrdered(i);
			i = i - 1;
		}

		array<ref PCT_LightPreset> builtins = new array<ref PCT_LightPreset>();
		CreateBuiltinLightPresets(builtins);

		int count = builtins.Count();
		for (int b = 0; b < count; b++)
		{
			presets.Insert(builtins[b]);
		}
	}

	static void CreateBuiltinLightPresets(array<ref PCT_LightPreset> outPresets)
	{
		// Hook fuer Phase 4 (Licht-Presets, Plan_A1 Section 8) -- bewusst leer, siehe Kommentar oben.
	}

	static PCT_LightPresetFile LoadLightPresets()
	{
		EnsureDirectories();

		PCT_LightPresetFile file = new PCT_LightPresetFile();
		bool skipAutoSave = false;

		if (FileExist(PCT_Constants.FILE_PRESETS_LIGHTS))
		{
			string errorMessage;
			bool loadedOk = JsonFileLoader<PCT_LightPresetFile>.LoadFile(PCT_Constants.FILE_PRESETS_LIGHTS, file, errorMessage);
			if (!loadedOk)
			{
				CF_Log.Warn("PCT: Presets/Lights.json ist defekt, wird uebersprungen (Datei bleibt unveraendert): " + errorMessage);
				file = new PCT_LightPresetFile();
				skipAutoSave = true;
			}
		}

		PCT_Migration.MigrateLightPresetFile(file);
		PCT_Validation.ValidateLightPresetFile(file);
		MergeBuiltinLightPresets(file.presets);

		if (!skipAutoSave)
			SaveLightPresets(file);

		return file;
	}

	static bool SaveLightPresets(PCT_LightPresetFile file)
	{
		if (!file)
			return false;
		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: SaveLightPresets verweigert -- schemaVersion " + file.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_PRESETS + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();
		file.schemaVersion = PCT_Constants.SCHEMA_VERSION_PRESETS;

		string errorMessage;
		bool saved = JsonFileLoader<PCT_LightPresetFile>.SaveFile(PCT_Constants.FILE_PRESETS_LIGHTS, file, errorMessage);
		if (!saved)
			CF_Log.Warn("PCT: SaveLightPresets fehlgeschlagen: " + errorMessage);

		return saved;
	}

	// ---- WorldState-Familie: Phase 4 (Plan_B3 Section 5.3) -- Hook bewusst leer, Merge/Load/Save-
	// Infrastruktur aber bereits vollstaendig (inkl. eingebetteter PCT_WorldState-Validierung, siehe
	// PCT_Validation.ValidateWorldStatePresetFile). ----

	static void MergeBuiltinWorldStatePresets(array<ref PCT_WorldStatePreset> presets)
	{
		if (!presets)
			return;

		int i = presets.Count() - 1;
		while (i >= 0)
		{
			PCT_WorldStatePreset entry = presets[i];
			if (entry && entry.isBuiltin)
				presets.RemoveOrdered(i);
			i = i - 1;
		}

		array<ref PCT_WorldStatePreset> builtins = new array<ref PCT_WorldStatePreset>();
		CreateBuiltinWorldStatePresets(builtins);

		int count = builtins.Count();
		for (int b = 0; b < count; b++)
		{
			presets.Insert(builtins[b]);
		}
	}

	static void CreateBuiltinWorldStatePresets(array<ref PCT_WorldStatePreset> outPresets)
	{
		// Hook fuer Phase 4 (Welt-Snapshots wie "Golden Hour"/"Moonlight", Plan_B3 Section 3.4-Beispiel) --
		// bewusst leer, siehe Kommentar oben.
	}

	static PCT_WorldStatePresetFile LoadWorldStatePresets()
	{
		EnsureDirectories();

		PCT_WorldStatePresetFile file = new PCT_WorldStatePresetFile();
		bool skipAutoSave = false;

		if (FileExist(PCT_Constants.FILE_PRESETS_WORLDSTATES))
		{
			string errorMessage;
			bool loadedOk = JsonFileLoader<PCT_WorldStatePresetFile>.LoadFile(PCT_Constants.FILE_PRESETS_WORLDSTATES, file, errorMessage);
			if (!loadedOk)
			{
				CF_Log.Warn("PCT: Presets/WorldStates.json ist defekt, wird uebersprungen (Datei bleibt unveraendert): " + errorMessage);
				file = new PCT_WorldStatePresetFile();
				skipAutoSave = true;
			}
		}

		PCT_Migration.MigrateWorldStatePresetFile(file);
		PCT_Validation.ValidateWorldStatePresetFile(file);
		MergeBuiltinWorldStatePresets(file.presets);

		if (!skipAutoSave)
			SaveWorldStatePresets(file);

		return file;
	}

	static bool SaveWorldStatePresets(PCT_WorldStatePresetFile file)
	{
		if (!file)
			return false;
		if (file.schemaVersion > PCT_Constants.SCHEMA_VERSION_PRESETS)
		{
			CF_Log.Warn("PCT: SaveWorldStatePresets verweigert -- schemaVersion " + file.schemaVersion + " ist neuer als unterstuetzt (" + PCT_Constants.SCHEMA_VERSION_PRESETS + "), read-only (Regel M3).");
			return false;
		}

		EnsureDirectories();
		file.schemaVersion = PCT_Constants.SCHEMA_VERSION_PRESETS;

		string errorMessage;
		bool saved = JsonFileLoader<PCT_WorldStatePresetFile>.SaveFile(PCT_Constants.FILE_PRESETS_WORLDSTATES, file, errorMessage);
		if (!saved)
			CF_Log.Warn("PCT: SaveWorldStatePresets fehlgeschlagen: " + errorMessage);

		return saved;
	}
}
