// Task P1-8 (Docs/Plan_B5_UI_UX.md Section 4 "Kompositions-Overlays"; Docs/Plan_B1_Architektur.md
// Section 3 Datei-Baum + Section 4 Rollentabelle): Kompositions-Overlays (Drittelraster,
// Bildzentrum/Fadenkreuz, Seitenverhaeltnis-Masken) als eigene Vollbild-Workspace-Widgets, UNABHAENGIG
// vom COT-Formular (pct_cinematic_form.layout/PCT_CinematicForm) -- das Overlay wird direkt auf den
// Workspace gelegt (g_Game.GetWorkspace().CreateWidgets, Vanilla-Muster CameraToolsMenu.c:141, siehe
// unten), NICHT als Kind des COT-Fensters, deshalb bleibt es auch bei geschlossenem PCT-Fenster
// sichtbar, solange die PCT-Kamera aktiv ist (Architektur-Vorgabe Task-Brief).
//
// Datei-/Klassen-/Layoutname: B1 Section 3 (Datei-Baum: "PCT_OverlayHud.c" / "pct_overlay.layout")
// UND B5 Section 9 (Layout-Datei-Tabelle: "pct_overlay.layout", "geladen von PCT_CinematicModule
// (Workspace)") stimmen fuer DIESE Datei/Klasse ueberein -- kein B1/B5-Namenskonflikt (Report
// dokumentiert das trotzdem explizit, wie vom Task-Brief gefordert). Einzige Quelle mit einem
// abweichenden Phasen-Label ist Plan_B6_Roadmap_Risiken.md Section 1 (fuehrt "PCT_OverlayHud"/
// "pct_overlay.layout" unter Phase 2, nicht Phase 1) -- siehe Report fuer die Einordnung dieses
// Konflikts.
//
// Lazy-Erzeugung (Architektur-Vorgabe Task-Brief): EnsureCreated() baut die Widgets beim ERSTEN Show()
// (typischerweise der erste Kamera-Enter, siehe PCT_CinematicModule.Client_Enter/Server_Enter-
// Offline-Zweig) und zerstoert sie danach nie wieder -- Leave/K4 verstecken nur (Hide(), kein
// Unlink()), Wiederverwendung beim naechsten Enter.
//
// Aufloesungsabhaengige Geometrie (Masken-Balken): NUR bei jedem Show() und bei jedem Masken-Wechsel
// neu berechnet (RecalculateMask()), bewusst OHNE Per-Frame-Update/OnUpdate-Hook (Architektur-Vorgabe
// Task-Brief: "kein Per-Frame-Update noetig"). Ein Aufloesungswechsel WAEHREND aktiver Kamera (z. B.
// Alt-Tab + Settings-Aenderung mitten in der Session) wird dadurch erst beim naechsten Show()
// (naechstes Enter) neu berechnet -- akzeptierter Trade-off, siehe Report.
//
// Drittelraster/Fadenkreuz sind reine RELATIVE Layout-Geometrie (Fraktionen 0.3333/center_ref direkt
// im .layout, siehe pct_overlay.layout) nach dem Vorbild von COTs eigenem, im Formular ungenutztem
// Referenz-Layout JM/COT/GUI/layouts/camera_form_ROT.layout ("Rule of Thirds"-Overlay: 4 PanelWidgets,
// Positionen 0.3333 von den jeweiligen Raendern, vexactsize/hexactsize 1 fuer die 1px-Linienstaerke) --
// dafuer ist AUSSER Show/Hide keine Laufzeit-Geometrieberechnung noetig, sie skalieren automatisch mit
// jeder Bildschirmaufloesung. NUR die Seitenverhaeltnis-Masken brauchen Laufzeitmathe (kein
// Engine-Aspect-API, Research_Vanilla_APIs.md Section 10), siehe RecalculateMask() unten.
class PCT_OverlayHud
{
	protected Widget m_Root;

	protected Widget m_ThirdH1;
	protected Widget m_ThirdH2;
	protected Widget m_ThirdV1;
	protected Widget m_ThirdV2;

	protected Widget m_CenterH;
	protected Widget m_CenterV;

	protected Widget m_MaskTop;
	protected Widget m_MaskBottom;
	protected Widget m_MaskLeft;
	protected Widget m_MaskRight;

	// Task P3-6: Pfad-Visualisierung (Plan_B6 Phase-3-Deliverable: Marker via GetScreenPosRelative;
	// CanvasWidget.DrawLine/Clear verifiziert EnWidgets.c:341-344 -- der von B7 V7 benannte Fallback-Weg,
	// da Shape-Rendering im Retail-Client unverifiziert ist).
	protected CanvasWidget m_PathCanvas;
	static const int PCT_PATH_VIZ_CAM_COLOR = 0xCCFFFFFF;
	static const int PCT_PATH_VIZ_LINE_COLOR = 0x66FFFFFF;
	static const int PCT_PATH_VIZ_LOOK_COLOR = 0xCCFF8800;

	protected bool m_ThirdsEnabled = true;
	protected bool m_CenterEnabled = true;
	protected string m_AspectMask = "16:9";

	// Layout-Pfad mit Forward-Slashes gemaess B5 Section 9 / Vanilla-Konvention (CameraToolsMenu.c:141)
	// -- das ist NICHT die config.cpp-Backslash-Regel aus CLAUDE.md, die gilt ausschliesslich fuer
	// config.cpp-Pfade (CfgSoundShaders/CfgVehicles etc.), nicht fuer Layout-Pfade in EnScript-Code.
	static const string LAYOUT_PATH = "PsyernsCinematicTool/GUI/layouts/pct_overlay.layout";

	// Ziel-Seitenverhaeltnis (Breite : Hoehe) je Masken-Bezeichner, 1:1 aus dem Task-Brief
	// ("2.39:1, 16:9, 4:3, 9:16, 1:1"). Rueckgabe 0.0 fuer "Aus" bzw. jeden unbekannten Bezeichner --
	// RecalculateMask() interpretiert das als "kein Maskenbalken, alle 4 Panels verstecken".
	protected float AspectRatioForMask( string mask )
	{
		if ( mask == "2.39:1" )
			return 2.39;
		if ( mask == "16:9" )
			return 16.0 / 9.0;
		if ( mask == "4:3" )
			return 4.0 / 3.0;
		if ( mask == "9:16" )
			return 9.0 / 16.0;
		if ( mask == "1:1" )
			return 1.0;

		return 0.0;
	}

	// Lazy-Erzeugung, siehe Klassenkommentar. false = Erzeugung fehlgeschlagen (Layout nicht ladbar
	// bzw. g_Game/Workspace noch nicht bereit) -- Aufrufer bricht dann ab, statt mit Nullpointer auf
	// den Kind-Widgets weiterzuarbeiten.
	protected bool EnsureCreated()
	{
		if ( m_Root )
			return true;

		if ( !g_Game )
			return false;

		m_Root = g_Game.GetWorkspace().CreateWidgets( LAYOUT_PATH );
		if ( !m_Root )
			return false;

		m_ThirdH1 = m_Root.FindAnyWidget( "pct_overlay_third_h1" );
		m_ThirdH2 = m_Root.FindAnyWidget( "pct_overlay_third_h2" );
		m_ThirdV1 = m_Root.FindAnyWidget( "pct_overlay_third_v1" );
		m_ThirdV2 = m_Root.FindAnyWidget( "pct_overlay_third_v2" );

		m_CenterH = m_Root.FindAnyWidget( "pct_overlay_center_h" );
		m_CenterV = m_Root.FindAnyWidget( "pct_overlay_center_v" );

		m_MaskTop = m_Root.FindAnyWidget( "pct_overlay_mask_top" );
		m_MaskBottom = m_Root.FindAnyWidget( "pct_overlay_mask_bottom" );
		m_MaskLeft = m_Root.FindAnyWidget( "pct_overlay_mask_left" );
		m_MaskRight = m_Root.FindAnyWidget( "pct_overlay_mask_right" );

		Class.CastTo( m_PathCanvas, m_Root.FindAnyWidget( "pct_overlay_path_canvas" ) );

		return true;
	}

	// Aufgerufen vom Modul bei jedem Kamera-Erhalt (Client_Enter, Offline-Zweig von Server_Enter --
	// Architektur-Vorgabe Task-Brief: "Modul ruft Show/Hide -- verdrahte in Client_Enter, Client_Leave,
	// K4-Zweig, Offline-Enter"). thirdsEnabled/centerEnabled/aspectMask kommen vom Modul (Zustands-
	// Besitzer); diese Methode uebernimmt den Zustand UND macht das Overlay sichtbar. Einzel-Toggles
	// danach ueber SetThirdsVisible/SetCenterVisible/SetAspectMask (Form-Checkboxen/SelectBox, "Auswahl
	// wirkt sofort").
	void Show( bool thirdsEnabled, bool centerEnabled, string aspectMask )
	{
		if ( !EnsureCreated() )
			return;

		m_ThirdsEnabled = thirdsEnabled;
		m_CenterEnabled = centerEnabled;
		m_AspectMask = aspectMask;

		m_Root.Show( true );

		ApplyThirdsVisibility();
		ApplyCenterVisibility();
		RecalculateMask();
	}

	// Leave/K4 (Architektur-Vorgabe Task-Brief): NUR verstecken, kein Unlink() -- Widgets bleiben fuer
	// den naechsten Enter erhalten (Wiederverwendung).
	void Hide()
	{
		if ( !m_Root )
			return;

		m_Root.Show( false );
	}

	// ===== Einzel-Toggles (Form-Checkboxen/SelectBox, Phase-1-DoD 5 "einzeln zu-/abschaltbar") =====
	// Kein Apply-Button -- jeder Aufruf schreibt sofort in den sichtbaren Widget-Zustand, analog zum
	// bestehenden Rig-Panel-Muster (PCT_CinematicForm.OnChange_FocalLength).

	void SetThirdsVisible( bool enabled )
	{
		m_ThirdsEnabled = enabled;
		ApplyThirdsVisibility();
	}

	void SetCenterVisible( bool enabled )
	{
		m_CenterEnabled = enabled;
		ApplyCenterVisibility();
	}

	// Masken-Wechsel: Geometrie wird HIER neu berechnet (Architektur-Vorgabe Task-Brief: "bei ...
	// Masken-Wechsel neu berechnen"), nicht nur bei Show().
	void SetAspectMask( string aspectMask )
	{
		m_AspectMask = aspectMask;
		RecalculateMask();
	}

	bool IsThirdsVisible()
	{
		return m_ThirdsEnabled;
	}

	bool IsCenterVisible()
	{
		return m_CenterEnabled;
	}

	string GetAspectMask()
	{
		return m_AspectMask;
	}

	protected void ApplyThirdsVisibility()
	{
		if ( !m_ThirdH1 )
			return;

		m_ThirdH1.Show( m_ThirdsEnabled );
		m_ThirdH2.Show( m_ThirdsEnabled );
		m_ThirdV1.Show( m_ThirdsEnabled );
		m_ThirdV2.Show( m_ThirdsEnabled );
	}

	protected void ApplyCenterVisibility()
	{
		if ( !m_CenterH )
			return;

		m_CenterH.Show( m_CenterEnabled );
		m_CenterV.Show( m_CenterEnabled );
	}

	// Masken-Mathe (Report Selbstpruefung 1; Docs/Plan_B5_UI_UX.md Section 4 "UpdateAspectMask"-
	// Codeblock, uebernommen -- SetScreenSize/SetScreenPos/GetScreenSize-Signaturen verifiziert in
	// scripts-1.29 1_Core/DayZ/proto/EnWidgets.c:142-143/154, siehe Klassenkommentar). "Aus" bzw. jeder
	// unbekannte Wert (AspectRatioForMask() liefert dann 0.0) versteckt alle 4 Balken.
	// ===== Task P3-6: Pfad-Visualisierung =====

	void ClearPathViz()
	{
		if ( m_PathCanvas )
			m_PathCanvas.Clear();
	}

	// Marker (X) je Kamerapunkt + Verbindungslinien in Punktreihenfolge; Blickziele als orange Marker.
	// Projektion via g_Game.GetScreenPosRelative (x/y 0..1, z = Distanz; z<=0 = hinter der Kamera,
	// Research_Vanilla_APIs Section 5 / Plan_B7 Section 5.2-Muster). Linienzug ist die Punktverbindung,
	// nicht der Spline-Verlauf (Spline-Treue: B7 Section 12/V7, spaetere Ausbaustufe).
	void DrawPathViz( PCT_Shot shot )
	{
		if ( !m_PathCanvas )
			return;

		m_PathCanvas.Clear();
		if ( !shot )
			return;

		float screenW;
		float screenH;
		m_Root.GetScreenSize( screenW, screenH );

		float prevX = 0.0;
		float prevY = 0.0;
		bool hasPrev = false;

		int camCount = shot.cameraPath.points.Count();
		for ( int i = 0; i < camCount; i++ )
		{
			PCT_PathPoint point = shot.cameraPath.points.Get( i );
			if ( !point )
				continue;

			vector rel = g_Game.GetScreenPosRelative( point.position );
			if ( rel[2] <= 0.0 )
			{
				hasPrev = false;
				continue;
			}

			float sx = rel[0] * screenW;
			float sy = rel[1] * screenH;

			DrawPathMarker( sx, sy, PCT_PATH_VIZ_CAM_COLOR );
			if ( hasPrev )
				m_PathCanvas.DrawLine( prevX, prevY, sx, sy, 1.0, PCT_PATH_VIZ_LINE_COLOR );

			prevX = sx;
			prevY = sy;
			hasPrev = true;
		}

		if ( shot.lookPath )
		{
			int lookCount = shot.lookPath.points.Count();
			for ( int l = 0; l < lookCount; l++ )
			{
				PCT_LookPoint lookPoint = shot.lookPath.points.Get( l );
				if ( !lookPoint )
					continue;

				vector lookRel = g_Game.GetScreenPosRelative( lookPoint.worldTarget );
				if ( lookRel[2] <= 0.0 )
					continue;

				float lx = lookRel[0] * screenW;
				float ly = lookRel[1] * screenH;
				DrawPathMarker( lx, ly, PCT_PATH_VIZ_LOOK_COLOR );
			}
		}
	}

	protected void DrawPathMarker( float x, float y, int color )
	{
		float markerSize = 6.0;
		float x1 = x - markerSize;
		float y1 = y - markerSize;
		float x2 = x + markerSize;
		float y2 = y + markerSize;
		m_PathCanvas.DrawLine( x1, y1, x2, y2, 2.0, color );
		m_PathCanvas.DrawLine( x1, y2, x2, y1, 2.0, color );
	}

	protected void RecalculateMask()
	{
		if ( !m_MaskTop || !m_Root )
			return;

		float targetAspect = AspectRatioForMask( m_AspectMask );
		if ( targetAspect <= 0.0 )
		{
			m_MaskTop.Show( false );
			m_MaskBottom.Show( false );
			m_MaskLeft.Show( false );
			m_MaskRight.Show( false );
			return;
		}

		float screenW;
		float screenH;
		m_Root.GetScreenSize( screenW, screenH );

		if ( screenW <= 0.0 || screenH <= 0.0 )
			return;

		float screenAspect = screenW / screenH;

		if ( targetAspect >= screenAspect )
		{
			// Letterbox (Balken oben/unten). Beispielrechnung Report Selbstpruefung 1 (1920x1080,
			// 2.39:1): contentH = 1920 / 2.39 = 803.35 px; barH = (1080 - 803.35) * 0.5 = 138.33 px.
			float contentH = screenW / targetAspect;
			float barH = ( screenH - contentH ) * 0.5;

			m_MaskTop.SetScreenSize( screenW, barH );
			m_MaskTop.SetScreenPos( 0, 0 );

			m_MaskBottom.SetScreenSize( screenW, barH );
			float bottomY = screenH - barH;
			m_MaskBottom.SetScreenPos( 0, bottomY );

			m_MaskLeft.Show( false );
			m_MaskRight.Show( false );
			m_MaskTop.Show( true );
			m_MaskBottom.Show( true );
		}
		else
		{
			// Pillarbox (Balken links/rechts). Beispielrechnung Report Selbstpruefung 1 (1920x1080,
			// 9:16): contentW = 1080 * (9/16) = 607.5 px; barW = (1920 - 607.5) * 0.5 = 656.25 px.
			float contentW = screenH * targetAspect;
			float barW = ( screenW - contentW ) * 0.5;

			m_MaskLeft.SetScreenSize( barW, screenH );
			m_MaskLeft.SetScreenPos( 0, 0 );

			m_MaskRight.SetScreenSize( barW, screenH );
			float rightX = screenW - barW;
			m_MaskRight.SetScreenPos( rightX, 0 );

			m_MaskTop.Show( false );
			m_MaskBottom.Show( false );
			m_MaskLeft.Show( true );
			m_MaskRight.Show( true );
		}
	}
}
