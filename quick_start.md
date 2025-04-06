## Schnellstart: TaupunktLueftung auf ESP32

**1. Benötigte Hardware**

ESP32 Dev Kit (z. B. dieses [Board] (https://amzn.eu/d/gyWvNsA))

Relais, Sensoren, LEDs, Lüfter (siehe Stückliste in README)



---

**2. Arduino IDE vorbereiten**

Installiere die Arduino IDE

ESP32-Unterstützung einrichten:

Datei → Voreinstellungen → Boardverwalter-URL: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

Dann: Werkzeuge → Board → Boardverwalter → "ESP32" installieren


Board auswählen: z. B. "ESP32 Dev Module"



---

**3. Projekt vorbereiten**

Lade den Projektordner herunter (GitHub oder ZIP)

Öffne den Ordner in der Arduino IDE (TaupunktLueftung.ino)

Datei secrets_template.h umbenennen zu secrets.h

Trage dort deine WLAN-Zugangsdaten und ggf. MQTT-Parameter ein

(-> für mqtt autodiscovery beachte [Debug-Hinweise](debug_hinweise.md))



---

**4. ESP32 verbinden und flashen**

Schließe das Board per USB an

Wähle den richtigen COM-Port unter Werkzeuge → Port

Klicke auf "Hochladen"

Nach dem Flashen → Seriellen Monitor öffnen (115200 Baud) → Status prüfen



---

**5. Hardware aufbauen & verdrahten**

Sensoren anschließen:

Innen z. B. SHT31 via I2C (GPIO21+22)

Außen z. B. DHT22 (GPIO17)


Relais-Modul an GPIO16

LEDs (Status) an GPIO2, GPIO18, GPIO19

Tipp: Kabellängen für Sensoren möglichst kurz halten (ca. max. 2 m, abgeschirmt besser)



---

**6. Gehäusewahl**

Innen: z. B. Verteilerdose mit Platz für ESP32, Relais, LEDs, ggf. Innensensor

Außen: wettergeschütztes Gehäuse mit Belüftung für DHT22
(z. B. Wetterschutzgehäuse, Insektenschutzgitter)



---

**7. Lüfter anschließen**

230 V-Ventilatoren über das Relais schalten

Alternativ: MQTT-gesteuerte Steckdose zur Lüftersteuerung (s. README)



---

**8. Fertig!**

Webinterface aufrufen: http://[IP-Adresse deines ESP32] (wird auch in seriellem Monitor ausgegeben)

Einstellungen wie MQTT, Sensorquellen etc. im Webinterface anpassen

Live-Daten und Charts direkt im Browser


