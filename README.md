***TaupunktLueftung***

TaupunktLueftung ist ein ESP32-basiertes System zur intelligenten Lüftungssteuerung auf Grundlage der Taupunktdifferenz. Es ist ideal zum gezielten Trocknen feuchter Räume wie Keller, Waschküchen oder Vorratskammern – effizient und vollautomatisch.

Das System misst Temperatur und Luftfeuchtigkeit innen und außen (über Sensoren wie z. B. SHT31 oder DHT22 - oder per MQTT), berechnet daraus die Taupunkte und aktiviert die Lüftung nur dann, wenn trocknende Bedingungen vorliegen – also wenn die Außenluft in der Lage ist, Feuchtigkeit aufzunehmen, statt sie einzutragen.

Die Lüftung wird über ein Relais geschaltet – optimalerweise in einem Setup mit zwei Lüftern: einer zieht trockene Luft hinein, der andere führt feuchte Luft ab. Dadurch entsteht ein effektiver Luftstrom zum gezielten Entfeuchten.

**Features:**

- Steuerung über Webinterface mit Live-Datenanzeige und Echtzeitdiagrammen

- Optionaler Zugriff und Integration via MQTT

- Modularer Aufbau: interne Sensoren oder externe MQTT-Daten

- Algorithmus zur Taupunktanalyse steuert die Lüftung je nach Differenz

- Unterstützt Firmware-Updates über das Webinterface (OTA)