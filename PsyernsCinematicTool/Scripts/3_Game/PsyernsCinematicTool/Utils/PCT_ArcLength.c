// Task P3-1 -- Arc-Length-Zeit-Remapping, zeichengetreu nach Docs/Plan_B4_Kamera_Engine.md Section 4.4
// (Pseudocode "Arc-Length-LUT bauen" / "Laufzeit-Lookup (RemapToSplineParam)") und Section 4.3
// (Zeitparametrisierungs-Konsequenz von Math3D.Curve: EIN Parameter 0..1 UEBER DIE GESAMTE Punktmenge --
// EnMath3D.c:471 "proto static native vector Curve(ECurveType type, float param, notnull array<vector>
// points)"). Docs/Plan_B7_Kamerafahrten_Bewegungssteuerung.md Section 1.3-Tabelle bestaetigt: "Section 4.4
// Arc-Length-Zeit-Remapping (LUT-Aufbau, Laufzeit-Lookup) -- der Kamerapfad-Evaluator nutzt es 1:1". B7
// uebernimmt diesen Mechanismus unveraendert; NUR der Kurven-Evaluator wechselt ab Phase 4 pro Segment
// (B7 Section 4 "Pro-Segment-Lokalitaet") auf PCT_Math.CatmullRomSegment. Diese Datei implementiert daher
// bewusst die B4-Fassung (globale Math3D.Curve-Parametrisierung ueber die gesamte Punktmenge, via
// PCT_Math.SampleCatmullRom) -- die per-Segment-lokale Ergaenzung (B7 Section 4) ist NICHT Teil des
// Task-P3-1-Scopes ("Zu implementieren" listet nur B4 Section 4.4, keine B7-Section-4-Pfadtypen).
//
// Ablage: Utils/ (Berechnungshelfer ohne JSON-Persistenz -- kein Datenmodell-Member ohne Praefix, deshalb
// NICHT unter Data/, analog zu PCT_Math.c).
//
// Aufbau (BuildLut) ist EREIGNISGETRIEBEN (beim Laden/Aendern der Punktmenge), NICHT pro Frame (Plan_B4
// Section 4.4 Kommentar "einmalig beim Laden/Aendern der Sequenz, nicht pro Frame -- Section 9"). Der
// Laufzeit-Lookup (RemapToSplineParam) macht dagegen NULL Allokationen (Task-P3-1-Brief "keine
// Allokationen im Laufzeit-Lookup") -- er liest ausschliesslich aus den vorab befuellten Arrays.
//
// Segfault-Regel (CLAUDE.md "Complex expressions in array index assignments -> Segfault. Always use
// intermediate variable"): der LUT-Aufbau unten verwendet ausschliesslich array<T>.Insert(value)
// (Anhaengen), NIE eine Index-Zuweisung array[i] = komplexerAusdruck -- die Regel kann so gar nicht erst
// greifen. Kumulierte Zwischenwerte (kumulativ/delta/segDist/uOffset/u) liegen zusaetzlich bereits in
// eigenen Zwischenvariablen, bevor sie eingefuegt bzw. weiterverwendet werden.
//
// Arc-Length-Plausibilitaetsrechnung (Selbstpruefungs-Vorgabe des Task-Briefs, "3 kollineare Punkte
// gleichen Abstands -> LUT nahezu linear, begruenden"): fuer die STANDARD-Catmull-Rom-Formel
// P(u) = 0.5*(2*P1 + (-P0+P2)*u + (2P0-5P1+4P2-P3)*u^2 + (-P0+3P1-3P2+P3)*u^3) (dieselbe Form wie die
// Clean-Room-Referenz PCT_Math.CatmullRomSegment, Plan_B7 Section 4) gilt fuer vier kollineare, gleich
// beabstandete Kontrollpunkte P0=0, P1=1, P2=2, P3=3 (Skalartest, 1D reicht wegen Kollinearitaet):
// quadratischer Koeffizient (2*0-5*1+4*2-3) = (0-5+8-3) = 0, kubischer Koeffizient (-0+3*1-3*2+3) =
// (3-6+3) = 0 -- BEIDE hoeheren Terme verschwinden exakt, uebrig bleibt P(u) = 0.5*(2*1+2u) = 1+u, also
// EXAKT linear in u mit konstanter Ableitung dP/du = 1. Die Bogenlaenge waechst damit exakt proportional
// zu u, die LUT (kumulierte Distanz vs. u) ist fuer diesen Fall nicht nur "nahezu", sondern (bei exakter
// Arithmetik) GENAU linear -- "nahezu" im Sinn des Task-Briefs deckt die numerische Sampling-/Float-
// Rundung sowie die fuer die native Engine-Funktion (Math3D.Curve) formal ungeklaerte Randbedingung ab
// (Plan_B4 Section 4.4 OFFEN-V4: Endpunktverhalten von Math3D.Curve bei CatmullRom nicht verifiziert) --
// diese Herleitung nimmt Standard-Catmull-Rom-Semantik an, was plausibel, aber nicht Laufzeit-verifiziert
// ist (V4-Testschritt: Print-Vergleich).
class PCT_ArcLengthSegment
{
	ref array<float> distances;	// kumulierte Bogenlaenge je Sample, monoton steigend, distances[0] = 0
	ref array<float> params;		// zugehoeriger GLOBALER Spline-Parameter u je Sample (index-uniform, Section 4.3)
	float totalLength;				// segmentLaenge[k] (Plan_B4 Section 4.4) -- Gesamtlaenge dieses Segments

	void PCT_ArcLengthSegment()
	{
		distances = new array<float>();
		params = new array<float>();
		totalLength = 0.0;
	}
}

class PCT_ArcLength
{
	// Plan_B4 Section 4.4 Pseudocode: "S = 64  // Samples pro Segment"
	static const int PCT_ARC_LENGTH_SAMPLES = 64;

	// Plan_B4 Section 4.4 "Arc-Length-LUT bauen": fuer JEDES Segment k in [0..n-2] werden
	// PCT_ARC_LENGTH_SAMPLES Zwischenpunkte entlang der globalen Catmull-Rom-Kurve (PCT_Math.
	// SampleCatmullRom -> Math3D.Curve(ECurveType.CatmullRom, u, positions)) abgetastet und ihre
	// kumulierte Luftlinien-Distanz (|p - pPrev|) als Lookup-Tabelle (Distanz -> Spline-Parameter u)
	// gespeichert. Randfaelle: < 2 Punkte -> kein Segment moeglich, leere LUT (kein Crash).
	static array<ref PCT_ArcLengthSegment> BuildLut( array<vector> positions )
	{
		array<ref PCT_ArcLengthSegment> lut = new array<ref PCT_ArcLengthSegment>();
		if ( !positions )
			return lut;

		int n = positions.Count();
		if ( n < 2 )
			return lut;

		float denom = n - 1;				// Zwischenvariable -- "n - 1" nur einmal berechnet
		int segmentCount = n - 1;
		int k;
		for ( k = 0; k < segmentCount; k++ )
		{
			PCT_ArcLengthSegment seg = new PCT_ArcLengthSegment();

			float uStart = k / denom;					// "u = k/(n-1)"
			vector pPrev = PCT_Math.SampleCatmullRom( positions, uStart );

			// "lut[k].Insert(distanz=0, u=k/(n-1))" -- Startpunkt des Segments, Distanz 0
			seg.distances.Insert( 0.0 );
			seg.params.Insert( uStart );

			float kumulativ = 0.0;
			int i;
			for ( i = 1; i <= PCT_ARC_LENGTH_SAMPLES; i++ )
			{
				float sampleFrac = i;
				sampleFrac = sampleFrac / PCT_ARC_LENGTH_SAMPLES;		// "i/S"
				float uOffset = k + sampleFrac;						// "(k + i/S)"
				float u = uOffset / denom;								// "/ (n-1)" -- globaler Spline-Parameter
				vector p = PCT_Math.SampleCatmullRom( positions, u );
				vector delta = p - pPrev;								// Zwischenvariable (Segfault-Regel)
				float segDist = delta.Length();
				kumulativ = kumulativ + segDist;
				seg.distances.Insert( kumulativ );
				seg.params.Insert( u );
				pPrev = p;
			}
			seg.totalLength = kumulativ;			// "segmentLaenge[k] = kumulativ"
			lut.Insert( seg );
		}

		return lut;
	}

	// Plan_B4 Section 4.4 "Laufzeit-Lookup (RemapToSplineParam)": normalisierte Bogenlaenge (tEased,
	// bereits ge-easetes Segment-t) -> globaler Spline-Parameter u, lineare Interpolation zwischen den
	// beiden LUT-Stuetzpunkten, die sSoll einschliessen. Vorwaerts-Scan (die LUT ist monoton in
	// distances, Plan_B4 Section 4.4 "Vorwaertszeiger, da monoton") -- KEINE Allokation, liest nur.
	static float RemapToSplineParam( PCT_ArcLengthSegment seg, float tEased )
	{
		if ( !seg )
			return 0.0;

		int sampleCount = seg.distances.Count();
		if ( sampleCount < 2 )
			return 0.0;

		float clampedT = Math.Clamp( tEased, 0.0, 1.0 );
		float sSoll = clampedT * seg.totalLength;			// "sSoll = tEased * segmentLaenge[k]"

		int lastIndex = sampleCount - 2;
		int j = 0;
		while ( j < lastIndex )
		{
			float nextDist = seg.distances.Get( j + 1 );
			if ( sSoll <= nextDist )
				break;
			j++;
		}

		float distA = seg.distances.Get( j );
		float distB = seg.distances.Get( j + 1 );
		float span = distB - distA;
		float f = 0.0;
		if ( span > 0.0 )
			f = ( sSoll - distA ) / span;

		float uA = seg.params.Get( j );
		float uB = seg.params.Get( j + 1 );
		float u = Math.Lerp( uA, uB, f );
		return u;
	}
}
