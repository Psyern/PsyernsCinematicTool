// Task P1-5 / PCT_Math (Teil 1) -- Docs/Plan_B4_Kamera_Engine.md Section 2.1 (Grundformel FOV<->Brennweite),
// Section 4.5/4.6 (Winkel-Interpolation, Easing-Basisfunktionen) und Docs/Research/Research_Framework_Patterns.md
// Section 7 (Clean-Room-Vorgabe: "port the one-line formulas into PCT_Math", InterpolateAngles "uses
// Math.NormalizeAngle + shortest-path"). Statische Utility-Klasse, COT-frei -- nur Engine-`Math`
// (1_Core/DayZ/proto/EnMath.c), keine Allokationen. Formeln/Signaturen sind Clean-Room-Neuimplementierungen
// nach Beschreibung, kein Code aus Expansion/COT kopiert (Plan_B7-Basisfunktionen dieses Kapitels bleiben in
// Phase 3 gueltig, siehe Plan_B4 Abstract).
//
// Task P3-1 -- Erweiterung um den vollstaendigen Easing-Katalog Docs/Plan_B7_Kamerafahrten_Bewegungssteuerung.md
// Section 9 (EPCT_Easing, Data/PCT_Enums.c) und den Catmull-Rom-Sampling-Wrapper (Plan_B4 Section 4.3/4.4,
// EnMath3D.c:471 "proto static native vector Curve(ECurveType type, float param, notnull array<vector>
// points)"). Die alte Ease(bool,bool,float)-Zwei-Parameter-Notloesung von Task P1-5 wird dabei ENTFERNT
// (nicht als Wrapper erhalten) -- Begruendung siehe Kommentar bei der neuen Ease(int,float)-Funktion unten.
class PCT_Math
{
	// Task P1-6 / Plan_B4 Section 3.2: globaler Kalibrier-Gain fuer BlurFromAperture (kalibrierbare
	// Heuristik, kein physikalischer Wert -- Plan_B4 Section 3.2 "Ausdruecklich eine kalibrierbare
	// Naeherung, keine physikalische Simulation"). O-Punkt V2 (OFFEN — ZU VERIFIZIEREN, Plan_B4 Section
	// 3.2): wirksame Skala des Engine-`blur`-Parameters noch nicht per Screenshot-Vergleich kalibriert,
	// Default bleibt 1.0 bis dahin. Statics immer s_PCT_* (CLAUDE.md-Namenskonvention).
	static float s_PCT_BlurGain = 1.0;

	// Grundformel Plan_B4 Section 2.1: fovVertical = 2 * atan(sensorHeightMm / (2 * focalLengthMm)), Radiant.
	// Math.Atan(float x) -- EnMath.c:386 ("Returns angle in radians from tangent").
	// Guard: focalLengthMm <= 0 waere Division durch 0 (oder Vorzeichenfehler) -- 0.0 zurueckgeben statt zu crashen.
	//
	// Handrechnung (Reihenentwicklung atan(x) = x - x^3/3 + x^5/5 - x^7/7 + ...):
	// FovVFromFocal(24, 50): ratio = 24 / 100 = 0.24
	//   atan(0.24) ~= 0.24 - 0.004608 + 0.000159 - 0.0000065 ~= 0.235545 rad
	//   fovRad = 2 * 0.235545 ~= 0.471090 rad  -- deckt sich mit Plan_B4 Section 2.5 Tabelle (50 mm -> 0.4711 rad).
	static float FovVFromFocal( float sensorHeightMm, float focalLengthMm )
	{
		if ( focalLengthMm <= 0.0 )
			return 0.0;

		float denom = 2.0 * focalLengthMm;
		float ratio = sensorHeightMm / denom;
		float fovRad = 2.0 * Math.Atan( ratio );
		return fovRad;
	}

	// Umkehrfunktion von FovVFromFocal: focalLengthMm = sensorHeightMm / (2 * tan(fovRad / 2)).
	// Math.Tan(float angle) -- EnMath.c:355 ("Returns tangent of angle in radians").
	// Guards: fovRad <= 0 (kein gueltiger FOV, waere ausserdem Tan-Null bei 0) UND tanHalf == 0 als zweite
	// Absicherung gegen Division durch 0 (deckt denselben Fall nochmal explizit ab, falls fovRad minimal > 0
	// aber numerisch auf 0 rundet).
	//
	// Handrechnung: FocalFromFovV(24, 0.4711): halfFov = 0.23555
	//   tan(0.23555) ~= 0.23555 + 0.013072/3 + 2*0.0007251/15 ~= 0.23555 + 0.004357 + 0.0000967 ~= 0.240004
	//   focalLengthMm = 24 / (2 * 0.240004) ~= 24 / 0.480008 ~= 49.999 ~= 50.0 mm -- Rundtrip mit FovVFromFocal(24,50) bestaetigt.
	static float FocalFromFovV( float sensorHeightMm, float fovRad )
	{
		if ( fovRad <= 0.0 )
			return 0.0;

		float halfFov = fovRad * 0.5;
		float tanHalf = Math.Tan( halfFov );
		if ( tanHalf == 0.0 )
			return 0.0;

		float focalLengthMm = sensorHeightMm / ( 2.0 * tanHalf );
		return focalLengthMm;
	}

	// Plan_B4 Section 4.6: SMOOTH = t*t*(3-2t) (COT JMCinematicCamera.SmoothStep, JMCinematicCamera.c:243 --
	// dort `private`, aus Subklassen nicht aufrufbar, Research_COT_Infrastructure.md Section 1 -- deshalb hier
	// als eigene Ein-Zeilen-Portierung). t wird vorher auf [0,1] geclampt (Math.Clamp -- EnMath.c:540,
	// "Clamps 'value' to 'min' if it is lower than 'min', or to 'max' if it is higher than 'max'").
	//
	// Handrechnung: SmoothStep(0.5) = 0.25 * (3 - 1) = 0.25 * 2 = 0.5
	//               SmoothStep(0.25) = 0.0625 * (3 - 0.5) = 0.0625 * 2.5 = 0.15625
	//               SmoothStep(-1) -> clamp auf 0 -> 0.0; SmoothStep(2) -> clamp auf 1 -> 1.0
	static float SmoothStep( float t )
	{
		float clampedT = Math.Clamp( t, 0.0, 1.0 );
		float result = clampedT * clampedT * ( 3.0 - 2.0 * clampedT );
		return result;
	}

	// Plan_B4 Section 4.6: SMOOTHER = t*t*t*(t*(6t-15)+10) (quintisch; COT JMCinematicCamera.SmootherStep,
	// JMCinematicCamera.c:248 -- ebenfalls `private`, gleiche Begruendung wie SmoothStep). t wird vorher
	// auf [0,1] geclampt.
	//
	// Handrechnung: SmootherStep(0.5): t^3=0.125, innerer Term t*(6t-15)+10 = 0.5*(3-15)+10 = 0.5*(-12)+10 = 4.0
	//               result = 0.125 * 4.0 = 0.5
	//               SmootherStep(0.25): t^3=0.015625, innerer Term = 0.25*(1.5-15)+10 = 0.25*(-13.5)+10 = 6.625
	//               result = 0.015625 * 6.625 = 0.103515625
	static float SmootherStep( float t )
	{
		float clampedT = Math.Clamp( t, 0.0, 1.0 );
		float result = clampedT * clampedT * clampedT * ( clampedT * ( 6.0 * clampedT - 15.0 ) + 10.0 );
		return result;
	}

	// Task P3-1 -- ABLOESUNG der alten Ease(bool,bool,float)-Notloesung (Task P1-5, siehe Doku dort: "B4
	// selbst spezifiziert Ease(int easingType, float t) ... dieses Enum existiert in Phase 1 noch nicht").
	// Das Enum existiert jetzt (EPCT_Easing, Data/PCT_Enums.c) -- die damalige Vorwaerts-Abhaengigkeit
	// besteht nicht mehr, die Zwei-Bool-Abbildung ist damit ueberfluessig. Docs/Phase1_DoD_Protokoll.md
	// Zeile 40 kuendigt diese Abloesung bereits an: "PCT_Math.Ease(bool,bool) wird bei EPCT_Easing-
	// Einfuehrung (Plan_B7 Section 9) abgeloest."
	//
	// Entscheidung ENTFERNEN statt Kompat-Wrapper (Task-P3-1-Brief-Vorgabe: "behalte sie als duennen
	// Kompat-Wrapper ... ODER entferne sie, WENN kein Bestandscode sie aufruft (Grep!)"): Grep ueber das
	// gesamte PsyernsCinematicTool/Scripts-Verzeichnis nach ".Ease(" / "PCT_Math.Ease" (Task-P3-1-Report)
	// fand AUSSER den eigenen Handrechnungs-Kommentaren dieser Datei KEINEN einzigen Aufrufer -- weder in
	// 3_Game noch 4_World/5_Mission. Ein Kompat-Wrapper haette daher keinen Konsumenten und waere reine
	// spekulative Vorratshaltung (CLAUDE.md "Avoid speculative or nice-to-have changes"). Ersetzt durch die
	// von Plan_B7 Section 9.3 namentlich geforderte Signatur `Ease(int easingType, float t)`.
	//
	// Plan_B7 Section 9.2-Tabelle -- vollstaendiger Katalog. LINEAR/SMOOTH/SMOOTHER/EASE_IN/EASE_OUT sind
	// Plan_B4 Section 4.6 (Basis, Formeln unveraendert); EASE_IN_OUT/HARD_START/HARD_STOP/FAST_TRANSITION/
	// SMOOTH_TRANSITION/CUSTOM_CURVE sind die Plan_B7-Erweiterung. CUSTOM_CURVE braucht zusaetzlich
	// Kurvendaten (PCT_CustomCurve) und kann daher NICHT ueber diese (kurvenlose) Signatur ausgewertet
	// werden (Plan_B7 Section 9.3, letzter Satz) -- Aufrufer mit easingType == EPCT_Easing.CUSTOM_CURVE
	// muessen stattdessen EaseCustom(curve, t) direkt aufrufen; dieser Zweig liefert hier defensiv nur die
	// Identitaet t zurueck (kein Crash, aber auch keine echte Kurvenauswertung ohne Kurvendaten).
	//
	// Handrechnung (Selbstpruefungs-Vorgabe des Task-Briefs): Ease(EPCT_Easing.EASE_IN, 0.5) = 0.5*0.5 = 0.25;
	// Ease(EPCT_Easing.SMOOTHER, 0.5) = SmootherStep(0.5) = 0.5 (siehe Handrechnung bei SmootherStep oben).
	// Weitere volle t=0.25/0.5/0.75-Stuetzwert-Sweeps (3 repraesentative Katalog-NEUZUGAENGE) stehen bei
	// EaseHardStart/EaseHardStop/EaseRamped unten.
	static float Ease( int easingType, float t )
	{
		float clampedT = Math.Clamp( t, 0.0, 1.0 );

		switch ( easingType )
		{
			case EPCT_Easing.LINEAR:
				return clampedT;

			case EPCT_Easing.SMOOTH:
				return SmoothStep( clampedT );

			case EPCT_Easing.SMOOTHER:
				return SmootherStep( clampedT );

			case EPCT_Easing.EASE_IN:
			{
				float easeInResult = clampedT * clampedT;
				return easeInResult;
			}

			case EPCT_Easing.EASE_OUT:
			{
				float inv = 1.0 - clampedT;
				float easeOutResult = 1.0 - inv * inv;
				return easeOutResult;
			}

			case EPCT_Easing.EASE_IN_OUT:
				// Plan_B7 Section 9.2: "EASE_IN_OUT / SMOOTH" teilen sich dieselbe Formel (Quelle:
				// "Ease In/Out" == SMOOTH-Semantik) -- kein eigener Handrechnungs-Fall noetig, deckungsgleich
				// mit SMOOTH oben.
				return SmoothStep( clampedT );

			case EPCT_Easing.HARD_START:
				return EaseHardStart( clampedT );

			case EPCT_Easing.HARD_STOP:
				return EaseHardStop( clampedT );

			case EPCT_Easing.FAST_TRANSITION:
				return EaseRamped( clampedT, 0.15 );

			case EPCT_Easing.SMOOTH_TRANSITION:
				return EaseRamped( clampedT, 0.45 );

			case EPCT_Easing.CUSTOM_CURVE:
				// Kurvendaten fehlen in dieser Signatur -- Aufrufer muss EaseCustom(curve, t) direkt nutzen
				// (Plan_B7 Section 9.3). Identitaets-Fallback, kein Crash.
				return clampedT;
		}

		return clampedT;
	}

	// Plan_B7 Section 9.3 (Quelle Section 6, zeichengetreu aus dem Plan uebernommen) -- parametrisierte
	// Rampen fuer FAST_TRANSITION (ramp=0.15) und SMOOTH_TRANSITION (ramp=0.45): SmoothStep-Einstieg auf
	// [0,ramp], linear/konstant in der Mitte, SmoothStep-Ausstieg auf [1-ramp,1]. Kontinuitaet an den
	// Rampengrenzen: bei t=ramp liefert der SmoothStep-Zweig a=1 -> sA=1 -> result=ramp, identisch zum
	// direkt anschliessenden linearen Mittelteil (kein Sprung); symmetrisch am oberen Rand.
	//
	// Handrechnung (1 von 3 repraesentativen Katalog-Neuzugaengen mit vollem t=0.25/0.5/0.75-Sweep;
	// SMOOTH_TRANSITION/ramp=0.45 durchlaeuft dabei alle drei Zweige der Funktion):
	//   EaseRamped(0.25, 0.45): t < ramp (0.25 < 0.45) -> a = t/ramp = 0.25/0.45 = 0.555556
	//     sA = a*a*(3-2a) = 0.308642*(3-1.111111) = 0.308642*1.888889 = 0.583019
	//     result = sA*ramp = 0.583019*0.45 = 0.262358
	//   EaseRamped(0.5, 0.45): ramp <= t <= 1-ramp (0.45 <= 0.5 <= 0.55) -> Mittelzone, result = t = 0.5
	//   EaseRamped(0.75, 0.45): t > 1-ramp (0.75 > 0.55) -> bStart = 0.55, b = (0.75-0.55)/0.45 = 0.444444
	//     sB = b*b*(3-2b) = 0.197531*(3-0.888889) = 0.197531*2.111111 = 0.417147
	//     result = bStart + sB*ramp = 0.55 + 0.417147*0.45 = 0.55 + 0.187716 = 0.737716
	// Randprobe FAST_TRANSITION (ramp=0.15): bei t=0.25/0.5/0.75 liegen ALLE drei Stuetzwerte bereits in
	// der linearen Mittelzone (0.15 <= t <= 0.85) -> EaseRamped(t,0.15) = t fuer alle drei (Ausgabe =
	// Eingabe); die S-Kurve wirkt bei diesem schmalen Rampenfenster nur innerhalb [0,0.15] bzw. [0.85,1].
	static float EaseRamped( float t, float ramp )
	{
		if ( ramp <= 0.0 )
			return t;

		if ( t < ramp )
		{
			float a = t / ramp;
			float sA = a * a * ( 3.0 - 2.0 * a );
			return sA * ramp;
		}

		if ( t > 1.0 - ramp )
		{
			float bStart = 1.0 - ramp;
			float b = ( t - bStart ) / ramp;
			float sB = b * b * ( 3.0 - 2.0 * b );
			return bStart + sB * ramp;
		}

		return t;
	}

	// Plan_B7 Section 9.3 (Quelle Section 6) -- Grenzfall "ohne Beschleunigungsphase": steiler kubischer
	// Onset (EaseOut-Kubik-Form), keine echte Geschwindigkeits-Diskontinuitaet mit einer normierten
	// f(0)=0,f(1)=1-Kurve darstellbar (Plan_B7 Section 9.2 Begruendung) -- praktikable Naeherung.
	//
	// Handrechnung (2. von 3 repraesentativen Katalog-Neuzugaengen, t=0.25/0.5/0.75):
	//   EaseHardStart(0.25): iu=0.75, iu^3=0.421875, result=1-0.421875=0.578125
	//   EaseHardStart(0.5):  iu=0.5,  iu^3=0.125,    result=1-0.125=0.875
	//   EaseHardStart(0.75): iu=0.25, iu^3=0.015625, result=1-0.015625=0.984375
	// (monoton steigend, ueberwiegender Teil der Bewegung liegt frueh im Segment -- konsistent mit
	// Quelle Section 6 "Sprung auf ~Vollgeschwindigkeit am Anfang, ohne Beschleunigungsphase")
	static float EaseHardStart( float t )
	{
		float iu = 1.0 - t;
		return 1.0 - iu * iu * iu;
	}

	// Plan_B7 Section 9.3 (Quelle Section 6) -- Grenzfall "stoppt abrupt": steile kubische Ankunfts-
	// geschwindigkeit am Segmentende.
	//
	// Handrechnung (3. von 3 repraesentativen Katalog-Neuzugaengen, t=0.25/0.5/0.75):
	//   EaseHardStop(0.25) = 0.25^3 = 0.015625
	//   EaseHardStop(0.5)  = 0.5^3  = 0.125       (Selbstpruefungs-Vorgabe des Task-Briefs bestaetigt)
	//   EaseHardStop(0.75) = 0.75^3 = 0.421875
	// (ueberwiegender Teil der Bewegung liegt spaet im Segment -- konsistent mit "stoppt abrupt")
	static float EaseHardStop( float t )
	{
		return t * t * t;
	}

	// Plan_B7 Section 9.4 (Quelle Section 6 "benutzerdefinierte Geschwindigkeitskurve") -- zeichengetreu aus
	// dem Plan uebernommen. Stueckweise LINEARE Auswertung zwischen den PCT_CustomCurve.controlPoints
	// (monoton in time vorausgesetzt, siehe Data/PCT_CustomCurve.c); Vorwaerts-Scan, keine Allokation.
	// Nicht ueber Ease(int,float) erreichbar (siehe dort, Fall CUSTOM_CURVE) -- eigener Aufruf noetig, weil
	// die Kurvendaten (curve) nicht Teil der (int,float)-Signatur sind. Randfall < 2 Kontrollpunkte:
	// keine Kurve auswertbar -> Identitaet t.
	//
	// Handrechnung (Kontrollpunkte (0,0) / (0.5,0.8) / (1,1), 2 Segmente):
	//   EaseCustom(t=0.25): faellt in Segment 0 (a=(0,0), b=(0.5,0.8)); span=0.5, local=(0.25-0)/0.5=0.5
	//     result = Lerp(0, 0.8, 0.5) = 0.4
	//   EaseCustom(t=0.75): faellt in Segment 1 (a=(0.5,0.8), b=(1,1)); span=0.5, local=(0.75-0.5)/0.5=0.5
	//     result = Lerp(0.8, 1.0, 0.5) = 0.9
	static float EaseCustom( PCT_CustomCurve curve, float t )
	{
		if ( !curve || curve.controlPoints.Count() < 2 )
			return t;

		int count = curve.controlPoints.Count();
		int i = 0;
		while ( i < count - 1 )
		{
			PCT_TrackKey k1 = curve.controlPoints.Get( i + 1 );
			if ( t < k1.time )
				break;
			i++;
		}
		if ( i >= count - 1 )
			i = count - 2;

		PCT_TrackKey a = curve.controlPoints.Get( i );
		PCT_TrackKey b = curve.controlPoints.Get( i + 1 );
		float span = b.time - a.time;
		float local = 0.0;
		if ( span > 0.0 )
			local = ( t - a.time ) / span;

		float result = Math.Lerp( a.value, b.value, local );
		return result;
	}

	// Plan_B4 Section 4.3/4.4 -- Kurven-Sampling-Wrapper, signatur-treuer Aufruf von Math3D.Curve mit
	// ECurveType.CatmullRom (EnMath3D.c:20-25 `enum ECurveType { CatmullRom, NaturalCubic, UniformCubic }`;
	// EnMath3D.c:471 `proto static native vector Curve(ECurveType type, float param, notnull
	// array<vector> points)`). GLOBALE (indexuniforme) Parametrisierung ueber die GESAMTE Punktmenge
	// (Plan_B4 Section 4.3) -- genutzt von der Arc-Length-LUT (Utils/PCT_ArcLength.c, BuildLut) und von
	// jedem anderen Aufrufer, der dieselbe globale Catmull-Rom-Kurve braucht.
	//
	// Randfaelle (< 2 Punkte): Math3D.Curve verlangt laut Signatur ein "notnull array<vector> points" --
	// ein leeres oder zu kleines Array in einen nativen Aufruf zu geben waere unspezifiziertes/riskantes
	// Verhalten (Segfault-Risiko). Deshalb wird VOR dem nativen Aufruf abgefangen: 0 Punkte -> "0 0 0",
	// 1 Punkt -> dieser eine Punkt selbst (keine Kurve mit nur einem Punkt moeglich), sonst Delegation an
	// die Engine.
	//
	// Handrechnung: Math3D.Curve ist `proto native` (Engine-intern, kein einsehbarer Quellcode) -- eine
	// exakte Vorab-Handrechnung des Kurvenwerts ist damit nicht moeglich (das ist genau der Kern von
	// Plan_B4 Section 4.4 OFFEN-V4: Endpunktverhalten von Math3D.Curve bei CatmullRom ist NICHT verifiziert,
	// Testschritt = Print-Vergleich zur Laufzeit). Diese Funktion beschraenkt sich daher auf strukturell
	// verifizierbare Handrechnung -- die Randfaelle: SampleCatmullRom({}, 0.5) = "0 0 0" (0 Punkte);
	// SampleCatmullRom({P}, 0.5) = P (1 Punkt, unabhaengig von t); ab 2 Punkten wird 1:1 an
	// Math3D.Curve(CatmullRom, t, points) delegiert (offizielles Doku-Beispiel EnMath3D.c:461-468: 4 Punkte
	// (0,0,0)/(5,0,0)/(8,3,0)/(6,1,0), t=0.5 -> vom Aufrufer zur Laufzeit gegen Print() zu verifizieren,
	// nicht Handrechnung-faehig).
	// Task P3-7 -- Rotation eines Punkts um die Y-Achse durch center (Plan_B4 Section 5.6 Orbit-Baustein,
	// Clean-Room nach dem ExRotateAroundPoint-MUSTER aus Research_Framework_Patterns Section 7 -- kein
	// Code-Kopieren). Standard-2D-Rotation in der XZ-Ebene; angleDeg > 0 = mathematisch positive Richtung.
	// Handrechnung: RotateAroundPointY("0 0 0", "1 0 0", 90) -> x' = 1*cos90 - 0*sin90 = 0,
	// z' = 1*sin90 + 0*cos90 = 1 => "0 0 1".
	static vector RotateAroundPointY( vector center, vector point, float angleDeg )
	{
		float rad = angleDeg * Math.DEG2RAD;
		float s = Math.Sin( rad );
		float c = Math.Cos( rad );
		float dx = point[0] - center[0];
		float dz = point[2] - center[2];
		float rx = dx * c - dz * s;
		float rz = dx * s + dz * c;
		vector result = point;
		float newX = center[0] + rx;
		float newZ = center[2] + rz;
		result[0] = newX;
		result[2] = newZ;
		return result;
	}

	static vector SampleCatmullRom( array<vector> points, float t )
	{
		if ( !points )
			return "0 0 0";

		int count = points.Count();
		if ( count <= 0 )
			return "0 0 0";
		if ( count == 1 )
			return points.Get( 0 );

		float clampedT = Math.Clamp( t, 0.0, 1.0 );
		vector result = Math3D.Curve( ECurveType.CatmullRom, clampedT, points );
		return result;
	}

	// Kuerzester-Weg-Winkelinterpolation in Grad (skalar, eine Achse). Clean-Room nach dem in
	// Research_Framework_Patterns.md Section 7 beschriebenen Muster ("uses Math.NormalizeAngle +
	// shortest-path") -- KEIN Kopieren des tatsaechlichen ExpansionMath.InterpolateAngles-Codes (der dort
	// zusaetzliche mult/pow-Federungsparameter hat, die hier nicht gefordert sind), sondern
	// Neuimplementierung nach der Kurzbeschreibung mit der nativen Engine-Funktion.
	// Math.NormalizeAngle(float ang) -- EnMath.c:155 ("Normalizes the angle (0...360)", Beispiel
	// NormalizeAngle(390)=30, NormalizeAngle(-90)=270).
	// Ablauf: beide Eingabewinkel normalisieren, kuerzeste Differenz bestimmen (>180 Grad -> Gegenrichtung
	// waehlen, analog Plan_B4 Section 4.5 ShortestAngleDelta), linear entlang der Differenz interpolieren,
	// Ergebnis erneut normalisieren (deckt den Wrap-Fall a=350/b=10 exakt auf 0 statt 360 ab).
	//
	// Handrechnung: InterpolateAngles(350, 10, 0.5):
	//   normA=350, normB=10, delta = NormalizeAngle(10-350) = NormalizeAngle(-340) = 20 (<=180, bleibt)
	//   raw = 350 + 20*0.5 = 360 -> NormalizeAngle(360) = 0   [Erwartungswert laut Task-Brief]
	// Gegenprobe InterpolateAngles(10, 200, 0.5) (Differenz > 180, muss Gegenrichtung nehmen):
	//   normA=10, normB=200, delta = NormalizeAngle(190) = 190 (>180) -> delta = 190-360 = -170
	//   raw = 10 + (-170)*0.5 = 10-85 = -75 -> NormalizeAngle(-75) = 285
	static float InterpolateAngles( float a, float b, float t )
	{
		float normA = Math.NormalizeAngle( a );
		float normB = Math.NormalizeAngle( b );
		float delta = Math.NormalizeAngle( normB - normA );
		if ( delta > 180.0 )
			delta = delta - 360.0;

		float raw = normA + delta * t;
		float result = Math.NormalizeAngle( raw );
		return result;
	}

	// Task P1-6 / Plan_B4 Section 3.1/3.2: Blende (f-Stop) + Brennweite -> normierter, dimensionsloser
	// Blur-Faktor fuer PPEffects.OverrideDOF/Camera.SetFocus (Code zeichengetreu aus dem Plan
	// uebernommen). blurBase kommt aus der Tabellen-LUT (LookupBlurBase, s.u.); focalScale skaliert
	// laengere Brennweiten staerker (50 mm = Referenz), geclampt auf [0.5, 3.0] (deckt die Plan_B4
	// Section 2.4 Brennweiten-Preset-Reihe 14-200 mm ab: 14/50=0.28 -> clamp 0.5, 200/50=4.0 -> clamp
	// 3.0); s_PCT_BlurGain ist der globale Kalibrier-Gain (O-Punkt V2, s.o.). Endergebnis auf [0,1]
	// geclampt (Engine-`blur` ist dimensionslos, PPEDOF PARAM_BLUR Default 1.0, Research_Vanilla_APIs.md
	// Section 3).
	//
	// Handrechnung (Task-P1-6-Brief):
	// BlurFromAperture(2.0, 50): LookupBlurBase(2.0) trifft exakt den Stuetzpunkt 2.0 -> blurBase=0.75
	//   (LerpBlurSegment(2.0,1.4,0.90,2.0,0.75): t=InverseLerp(1.4,2.0,2.0)=1.0, Lerp(0.90,0.75,1.0)=0.75)
	//   focalScale = 50/50 = 1.0 (kein Clamp) -> blur = 0.75 * 1.0 * 1.0(Gain) = 0.75
	//   Math.Clamp(0.75,0,1) = 0.75
	// BlurFromAperture(1.5, 85): LookupBlurBase(1.5) liegt zwischen 1.4 (0.90) und 2.0 (0.75):
	//   t = InverseLerp(1.4,2.0,1.5) = (1.5-1.4)/(2.0-1.4) = 0.1/0.6 = 0.166667
	//   blurBase = Lerp(0.90,0.75,0.166667) = 0.90 + (0.75-0.90)*0.166667 = 0.90 - 0.025 = 0.875
	//   focalScale = 85/50 = 1.7 (innerhalb [0.5,3.0], kein Clamp) -> blur = 0.875 * 1.7 * 1.0(Gain) = 1.4875
	//   Math.Clamp(1.4875,0,1) = 1.0 (oberer Clamp greift -- 1.5 f/85mm ist ein sehr starker Blur-Fall)
	static float BlurFromAperture( float fStop, float focalLengthMm )
	{
		float blurBase = LookupBlurBase( fStop );				// Tabellen-LUT, linear interpoliert
		float focalScale = focalLengthMm / 50.0;			// 50 mm als Referenz
		float clampedScale = Math.Clamp( focalScale, 0.5, 3.0 );
		float blur = blurBase * clampedScale * s_PCT_BlurGain;	// Statics immer s_PCT_*
		return Math.Clamp( blur, 0.0, 1.0 );
	}

	// Tabellen-LUT Plan_B4 Section 3.2 (f-Stop -> blurBase, normiert 0..1), Werte EXAKT aus der
	// Plan-Tabelle, linear interpoliert zwischen den Stuetzpunkten, ausserhalb geclampt (fStop <= 1.2
	// -> 1.00, fStop >= 22 -> 0.00, DOF-aus-Fall). Bewusst als feste If-Kette statt Array/Objekt-LUT:
	// CLAUDE.md/Task-Brief verbietet Allokationen im Per-Frame-Pfad (Risiko R5) -- eine If-Kette braucht
	// kein `new` und keine Laufzeit-Allokation, ein pro Aufruf neu gebautes Stuetzpunkt-Array (oder ein
	// Member-Array, das ebenfalls indiziert/durchsucht werden muesste) waere hier unnoetiger Aufwand.
	// private, weil nur intern von BlurFromAperture gebraucht (Task-Brief: "private LUT-Lookup").
	private static float LookupBlurBase( float fStop )
	{
		if ( fStop <= 1.2 )
			return 1.00;
		if ( fStop <= 1.4 )
			return LerpBlurSegment( fStop, 1.2, 1.00, 1.4, 0.90 );
		if ( fStop <= 2.0 )
			return LerpBlurSegment( fStop, 1.4, 0.90, 2.0, 0.75 );
		if ( fStop <= 2.8 )
			return LerpBlurSegment( fStop, 2.0, 0.75, 2.8, 0.60 );
		if ( fStop <= 4.0 )
			return LerpBlurSegment( fStop, 2.8, 0.60, 4.0, 0.45 );
		if ( fStop <= 5.6 )
			return LerpBlurSegment( fStop, 4.0, 0.45, 5.6, 0.32 );
		if ( fStop <= 8.0 )
			return LerpBlurSegment( fStop, 5.6, 0.32, 8.0, 0.20 );
		if ( fStop <= 11.0 )
			return LerpBlurSegment( fStop, 8.0, 0.20, 11.0, 0.12 );
		if ( fStop <= 16.0 )
			return LerpBlurSegment( fStop, 11.0, 0.12, 16.0, 0.06 );
		if ( fStop <= 22.0 )
			return LerpBlurSegment( fStop, 16.0, 0.06, 22.0, 0.00 );

		return 0.00;
	}

	// Segment-Interpolationshelfer fuer LookupBlurBase: normalisiert fStop auf [0,1] zwischen den beiden
	// Stuetzpunkten (Math.InverseLerp(a,b,value) -- EnMath.c:622) und interpoliert linear auf blurBase
	// (Math.Lerp(a,b,time) -- EnMath.c:608, kein eigenes Clamp). Kein zusaetzlicher Clamp noetig -- wird
	// ausschliesslich von der If-Kette in LookupBlurBase mit fStop bereits innerhalb [x0,x1] aufgerufen.
	private static float LerpBlurSegment( float fStop, float x0, float y0, float x1, float y1 )
	{
		float t = Math.InverseLerp( x0, x1, fStop );
		float blurBase = Math.Lerp( y0, y1, t );
		return blurBase;
	}
}
