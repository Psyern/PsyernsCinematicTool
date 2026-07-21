Stoff- und Anforderungssammlung

1. Grundidee für die Mod

Der Mod soll Filmszenen in einer frei begehbaren 3D-Umgebung nachstellen. Nutzer können Räume, Darsteller, Requisiten, Lichtquellen und Kameras platzieren und daraus konkrete Kameraeinstellungen und vollständige Shot-Sequenzen planen.

Das Ergebnis ist kein fertiger Film, sondern eine virtuelle Drehvorbereitung beziehungsweise Previsualisierung. der Mod soll zeigen:

Wo die Kamera stehen muss.
Welches Objektiv verwendet werden könnte.
Wie groß der Bildausschnitt ist.
Wie Darsteller positioniert werden.
Wie sich Kamera und Darsteller bewegen.
Ob Blickrichtungen und Anschluss funktionieren.
Welche Wirkung eine Einstellung voraussichtlich erzeugt.
Ob die geplante Einstellung am realen Drehort räumlich machbar ist.

Ein Establishing Shot liefert beispielsweise Informationen über Ort, Zeit und räumlichen Zusammenhang, während nähere Einstellungen stärker Emotionen und Details hervorheben. Kameraabstand, Perspektive, Brennweite und Bewegung müssen deshalb im Programm als getrennte, aber kombinierbare Parameter behandelt werden.

2. Wichtigste begriffliche Trennung

der Mod darf Wide Shot, Weitwinkelobjektiv und hohe Kameraperspektive nicht miteinander vermischen.

Es gibt mindestens fünf voneinander unabhängige Ebenen:

Ebene	Frage	Beispiele
Einstellungsgröße	Wie groß erscheint das Motiv?	Wide Shot, Medium Shot, Close-up
Kameraperspektive	Von wo blickt die Kamera?	Eye Level, High Angle, Low Angle
Objektiv	Wie wird der Raum optisch dargestellt?	24 mm, 50 mm, 85 mm
Kamerabewegung	Wie bewegt sich die Kamera?	Dolly, Pan, Tracking, Handheld
Bildkomposition	Wo befindet sich das Motiv im Frame?	Zentriert, Drittelregel, Negative Space

Ein Close-up kann beispielsweise mit einem 35-mm-, 85-mm- oder 135-mm-Objektiv aufgenommen werden. Der Bildausschnitt kann ähnlich sein, aber Perspektive, Gesichtsabbildung, Hintergrund und Kameradistanz verändern sich deutlich. Adobe beschreibt Brennweite und Kameradistanz ausdrücklich als getrennte Faktoren: kurze Brennweiten zeigen mehr Raum, längere Brennweiten verengen und komprimieren die sichtbare Szene.

Konsequenz für das Preset-System

Ein vollständiges Kamera-Preset sollte nicht nur „Close-up“ heißen, sondern beispielsweise:

Close-up · Eye Level · 85 mm · leichte Gegenposition · statisch · Fokus auf linkes Auge

Die Presets müssen modular kombinierbar sein:

Einstellungsgröße wählen.
Kamerawinkel wählen.
Sensor und Brennweite wählen.
Bewegung wählen.
Komposition und Fokus festlegen.
3. Einstellungsgrößen als Presets

Die folgenden Brennweiten sind lediglich sinnvolle Startwerte in Vollformat-Äquivalenten. Sie dürfen nicht als feste filmische Regeln verstanden werden.

Preset	Kürzel	Automatisches Framing	Möglicher Startwert	Hauptwirkung
Extreme Wide Shot	EWS/ELS	Umgebung dominiert, Figur sehr klein	14–24 mm	Größe, Isolation, Orientierung
Establishing Shot	ES	Ort, Architektur oder Landschaft	18–35 mm	Ort, Zeit und Atmosphäre
Wide/Long Shot	WS/LS	Figur vollständig mit viel Raum	24–35 mm	Verhältnis zwischen Person und Umgebung
Full Shot	FS	Person vollständig von Kopf bis Fuß	35–50 mm	Bewegung und Körpersprache
Medium Long Shot	MLS	ungefähr ab Knie oder Oberschenkel	40–65 mm	Körpersprache plus Gesicht
Cowboy Shot	CS	ungefähr ab Mitte Oberschenkel	40–65 mm	Handlung, Hände, Ausrüstung
Medium Shot	MS	ungefähr ab Hüfte	50–85 mm	Dialog und Gestik
Medium Close-up	MCU	Brust oder Schulter bis Kopf	65–100 mm	Nähe bei weiterhin sichtbarer Körpersprache
Close-up	CU	Gesicht beziehungsweise wichtiges Objekt	85–135 mm	Emotion und Aufmerksamkeit
Big Close-up	BCU	Gesicht füllt das Bild stärker	100–150 mm	hohe emotionale Nähe
Extreme Close-up	ECU	Augen, Mund, Hände oder Detail	100–200 mm/Makro	Bedeutung, Spannung, Intimität
Insert Shot	INSERT	isoliertes Requisit oder Handlung	frei	erzählerisches Detail
Cut-in	CI	Detail innerhalb einer laufenden Handlung	frei	Information und Schnittmöglichkeit
Cutaway	CA	Reaktion oder anderes Motiv	frei	Rhythmus und Schnittabdeckung

Wide und Medium Shots eignen sich besonders zur Darstellung von Raum und Handlung, während Close-ups Emotionen und feine Details hervorheben. Extreme Close-ups isolieren einzelne Merkmale oder Gegenstände.

Zusätzliche Framing-Presets

Diese Presets definieren nicht ausschließlich die Größe, sondern auch die Beziehung zwischen Motiven:

Single Shot
Two Shot
Three Shot
Group Shot
Over-the-Shoulder – Dirty
Over-the-Shoulder – Clean
Wide Over-the-Shoulder
Point of View
Subjective Camera
Profile Shot
Reaction Shot
Master Shot
Symmetrical Shot
Silhouette Shot
Product Hero Shot
Top-down Detail Shot

Beim Over-the-Shoulder Shot bleibt ein Teil der vorderen Person sichtbar. Dadurch entstehen räumliche Tiefe, Perspektivzuordnung und eine stärkere Verbindung zur beobachtenden Figur.

4. Kamerawinkel und Kamerahöhen

Artlist unterscheidet grundsätzlich zwischen hoher, niedriger und neutraler Perspektive, ergänzt um extremere Varianten wie Bird’s Eye, Overhead, Worm’s Eye und Dutch Angle. Die Position kann Macht, Verletzlichkeit, Nähe oder Orientierungslosigkeit vermitteln.

Perspektiven-Preset	Technische Umsetzung	Typische Wirkung
Eye Level	Kamera auf Augenhöhe	neutral, direkt, menschlich
Slight High Angle	leicht oberhalb der Augen	zugänglich, zurückhaltend
High Angle	deutlich über dem Motiv	klein, verletzlich, unterlegen
Overhead	fast oder vollständig senkrecht von oben	Übersicht, Muster, Kontrolle
Bird’s Eye	weit entfernt und stark von oben	Orientierung, Abstraktion, Distanz
Low Angle	Kamera unterhalb der Augen	Stärke, Macht, Dominanz
Worm’s Eye	sehr bodennah, stark nach oben	Größe, Bedrohung, Monumentalität
Ground Level	etwa 10–40 cm Höhe	Nähe zum Boden, Dynamik
Hip Level	etwa Hüfthöhe	Bewegung, Hände, Western-Wirkung
Knee Level	Kniehöhe	ungewöhnliche, dynamische Perspektive
Shoulder Level	Schulterhöhe	leicht beobachtend
Top-down	90 Grad nach unten	Arbeitsfläche, Produkt, Choreografie
Dutch Angle	Roll-Achse gekippt	Instabilität, Spannung, Verwirrung
Aerial/Drone	freie hohe Position	Landschaft, Wege, räumliche Übersicht
POV	Kamera an Augenposition einer Figur	subjektive Wahrnehmung
OTS	hinter Schulter oder Kopf	Dialog, Beziehung, Perspektivzuordnung
Profile	ungefähr 90 Grad seitlich	Beobachtung, Distanz, Silhouette

Für Dutch Angles kann der Mod beispielsweise feste Ausgangswerte von 15, 25 und 40 Grad anbieten. Adobe nennt ungefähr 15 bis 45 Grad als typischen Bereich, wobei die Neigung bewusst erkennbar sein sollte.

5. Virtuelle Kamera und Objektivsystem
Kameraeigenschaften

Jede virtuelle Kamera benötigt mindestens:

Kameraname und Nummer
Kameramodell
Sensorformat
Sensorbreite und Sensorhöhe
Crop-Faktor
Brennweite
horizontales und vertikales Sichtfeld
Seitenverhältnis
Auflösung
Bildrate
Verschlusszeit oder Shutter Angle
Blende
ISO
Weißabgleich
ND-Filter
Fokusdistanz
Nahgrenze des Objektivs
Schärfentiefe
Anamorphotischer Faktor
Lens Shift
Near und Far Clipping Plane
Kamera- und Rig-Gewicht
Stabilisierung beziehungsweise Rig-Typ

Three.js kann eine perspektivische Kamera mit Sichtfeld, Seitenverhältnis sowie Nah- und Ferngrenze abbilden. Die Brennweite lässt sich über setFocalLength() in Bezug auf die Film- beziehungsweise Sensorbreite einstellen. Blender behandelt Brennweite, Sensorgröße, Fokusdistanz und Schärfentiefe ebenfalls als separate Kameraparameter.

Sensor-Presets

der Mod sollte fertige Sensorformate anbieten:

Full Frame 36 × 24 mm
Super 35
APS-C
Micro Four Thirds
1-Zoll-Sensor
Smartphone
IMAX-ähnliche Formate
Benutzerdefinierter Sensor
Anamorphic Open Gate

Zusätzlich sollten konkrete Kameramodelle als Bibliothek angelegt werden können, etwa Sony FX3, FX6 oder andere tatsächlich verwendete Systeme.

Brennweiten-Presets

Sinnvolle Standardbuttons:

14 · 18 · 21 · 24 · 28 · 32 · 35 · 40 · 50 · 65 · 75 · 85 · 100 · 135 · 200 mm

Dazu kommen:

Zoomobjektive mit Anfangs- und Endbrennweite
eigene Lens Kits
anamorphotische Objektive
Makroobjektive
Mindestfokusdistanz
Objektivdurchmesser und Gewicht
reale Verzeichnung
Vignettierung
Focus Breathing
Lens Distortion
Bokeh-Charakter
Automatische Kameradistanz

Die Kamera sollte bei Auswahl eines Framing-Presets automatisch so positioniert werden, dass die Figur entsprechend im Bild erscheint.

Grundlage:

FOV = 2 × arctan(Sensorgröße / (2 × Brennweite))

Aus Motivhöhe, gewünschtem Anteil im Bild und vertikalem Sichtfeld kann anschließend eine passende Kameradistanz angenähert werden.

Wichtig ist, dass ein Close-up nicht über eine fest gespeicherte Entfernung definiert wird. Die Entfernung muss sich an folgende Faktoren anpassen:

Körpergröße der Figur
Sensorformat
Brennweite
Seitenverhältnis
gewünschte Headroom-Größe
Kamerawinkel
Pose der Figur
6. Kamerabewegungen

der Mod benötigt eine Timeline mit Keyframes und Bewegungswegen.

Grundbewegungen
Bewegung	Beschreibung
Static	Kamera bleibt vollständig stehen
Pan	horizontale Drehung
Tilt	vertikale Drehung
Roll	Drehung um die optische Achse
Dolly In/Out	räumlich auf das Motiv zu oder davon weg
Truck Left/Right	seitliche Kamerafahrt
Pedestal Up/Down	vertikale Kamerafahrt
Tracking	Kamera begleitet eine Figur
Follow	Kamera folgt hinter oder neben einer Figur
Lead	Kamera bewegt sich vor der Figur
Orbit/Arc	Bogenfahrt um das Motiv
Crane/Jib	kombinierte vertikale und räumliche Bewegung
Drone	freier dreidimensionaler Flug
Handheld	simulierte menschliche Bewegung
Shoulder Rig	kontrollierte, körpernahe Bewegung
Gimbal	geglättete Bewegung
Steadicam	gleitende Bewegung mit leichter organischer Reaktion
Whip Pan	sehr schnelle Schwenkbewegung
Zoom In/Out	Veränderung der Brennweite ohne Kamerafahrt
Dolly Zoom	Fahrt und entgegengesetzte Brennweitenänderung
Rack Focus	Fokuswechsel zwischen Motiven

Ein Dolly Zoom kombiniert eine räumliche Kamerafahrt mit einer Brennweitenänderung in Gegenrichtung. Das Motiv bleibt dabei annähernd gleich groß, während sich der Hintergrund sichtbar streckt oder verdichtet.

Bewegungsparameter

Für jede Bewegung:

Start- und Endpunkt
Dauer
Geschwindigkeit
Beschleunigung
Ease In und Ease Out
Zielobjekt
Look-at-Verhalten
Position Lock
Horizon Lock
Collision Avoidance
Kamerahöhe
Kurventyp
Handheld-Amplitude
Handheld-Frequenz
Verzögerung beim Folgen
Fokusverlauf
Brennweitenverlauf

Die Bewegung muss als sichtbarer Pfad im Raum dargestellt werden. Auf dem Pfad sollten Start, Ende, Richtung, Geschwindigkeit und mögliche Kollisionen erkennbar sein.

7. Szenenaufbau
3D-Elemente

der Mod benötigt eine Bibliothek aus:

Menschen unterschiedlicher Größe
stehenden, sitzenden und liegenden Posen
einfachen Lauf- und Bewegungsanimationen
Möbeln
Türen und Fenstern
Fahrzeugen
Pflanzen
Wänden, Böden und Decken
Straßen und Landschaftselementen
Filmtechnik
Stativen
Dollys
Kränen
Lichtstativen
Mikrofonen
Flags, Diffusionsrahmen und Reflektoren

3D-Modelle sollten vorzugsweise als glTF beziehungsweise GLB importiert werden. glTF ist als runtime-neutrales 3D-Austauschformat für Geometrie, Materialien, Kameras und Animationen ausgelegt und wird direkt von Three.js unterstützt.

Raum- und Location-Aufbau

Mögliche Eingaben:

Raumabmessungen
Wandhöhe
Tür- und Fensterpositionen
Grundriss als Bild oder PDF
Grundrissskalierung über eine bekannte Strecke
3D-Scan
Photogrammetrie
LiDAR-Import
eigene 3D-Modelle
GPS-/Geländedaten
Einheiten in Metern, Zentimetern oder Feet
Darsteller-Blocking

Jede Figur erhält:

Name und Rolle
Körpergröße
Augenhöhe
Schulterhöhe
aktuelle Pose
Blickrichtung
Bewegungsweg
Laufgeschwindigkeit
Start- und Endmarke
Handlungen
zugehörige Requisiten
Dialog beziehungsweise Cue
zeitliche Markierungen

Im Grundriss müssen Darstellermarken wie auf einem echten Set sichtbar sein.

8. Lichtplanung

Auch wenn die Kamera im Mittelpunkt steht, ist die Bildwirkung ohne Licht nicht sinnvoll beurteilbar.

Lichtquellen
Sonne und Himmel
Directional Light
Point Light
Spot Light
Area Light
LED Panel
Fresnel
Tube Light
Softbox
Fensterlicht
Practical Light
Kerze, Feuer und Neon
Reflektor
Negativ-Fill
Diffusion
Gobos und Flags
Parameter
Position
Ausrichtung
Höhe
Leistung
Abstrahlwinkel
Farbtemperatur
Tint
Größe der Lichtquelle
Härte und Weichheit
Schatten
Falloff
IES-Profil
Gel beziehungsweise Farbfilter
Dimmer
Flicker
Sonnenstand
Tageszeit
Wetter
Licht-Presets
Three-Point Lighting
Soft Interview
Hard Sun
Window Light
Rembrandt
High Key
Low Key
Horror
Moonlight
Golden Hour
Product Lighting
Silhouette
Motivated Practical Lighting
9. Kompositionshilfen

Im Kamerabild sollten einblendbar sein:

Drittelraster
Fadenkreuz
Bildzentrum
Golden Ratio
Diagonalen
Symmetrieachse
Horizon Level
Headroom
Look Room
Lead Room
Action Safe
Title Safe
Social-Media-Safe-Areas
2.39:1-, 16:9-, 4:3-, 9:16- und 1:1-Masken
Augenlinie
Fokusbereich
Tiefenschärfe
Kamera-Frustum
Sichtbare und verdeckte Objekte
Lens Distortion Preview

Komposition sollte nicht nur als Raster behandelt werden. Balance, Symmetrie, Führungslinien und negativer Raum beeinflussen, wohin der Blick geführt wird und ob eine Figur beispielsweise isoliert oder dominant wirkt.

10. Kontinuitäts- und Regieassistent

der Mod sollte filmische Fehler nicht verbieten, aber sichtbar kennzeichnen.

180-Grad-Linie

Zwischen zwei Figuren oder entlang einer Bewegungsrichtung wird automatisch eine Handlungsachse eingezeichnet. Kameras auf der gegenüberliegenden Seite erhalten eine Warnung:

„Diese Kamera überschreitet die 180-Grad-Linie. Die Bildschirmpositionen der Figuren können sich beim Schnitt umkehren.“

Das Einhalten der Achse bewahrt räumliche Orientierung. Ein bewusster Achsensprung kann hingegen Desorientierung oder einen Machtwechsel ausdrücken.

30-Grad-Warnung

Bei zwei direkt aufeinanderfolgenden Einstellungen:

„Die Kameraposition unterscheidet sich um weniger als 30 Grad. Der Schnitt könnte wie ein unbeabsichtigter Jump Cut wirken.“

Adobe beschreibt die 30-Grad-Regel als Hilfsmittel, um abrupte beziehungsweise störende Schnitte zwischen zu ähnlichen Kamerawinkeln zu vermeiden.

Weitere automatische Prüfungen
Blickachsen stimmen nicht überein.
Figur schaut im Gegenschuss in die falsche Richtung.
Motiv wird an Knie, Knöchel oder Gelenk ungünstig angeschnitten.
Kamera kollidiert mit Wand oder Requisite.
Kamerapfad ist für den ausgewählten Dolly nicht möglich.
Kamera befindet sich außerhalb des Raums.
Objektiv unterschreitet seine Mindestfokusdistanz.
Mikrofon oder Lichtstativ befindet sich im Bild.
Bewegung verlässt den Fokusbereich.
Darsteller verlässt den Frame.
Headroom ändert sich unbeabsichtigt.
Horizont ist leicht schief.
Zwei Einstellungen sind für einen sauberen Schnitt zu ähnlich.
Requisitenposition unterscheidet sich zwischen Shots.
Screen Direction wechselt.
Eyeline Match ist unplausibel.

Eyeline Matching, die 180-Grad-Regel und Match-on-Action gehören zu den zentralen Techniken, mit denen zeitliche und räumliche Kontinuität über mehrere Einstellungen erhalten wird.

11. Shot- und Sequenzsystem
Hierarchie
Projekt
└── Sequenz
    └── Szene
        └── Setup
            └── Shot
                └── Take-Planung
Inhalt eines Shots
Szenennummer
Shot-Nummer
Shot-Name
Beschreibung
Einstellungsgröße
Kamerawinkel
Sensor
Brennweite
Blende
Fokusdistanz
Bildrate
Shutter Angle
ISO
Weißabgleich
Seitenverhältnis
Kameraabstand
Kamerahöhe
Kameraposition
Bewegung
Bewegungsdauer
Fokusbewegung
Darstelleraktion
Dialog
benötigte Technik
Lichtsetup
Tonanforderung
Priorität
geschätzte Drehzeit
Thumbnail
Storyboard
Regiehinweis
Status
Coverage-Assistent

Zu einer Dialogszene könnte der Mod automatisch folgende Abdeckung vorschlagen:

Establishing oder Wide Master.
Two Shot.
OTS Person A.
Reverse OTS Person B.
Close-up Person A.
Close-up Person B.
Reaktionsaufnahme.
Insert eines wichtigen Gegenstands.
Cutaway.
Safety Shot.

Eine Shot List soll übersichtlich bleiben und sicherstellen, dass alle für einen verständlichen Schnitt benötigten Einstellungen vorhanden sind. Adobe empfiehlt, Szenen gemeinsam nach Blocking, Kamerawinkel und benötigter Technik zu planen.

12. Benutzeroberfläche
Hauptaufbau
Linke Seitenleiste: Bibliothek
Szenen
Darsteller
Requisiten
Gebäude
Filmtechnik
Kameras
Objektive
Lichter
Presets
gespeicherte Setups
Mitte: 3D-Viewport
freie Perspektive
aktive Kamera
Grundriss von oben
Vierfachansicht
Splitscreen zwischen 3D-Ansicht und Kamerabild
Vergleich von Kamera A und Kamera B
Rechte Seitenleiste: Inspector

Register:

Transform
Kamera
Objektiv
Framing
Fokus
Bewegung
Licht
Darsteller
Kontinuität
Notizen
Unterer Bereich
Shot-Timeline
Animationsspuren
Kameraspuren
Fokusspur
Darstellerbewegungen
Brennweitenverlauf
Shot Cards
Wiedergabesteuerung
Bedienmodi
Build Mode: Räume und Objekte aufbauen.
Blocking Mode: Darsteller positionieren und bewegen.
Camera Mode: Kameras und Presets einstellen.
Lighting Mode: Licht planen.
Director Mode: Shots bewerten und kommentieren.
Sequence Mode: Shots schneiden und als Animatic abspielen.
Plan View: Technischer Grundriss.
Presentation Mode: Szene ohne Bearbeitungswerkzeuge präsentieren.
13. Preset-System
Preset-Arten
Shot-Size-Preset
Angle-Preset
Lens-Preset
Camera-Body-Preset
Movement-Preset
Focus-Preset
Lighting-Preset
Complete-Shot-Preset
Dialogue-Coverage-Preset
Genre-Preset
User-Preset
Beispiel für ein vollständiges Preset
{
  "id": "cu_emotional_85_eye",
  "name": "Emotional Close-up – 85 mm",
  "category": "completeShot",
  "framing": {
    "type": "closeUp",
    "subjectScreenHeight": 0.82,
    "headroom": 0.06,
    "lookRoom": 0.12,
    "focusTarget": "nearestEye"
  },
  "camera": {
    "sensorPreset": "fullFrame",
    "focalLengthMm": 85,
    "aperture": 2.0,
    "heightMode": "eyeLevel",
    "pitchDegrees": 0,
    "rollDegrees": 0
  },
  "movement": {
    "type": "static",
    "durationSeconds": 5
  },
  "composition": {
    "alignment": "ruleOfThirds",
    "subjectSide": "left"
  },
  "tags": [
    "portrait",
    "emotion",
    "dialogue",
    "intimate"
  ]
}
Preset-Verhalten

Nach Auswahl eines Presets sollte das System:

das Zielmotiv identifizieren,
dessen Abmessungen messen,
eine geeignete Kameraposition berechnen,
die Augenhöhe berücksichtigen,
den Fokus setzen,
das Framing anwenden,
die Kamera auf das Motiv ausrichten,
mögliche Kollisionen prüfen,
bei Bedarf alternative Positionen vorschlagen.
14. Exportfunktionen
Produktionsunterlagen
Shot List als PDF
Shot List als CSV oder Excel
Storyboard-PDF
Floor Plan
Camera Plot
Lighting Plot
Equipment List
Tagesdisposition-Grundlage
Objektivliste
Kamera- und Darstellermarken
Screenshots mit technischen Daten
QR-Code pro Shot
Projektbericht
Digitale Formate
Projekt-JSON
GLB/glTF
Einzelbilder
MP4-Animatic
Bildsequenz
EDL
OpenTimelineIO
Blender-Kameraexport
Unreal-/Unity-kompatible Kameradaten
CSV für Produktionssoftware
15. Technische Umsetzung
Empfohlener erster Technologie-Stack

Für eine schnell erreichbare und teilbare erste Version:

Frontend
React
TypeScript
Three.js
React Three Fiber
Zustand oder Redux Toolkit
Tailwind oder eigenes UI-System
WebGL 2
optional WebGPU
3D-Formate
GLB/glTF für Modelle
HDRI für Umgebungslicht
PNG/JPEG/WebP für Texturen
JSON für Szenen und Presets
Backend
Node.js
PostgreSQL
Objektspeicher für 3D-Modelle und Vorschaubilder
Benutzerkonten
Projektversionierung
Teamfreigaben
Kommentare und Rollen

Three.js unterstützt perspektivische Kameras, reale Brennweitenwerte, animierte glTF-Modelle und Kamera-Frustum-Darstellungen. Dadurch eignet es sich gut für einen webbasierten Previsualisierungs-MVP.

Langfristige Alternative

Für besonders realistische Beleuchtung, komplexe Szenen und Virtual Production wäre eine spätere Unreal-Engine-Version sinnvoll. Die Webanwendung könnte dann als Projektverwaltung, Shot-Planung und Kundenfreigabe bestehen bleiben.

Empfohlene Architektur
Editor UI
├── Project Manager
├── Scene Graph
├── Asset Library
├── Camera System
├── Lens System
├── Preset Engine
├── Character Blocking
├── Lighting System
├── Animation Timeline
├── Continuity Validator
├── Shot Manager
├── Render Preview
└── Export Manager
16. Entwicklungsstufen
Phase 1: Funktionierender MVP
leerer 3D-Raum
primitive Wände und Böden
eine menschliche Figur
eine Kamera
freie Kamerapositionierung
Full-Frame-Sensor
Brennweitenwahl
Wide-, Medium- und Close-up-Presets
Eye-, High- und Low-Angle
Kameraansicht und freie 3D-Ansicht
Shot speichern
Screenshot und einfache Shot List exportieren
Phase 2: Praktische Drehplanung
mehrere Figuren
Raumeditor
Objektbibliothek
Lens Kits
Fokus und Schärfentiefe
Bewegungswege
Darsteller-Blocking
OTS und POV
180-Grad-Linie
Timeline
PDF-Storyboard
Projekt speichern und laden
Phase 3: Vollständige Previsualisierung
Lichtplanung
Kameraanimationen
Figuranimationen
Dolly, Gimbal, Crane und Drone
Continuity Checker
Coverage-Assistent
Multi-Camera
Animatic-Export
Teamkommentare
Versionen und Freigaben
Phase 4: Professionelles Produktionssystem
konkrete Kameramodelle
reale Objektivdatenbanken
Focus Breathing und Verzeichnung
LiDAR- und Location-Scan-Import
Sonnenstandsberechnung
Unreal-/Blender-Export
AR-Ansicht am Drehort
Tablet-Modus
Shot-Abgleich zwischen Planung und realem Kamerabild
KI-gestützte Shot-Vorschläge
17. Kompakte Projektbeschreibung

Entwickelt werden soll ein interaktives 3D-Previsualisierungsprogramm für Film-, Werbe- und Videoproduktionen. Nutzer bauen darin reale oder fiktive Drehorte nach, positionieren Darsteller, Requisiten, Kameras und Lichtquellen und planen einzelne Kameraeinstellungen sowie vollständige Szenen.

Das Kamerasystem arbeitet mit realistischen Sensorgrößen, Brennweiten, Sichtfeldern, Fokusdistanzen und Seitenverhältnissen. Filmische Presets wie Establishing Shot, Wide Shot, Medium Shot, Close-up, Over-the-Shoulder, Point of View, High Angle, Low Angle oder Dutch Angle positionieren die Kamera automatisch relativ zum ausgewählten Motiv.

Einstellungsgröße, Kamerawinkel, Objektiv, Bewegung und Komposition bleiben getrennt editierbar. Dadurch kann beispielsweise ein Close-up mit unterschiedlichen Brennweiten und Perspektiven verglichen werden.

Kameras und Darsteller können auf einer Timeline animiert werden. Das System zeigt Kamerapfade, Blickrichtungen, Fokusbereiche und Bildausschnitte an. Ein Continuity-Assistent prüft unter anderem die 180-Grad-Regel, Blickachsen, Bildschirmrichtungen, Kamerakollisionen und zu ähnliche aufeinanderfolgende Einstellungen.

Jeder geplante Shot wird mit Vorschaubild und technischen Daten gespeichert. Aus dem Projekt lassen sich Shot Lists, Storyboards, Kamera- und Lichtpläne sowie ein einfaches 3D-Animatic exportieren.