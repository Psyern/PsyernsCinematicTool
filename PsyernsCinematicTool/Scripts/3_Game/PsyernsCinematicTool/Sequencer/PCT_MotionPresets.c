// Task P3-7 -- Bewegungs-Presets als GENERATOREN (Plan_B7 Section 1.3-Tabelle: "die Presets erzeugen
// jetzt PCT_CameraPath/PCT_LookPath-Strukturen statt flacher PCT_Keyframe-Listen"; Plan_B4 Section 5
// liefert die fachlichen Muster: Dolly In/Out/Truck 5.6ff, Orbit 5.6, Dolly-Zoom 5.10). Phase-3-Satz
// gemaess Plan_B7 Section 16: Dolly/Truck/Orbit (gebacken) /Dolly-Zoom; Follow ist Blickmodus
// (FollowTarget, PCT_MotionPlayer), Handheld-Feintuning ist Phase 5 (B4 Section 5.8).
// 3_Game, COT-frei, reine Datenerzeugung -- keine Kamera-/UI-Abhaengigkeit; der Aufrufer (Modul)
// liefert Start-/Zielpunkte aus dem Kamerazustand.
class PCT_MotionPresets
{
	// Zwei-Punkt-Fahrt entlang dirNorm (normiert!): Start -> Start + dirNorm * distanceM.
	// EaseInOut auf dem einen Segment = weiches Anfahren und Abbremsen.
	static void GenerateDolly(PCT_Shot shot, vector startPos, vector dirNorm, float distanceM, float durationS)
	{
		if (!shot)
			return;

		AddPathPoint(shot, startPos, durationS, "EaseInOut", "SmoothCurve");
		vector endPos = startPos + dirNorm * distanceM;
		AddPathPoint(shot, endPos, 0.0, "EaseInOut", "SmoothCurve");
	}

	// Orbit um center: gebackene Punkte auf dem Kreisbogen (Plan_B7 Section 4: "Orbit/Arc als gebackene
	// Keyframes ab Phase 3"); der Radius ergibt sich aus dem Startabstand. Blick fest auf das Zentrum
	// (impliziter LookAt, Section 4 Orbit-Zeile). Segment-Easing Linear = konstante Winkelgeschwindigkeit.
	static void GenerateOrbit(PCT_Shot shot, vector startPos, vector center, float degrees, float durationS, int steps)
	{
		if (!shot)
			return;
		if (steps < 2)
			steps = 2;

		float stepCount = steps;
		float perStepDegrees = degrees / stepCount;
		float perStepDuration = durationS / stepCount;

		for (int i = 0; i <= steps; i++)
		{
			float stepIndex = i;
			float angle = perStepDegrees * stepIndex;
			vector orbitPos = PCT_Math.RotateAroundPointY(center, startPos, angle);

			float segDuration = perStepDuration;
			if (i == steps)
				segDuration = 0.0;

			AddPathPoint(shot, orbitPos, segDuration, "Linear", "SmoothCurve");
		}

		if (!shot.lookPath)
			shot.lookPath = new PCT_LookPath();

		PCT_LookPoint lookPoint = new PCT_LookPoint();
		lookPoint.worldTarget = center;
		lookPoint.timing.durationSeconds = durationS;
		shot.lookPath.points.Insert(lookPoint);
	}

	// Dolly-Zoom (Plan_B4 Section 5.10 / Plan_A1 Section 6): Fahrt auf das Ziel zu bei gegenlaeufiger
	// Brennweite, Motivgroesse ~ focal/dist bleibt konstant => focalEnd = focalStart * distEnd/distStart.
	// Fahrt Richtung Ziel (moveDistanceM > 0 = hinein, Brennweite wird kuerzer/weiter -- Vertigo-Effekt).
	static void GenerateDollyZoom(PCT_Shot shot, vector startPos, vector targetPos, float moveDistanceM, float durationS, float startFocalMm)
	{
		if (!shot)
			return;

		vector toTarget = targetPos - startPos;
		float distStart = toTarget.Length();
		if (distStart <= 0.1)
			return;

		float distEnd = distStart - moveDistanceM;
		if (distEnd < 0.5)
			distEnd = 0.5;								// nie durchs/hinter das Ziel fahren
		float effectiveMove = distStart - distEnd;

		float invLen = 1.0 / distStart;
		vector dirNorm = toTarget * invLen;

		AddPathPoint(shot, startPos, durationS, "EaseInOut", "SmoothCurve");
		vector endPos = startPos + dirNorm * effectiveMove;
		AddPathPoint(shot, endPos, 0.0, "EaseInOut", "SmoothCurve");

		float focalEnd = startFocalMm * distEnd / distStart;

		PCT_Track focalTrack = FindOrCreateTrack(shot, "focalLength");
		AddTrackKey(focalTrack, 0.0, startFocalMm, "Linear");
		AddTrackKey(focalTrack, durationS, focalEnd, "Linear");

		if (!shot.lookPath)
			shot.lookPath = new PCT_LookPath();

		PCT_LookPoint lookPoint = new PCT_LookPoint();
		lookPoint.worldTarget = targetPos;
		lookPoint.timing.durationSeconds = durationS;
		shot.lookPath.points.Insert(lookPoint);
	}

	// ===== Helfer =====

	private static void AddPathPoint(PCT_Shot shot, vector pos, float durationS, string easing, string pathType)
	{
		PCT_PathPoint point = new PCT_PathPoint();
		point.position = pos;
		point.pathType = pathType;
		point.easing = easing;
		point.speed.mode = "duration";
		point.speed.durationSeconds = durationS;
		shot.cameraPath.points.Insert(point);
	}

	private static PCT_Track FindOrCreateTrack(PCT_Shot shot, string channel)
	{
		int count = shot.tracks.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_Track existing = shot.tracks.Get(i);
			if (existing && existing.channel == channel)
				return existing;
		}

		PCT_Track track = new PCT_Track();
		track.channel = channel;
		shot.tracks.Insert(track);
		return track;
	}

	private static void AddTrackKey(PCT_Track track, float time, float value, string easing)
	{
		PCT_TrackKey key = new PCT_TrackKey();
		key.time = time;
		key.value = value;
		key.easing = easing;
		track.keys.Insert(key);
	}
}
