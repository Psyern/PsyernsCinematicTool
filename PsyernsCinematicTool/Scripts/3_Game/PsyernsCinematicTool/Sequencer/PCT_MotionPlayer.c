// Task P3-3 -- PCT_MotionPlayer, der Dual-Path-Playback-Kern nach Docs/Plan_B7_Kamerafahrten_
// Bewegungssteuerung.md Section 11 (Timeline/Scrub) und Section 14 (Tick, PositionEvaluator,
// LookEvaluator, Track-Sampler, Orientierungs-Akkumulator, Kompat-Sampling). Ersetzt den in
// Plan_B4 Section 4.2 skizzierten PCT_SequencePlayer (Kernentscheidung 1, Plan_B7 Section 1.3).
//
// Dokumentierte Abweichungen von den Plan-Skizzen (jeweils quellenverifiziert):
// 1. cam.SetYawPitchRoll(...) aus der Section-14.4-Skizze existiert in Vanilla 1.29 NICHT (Grep ueber
//    scripts - 1.29: 0 Treffer). Reale API mit identischer Semantik: Object.SetOrientation(vector
//    <yaw,pitch,roll> in Grad, Object.c:311) -- ApplyOrientation nutzt SetOrientation.
// 2. Winkel-Tracks: PCT_Math hat kein separates ShortestAngleDelta; PCT_Math.InterpolateAngles(a,b,t)
//    (Utils/PCT_Math.c, Task P1-5) liefert exakt "kuerzester Weg + linear" -- identische Semantik zu
//    "keyA + ShortestAngleDelta(keyA,keyB) * tEased" der Section-14.4-Skizze.
// 3. Arc-Length: PCT_ArcLength.BuildLut (Task P3-1) sampelt GLOBAL ueber Math3D.Curve -- fuer die von
//    Section 4 geforderte Pro-Segment-Lokalitaet baut der Player seine Segment-LUTs selbst, indem er
//    denselben per-Segment-Evaluator sampelt, der auch im Playback laeuft (EvaluateSegmentGeometry);
//    die LUT-Datenklasse PCT_ArcLengthSegment und der Lookup RemapToSplineParam werden wiederverwendet.
// 4. Phase-3-Pfadtypen (Plan_B7 Section 16): Linear, SmoothCurve, Bezier (Evaluator); Rail nutzt laut
//    Section 4 DENSELBEN Evaluator wie SmoothCurve (Unterschied ist der Blick-Constraint, Phase 5).
//    Orbit/Arc/HandheldPath werden in Phase 3 "gebacken" (Generator-Presets erzeugen SmoothCurve-
//    Punkte, Section 4/16) -- trifft der Player sie dennoch als pathType an, wertet er sie als
//    SmoothCurve aus (einmalige Warnung).
// 5. Phase-3-Blickmodi (Section 16): LookAt, FollowTarget (SmoothCD, Signatur EnMath.c:678
//    "proto static float SmoothCD(float val, float target, inout float velocity[], float smoothTime,
//    float maxVelocity, float dt)"), FreeRotation, HorizonLock. LeadTarget/OffsetTarget/TargetLock
//    (Phase 4) fallen auf LookAt-Verhalten zurueck (Offset wirkt bereits ueber ResolveLookTarget).
// 6. ResolveObject(targetRef): die persistente Zielobjekt-Referenzierung (ActorMark-Ids) ist laut
//    Section 5.1 eine Plan_B3-/Phase-4-Frage -- Phase 3 liefert null (Blickziele = worldTarget).
// 7. DirectionMarkerOnly ist an B4 V4 gekoppelt (Section 3.3) und in Phase 3 nicht aktiv -- der Punkt
//    wird wie ExactThrough behandelt (einmalige Warnung), bis V4 in-game verifiziert ist.
// 8. StopAt/ContinuePass (Section 3.3): ContinuePass ist in Phase 3 das Standardverhalten (kein
//    erzwungener geschwindigkeitsstetiger Uebergang zwischen Segmenten wird berechnet). StopAt hat
//    keine eigene Bremslogik -- ein Abbremsen zum Punkt wird nur ueber die Wahl von EaseOut/HardStop
//    als Segment-Easing des ankommenden Segments erreicht (einmalige Warnung, Phase-4-Ausbau).
class PCT_MotionPlayer
{
	ref PCT_CameraPath m_CameraPath;
	ref PCT_LookPath m_LookPath;
	ref array<ref PCT_Track> m_Tracks;

	ref array<vector> m_CamPositions;					// effektive Durch-Punkte (cameraHeight angewandt)
	ref array<ref PCT_ArcLengthSegment> m_CamSegLuts;	// pro Segment eine lokal gesampelte Arc-Length-LUT (null bei Linear)
	ref array<float> m_CamSegStart;						// globale Startzeit je Kamerasegment + Endzeit (letzter Eintrag)
	ref array<float> m_CamSegTravelEnd;					// globale Ankunftszeit je Kamerasegment (VOR einem etwaigen WaitHold am Zielpunkt -- Review-Fix-Wave Fix 1)
	ref array<float> m_LookSegStart;					// globale Startzeit je Blicksegment + Endzeit
	ref array<int> m_TrackCursors;						// Vorwaertszeiger je Track

	float m_Time;
	float m_Duration;
	float m_PlaybackSpeed;
	bool m_Playing;
	int m_CamSegment;									// gecachter Vorwaertszeiger Kamerapfad
	int m_LookSegment;									// gecachter Vorwaertszeiger Blickpfad
	vector m_OrientAngles;								// gemeinsamer Orientierungs-Akkumulator (Yaw/Pitch/Roll)
	bool m_HorizonLock;									// pro Frame aus dem aktiven Blickmodus gesetzt

	// FollowTarget-Glaettung (Math.SmoothCD braucht inout float velocity[] je Komponente)
	vector m_FollowSmoothed;
	bool m_FollowInitialized;
	float m_FollowVelX[1];
	float m_FollowVelY[1];
	float m_FollowVelZ[1];

	bool m_WarnedPathTypeFallback;
	bool m_WarnedMarkerFallback;
	bool m_WarnedStopAtFallback;

	void PCT_MotionPlayer()
	{
		m_CamPositions = new array<vector>();
		m_CamSegLuts = new array<ref PCT_ArcLengthSegment>();
		m_CamSegStart = new array<float>();
		m_CamSegTravelEnd = new array<float>();
		m_LookSegStart = new array<float>();
		m_TrackCursors = new array<int>();
		m_Time = 0.0;
		m_Duration = 0.0;
		m_PlaybackSpeed = 1.0;
		m_Playing = false;
		m_CamSegment = 0;
		m_LookSegment = 0;
		m_OrientAngles = "0 0 0";
		m_HorizonLock = false;
		m_FollowSmoothed = "0 0 0";
		m_FollowInitialized = false;
		m_WarnedPathTypeFallback = false;
		m_WarnedMarkerFallback = false;
		m_WarnedStopAtFallback = false;
	}

	// ===== Aufbau (ereignisgetrieben, NIE im Frame-Pfad -- Plan_B7 Section 14.5) =====

	// Uebernimmt die Dual-Path-Strukturen eines (validierten!) Shots und baut Zeitachsen + LUTs.
	// Liefert false, wenn kein abspielbarer Kamerapfad vorhanden ist (< 2 Punkte).
	bool Prepare(PCT_Shot shot)
	{
		if (!shot)
			return false;
		if (!shot.cameraPath)
			return false;

		m_CameraPath = shot.cameraPath;
		m_LookPath = shot.lookPath;
		m_Tracks = shot.tracks;

		int pointCount = m_CameraPath.points.Count();
		if (pointCount < 2)
			return false;

		BuildCamPositions();
		BuildCamSegments();
		BuildLookSegments();
		BuildTrackCursors();
		BuildDuration();

		SeekTo(0.0);
		return true;
	}

	private void BuildCamPositions()
	{
		m_CamPositions.Clear();

		int count = m_CameraPath.points.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_PathPoint point = m_CameraPath.points.Get(i);
			vector pos = point.position;
			if (point.cameraHeight > 0.0)
			{
				float overrideY = point.cameraHeight;
				pos[1] = overrideY;
			}
			m_CamPositions.Insert(pos);

			if (point.behavior == "DirectionMarkerOnly" && !m_WarnedMarkerFallback)
			{
				CF_Log.Warn("PCT: behavior 'DirectionMarkerOnly' ist an B4 V4 gekoppelt und in Phase 3 nicht aktiv -- Punkt wird wie ExactThrough behandelt.");
				m_WarnedMarkerFallback = true;
			}

			if (point.behavior == "StopAt" && !m_WarnedStopAtFallback)
			{
				CF_Log.Warn("PCT: behavior 'StopAt' hat in Phase 3 keine eigene Bremslogik -- EaseOut/HardStop als Segment-Easing des ankommenden Segments waehlen (Plan_B7 Section 3.3; Phase-4-Ausbau).");
				m_WarnedStopAtFallback = true;
			}
		}
	}

	// Segment-LUTs (pro Segment lokal gesampelt) + Zeitachse aus den Speed-Profilen (Plan_B7 Section 8.2:
	// Modus "speed" -> Dauer = Bogenlaenge / speedMS). WaitHold laesst die Kamera AM erreichten Punkt
	// warten (Plan_B7 Section 3.3: "Kamera wartet holdSeconds am Punkt") -- der Hold von points[seg+1]
	// wird deshalb NACH der Ankunft (TravelEnd von Segment seg) eingerechnet, nicht mehr vor dem Start
	// des ankommenden Segments (Review-Fix-Wave Fix 1: der fruehere Vor-Segment-Hold von points[seg]
	// streckte stattdessen das ANKOMMENDE Segment und liess die Kamera nie einfrieren).
	//
	// Trockenlauf (dokumentiert, siehe Review-Fix-Wave Fix 1): 3-Punkte-Pfad P0/P1/P2, beide
	// Segmentdauern 3.0s, P1.behavior == "WaitHold" mit holdSeconds = 1.5:
	//   seg=0: SegStart[0] = cumulative = 0.0
	//          cumulative += travelDuration(3.0) -> 3.0; TravelEnd[0] = 3.0
	//          arrival = points[1] ("WaitHold", 1.5) -> cumulative += 1.5 -> 4.5
	//   seg=1: SegStart[1] = cumulative = 4.5
	//          cumulative += travelDuration(3.0) -> 7.5; TravelEnd[1] = 7.5
	//          arrival = points[2] (kein WaitHold) -> cumulative unveraendert
	//   Ende:  SegStart[2] = 7.5 (Endzeit des Kamerapfads)
	//   Ergebnis: m_CamSegStart = [0.0, 4.5, 7.5], m_CamSegTravelEnd = [3.0, 7.5]
	//   FindCamSegment(t) liefert fuer t zwischen 3.0 und 4.5 (4.5 nicht mehr eingeschlossen)
	//   weiterhin Segment 0 (naechste Grenze ist SegStart[1] = 4.5); EvaluatePosition erkennt dort
	//   time >= TravelEnd[0] (3.0) und liefert eingefroren m_CamPositions[1] (P1) fuer das gesamte
	//   Hold-Fenster zwischen 3.0 und 4.5 (4.5 nicht mehr eingeschlossen).
	private void BuildCamSegments()
	{
		m_CamSegLuts.Clear();
		m_CamSegStart.Clear();
		m_CamSegTravelEnd.Clear();

		int segmentCount = m_CameraPath.points.Count() - 1;
		float cumulative = 0.0;

		for (int seg = 0; seg < segmentCount; seg++)
		{
			PCT_PathPoint a = m_CameraPath.points.Get(seg);

			PCT_ArcLengthSegment lut = BuildSegmentLut(seg);
			m_CamSegLuts.Insert(lut);

			float segLength = SegmentLength(seg, lut);
			if (a.speed)
				a.speed.distanceM = segLength;			// abgeleiteter Cache (Plan_B7 Section 8.1)

			m_CamSegStart.Insert(cumulative);

			float travelDuration = SegmentTravelDuration(a, segLength);
			cumulative = cumulative + travelDuration;
			m_CamSegTravelEnd.Insert(cumulative);

			PCT_PathPoint arrival = m_CameraPath.points.Get(seg + 1);
			if (arrival.behavior == "WaitHold" && arrival.holdSeconds > 0.0)
				cumulative = cumulative + arrival.holdSeconds;
		}

		m_CamSegStart.Insert(cumulative);				// Endzeit des Kamerapfads
	}

	private PCT_ArcLengthSegment BuildSegmentLut(int seg)
	{
		PCT_PathPoint a = m_CameraPath.points.Get(seg);
		int pathType = PCT_Validation.ParsePathType(a.pathType);
		if (pathType == EPCT_PathType.LINEAR)
			return null;								// Gerade: Lerp direkt, keine LUT noetig

		PCT_ArcLengthSegment lut = new PCT_ArcLengthSegment();
		int samples = PCT_ArcLength.PCT_ARC_LENGTH_SAMPLES;

		vector prev = EvaluateSegmentGeometry(seg, 0.0);
		lut.distances.Insert(0.0);
		lut.params.Insert(0.0);

		float kumulativ = 0.0;
		for (int i = 1; i <= samples; i++)
		{
			float sampleFrac = i;
			float u = sampleFrac / samples;
			vector sample = EvaluateSegmentGeometry(seg, u);
			vector delta = sample - prev;
			float segDist = delta.Length();
			kumulativ = kumulativ + segDist;
			lut.distances.Insert(kumulativ);
			lut.params.Insert(u);
			prev = sample;
		}

		lut.totalLength = kumulativ;
		return lut;
	}

	private float SegmentLength(int seg, PCT_ArcLengthSegment lut)
	{
		if (lut)
			return lut.totalLength;

		vector a = m_CamPositions.Get(seg);
		vector b = m_CamPositions.Get(seg + 1);
		vector delta = b - a;
		float length = delta.Length();
		return length;
	}

	private float SegmentTravelDuration(PCT_PathPoint a, float segLength)
	{
		if (!a.speed)
			return 0.0;

		if (a.speed.mode == "speed")
		{
			if (a.speed.speedMS > 0.0)
			{
				float duration = segLength / a.speed.speedMS;
				return duration;
			}
			CF_Log.Warn("PCT: speed.mode 'speed' mit speedMS 0 -- Fallback durationSeconds.");
		}

		return a.speed.durationSeconds;
	}

	private void BuildLookSegments()
	{
		m_LookSegStart.Clear();

		if (!m_LookPath)
			return;

		int pointCount = m_LookPath.points.Count();
		if (pointCount < 1)
			return;

		// Segment i laeuft von Blickpunkt i zu i+1 mit der Dauer aus points[i].timing
		// (Plan_B7 Section 5.3: "eigene Blick-Segmentzeit (Dauer bis zum naechsten Ziel)").
		float cumulative = 0.0;
		m_LookSegStart.Insert(cumulative);

		int segmentCount = pointCount - 1;
		for (int seg = 0; seg < segmentCount; seg++)
		{
			PCT_LookPoint a = m_LookPath.points.Get(seg);
			float duration = 0.0;
			if (a.timing)
				duration = a.timing.durationSeconds;
			cumulative = cumulative + duration;
			m_LookSegStart.Insert(cumulative);
		}
	}

	private void BuildTrackCursors()
	{
		m_TrackCursors.Clear();

		if (!m_Tracks)
			return;

		int count = m_Tracks.Count();
		for (int i = 0; i < count; i++)
		{
			m_TrackCursors.Insert(0);
		}
	}

	// m_Duration = Maximum aus Kamerapfad-, Blickpfad-Ende und spaetester Track-Key-Zeit
	// (Plan_B7 Section 11.1: kuerzere Pfade halten ihren Endzustand).
	private void BuildDuration()
	{
		float duration = 0.0;

		int camEntryCount = m_CamSegStart.Count();
		if (camEntryCount > 0)
		{
			float camEnd = m_CamSegStart.Get(camEntryCount - 1);
			if (camEnd > duration)
				duration = camEnd;
		}

		int lookEntryCount = m_LookSegStart.Count();
		if (lookEntryCount > 0)
		{
			float lookEnd = m_LookSegStart.Get(lookEntryCount - 1);
			if (lookEnd > duration)
				duration = lookEnd;
		}

		if (m_Tracks)
		{
			int trackCount = m_Tracks.Count();
			for (int t = 0; t < trackCount; t++)
			{
				PCT_Track track = m_Tracks.Get(t);
				if (!track || !track.enabled)
					continue;
				int keyCount = track.keys.Count();
				if (keyCount < 1)
					continue;
				PCT_TrackKey lastKey = track.keys.Get(keyCount - 1);
				if (lastKey && lastKey.time > duration)
					duration = lastKey.time;
			}
		}

		m_Duration = duration;
	}

	// ===== Transport (Plan_B7 Section 11.2) =====

	void Play()
	{
		if (m_Duration <= 0.0)
			return;
		if (m_Time >= m_Duration)
			SeekTo(0.0);
		m_Playing = true;
	}

	void Pause()
	{
		m_Playing = false;
	}

	bool IsPlaying()
	{
		return m_Playing;
	}

	float GetTime()
	{
		return m_Time;
	}

	float GetDuration()
	{
		return m_Duration;
	}

	// Scrub/Seek: setzt ALLE Vorwaertszeiger zurueck (Plan_B7 Section 11.2 -- die Zeiger laufen nur
	// monoton vor; ein Ruecksprung rollt die Segmentsuche neu von 0 auf).
	void SeekTo(float time)
	{
		m_Time = Math.Clamp(time, 0.0, m_Duration);
		m_CamSegment = 0;
		m_LookSegment = 0;
		m_FollowInitialized = false;
		m_Playing = false;					// API-Sicherheit: Seek stoppt die Wiedergabe (Review-Fix-Wave Fix 4, Section 11.3-Skizze)

		int cursorCount = m_TrackCursors.Count();
		for (int i = 0; i < cursorCount; i++)
		{
			m_TrackCursors.Set(i, 0);
		}
	}

	// ===== Frame-Tick (Plan_B7 Section 14.2; KEINE Allokation in diesem Pfad) =====

	void Tick(PCT_CinematicCamera cam, float timeslice)
	{
		if (!m_Playing || !cam)
			return;

		m_Time = m_Time + timeslice * m_PlaybackSpeed;
		if (m_Time >= m_Duration)
		{
			m_Time = m_Duration;
			m_Playing = false;
		}

		ApplyAt(cam, m_Time, timeslice);
	}

	// Wird von Tick UND vom Scrub-Pfad (einmalige Anwendung nach SeekTo) genutzt.
	void ApplyAt(PCT_CinematicCamera cam, float time, float timeslice)
	{
		if (!cam)
			return;

		m_HorizonLock = false;

		vector camPos = EvaluatePosition(time);
		cam.SetPosition(camPos);

		m_OrientAngles = EvaluateLookAngles(time, camPos, timeslice);
		SampleTracks(cam, time);
		ApplyOrientation(cam);
	}

	// ===== PositionEvaluator (Plan_B7 Section 14.3) =====

	private int FindCamSegment(float time)
	{
		int segmentCount = m_CamSegStart.Count() - 1;
		while (m_CamSegment < segmentCount - 1)
		{
			float nextStart = m_CamSegStart.Get(m_CamSegment + 1);
			if (time < nextStart)
				break;
			m_CamSegment = m_CamSegment + 1;
		}
		return m_CamSegment;
	}

	vector EvaluatePosition(float time)
	{
		int seg = FindCamSegment(time);
		PCT_PathPoint a = m_CameraPath.points.Get(seg);

		// Ab der Ankunft (TravelEnd) ist die Kamera eingefroren -- deckt sowohl das WaitHold-Fenster
		// als auch den frueheren tSeg-1-Clamp-Fall ab (Review-Fix-Wave Fix 1).
		float travelEnd = m_CamSegTravelEnd.Get(seg);
		if (time >= travelEnd)
		{
			vector frozenPos = m_CamPositions.Get(seg + 1);
			return frozenPos;
		}

		float segStart = m_CamSegStart.Get(seg);
		float segDur = travelEnd - segStart;
		float tSeg = 1.0;
		if (segDur > 0.0)
			tSeg = (time - segStart) / segDur;
		tSeg = Math.Clamp(tSeg, 0.0, 1.0);

		int easingType = PCT_Validation.ParseEasing(a.easing);
		float tEased = PCT_Math.Ease(easingType, tSeg);

		int pathType = PCT_Validation.ParsePathType(a.pathType);
		if (pathType == EPCT_PathType.LINEAR)
		{
			vector posA = m_CamPositions.Get(seg);
			vector posB = m_CamPositions.Get(seg + 1);
			vector linearPos = vector.Lerp(posA, posB, tEased);
			return linearPos;
		}

		float u = tEased;
		PCT_ArcLengthSegment lut = m_CamSegLuts.Get(seg);
		if (lut)
			u = PCT_ArcLength.RemapToSplineParam(lut, tEased);

		vector pos = EvaluateSegmentGeometry(seg, u);
		return pos;
	}

	// Reine Segment-Geometrie (ohne Zeit/Easing) -- gemeinsame Basis fuer Playback UND LUT-Aufbau,
	// damit Bogenlaengen exakt zur abgespielten Kurve passen (Abweichung 3 im Kopfkommentar).
	private vector EvaluateSegmentGeometry(int seg, float u)
	{
		PCT_PathPoint a = m_CameraPath.points.Get(seg);
		int pathType = PCT_Validation.ParsePathType(a.pathType);

		if (pathType == EPCT_PathType.LINEAR)
		{
			vector posA = m_CamPositions.Get(seg);
			vector posB = m_CamPositions.Get(seg + 1);
			vector linearPos = vector.Lerp(posA, posB, u);
			return linearPos;
		}

		if (pathType == EPCT_PathType.BEZIER)
		{
			PCT_PathPoint b = m_CameraPath.points.Get(seg + 1);
			vector p0 = m_CamPositions.Get(seg);
			vector p3 = m_CamPositions.Get(seg + 1);
			vector bezierPos = PCT_Math.CubicBezier(p0, a.bezierOut, b.bezierIn, p3, u);
			return bezierPos;
		}

		if (pathType != EPCT_PathType.SMOOTH_CURVE && pathType != EPCT_PathType.RAIL && !m_WarnedPathTypeFallback)
		{
			CF_Log.Warn("PCT: pathType '" + a.pathType + "' wird in Phase 3 als SmoothCurve ausgewertet (Orbit/Arc/HandheldPath: gebacken via Presets, Live ab Phase 4/5).");
			m_WarnedPathTypeFallback = true;
		}

		// SmoothCurve/Rail (und Phase-3-Fallbacks): per-Segment-lokales Catmull-Rom -- Endpunkte des
		// Segments + je ein Tangenten-Nachbar, Phantompunkt-Duplikation am Rand (Plan_B7 Section 14.3).
		int count = m_CamPositions.Count();
		int i1 = seg;
		int i2 = seg + 1;
		int i0 = i1 - 1;
		if (i0 < 0)
			i0 = i1;
		int i3 = i2 + 1;
		if (i3 > count - 1)
			i3 = i2;

		vector cp0 = m_CamPositions.Get(i0);
		vector cp1 = m_CamPositions.Get(i1);
		vector cp2 = m_CamPositions.Get(i2);
		vector cp3 = m_CamPositions.Get(i3);
		vector result = PCT_Math.CatmullRomSegment(cp0, cp1, cp2, cp3, u);
		return result;
	}

	// ===== LookEvaluator (Plan_B7 Section 14.3; KEIN direkter Orientierungs-Write) =====

	private int FindLookSegment(float time)
	{
		int segmentCount = m_LookSegStart.Count() - 1;
		while (m_LookSegment < segmentCount - 1)
		{
			float nextStart = m_LookSegStart.Get(m_LookSegment + 1);
			if (time < nextStart)
				break;
			m_LookSegment = m_LookSegment + 1;
		}
		return m_LookSegment;
	}

	// Phase 3: targetRef-Aufloesung (ActorMarks/Netz-Ids) ist Plan_B3-/Phase-4-Sache -- Blickziele
	// kommen aus worldTarget (Abweichung 6 im Kopfkommentar).
	private Object ResolveObject(string targetRef)
	{
		return null;
	}

	private vector ResolveLookTarget(PCT_LookPoint lp, Object obj)
	{
		if (!lp)
			return "0 0 0";
		if (!obj)
		{
			vector freeTarget = lp.worldTarget + lp.offset;
			return freeTarget;
		}

		vector anchor = obj.GetPosition();
		Human human = Human.Cast(obj);
		if (human && lp.targetBone != "")
		{
			int boneIndex = human.GetBoneIndexByName(lp.targetBone);
			if (boneIndex >= 0)
				anchor = human.GetBonePositionWS(boneIndex);
		}
		vector withOffset = anchor + lp.offset;
		return withOffset;
	}

	vector EvaluateLookAngles(float time, vector camPos, float timeslice)
	{
		if (!m_LookPath)
			return "0 0 0";
		if (m_LookPath.points.Count() < 1)
			return "0 0 0";

		int seg = FindLookSegment(time);
		PCT_LookPoint a = m_LookPath.points.Get(seg);

		// Additiver HorizonLock (Review-Fix-Wave Fix 2, Plan_B7 Section 5.2 -- "ueberlagert
		// LookAt/FreeRotation"): unabhaengig vom lookMode gesetzt, VOR den Modus-Zweigen; der
		// bestehende eigenstaendige EPCT_LookMode.HORIZON_LOCK-Modus bleibt als Kompat erhalten.
		if (a.horizonLock)
			m_HorizonLock = true;

		int mode = PCT_Validation.ParseLookMode(a.lookMode);

		if (mode == EPCT_LookMode.FREE_ROTATION)
			return "0 0 0";
		if (mode == EPCT_LookMode.HORIZON_LOCK)
		{
			// Standalone-HorizonLock-Punkt: Basis 0 (wie FreeRotation), Roll wird in ApplyOrientation
			// genullt (Plan_B7 Section 5.2: HorizonLock ueberlagert, ersetzt nicht).
			m_HorizonLock = true;
			return "0 0 0";
		}

		int lastLook = m_LookPath.points.Count() - 1;
		vector target;
		if (seg >= lastLook)
		{
			Object objLast = ResolveObject(a.targetRef);
			target = ResolveLookTarget(a, objLast);
		}
		else
		{
			PCT_LookPoint b = m_LookPath.points.Get(seg + 1);
			float segStart = m_LookSegStart.Get(seg);
			float segEnd = m_LookSegStart.Get(seg + 1);
			float segDur = segEnd - segStart;
			float tSeg = 1.0;
			if (segDur > 0.0)
				tSeg = (time - segStart) / segDur;
			tSeg = Math.Clamp(tSeg, 0.0, 1.0);

			int easingType = PCT_Validation.ParseEasing(a.easing);
			float tEased = PCT_Math.Ease(easingType, tSeg);

			Object objA = ResolveObject(a.targetRef);
			Object objB = ResolveObject(b.targetRef);
			vector ta = ResolveLookTarget(a, objA);
			vector tb = ResolveLookTarget(b, objB);
			target = vector.Lerp(ta, tb, tEased);
		}

		if (mode == EPCT_LookMode.FOLLOW_TARGET)
			target = SmoothFollowTarget(target, a.smoothTime, timeslice);

		vector dir = target - camPos;
		float dirLength = dir.Length();
		if (dirLength < 0.0001)
			return "0 0 0";

		vector baseAngles = dir.VectorToAngles();
		return baseAngles;
	}

	// FollowTarget-Daempfung (Plan_B4 Section 5.7 / Plan_B7 Section 5.2): Math.SmoothCD je Komponente,
	// Velocity-Zustand als float[1]-Member (Signatur "inout float velocity[]", EnMath.c:678).
	private vector SmoothFollowTarget(vector target, float smoothTime, float timeslice)
	{
		if (timeslice <= 0.0)
			return target;
		if (smoothTime <= 0.0)
			return target;

		if (!m_FollowInitialized)
		{
			m_FollowSmoothed = target;
			m_FollowVelX[0] = 0.0;
			m_FollowVelY[0] = 0.0;
			m_FollowVelZ[0] = 0.0;
			m_FollowInitialized = true;
			return target;
		}

		float smoothedX = Math.SmoothCD(m_FollowSmoothed[0], target[0], m_FollowVelX, smoothTime, 1000.0, timeslice);
		float smoothedY = Math.SmoothCD(m_FollowSmoothed[1], target[1], m_FollowVelY, smoothTime, 1000.0, timeslice);
		float smoothedZ = Math.SmoothCD(m_FollowSmoothed[2], target[2], m_FollowVelZ, smoothTime, 1000.0, timeslice);

		m_FollowSmoothed[0] = smoothedX;
		m_FollowSmoothed[1] = smoothedY;
		m_FollowSmoothed[2] = smoothedZ;
		return m_FollowSmoothed;
	}

	// ===== Track-Sampler (Plan_B7 Section 14.4) =====

	private void SampleTracks(PCT_CinematicCamera cam, float time)
	{
		if (!m_Tracks)
			return;

		int count = m_Tracks.Count();
		for (int i = 0; i < count; i++)
		{
			PCT_Track track = m_Tracks.Get(i);
			if (!track || !track.enabled)
				continue;
			if (track.keys.Count() < 1)
				continue;

			float value = SampleTrack(track, i, time);
			ApplyChannel(cam, track.channel, value);
		}
	}

	// Ein Track zwischen zwei Keys; Vorwaertszeiger je Track; tEased aus keyB.easing (Section 14.4);
	// Winkelkanaele ueber den kuerzesten Weg (PCT_Math.InterpolateAngles, Abweichung 2 im Kopfkommentar).
	private float SampleTrack(PCT_Track track, int trackIndex, float time)
	{
		int keyCount = track.keys.Count();

		int cursor = m_TrackCursors.Get(trackIndex);
		while (cursor < keyCount - 1)
		{
			PCT_TrackKey nextKey = track.keys.Get(cursor + 1);
			if (time < nextKey.time)
				break;
			cursor = cursor + 1;
		}
		m_TrackCursors.Set(trackIndex, cursor);

		PCT_TrackKey keyA = track.keys.Get(cursor);

		if (cursor >= keyCount - 1)
			return keyA.value;

		PCT_TrackKey keyB = track.keys.Get(cursor + 1);

		if (time <= keyA.time)
			return keyA.value;

		float segDur = keyB.time - keyA.time;
		float tSeg = 1.0;
		if (segDur > 0.0)
			tSeg = (time - keyA.time) / segDur;
		tSeg = Math.Clamp(tSeg, 0.0, 1.0);

		int easingType = PCT_Validation.ParseEasing(keyB.easing);
		float tEased = PCT_Math.Ease(easingType, tSeg);

		bool isAngle = false;
		if (track.channel == "pan")
			isAngle = true;
		if (track.channel == "tilt")
			isAngle = true;
		if (track.channel == "roll")
			isAngle = true;

		float result;
		if (isAngle)
		{
			result = PCT_Math.InterpolateAngles(keyA.value, keyB.value, tEased);
		}
		else
		{
			result = Math.Lerp(keyA.value, keyB.value, tEased);
		}
		return result;
	}

	// Kanal-Senke: pan/tilt/roll ADDIEREN auf den Orientierungs-Akkumulator; uebrige Kanaele direkt
	// auf Rig/Kamera (der Rig-Anwendungspfad in PCT_CinematicCamera.OnUpdate schreibt FOV/DOF selbst --
	// EIN Schreiber je Kanal, Plan_B7 Section 14.4).
	private void ApplyChannel(PCT_CinematicCamera cam, string channel, float value)
	{
		if (channel == "pan")
		{
			float newYaw = m_OrientAngles[0] + value;
			m_OrientAngles[0] = newYaw;
			return;
		}
		if (channel == "tilt")
		{
			float newPitch = m_OrientAngles[1] + value;
			m_OrientAngles[1] = newPitch;
			return;
		}
		if (channel == "roll")
		{
			float newRoll = m_OrientAngles[2] + value;
			m_OrientAngles[2] = newRoll;
			return;
		}

		ApplyScalarChannel(cam, channel, value);
	}

	private void ApplyScalarChannel(PCT_CinematicCamera cam, string channel, float value)
	{
		if (channel == "focalLength")
		{
			if (cam.m_PCT_Rig)
				cam.m_PCT_Rig.focalLengthMm = value;
			return;
		}
		if (channel == "focusDistance")
		{
			cam.m_PCT_FocusDistanceM = value;
			return;
		}
		if (channel == "aperture")
		{
			if (cam.m_PCT_Rig)
				cam.m_PCT_Rig.aperture = value;
			return;
		}
		if (channel == "height")
		{
			vector pos = cam.GetPosition();
			pos[1] = value;
			cam.SetPosition(pos);
			return;
		}
		// handheld/stabilize/subjectScreenX/subjectScreenY/speed: Senken folgen mit den
		// Bewegungs-Presets (Phase 3 P3-7) bzw. Phase 4/5 (Plan_B7 Section 10.1) -- bewusst kein Effekt.
	}

	// EINZIGER Orientierungs-Write pro Frame (Plan_B7 Section 14.4); reale API SetOrientation
	// (<yaw,pitch,roll> Grad, Object.c:311) statt der nicht existenten SetYawPitchRoll-Skizze.
	private void ApplyOrientation(PCT_CinematicCamera cam)
	{
		if (!cam)
			return;

		if (m_HorizonLock)
		{
			m_OrientAngles[2] = 0.0;
		}

		cam.SetOrientation(m_OrientAngles);
	}

	// ===== Kompat-/Export-Sampling (Plan_B7 Section 10.2/14.4) =====

	// Wertet Position, Basis-Blick + Winkel-Track-Offsets und die FOV-/Fokus-Kanaele an EINEM Zeitpunkt
	// aus und liefert ein flaches PCT_Keyframe (CSV-/JSON-Export, B3 Section 8). Nutzt bewusst KEINE
	// Kamera-Instanz (reine Datenauswertung); Track-Zeiger werden danach zurueckgesetzt, damit das
	// Sampling einen laufenden Playback-Zustand nicht verfaelscht.
	PCT_Keyframe SampleKeyframe(float time, PCT_CameraRig rig)
	{
		PCT_Keyframe kf = new PCT_Keyframe();
		kf.time = Math.Clamp(time, 0.0, m_Duration);

		int savedCamSegment = m_CamSegment;
		int savedLookSegment = m_LookSegment;
		m_CamSegment = 0;
		m_LookSegment = 0;

		kf.position = EvaluatePosition(kf.time);

		vector angles = EvaluateLookAngles(kf.time, kf.position, 0.0);
		float focalMm = 0.0;
		if (rig)
			focalMm = rig.focalLengthMm;
		float focusM = 0.0;

		if (m_Tracks)
		{
			int count = m_Tracks.Count();
			for (int i = 0; i < count; i++)
			{
				PCT_Track track = m_Tracks.Get(i);
				if (!track || !track.enabled)
					continue;
				if (track.keys.Count() < 1)
					continue;

				int savedCursor = m_TrackCursors.Get(i);
				m_TrackCursors.Set(i, 0);
				float value = SampleTrack(track, i, kf.time);
				m_TrackCursors.Set(i, savedCursor);

				if (track.channel == "pan")
				{
					float sampledYaw = angles[0] + value;
					angles[0] = sampledYaw;
				}
				if (track.channel == "tilt")
				{
					float sampledPitch = angles[1] + value;
					angles[1] = sampledPitch;
				}
				if (track.channel == "roll")
				{
					float sampledRoll = angles[2] + value;
					angles[2] = sampledRoll;
				}
				if (track.channel == "focalLength")
					focalMm = value;
				if (track.channel == "focusDistance")
					focusM = value;
			}
		}

		kf.orientation = angles;

		if (rig && focalMm > 0.0)
		{
			float fovRad = PCT_Math.FovVFromFocal(rig.sensorHeightMm, focalMm);
			kf.fovDegrees = fovRad * Math.RAD2DEG;
		}
		if (focusM > 0.0)
			kf.focusDistance = focusM;

		m_CamSegment = savedCamSegment;
		m_LookSegment = savedLookSegment;
		return kf;
	}
}
