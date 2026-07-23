// Task P2-5 (Docs/Plan_B8_Objektivsystem_LensKits.md Section 5.1) -- Lens-Preset -> Rig anwenden.
// Aufloesung der IDs (Kit -> Linsen-ids -> PCT_LensPreset-Objekte) macht der Aufrufer (PCT_Storage haelt
// die Familien, siehe PCT_Storage.LoadLensPresets/LoadLensKits/GetLensPresetById/GetLensKits, Task P2-5).
// Statische Utility-Klasse, COT-frei (3_Game/Utils, wie PCT_Math.c) -- nur Engine-`Math`, keine Widgets/
// UI-Aufrufe, kein g_Game-Zugriff. Code zeichengetreu aus dem Plan uebernommen.
class PCT_LensLibrary
{
	static void ApplyLensToRig(PCT_LensPreset lens, PCT_CameraRig rig)
	{
		if (!lens || !rig)
			return;

		rig.lensPresetId = lens.id;
		rig.focalLengthMm = lens.focalLengthMm;
		rig.minFocusDistanceM = lens.minFocusDistanceM;
		rig.anamorphicFactor = lens.anamorphicFactor;
		rig.aperture = ClampApertureToLens(rig.aperture, lens);
	}

	static float ClampApertureToLens(float aperture, PCT_LensPreset lens)
	{
		if (!lens)
			return aperture;

		float minStop = lens.maxAperture;		// groesste Oeffnung = kleinste Zahl
		float maxStop = ResolveMinAperture(lens);
		return Math.Clamp(aperture, minStop, maxStop);
	}

	// Task P2-5 (Review-Fix): der "unbekannt -> 22"-Fallback (Plan_B8 Section 5.1-Kommentar
	// "aperture 0.7..32") wird sowohl von ClampApertureToLens oben ALS AUCH vom Lens-Kit-UI-Panel
	// (PCT_CinematicForm, dynamische Blenden-Slider-Grenzen/Info-Zeile, Task 4) gebraucht -- ausgelagert
	// statt zweimal inline dupliziert, Verhalten von ClampApertureToLens dadurch unveraendert (reiner
	// Delegations-Refactor, keine neue Semantik).
	static float ResolveMinAperture(PCT_LensPreset lens)
	{
		if (!lens)
			return 22.0;

		if (lens.minAperture <= 0.0)
			return 22.0;

		return lens.minAperture;
	}

	static bool IsPrime(PCT_LensPreset lens)
	{
		if (!lens)
			return true;
		return lens.focalLengthMaxMm <= 0.0;
	}
}
