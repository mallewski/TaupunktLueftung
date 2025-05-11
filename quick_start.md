## Schnellstart: TaupunktLueftung auf ESP32

**1. Benötigte Hardware**

ESP32 Dev Kit (z. B. dieses [Board](https://amzn.eu/d/gyWvNsA))

Relais, Sensoren, LEDs, Lüfter (siehe Stückliste in README)



---

**2. Arduino IDE vorbereiten**

Installiere die Arduino IDE

ESP32-Unterstützung einrichten:

Datei → Voreinstellungen → Boardverwalter-URL: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

Dann: Werkzeuge → Board → Boardverwalter → "ESP32" installieren


Board auswählen: z. B. "ESP32 Dev Module"

Board vi USB anschließen und verbinden



---

**3. Projekt vorbereiten**

Lade den Projektordner herunter (GitHub oder ZIP)

Öffne den Ordner in der Arduino IDE (TaupunktLueftung.ino)

Datei secrets_template.h umbenennen zu secrets.h

Trage dort dein OTA-Passwort ein (wenn gewünscht).



---

**4. Hardware aufbauen & verdrahten**

Sensoren anschließen:

Innen z. B. SHT31 via I2C (GPIO21+22)

Außen z. B. DHT22 (GPIO17) oder ebenfalls SHT31


Relais-Modul an GPIO16

LEDs (Status) an GPIO2, GPIO18, GPIO19

Tipp: Kabellängen für Sensoren möglichst kurz halten (ca. max. 2 m, abgeschirmt besser)



---

**5. Gehäusewahl**

Innen: z. B. Verteilerdose mit Platz für ESP32, Relais, LEDs, ggf. Innensensor

Außen: wettergeschütztes Gehäuse mit Belüftung für DHT22
(z. B. Wetterschutzgehäuse, Insektenschutzgitter)



---

**6. ESP32 verbinden und flashen**

Schließe das Board per USB an

Wähle den richtigen COM-Port unter Werkzeuge → Port

Klicke auf "Hochladen"

Nach dem Flashen → Seriellen Monitor öffnen (115200 Baud) → Status prüfen



---

**7. WLAN einrichten**

Wenn kein WLAN gespeichert oder nicht erreichbar:
→ ESP startet im Access Point-Modus

Verbinde dich mit dem WLAN TaupunktLueftung-Setup

Einfach nach dem WLAN mit der SSID "TaupunktLueftung-Setup" suchen und damit verbinden (oder im Browser öffnen: http://192.168.4.1)

Dein WLAN auswählen und Passwort eingeben

ESP verbindet sich automatisch, speichert die Daten und startet neu



---

**8. Lüfter anschließen**

230 V-Ventilatoren über das Relais schalten

Alternativ: MQTT-gesteuerte Steckdose zur Lüftersteuerung (s. README)



---

**9. Fertig!**

Webinterface aufrufen: Wenn bei der Konfiguration des WLAN der Gerätename nich geändert wurde: "http://TaupunktLueftung.local" ansonsten "http://[neuer_Gerätename].local

Einstellungen wie MQTT, Sensorquellen, Temperaturschutz etc. im Webinterface anpassen

(-> für mqtt autodiscovery beachte [Debug-Hinweise](debug_hinweise.md))

Live-Daten und Charts direkt im Browser

