## Hinweis zur MQTT Discovery

Ab Firmware v4.2 ruft der Sketch beim Start `mqttClient.setBufferSize(1024)` auf – das behebt das früher hier beschriebene Problem (Discovery-Nachrichten wurden bei zu kleiner Standard-Paketgröße der Bibliothek PubSubClient stillschweigend nicht gesendet) direkt im eigenen Code. **Der manuelle Eingriff in `PubSubClient.h` ist damit nicht mehr nötig.**

Falls du eine ältere Firmware-Version (vor v4.2) einsetzt, gilt weiterhin:

Standardmäßig ist die maximale Paketgröße in der Bibliothek PubSubClient auf 256 Bytes beschränkt. Das ist zu wenig für umfangreiche MQTT-Discovery-Payloads – sie werden dann nicht gesendet, ohne dass ein Fehler erscheint.

Lösung (nur für Firmware-Versionen vor v4.2 nötig):
In der Datei `PubSubClient.h` den Wert von `MQTT_MAX_PACKET_SIZE` erhöhen, z. B. auf 1024:

`#define MQTT_MAX_PACKET_SIZE 1024`

Dateipfad:
Je nach IDE und System findest du die Datei hier:

- Arduino IDE: `<Benutzerverzeichnis>/Arduino/libraries/PubSubClient/src/PubSubClient.h`
- PlatformIO: `.pio/libdeps/.../PubSubClient/src/PubSubClient.h`

Empfehlung: Auf v4.2 oder neuer aktualisieren, dann entfällt dieser manuelle Schritt dauerhaft.

## Webinterface zeigt nach einem Firmware-Update noch die alte Oberfläche / fehlende Funktionen

Ab v4.2 werden CSS (`/style.css`) und JavaScript (`/script.js`) vom Browser eine Stunde lang zwischengespeichert (`Cache-Control: max-age=3600`), um Ladezeit und Datenmenge bei jedem Seitenaufruf zu reduzieren. Ein Cache-Buster (`?v=<Firmware-Version>`) sorgt normalerweise dafür, dass der Browser nach einem Update automatisch die aktuelle Version lädt.

**Falls du von einer Firmware-Version vor v4.2 aktualisierst**, kann es einmalig vorkommen, dass der Browser trotzdem noch alte, zwischengespeicherte Inhalte anzeigt (z. B. fehlende neue Buttons, altes Design trotz Update). In diesem Fall hilft:

- Ein Hard-Refresh im Browser (meist `Strg+Shift+R` bzw. `Cmd+Shift+R` auf dem Desktop)
- Auf dem Smartphone: Browser-Cache für die Seite gezielt leeren, oder die Seite einmal im privaten/Inkognito-Tab öffnen, um zu prüfen, ob das Problem dadurch verschwindet

Ab v4.2 sollte das bei künftigen Updates nicht mehr nötig sein, da der eingebaute Cache-Buster das automatisch erledigt.

## Webinterface reagiert manchmal spürbar langsam oder verzögert

Der ESP32-WLAN-Treiber aktiviert den Modem-Sleep-Modus (Stromsparmodus der WLAN-Funkeinheit) standardmäßig bei **jedem** Verbindungsaufbau erneut – nicht nur beim ersten Verbinden, sondern auch bei jeder automatischen Wiederverbindung im laufenden Betrieb (z. B. nach einem kurzen WLAN-Aussetzer bei schwachem Signal). Im Modem-Sleep-Modus "döst" die Funkeinheit zwischen den WLAN-Beacon-Intervallen, was eingehende HTTP-Anfragen spürbar verzögern kann.

Ab v4.2 setzt ein Event-Handler (`onWifiVerbunden`) `WiFi.setSleep(false)` bei jeder erfolgreichen Verbindung erneut, statt sich nur auf den einmaligen Aufruf direkt nach dem Setup zu verlassen. Falls du eine ältere Firmware-Version einsetzt und das Interface nach WLAN-Aussetzern spürbar träger reagiert, ist das eine wahrscheinliche Ursache – ein Update auf v4.2 oder neuer behebt das dauerhaft.

## Webinterface fragt ständig nach Benutzername/Passwort

Das Webinterface ist per HTTP Basic Auth geschützt. Standard-Zugangsdaten nach dem ersten Flashen: Benutzername `admin`, Passwort wie in `secrets.h` unter `CONFIG_PASSWORD` hinterlegt – bei den fertigen Release-`.bin`-Dateien ist das `admin123`. Solange diese Standard-Zugangsdaten aktiv sind, zeigt das Dashboard automatisch einen roten Warnhinweis an. **Dieser Hinweis sollte ernst genommen werden** – bitte zeitnah ein eigenes Passwort unter Einstellungen → Zugang (Login) vergeben.

- Der Browser fragt beim ersten Aufruf einmalig danach und merkt sich die Zugangsdaten anschließend für die laufende Sitzung.
- Nach einer Änderung von Benutzername oder Passwort über die Einstellungsseite lädt die Seite automatisch neu und der Browser fragt sofort erneut – das ist erwartetes Verhalten, kein Fehler.
- Falls du Benutzername/Passwort vergessen hast, hilft nur ein Zurücksetzen über `DEBUG_ERASE_NVS` (siehe unten) – dabei gehen allerdings **alle** gespeicherten Einstellungen verloren, nicht nur der Zugang.

## Keine WLAN-Verbindung möglich

Problem: Das Gerät verbindet sich nicht mit dem WLAN.

- Achte darauf, dass dein Router 2.4 GHz unterstützt – ESP32 unterstützt kein 5 GHz-WLAN.
- Aktiviere ggf. den seriellen Monitor (115200 Baud), um mehr Details zu sehen.
- Über den Button „WLAN neu konfigurieren“ in den Einstellungen (Bereich „Gerät“) lassen sich die gespeicherten WLAN-Zugangsdaten gezielt löschen, ohne die übrigen Einstellungen zu verlieren – das Gerät öffnet danach automatisch wieder den Setup-Access-Point.

## Sensorfehler / NaN-Werte trotz angeschlossenem SHT31

- Prüfe, ob der `AD`-/`ADR`-Pin des Außensensors wirklich fest auf 3.3 V liegt (Adresse `0x45`) – offen oder auf GND ergibt `0x44` und kollidiert mit dem Innensensor.
- Prüfe im seriellen Monitor, ob `SHT31 außen Re-Init erfolgreich` oder `fehlgeschlagen` erscheint – „fehlgeschlagen“ deutet auf ein Verkabelungs-/Adressproblem hin, kein Software-Fehler.
- Stelle sicher, dass im Quellcode `#define SENSOR_TYP_AUSSEN_SHT31` aktiv (nicht auskommentiert) ist, bzw. dass du die passende Release-Datei (`TaupunktLueftung_sht31.bin`) geflasht hast.

## Flash/NVS vollständig löschen

Wenn du die Firmware neu aufsetzt oder Probleme mit alten Einstellungen hast, kannst du den Flash-Speicher (NVS) komplett zurücksetzen.

### Schritt-für-Schritt-Anleitung

1. Öffne die Datei `TaupunktLueftung.ino`.
2. Suche am Anfang des Sketches folgende Zeile:

   ```
   //#define DEBUG_ERASE_NVS
   ```

3. Entferne die Kommentarzeichen, sodass die Zeile wie folgt aussieht:

   ```
   #define DEBUG_ERASE_NVS
   ```

4. Lade den Sketch auf das Gerät hoch. Beim nächsten Start wird **der gesamte NVS-Speicher gelöscht**, einschließlich:
   - WLAN-Zugangsdaten
   - MQTT- und Sensoreinstellungen
   - Feuchte-/Temperatur-Schwellwerte
   - Hostname
   - Webinterface-Benutzername und -Passwort (fallen zurück auf die Defaults `admin` / `CONFIG_PASSWORD` aus `secrets.h`)

5. Sobald das Gerät neugestartet ist, wird automatisch der Setup-Modus aktiviert und ein neuer WLAN-Access-Point (`TaupunktLueftung-Setup`) geöffnet.

6. **Wichtig:** Kommentiere die Zeile danach wieder aus, um versehentliches Löschen beim nächsten Start zu vermeiden:

   ```
   //#define DEBUG_ERASE_NVS
   ```

   Danach das Sketch noch einmal erneut auf das Gerät hochladen.

⚠️ **Achtung:** Dieser Vorgang ist irreversibel – alle gespeicherten Daten gehen verloren.## Hinweis zur MQTT Discovery

Standardmäßig ist die maximale Paketgröße in der verwendeten MQTT-Bibliothek PubSubClient auf 128 Bytes beschränkt. Das ist zu wenig für umfangreiche MQTT Discovery Payloads – sie werden dann nicht gesendet. Es erscheint auch kein Fehler.

Lösung:
In der Datei `PubSubClient.h` den Wert von `MQTT_MAX_PACKET_SIZE` erhöhen, z. B. auf 1024:

`#define MQTT_MAX_PACKET_SIZE 1024`

Dateipfad:
Je nach IDE und System findest du die Datei hier:

- Arduino IDE: `<Benutzerverzeichnis>/Arduino/libraries/PubSubClient/src/PubSubClient.h`
- PlatformIO: `.pio/libdeps/.../PubSubClient/src/PubSubClient.h`

## Webinterface fragt ständig nach Benutzername/Passwort

Das Webinterface ist per HTTP Basic Auth geschützt. Standard-Zugangsdaten nach dem ersten Flashen: Benutzername `admin`, Passwort wie in `secrets.h` unter `CONFIG_PASSWORD` hinterlegt – bei den fertigen Release-`.bin`-Dateien ist das `admin123`. Solange diese Standard-Zugangsdaten aktiv sind, zeigt das Dashboard automatisch einen roten Warnhinweis an. **Dieser Hinweis sollte ernst genommen werden** – bitte zeitnah ein eigenes Passwort unter Einstellungen → Zugang (Login) vergeben.

- Der Browser fragt beim ersten Aufruf einmalig danach und merkt sich die Zugangsdaten anschließend für die laufende Sitzung.
- Nach einer Änderung von Benutzername oder Passwort über die Einstellungsseite lädt die Seite automatisch neu und der Browser fragt sofort erneut – das ist erwartetes Verhalten, kein Fehler.
- Falls du Benutzername/Passwort vergessen hast, hilft nur ein Zurücksetzen über `DEBUG_ERASE_NVS` (siehe unten) – dabei gehen allerdings **alle** gespeicherten Einstellungen verloren, nicht nur der Zugang.

## Keine WLAN-Verbindung möglich

Problem: Das Gerät verbindet sich nicht mit dem WLAN.

- Achte darauf, dass dein Router 2.4 GHz unterstützt – ESP32 unterstützt kein 5 GHz-WLAN.
- Aktiviere ggf. den seriellen Monitor (115200 Baud), um mehr Details zu sehen.
- Über den Button „WLAN neu konfigurieren“ in den Einstellungen (Bereich „Gerät“) lassen sich die gespeicherten WLAN-Zugangsdaten gezielt löschen, ohne die übrigen Einstellungen zu verlieren – das Gerät öffnet danach automatisch wieder den Setup-Access-Point.

## Sensorfehler / NaN-Werte trotz angeschlossenem SHT31

- Prüfe, ob der `AD`-/`ADR`-Pin des Außensensors wirklich fest auf 3.3 V liegt (Adresse `0x45`) – offen oder auf GND ergibt `0x44` und kollidiert mit dem Innensensor.
- Prüfe im seriellen Monitor, ob `SHT31 außen Re-Init erfolgreich` oder `fehlgeschlagen` erscheint – „fehlgeschlagen“ deutet auf ein Verkabelungs-/Adressproblem hin, kein Software-Fehler.
- Stelle sicher, dass im Quellcode `#define SENSOR_TYP_AUSSEN_SHT31` aktiv (nicht auskommentiert) ist, bzw. dass du die passende Release-Datei (`TaupunktLueftung_sht31.bin`) geflasht hast.
