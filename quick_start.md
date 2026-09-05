## Schnellstart: TaupunktLueftung auf ESP32

**1. Benötigte Hardware**

ESP32 Dev Kit (z. B. dieses [Board](https://amzn.eu/d/gyWvNsA))

Relais, Sensoren, LEDs, Lüfter (siehe Stückliste in README)

---

**2. Arduino IDE vorbereiten**

Installiere die Arduino IDE

ESP32-Unterstützung einrichten:

Datei → Voreinstellungen → Boardverwalter-URL: <https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json>

Dann: Werkzeuge → Board → Boardverwalter → "ESP32" installieren

Board auswählen: z. B. "ESP32 Dev Module"

Board via USB anschließen und verbinden

*Alternative ohne Arduino IDE:* Für den ersten Flash-Vorgang wird zwingend ein Kompiliervorgang benötigt (z. B. für `secrets.h`). Für spätere Updates reicht es, eine der beiden fertigen `.bin`-Dateien (`TaupunktLueftung_dht22.bin` oder `TaupunktLueftung_sht31.bin`) von den [Releases](https://github.com/mallewski/TaupunktLueftung/releases) herunterzuladen und per Webinterface (OTA) einzuspielen.

---

**3. Projekt vorbereiten**

Lade den Projektordner herunter (GitHub oder ZIP)

Öffne den Ordner in der Arduino IDE (`TaupunktLueftung.ino`)

Datei `secrets_template.h` umbenennen zu `secrets.h`

Trage dort ein Passwort ein (`CONFIG_PASSWORD`) – das ist gleichzeitig das Login-Passwort fürs Webinterface (Benutzername `admin`) und wird für den OTA-Firmware-Upload verwendet. Beides lässt sich später im Webinterface unter Einstellungen → Zugang (Login) ändern.

---

**4. Sensortyp für den Außensensor festlegen**

Standardmäßig ist im Quellcode **SHT31** als Außensensor eingestellt. Für **DHT22** die Zeile `#define SENSOR_TYP_AUSSEN_SHT31` in `TaupunktLueftung.ino` mit `//` auskommentieren, bevor du kompilierst und hochlädst.

*Hinweis:* Wenn du stattdessen eine fertige `.bin`-Datei aus den [Releases](https://github.com/mallewski/TaupunktLueftung/releases) nutzt, entfällt dieser Schritt – dort gibt es beide Varianten bereits fertig kompiliert.

---

**5. Hardware aufbauen & verdrahten**

Sensoren anschließen:

- Innen: SHT31 via I²C (GPIO21 + GPIO22)
- Außen: DHT22 (GPIO17) **oder** ein zweiter SHT31 am selben I²C-Bus (GPIO21 + GPIO22), dessen `AD`-/`ADR`-Pin fest auf 3.3 V liegt (Adresse `0x45` statt `0x44`) – siehe README für Details

Relais-Modul an GPIO16

LEDs (Status) an GPIO2, GPIO18, GPIO19

Tipp: Kabellängen für Sensoren möglichst kurz halten (ca. max. 2 m, abgeschirmt besser)

---

**6. Gehäusewahl**

Innen: z. B. Verteilerdose mit Platz für ESP32, Relais, LEDs, ggf. Innensensor

Außen: wettergeschütztes, aber luftdurchlässiges Gehäuse für den Außensensor (z. B. Wetterschutzgehäuse, Insektenschutzgitter)

---

**7. ESP32 verbinden und flashen**

Schließe das Board per USB an

Wähle den richtigen COM-Port unter Werkzeuge → Port

Klicke auf "Hochladen"

Nach dem Flashen → Seriellen Monitor öffnen (115200 Baud) → Status prüfen

---

**8. WLAN einrichten**

Wenn kein WLAN gespeichert oder nicht erreichbar:
→ ESP startet im Access Point-Modus

Verbinde dich mit dem WLAN `TaupunktLueftung-Setup`

Einfach nach dem WLAN mit der SSID "TaupunktLueftung-Setup" suchen und damit verbinden (oder im Browser öffnen: <http://192.168.4.1>)

Dein WLAN auswählen und Passwort eingeben

ESP verbindet sich automatisch, speichert die Daten und startet neu

*Später das WLAN wechseln:* Über den Button „WLAN neu konfigurieren“ in den Einstellungen (Bereich „Gerät“) lassen sich die WLAN-Zugangsdaten gezielt zurücksetzen, ohne alle anderen Einstellungen zu verlieren.

---

**9. Lüfter anschließen**

230 V-Ventilatoren über das Relais schalten

Alternativ: MQTT-gesteuerte Steckdose zur Lüftersteuerung (s. README)

---

**10. Fertig!**

Webinterface aufrufen: Wenn bei der Konfiguration des WLAN der Gerätename nicht geändert wurde: `http://TaupunktLueftung.local`, ansonsten `http://[neuer_Gerätename].local`

Beim ersten Aufruf fragt der Browser nach den Zugangsdaten (Benutzername `admin`, Passwort aus `secrets.h` bei selbst kompilierter Firmware, bzw. `admin123` bei den fertigen Release-Dateien) – änderbar unter Einstellungen → Zugang (Login). Solange die Standard-Zugangsdaten aktiv sind, zeigt das Dashboard einen Warnhinweis an, bis ein eigenes Passwort vergeben wurde.

Einstellungen wie MQTT, Sensorquellen, Temperaturschutz etc. im Webinterface anpassen

Design nach Wunsch auf Hell/Dunkel/System umstellen (Dropdown oben rechts)

(-> für MQTT-Autodiscovery beachte [Debug-Hinweise](https://github.com/mallewski/TaupunktLueftung/blob/main/debug_hinweise.md))

Live-Daten und Charts direkt im Browser
