## Hinweis zur MQTT Discovery:

Standardmäßig ist die maximale Paketgröße in der verwendeten MQTT-Bibliothek PubSubClient auf 128 Bytes beschränkt. Das ist zu wenig für umfangreiche MQTT Discovery Payloads – sie werden dann nicht gesendet. Es erscheint auch kein Fehler.

Lösung:
In der Datei `PubSubClient.h` den Wert von `MQTT_MAX_PACKET_SIZE` erhöhen, z. B. auf 1024:

`#define MQTT_MAX_PACKET_SIZE 1024`

Dateipfad:
Je nach IDE und System findest du die Datei hier:
- Arduino IDE: `<Benutzerverzeichnis>/Arduino/libraries/PubSubClient/src/PubSubClient.h`
- PlatformIO: `.pio/libdeps/.../PubSubClient/src/PubSubClient.h`

## Keine WLAN-Verbindung möglich

Problem: Das Gerät verbindet sich nicht mit dem WLAN.
- Achte darauf, dass dein Router 2.4 GHz unterstützt – ESP32 unterstützt kein 5 GHz-WLAN.
- Aktiviere ggf. den seriellen Monitor (115200 Baud), um mehr Details zu sehen.

## Flash/NVS vollständig löschen

Wenn du die Firmware neu aufsetzt oder Probleme mit alten Einstellungen hast, kannst du den Flash-Speicher (NVS) komplett zurücksetzen.

### Schritt-für-Schritt-Anleitung

1. Öffne die Datei `TaupunktLueftung.ino`.
2. Suche am Anfang des Sketches folgende Zeile:

    ```cpp
    //#define DEBUG_ERASE_NVS
    ```

3. Entferne die Kommentarzeichen, sodass die Zeile wie folgt aussieht:

    ```cpp
    #define DEBUG_ERASE_NVS
    ```

4. Lade den Sketch auf das Gerät hoch. Beim nächsten Start wird **der gesamte NVS-Speicher gelöscht**, einschließlich:

    - WLAN-Zugangsdaten
    - MQTT- und Sensoreinstellungen
    - Feuchte-/Temperatur-Schwellwerte
    - Hostname

5. Sobald das Gerät neugestartet ist, wird automatisch der Setup-Modus aktiviert und ein neuer WLAN-Access-Point (`TaupunktLueftung-Setup`) geöffnet.

6. **Wichtig:** Kommentiere die Zeile danach wieder aus, um versehentliches Löschen beim nächsten Start zu vermeiden:

    ```cpp
    //#define DEBUG_ERASE_NVS
    ```
    Danach das Sketch nocheinmal erneut auf das Gerät hochladen.

⚠️ **Achtung:** Dieser Vorgang ist irreversibel – alle gespeicherten Daten gehen verloren.
