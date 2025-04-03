## TaupunktLueftung

TaupunktLueftung ist ein ESP32-basiertes System zur intelligenten Lüftungssteuerung auf Grundlage der Taupunktdifferenz. Es ist ideal zum gezielten Trocknen feuchter Räume wie Keller, Waschküchen oder Vorratskammern – effizient und vollautomatisch.

Das System misst Temperatur und Luftfeuchtigkeit innen und außen (über Sensoren wie z. B. SHT31 oder DHT22 - oder per MQTT), berechnet daraus die Taupunkte und aktiviert die Lüftung nur dann, wenn trocknende Bedingungen vorliegen – also wenn die Außenluft in der Lage ist, Feuchtigkeit aufzunehmen, statt sie einzutragen.

Die Lüftung wird über ein Relais geschaltet – optimalerweise in einem Setup mit zwei Lüftern: einer zieht trockene Luft hinein, der andere führt feuchte Luft ab. Dadurch entsteht ein effektiver Luftstrom zum gezielten Entfeuchten.


## Features:

- Steuerung über Webinterface mit Live-Datenanzeige und Echtzeitdiagrammen

- Optionaler Zugriff und Integration via MQTT

- Modularer Aufbau: interne Sensoren oder externe MQTT-Daten

- Algorithmus zur Taupunktanalyse steuert die Lüftung je nach Differenz

- Unterstützt Firmware-Updates über das Webinterface (OTA)


## Benötigte Komponenten und Aufbau

### Hardware-Komponenten

| Komponente                        | Beschreibung                                            | Ca. Preis (€) |
|----------------------------------|---------------------------------------------------------|---------------|
| **ESP32 Dev Board** (z. B. DOIT) | Mikrocontroller mit WLAN                                | 6–10 €        |
| **SHT31 Sensor (Innen)**         | Temperatur & Luftfeuchte (hochgenau)                    | 5–8 €         |
| **DHT22 Sensor (Außen)**         | Temperatur & Luftfeuchte (günstiger, für Außenbereich)  | 3–5 €         |
| **LEDs (3×)** + Vorwiderstände   | Statusanzeigen: grün, gelb, rot                         | 1–2 €         |
| **Relais-Modul (1 Kanal)**       | Zur Ansteuerung der Lüftung                             | 2–4 €         |
| **Jumper-Kabel / Breadboard**    | Für Aufbau und Tests                                    | 2–5 €         |
| **Gehäuse (optional)**           | Schutz für ESP32 und Verkabelung                        | 3–8 €         |

**Gesamtkosten:** ca. **20–30 €**, je nach Ausstattung und Bezugsquelle.


### Aufbau und Verdrahtung

**ESP32-Pinbelegung:**

| ESP32-Pin | Funktion           | Angeschlossen an             |
|-----------|--------------------|-------------------------------|
| GPIO17    | DHT22 Datenleitung | DHT22 (mit 10 kΩ Pull-Up)    |
| GPIO16    | Relais             | Relais IN                     |
| GPIO2     | LED grün           | Vorwiderstand + LED           |
| GPIO18    | LED rot            | Vorwiderstand + LED           |
| GPIO19    | LED gelb           | Vorwiderstand + LED           |
| SDA/SCL   | I2C                | SHT31 (Innen)                 |
| 3.3 V/GND | Stromversorgung    | Alle Komponenten              |



### Hinweise

- **Relais-Modul:** Kann zum Schalten einer 230 V-Lüftung verwendet werden.  
  **Achtung:** Netzspannung nur durch Fachpersonal anschließen lassen.
- **MQTT:** Zum Empfangen externer Sensordaten wird ein MQTT-Broker benötigt  
  (z. B. Mosquitto oder Home Assistant).
- **Webinterface:** Alle Einstellungen wie MQTT, Sensorquellen und Schwellenwerte  
  sind direkt über das Browser-Interface konfigurierbar.
- **DHT22 Sensor wettergeschützt montieren**, z. B. unter einem Vordach oder in einem geeigneten Gehäuse.
