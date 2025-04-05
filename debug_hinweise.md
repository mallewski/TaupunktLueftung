## Hinweis zur MQTT Discovery:

Standardmäßig ist die maximale Paketgröße in der verwendeten MQTT-Bibliothek PubSubClient auf 128 Bytes beschränkt. Das ist zu wenig für umfangreiche MQTT Discovery Payloads – sie werden dann nicht gesendet. Es erscheint auch kein Fehler.

Lösung:
In der Datei 'PubSubClient.h' den Wert von 'MQTT_MAX_PACKET_SIZE' erhöhen, z. B. auf 1024:

'''#define MQTT_MAX_PACKET_SIZE 1024'''

Dateipfad:
Je nach IDE und System findest du die Datei hier:
- Arduino IDE: '<Benutzerverzeichnis>/Arduino/libraries/PubSubClient/src/PubSubClient.h'
- PlatformIO: '.pio/libdeps/.../PubSubClient/src/PubSubClient.h'

## Keine WLAN-Verbindung möglich

Problem: Das Gerät verbindet sich nicht mit dem WLAN.
- Stelle sicher, dass die SSID und das Passwort korrekt im Code (secrets.h) hinterlegt sind.
- Achte darauf, dass dein Router 2.4 GHz unterstützt – ESP32 unterstützt kein 5 GHz-WLAN.
- Aktiviere ggf. den seriellen Monitor (115200 Baud), um mehr Details zu sehen.
