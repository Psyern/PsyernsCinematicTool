// Task P1-5 -- Code zeichengetreu aus Docs/Plan_B4_Kamera_Engine.md Section 2.2 uebernommen (JSON-Datenklasse,
// Member OHNE Praefix = JSON-Key-Treue, Schema siehe Docs/Plan_B3_Datenmodell_Persistenz.md Section 2.3; kein
// eigenes schemaVersion -- eingebettete Klasse, Version liegt an der Wurzeldatei, Plan_B3 Section 5).
// Dokumentierte Abweichungen vom Plan-Code (Task-P1-5-Brief, sonst 1:1):
//   1. GetVerticalFovRad() clampt sein Ergebnis zusaetzlich auf [PCT_Constants.PCT_FOV_MIN_RAD,
//      PCT_FOV_MAX_RAD] (O1-Konsequenz #1, Docs/Phase1_O1_SetFOV_Protokoll.md -- Camera.SetFOV clampt selbst
//      NICHT, das Rig soll daher nie einen undarstellbaren Wert liefern).
//   2. Dateiname PCT_CameraRig.c folgt dem Plan_B6-Phase-1-Deliverable (Roadmap-Tabelle) statt einem
//      abweichenden Namen aus frueheren Entwurfsstaenden von Plan_B1 Section 3 -- siehe Task-P1-5-Report.
class PCT_CameraRig
{
	string cameraName = "CAM A";
	string sensorPresetId = "sensor_fullframe";	// Tabelle 2.3
	float sensorWidthMm = 36.0;
	float sensorHeightMm = 24.0;
	string lensPresetId = "";
	float focalLengthMm = 50.0;
	float aperture = 2.8;				// f-Stop, z. B. 2.0
	float minFocusDistanceM = 0.45;		// Nahgrenze des Objektivs (fuer Paragraph 7 Warnung)
	float anamorphicFactor = 1.0;		// 1.0 = sphaerisch; nur Anzeige/Maske, siehe Hinweis unten
	string aspectMask = "16:9";			// Overlay-Maske (Plan_B5)
	float nearPlaneM = 0.05;			// Camera.SetNearPlane, min. 0.01 m

	// Abweichung 1 (siehe Datei-Kopfkommentar): zusaetzlicher Clamp auf PCT_Constants.PCT_FOV_MIN_RAD/MAX_RAD
	// (O1-verifiziert, Docs/Phase1_O1_SetFOV_Protokoll.md Section "Konsequenzen" Punkt 1) -- Math.Clamp,
	// EnMath.c:540. Deckt die gesamte Brennweiten-Preset-Reihe 14-200 mm ab (Plan_B4 Section 2.4/2.5), das
	// Rig liefert dadurch nie einen Wert, den Camera.SetFOV zwar unclamped annehmen wuerde, der aber ausserhalb
	// des verifizierten/beabsichtigten Darstellungsbereichs liegt.
	// Task P1-6 -- Abweichung 3 (Guard vor der Division): Plan_B4 Section 2.2 hat hier keinen Guard, der Code
	// dividiert direkt durch focalLengthMm. Plan_B6 Section 1 (Phase-1-Deliverable-Zeile "Kamera-Rig") nennt
	// dagegen die div-sichere Atan2-Form (fovVertical = 2 * Math.Atan2(sensorHeightMm, 2 * focalLengthMm),
	// Math.Atan2(y,x) -- EnMath.c:399 -- dividiert intern nicht) fuer exakt dieselbe Formel -- dokumentierter
	// Plan-Konflikt zwischen B4 und B6. Geloest zugunsten der bestehenden Atan-Form (keine ungeplante
	// Funktionsaenderung) plus explizitem Guard: focalLengthMm <= 0 waere Division durch 0/negativer
	// Nenner -- statt NaN/Inf oder einem Vorzeichenfehler wird der sichere obere Rig-Grenzwert
	// PCT_FOV_MAX_RAD zurueckgegeben (Task P1-7 Wording-Fix: das ist der weiteste Bildwinkel (14 mm) im
	// gueltigen Bereich, nicht die "engste" Brennweite -- 14 mm ist die KUERZESTE Brennweite der
	// Preset-Reihe und liefert dabei den GROESSTEN FOV, siehe PCT_Constants.PCT_FOV_MAX_RAD-Kommentar:
	// 1.4173 rad = 14 mm).
	float GetVerticalFovRad()
	{
		if (focalLengthMm <= 0.0)
			return PCT_Constants.PCT_FOV_MAX_RAD;

		float denom = 2.0 * focalLengthMm;
		float fovRad = 2.0 * Math.Atan(sensorHeightMm / denom);
		float clampedFovRad = Math.Clamp(fovRad, PCT_Constants.PCT_FOV_MIN_RAD, PCT_Constants.PCT_FOV_MAX_RAD);
		return clampedFovRad;
	}

	float GetVerticalFovDeg()
	{
		float fovRad = GetVerticalFovRad();
		return fovRad * Math.RAD2DEG;
	}

	// Task P2-3 (PCT_CinematicModule.CaptureCurrentShot/ApplyShot, Docs/Plan_B3_Datenmodell_Persistenz.md
	// Section 2.4 "cameraRig = tiefe KOPIE des aktuellen m_CameraRig -- keine Referenz-Teilung!"): alle Member
	// sind Skalare/Strings (kein verschachteltes ref-Objekt in PCT_CameraRig) -- ein Feld-fuer-Feld-Kopieren ist
	// daher bereits eine vollstaendige, referenzfreie Tiefenkopie. CopyInto() kopiert IN eine bestehende
	// Ziel-Instanz (noetig fuer ApplyShot: m_CameraRig ist bereits von der aktiven Kamera referenziert, siehe
	// PCT_CinematicCamera.m_PCT_Rig -- die Referenz selbst darf nicht ersetzt werden, nur ihre Feldwerte).
	void CopyInto( PCT_CameraRig destination )
	{
		if ( !destination )
			return;

		destination.cameraName = cameraName;
		destination.sensorPresetId = sensorPresetId;
		destination.sensorWidthMm = sensorWidthMm;
		destination.sensorHeightMm = sensorHeightMm;
		destination.lensPresetId = lensPresetId;
		destination.focalLengthMm = focalLengthMm;
		destination.aperture = aperture;
		destination.minFocusDistanceM = minFocusDistanceM;
		destination.anamorphicFactor = anamorphicFactor;
		destination.aspectMask = aspectMask;
		destination.nearPlaneM = nearPlaneM;
	}

	// Fuer CaptureCurrentShot: liefert eine frische, unabhaengige PCT_CameraRig-Instanz (fuer PCT_Shot.cameraRig)
	// statt der geteilten Modul-Instanz m_CameraRig -- baut auf CopyInto() auf, keine doppelte Feldliste.
	PCT_CameraRig Clone()
	{
		PCT_CameraRig copy = new PCT_CameraRig();
		CopyInto( copy );
		return copy;
	}
}
