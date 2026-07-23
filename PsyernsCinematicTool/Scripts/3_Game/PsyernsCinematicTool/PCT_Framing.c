// Task P2-4 -- Auto-Framing-Engine (Docs/Plan_B4_Kamera_Engine.md Section 6, "Auto-Framing (Framing-Preset
// -> Kameraposition)"). Reine Berechnung + einmaliges Ergebnis (B4 Section 6 Entwurfsentscheidung: "Auto-
// Framing ist eine reine Berechnung + einmaliges Setzen (kein per-Frame-Constraint-Solver)") -- diese Datei
// ist COT-frei (keine COT-Klassen/-Aufrufe) und layer-sauber (3_Game: keine 4_World/5_Mission-Typen). Die
// Zielauswahl (Raycast, PlayerBase-Erkennung) liegt bewusst NICHT hier, sondern in
// PCT_CinematicModule.ResolveAutoFramingTarget (5_Mission) -- diese Klasse nimmt nur fertige
// vector/float-Eingaben entgegen (Fusspunkt + Motivhoehe), siehe Task-Brief "Eingabe (Zielpunkt +
// Motivhoehe, FramingPreset, AnglePreset, Rig)".
//
// === API-Fundstellen (verifiziert) ===
//   vector.Normalized(): vector -- scripts - 1.29\1_Core\DayZ\proto\EnConvert.c:156.
//   vector.VectorToAngles(): vector -- EnConvert.c:340-353 ("Converts vector to spherical coordinates...
//     return <yaw,pitch,roll in degrees>", roll immer 0; Beispiel v2=<1,1,1> -> pitch=+35.2644 fuer
//     positive Y-Komponente -- WICHTIG: positiver Y-Anteil der Blickrichtung => POSITIVER Pitch, d. h.
//     "nach oben schauen" = positiv, "nach unten schauen" = negativ. Deckt sich exakt mit dem in Plan_B3
//     Section 2.10 dokumentierten Vorzeichen "pitchDegrees: negativ = nach unten (High Angle)").
//   Math.Sqrt(float)/Math.Tan(float)/Math.Clamp(float,float,float) -- EnMath.c:305/355/540 (bereits in
//     PCT_Math.c/PCT_Validation.c dieses Mods verwendet).
//   Verifiziertes Realwelt-Vorbild fuer die Normalized()+VectorToAngles()-Kombination:
//     scripts - 1.29\5_Mission\DayZ\GUI\CameraTools\CameraToolsMenu.c:707
//     "m_SelectedActor.SetRotation( vector.Direction( from,to ).Normalized().VectorToAngles() );" -- diese
//     Datei baut den Differenzvektor manuell (aimPoint - cameraPos) statt ueber vector.Direction(from,to),
//     weil beide Punkte ohnehin bereits als vector-Werte vorliegen (keine Signaturpruefung von
//     vector.Direction noetig, Subtraktion ist elementare vector-Arithmetik, bereits in
//     PCT_CinematicModule/PCT_Math dieses Mods genutzt).
//
// === Pitch-Modell (Entwurfsentscheidung, B4 Section 6 laesst dies offen -- Task-Brief erlaubt
// "dokumentierte Naeherung") ===
// B4 Section 6 Schritt 7 sagt nur "Ausrichten: cam.LookAt(zielAnker)" -- das ergibt eine automatische,
// rein geometrische Blickrichtung aus Kamera-/Zielposition. `PCT_AnglePreset.pitchDegrees` (Plan_B3
// Section 2.10) ist trotzdem ein eigenes, gespeichertes Feld. Entscheidung: der finale Pitch ist die
// GEOMETRISCHE Look-at-Neigung (aus Kamerahoehe/Zielhoehe/Distanz, exakt was LookAt ergeben wuerde) PLUS
// `pitchDegrees` als zusaetzlicher ARTISTISCHER Korrekturwert obendrauf. Fuer die meisten Builtin-
// AnglePresets (siehe PCT_Storage.CreateBuiltinAnglePresets) ist dieser Korrekturwert 0 -- Hoehe/Distanz
// allein erzeugen bereits den gewuenschten High-/Low-Angle-Effekt. `angle_ground` und `angle_overhead`
// trugen vor dem Fix-Wave-Review zusaetzlich einen Ad-hoc-Korrekturwert (+5 Grad / -55 Grad); der war jedoch
// gegen die fehlerhafte Distanz-/Aim-Geometrie (siehe oben, "Fix-Wave") kalibriert und daher mit dem
// Geometrie-Fix hinfaellig. Beide stehen jetzt auf 0.0 (Kalibrierung nach Geometrie-Fix ausstehend,
// In-Game -- siehe PCT_Storage.CreateBuiltinAnglePresets-Kommentar). Verworfene
// Alternative: pitchDegrees direkt als finalen Pitch setzen (ignoriert Kamerahoehe/-distanz komplett) --
// wuerde bedeuten, dass zwei Motive unterschiedlicher Groesse/Entfernung bei gleichem AnglePreset exakt
// denselben Pitch bekommen, obwohl die tatsaechliche Blickgeometrie voellig verschieden ist.
//
// === Fix-Wave (Review Task P2-4, siehe Task-P2-4-Report.md "Fix-Wave"-Abschnitt) -- ersetzt die vorherige
// Distanz-/Headroom-Berechnung, Design-Entscheidung "Visible-Height-Reinterpretation" (verbindlich, vom
// Orchestrator) ===
// Das Review hat zwei Fehler nachgewiesen:
//   (1) Distanzformel: `subjectScreenHeight` wurde direkt auf die VOLLE Motivhoehe (`subjectHeightM`)
//       bezogen. Fuer enge Einstellungen (MCU..ECU) ist aber nur ein Teil des Motivs (Kopf bis
//       Schnittlinie) im Bild -- das zwang `subjectScreenHeight` > 1.0 (ausserhalb des in Plan_B3
//       Section 2.10 dokumentierten Bereichs 0..1) und ergab unplausible Distanzen (z. B. CU/85mm vorher
//       ~7.8 m statt einer plausiblen Portraet-Distanz).
//   (2) Aim-/Headroom-Geometrie: der Blickpunkt war auf die Koerpermitte verankert, die Headroom-Korrektur
//       nur eine analytische Naeherung -- dadurch ging das B6-Phase-2-DoD-3-Kriterium ("MS -> Hueftlinie
//       unteres Bilddrittel") nur "tendenziell", nicht exakt auf (Task-P2-4-Report Section 8).
// Loesung: `subjectScreenHeight` bleibt im dokumentierten Bereich 0..1, bezieht sich ab jetzt aber auf die
// SICHTBARE Motivspanne (Kopfoberkante bis Schnittlinie), nicht die volle Koerpergroesse. Dafuer traegt
// `PCT_FramingPreset` additiv das neue Feld `bodyCutRatio` (Anteil der Koerperhoehe UNTERHALB der
// Schnittlinie, 0 = Ganzkoerper -- siehe PCT_Presets.c).
//
// Neue Distanzformel (Schritt 4):
//   visibleHeightM = subjectHeightM * (1.0 - bodyCutRatio)    -- sichtbare Motivspanne in Metern
//   frameHeightM   = visibleHeightM / subjectScreenHeight     -- volle Bildhoehe in Metern in Motivdistanz
//   dist           = (frameHeightM / 2.0) / tanHalfFov
//
// Neue Aim-/Headroom-Geometrie (Schritt 7, ersetzt die alte "Koerpermitte + analytische Naeherung"-Loesung):
// Referenzpunkt ist jetzt die KOPFOBERKANTE (`headTopY = footY + subjectHeightM`), nicht die Koerpermitte --
// `headroom` ist per Definition (Plan_A1/B3) der Bildanteil ZWISCHEN Bildoberkante und Kopfoberkante; das
// laesst sich nur exakt erfuellen, wenn der Referenzpunkt selbst die Kopfoberkante ist. Der Blickpunkt
// (= Bildmitte, da LookAt) wird so gesetzt, dass die Kopfoberkante exakt bei Bildanteil `headroom` von oben
// liegt:
//   aimY = headTopY + (headroom * frameHeightM) - (frameHeightM / 2.0)
// Algebraischer Nachweis: fractionTop(y) = 0.5 - (y - aimY) / frameHeightM (aimY projiziert per LookAt exakt
// auf Bildmitte = Fraktion 0.5). Eingesetzt: fractionTop(headTopY) = 0.5 - (headTopY - aimY) / frameHeightM
// = 0.5 - (0.5 - headroom) = headroom -- EXAKT, keine Naeherung mehr (im Unterschied zur vorherigen,
// ausdruecklich als "vereinfachte geschlossene Form" dokumentierten Loesung). Als Nebenprodukt gilt fuer die
// Schnittlinie (cutLineY = footY + subjectHeightM * bodyCutRatio) automatisch
// fractionTop(cutLineY) = headroom + subjectScreenHeight (da visibleHeightM / frameHeightM =
// subjectScreenHeight per Konstruktion) -- Grundlage der Builtin-Preset-Kalibrierung in PCT_Storage.c
// (CreateBuiltinFramingPresets-Kommentar).
//
// `eyeHeightM`/PCT_EYE_HEIGHT_RATIO bleibt unveraendert AUSSCHLIESSLICH fuer die Kamerahoehe im heightMode
// "eyeLevel" reserviert (HeightModeBaseHeight) -- von der Aim-Punkt-Aenderung nicht betroffen.
class PCT_FramingResult
{
	vector position;
	vector orientation;			// <Yaw,Pitch,Roll> Grad, wie Object.GetOrientation/SetOrientation
	float distanceM;
	bool hasRecommendedFocalLength;
	float recommendedFocalLengthMm;

	void PCT_FramingResult()
	{
		position = vector.Zero;
		orientation = vector.Zero;
		distanceM = 0.0;
		hasRecommendedFocalLength = false;
		recommendedFocalLengthMm = 0.0;
	}
}

class PCT_Framing
{
	// Anthropometrische Naeherungswerte fuer HeightModeBaseHeight (kalibrierbare Startwerte, siehe
	// Task-P2-4-Report fuer die Werte-Tabelle mit Quellenangabe je Zeile).
	static const float PCT_EYE_HEIGHT_RATIO = 0.933;		// hergeleitet: PCT_ActorMark-Defaults 1.68/1.80 (Plan_B3 Section 2.8)
	static const float PCT_SHOULDER_HEIGHT_RATIO = 0.82;	// hergeleitet: Standard-Anthropometrie
	static const float PCT_HIP_HEIGHT_RATIO = 0.53;		// hergeleitet: Standard-Anthropometrie
	static const float PCT_KNEE_HEIGHT_RATIO = 0.285;		// hergeleitet: Standard-Anthropometrie
	static const float PCT_GROUND_HEIGHT_M = 0.25;			// Plan_A1 Section 4 "Ground Level: etwa 10-40cm", Mittelwert
	static const float PCT_HORIZONTAL_EPSILON = 0.01;		// Meter; Schutz gegen Division-durch-0 bei Kamera direkt ueber dem Ziel
	static const float PCT_PITCH_CLAMP_DEG = 89.0;			// deckt sich mit PCT_Validation.ValidateKeyframe (orientation.pitch -89..89)

	// Kernberechnung (Task-Brief Punkt 2). Eingaben: Fusspunkt des Motivs (Weltkoordinaten), Motivhoehe in
	// Metern, Framing-/Angle-Preset, aktives Rig (liefert fovV) und die AKTUELLE Kameraposition (liefert die
	// horizontale Anflugrichtung -- B4 Section 6 Schritt 6: "Standard: aktuelle Kamerarichtung invertiert").
	// Keine Allokation ausser dem einen Rueckgabeobjekt (Task-Brief: Auto-Framing ist ereignisgetrieben,
	// nicht Teil des Per-Frame-Pfads -- eine Allokation pro Knopfdruck ist unkritisch).
	static PCT_FramingResult ComputeFraming(vector targetFootPos, float subjectHeightM, PCT_FramingPreset framing, PCT_AnglePreset angle, PCT_CameraRig rig, vector currentCameraPos)
	{
		PCT_FramingResult result = new PCT_FramingResult();

		if (!framing || !angle || !rig)
			return result;

		if (subjectHeightM <= 0.0)
			return result;

		float subjectScreenHeight = framing.subjectScreenHeight;
		if (subjectScreenHeight <= 0.0)
			return result;

		// Schritt 4 (B4 Section 6, Fix-Wave "Visible-Height-Reinterpretation" -- siehe Datei-Kopfkommentar):
		// Distanz aus SICHTBARER Motivspanne (Kopfoberkante..Schnittlinie)/Bildanteil/vertikalem FOV.
		float fovVertical = rig.GetVerticalFovRad();
		float halfFov = fovVertical * 0.5;
		float tanHalfFov = Math.Tan(halfFov);
		if (tanHalfFov <= 0.0)
			return result;

		float visibleHeightM = subjectHeightM * (1.0 - framing.bodyCutRatio);
		if (visibleHeightM <= 0.0)
			return result;

		float frameHeightM = visibleHeightM / subjectScreenHeight;
		float distance = (frameHeightM / 2.0) / tanHalfFov;
		result.distanceM = distance;

		// Schritt 5 (B4 Section 6): Kamerahoehe aus AnglePreset.heightMode + heightOffsetM.
		float cameraHeightAgl = HeightModeBaseHeight(angle.heightMode, subjectHeightM) + angle.heightOffsetM;
		float cameraY = targetFootPos[1] + cameraHeightAgl;

		// Schritt 6 (B4 Section 6): horizontale Anflugrichtung = aktuelle Kamerarichtung invertiert (vom
		// Ziel zur aktuellen Kameraposition), auf die XZ-Ebene projiziert und normalisiert.
		float dirX = currentCameraPos[0] - targetFootPos[0];
		float dirZ = currentCameraPos[2] - targetFootPos[2];
		float horizontalLenSq = dirX * dirX + dirZ * dirZ;
		float horizontalLen = Math.Sqrt(horizontalLenSq);

		if (horizontalLen < PCT_HORIZONTAL_EPSILON)
		{
			// Kamera steht (nahezu) exakt ueber/unter dem Ziel -- keine sinnvolle horizontale Richtung
			// ableitbar, Fallback auf Nord (+Z), analog anderer Fallback-Defaults in diesem Mod.
			dirX = 0.0;
			dirZ = 1.0;
			horizontalLen = 1.0;
		}

		float normX = dirX / horizontalLen;
		float normZ = dirZ / horizontalLen;

		float cameraPosX = targetFootPos[0] + normX * distance;
		float cameraPosZ = targetFootPos[2] + normZ * distance;

		vector cameraPos;
		cameraPos[0] = cameraPosX;
		cameraPos[1] = cameraY;
		cameraPos[2] = cameraPosZ;
		result.position = cameraPos;

		// Schritt 7 (B4 Section 6, Fix-Wave -- siehe Datei-Kopfkommentar "Fix-Wave"): Blickpunkt so gesetzt,
		// dass die Kopfoberkante exakt bei Bildanteil `headroom` von oben liegt (algebraischer Nachweis im
		// Datei-Kopfkommentar).
		float headTopY = targetFootPos[1] + subjectHeightM;
		float aimY = headTopY + (framing.headroom * frameHeightM) - (frameHeightM / 2.0);

		vector aimPoint;
		aimPoint[0] = targetFootPos[0];
		aimPoint[1] = aimY;
		aimPoint[2] = targetFootPos[2];

		// Ausrichtung: Differenzvektor Kamera->Blickpunkt, normalisiert, ueber VectorToAngles() in
		// <Yaw,Pitch,0> Grad umgerechnet (siehe Datei-Kopfkommentar "API-Fundstellen"). Pitch-Modell und
		// Roll siehe Datei-Kopfkommentar "Pitch-Modell".
		float lookDx = aimPoint[0] - cameraPos[0];
		float lookDy = aimPoint[1] - cameraPos[1];
		float lookDz = aimPoint[2] - cameraPos[2];

		vector lookDir;
		lookDir[0] = lookDx;
		lookDir[1] = lookDy;
		lookDir[2] = lookDz;

		vector normalizedLookDir = lookDir.Normalized();
		vector lookAngles = normalizedLookDir.VectorToAngles();

		float finalYaw = lookAngles[0];
		float rawPitch = lookAngles[1] + angle.pitchDegrees;
		float finalPitch = Math.Clamp(rawPitch, -PCT_PITCH_CLAMP_DEG, PCT_PITCH_CLAMP_DEG);
		float finalRoll = angle.rollDegrees;

		vector orientation;
		orientation[0] = finalYaw;
		orientation[1] = finalPitch;
		orientation[2] = finalRoll;
		result.orientation = orientation;

		// Empfohlene Brennweite (Task-Brief-Regel: "Mitte des Preset-Bereichs, nur wenn aktuelle
		// Brennweite ausserhalb" -- B3/B4 selbst geben KEINE Regel vor, Plan_B3 Section 2.10-Kommentar zu
		// focalLengthMinMm/MaxMm: "Startwerte, keine Regel"; die Task-Brief-Vorgabe ist daher die einzige
		// hier umgesetzte Regel, nicht aus B4 uebernommen).
		float currentFocalMm = rig.focalLengthMm;
		if (currentFocalMm < framing.focalLengthMinMm || currentFocalMm > framing.focalLengthMaxMm)
		{
			float midFocalMm = (framing.focalLengthMinMm + framing.focalLengthMaxMm) * 0.5;
			result.hasRecommendedFocalLength = true;
			result.recommendedFocalLengthMm = midFocalMm;
		}

		return result;
	}

	// heightMode -> Basis-Kamerahoehe ueber dem Fusspunkt (vor heightOffsetM). Siehe Klassenkommentar oben
	// fuer die Quelle jedes Verhaeltnisses.
	private static float HeightModeBaseHeight(string heightMode, float subjectHeightM)
	{
		if (heightMode == "eyeLevel")
			return subjectHeightM * PCT_EYE_HEIGHT_RATIO;

		if (heightMode == "shoulder")
			return subjectHeightM * PCT_SHOULDER_HEIGHT_RATIO;

		if (heightMode == "hip")
			return subjectHeightM * PCT_HIP_HEIGHT_RATIO;

		if (heightMode == "knee")
			return subjectHeightM * PCT_KNEE_HEIGHT_RATIO;

		if (heightMode == "ground")
			return PCT_GROUND_HEIGHT_M;

		if (heightMode == "absolute")
			return 0.0;			// heightOffsetM selbst ist bei "absolute" die volle Hoehe (Plan_B3 Section 2.10)

		CF_Log.Warn("PCT: PCT_Framing -- unbekannter AnglePreset.heightMode '" + heightMode + "', Fallback eyeLevel.");
		return subjectHeightM * PCT_EYE_HEIGHT_RATIO;
	}
}
