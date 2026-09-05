# TaupunktLueftung

TaupunktLueftung ist ein ESP32-basiertes System zur intelligenten Lüftungssteuerung auf Grundlage der Taupunktdifferenz. Es ist ideal zum gezielten Trocknen feuchter Räume wie Keller, Waschküchen oder Vorratskammern – effizient und vollautomatisch.

Das System misst Temperatur und Luftfeuchtigkeit innen und außen (über Sensoren wie z. B. SHT31 oder DHT22 – oder per MQTT), berechnet daraus die Taupunkte und aktiviert die Lüftung nur dann, wenn trocknende Bedingungen vorliegen – also wenn die Außenluft in der Lage ist, Feuchtigkeit aufzunehmen, statt sie einzutragen. (Im Feuchte-Regelungs-Modus wird versucht, eine Wunschluftfeuchtigkeit durch Be- oder Entfeuchten zu halten.)

Die Lüftung wird über ein Relais geschaltet – optimalerweise in einem Setup mit zwei Lüftern: einer zieht trockene Luft hinein, der andere führt feuchte Luft ab. Dadurch entsteht ein effektiver Luftstrom zum gezielten Entfeuchten.

-> [Schnell-Start-Anleitung](https://github.com/mallewski/TaupunktLueftung/blob/main/quick_start.md)

## Features

- Taupunktgesteuerte Lüftung zur gezielten Entfeuchtung – ein Algorithmus zur Taupunktanalyse steuert die Lüftung je nach Differenz.
- Die WLAN-Zugangsdaten werden über den Access Point (`TaupunktLueftung-Setup`) im Browser konfiguriert und dauerhaft gespeichert. Kein manuelles Editieren in `secrets.h` mehr nötig.
- Webinterface mit Live-Datenanzeige, Echtzeitdiagrammen (Stunden-, Tages- und Monatsansicht) und Konfigurationsbereich
- **Hell-/Dunkel-Design (Hell/Dunkel/System)**, per Dropdown umschaltbar – die Wahl wird im Browser gespeichert
- **Login-Schutz (HTTP Basic Auth)** fürs gesamte Webinterface, Benutzername und Passwort im laufenden Betrieb änderbar
- Optional einstellbarer Temperaturschutz gegen Auskühlung
- Optional einstellbarer Austrocknungsschutz gegen zu niedrige Luftfeuchtigkeit
- Optionaler Feuchte-Regelungs-Modus, um eine gewünschte Luftfeuchtigkeit zu halten
- **Fail-Safe bei Sensorausfall**: Hält ein Sensorfehler länger an, schaltet die Lüftung automatisch sicherheitshalber ab, statt unbegrenzt im letzten Zustand weiterzulaufen
- Einstellbare Verzögerungszeiten für den Lüfter-/Relaisschutz in Grenzbereichen
- MQTT-Schnittstelle (Publish & Subscribe) + Home Assistant Auto-Discovery
- Modular: wählbare Sensorquelle (Hardware oder MQTT-Daten), umschaltbar direkt im Bereich „Sensorquelle“
- Firmware-Backup: aktuell laufende Firmware lässt sich vor einem Update als `.bin` sichern
- WLAN-Zugangsdaten lassen sich über einen Button im Webinterface zurücksetzen, ohne die übrigen Einstellungen zu verlieren
- Unterstützt Firmware-Updates Over-The-Air (OTA). Nach erstmaligem Flashen via USB aus der Arduino IDE können angepasste Firmwarevarianten aus der Arduino IDE einfach per OTA erfolgen, oder aus einer Firmware-Datei per Webinterface eingespielt werden.

[![Screenshot_dashboard](https://github.com/mallewski/TaupunktLueftung/raw/main/docs/Screenshot_dashboard.png)](/mallewski/TaupunktLueftung/blob/main/docs/Screenshot_dashboard.png)

[![Screenshot_setting1](https://github.com/mallewski/TaupunktLueftung/raw/main/docs/Screenshot_setting1.png)](/mallewski/TaupunktLueftung/blob/main/docs/Screenshot_setting1.png)

[![Screenshot_setting2](https://github.com/mallewski/TaupunktLueftung/raw/main/docs/Screenshot_setting2.png)](/mallewski/TaupunktLueftung/blob/main/docs/Screenshot_setting2.png)

## Benötigte Komponenten und Aufbau

### Hardware-Komponenten

| Komponente                                 | Beschreibung                                            | Ca. Preis (€) |
| ------------------------------------------- | -------------------------------------------------------- | -------------- |
| **ESP32 Dev Board** (z. B. DOIT)            | Mikrocontroller mit WLAN                                  | 6–10 €         |
| **SHT31 Sensor (Innen, ggf. auch Außen)**   | Temperatur & Luftfeuchte, I²C, hochgenau                  | 5–8 € / Stück  |
| **DHT22 Sensor (Außen, Alternative)**       | Temperatur & Luftfeuchte, digitaler Single-Wire, günstiger | 3–5 €          |
| **LEDs (3×)** + Vorwiderstände (220–470 Ω)  | Statusanzeigen: grün, gelb, rot                           | 1–2 €          |
| **Relais-Modul (1 Kanal)**                  | Zur Ansteuerung der Lüftung                               | 2–4 €          |
| **Jumper-Kabel / Breadboard**               | Für Aufbau und Tests                                      | 2–5 €          |
| **Gehäuse (optional)**                      | Schutz für ESP32 und Verkabelung                          | 3–8 €          |

**Gesamtkosten:** ca. **20–30 €**, je nach Ausstattung und Bezugsquelle.

### Aufbau und Verdrahtung

**ESP32-Pinbelegung:**

| ESP32-Pin | Funktion             | Angeschlossen an                                      |
| --------- | --------------------- | ------------------------------------------------------- |
| GPIO17    | DHT22 Datenleitung     | DHT22 (mit 10 kΩ Pull-Up) auf GND – nur bei DHT22-Variante |
| GPIO16    | Relais                 | Relais IN                                                |
| GPIO2     | LED grün               | Vorwiderstand + LED                                      |
| GPIO18    | LED rot                | Vorwiderstand + LED                                      |
| GPIO19    | LED gelb               | Vorwiderstand + LED                                      |
| GPIO21    | SDA (I²C-Daten)        | SHT31 innen **und** SHT31 außen (gemeinsamer Bus)        |
| GPIO22    | SCL (I²C-Takt)         | SHT31 innen **und** SHT31 außen (gemeinsamer Bus)        |
| 3.3 V/GND | Stromversorgung        | Alle Komponenten                                         |

### Vereinfachter ASCII-Schaltplan

```
+----------------------------+
|         ESP32 Dev          |
|                            |
|  GPIO17 --> DHT22 data     |  (nur DHT22-Variante)
|  GPIO16 --> Relais IN      |
|  GPIO2  --> LED grün       |
|  GPIO18 --> LED rot        |
|  GPIO19 --> LED gelb       |
|  GPIO21 --> SDA (SHT31)    |
|  GPIO22 --> SCL (SHT31)    |
+-------------+--------------+
              |
           3.3 V / GND
```

LEDs: Anode (langer Pin) → Vorwiderstand → GPIO / Kathode (kurzer Pin) → GND

### Auswahl des Außensensors

Dieses Projekt unterstützt zwei Sensortypen für den Außensensor:

- **SHT31 (empfohlen)** – digitaler I²C-Sensor, deutlich genauer und stabiler als der DHT22 (typisch ±2 % RH statt ±2–5 % RH beim DHT22), reagiert schneller auf Änderungen, liefert auch bei niedrigen Temperaturen zuverlässige Werte und hat mit I²C ein robusteres, weniger störanfälliges Übertragungsprotokoll als das proprietäre Single-Wire-Timing des DHT22. Für eine Regelung, die auf kleinen Taupunkt-Differenzen basiert, wirkt sich die höhere Sensorgenauigkeit direkt auf die Qualität der Lüftungsentscheidungen aus.
- **DHT22** – günstiger, weniger genau, dafür simplere 3-Draht-Verkabelung ohne I²C-Adressierung.

**Besonderheit bei zwei SHT31-Sensoren:** Da sowohl der Innen- als auch ein optionaler Außen-SHT31 über I²C angesprochen werden, können beide Sensoren am **gemeinsamen Bus** (SDA = GPIO21, SCL = GPIO22) betrieben werden – es sind keine zusätzlichen Datenleitungen nötig. Damit der ESP32 die beiden Sensoren trotzdem unterscheiden kann, besitzt der SHT31 einen `AD`- bzw. `ADR`-Pin zur Adressauswahl:

| Sensor        | ADDR/AD-Pin       | Resultierende I²C-Adresse |
| ------------- | ------------------ | --------------------------- |
| SHT31 innen   | offen oder auf GND | `0x44` (Standard)            |
| SHT31 außen   | fest auf **3.3 V**  | `0x45`                       |

*(Manche SHT31-Module haben zusätzlich einen `AL`/`ALR`-Pin – das ist ein Alarm-/Interrupt-Ausgang und hat mit der Adresswahl nichts zu tun; er bleibt unbeschaltet.)*

**Sensortyp für den Außensensor festlegen:** Anders als Modus innen/außen (Hardware oder MQTT), der sich im Webinterface einstellen lässt, ist die Wahl zwischen SHT31 und DHT22 für den Außensensor bewusst eine **Compile-Zeit-Entscheidung** direkt im Quellcode – dadurch wird beim Kompilieren jeweils nur die tatsächlich benötigte Sensor-Bibliothek eingebunden, was Flash-Speicher spart und unnötige I²C-Kommunikation mit einem eventuell gar nicht vorhandenen zweiten Sensor vermeidet.

So stellst du den Sensortyp um:

1. Öffne die `TaupunktLueftung.ino`.
2. Suche die Zeile `//#define SENSOR_TYP_AUSSEN_SHT31`.
3. Für **SHT31 außen** (Standardeinstellung im Repository): Zeile aktiv (nicht auskommentiert) lassen.
4. Für **DHT22 außen**: Kommentarzeichen `//` wieder voranstellen und sicherstellen, dass der DHT22 an Pin 17 angeschlossen ist.

**Fertige Firmware-Varianten ohne selbst zu kompilieren:** Jedes [Release](https://github.com/mallewski/TaupunktLueftung/releases) auf GitHub enthält automatisch **zwei fertige `.bin`-Dateien** – `TaupunktLueftung_dht22.bin` und `TaupunktLueftung_sht31.bin`. Wähle beim OTA-Update im Webinterface einfach die zu deiner Hardware passende Datei, ganz ohne eigene Arduino-IDE-Installation.

### Hinweise

- **Relais-Modul:** Kann zum Schalten einer 230 V-Lüftung verwendet werden. **Achtung:** Netzspannung nur durch Fachpersonal anschließen lassen.
- **MQTT:** Zum Empfangen externer Sensordaten wird ein MQTT-Broker benötigt (z. B. Mosquitto oder Home Assistant). Mit Auto-Discovery (Home Assistant kompatibel).
- **Webinterface:** Alle Einstellungen wie MQTT, Sensorquellen und Schwellenwerte sind direkt über das Browser-Interface konfigurierbar, geschützt durch einen Login (Benutzername/Passwort).
- **Standard-Zugangsdaten nach dem ersten Flash:** Benutzername `admin`, Passwort aus `secrets.h` (`CONFIG_PASSWORD`). Beides lässt sich anschließend im Webinterface unter Einstellungen → Zugang (Login) ändern.
- **Außensensor wettergeschützt montieren**, z. B. unter einem Vordach oder in einem geeigneten, aber luftdurchlässigen Gehäuse – unabhängig davon, ob DHT22 oder SHT31 verwendet wird.

[![Beispielaufbau_1](https://github.com/mallewski/TaupunktLueftung/raw/main/docs/1747229085390.jpg)](/mallewski/TaupunktLueftung/blob/main/docs/1747229085390.jpg)

[![Beispielaufbau_2](https://github.com/mallewski/TaupunktLueftung/raw/main/docs/1747229085406.jpg)](/mallewski/TaupunktLueftung/blob/main/docs/1747229085406.jpg)

[![Beispielaufbau_Sensor_innen](https://github.com/mallewski/TaupunktLueftung/raw/main/docs/Sensor_Innen.jpg)](/mallewski/TaupunktLueftung/blob/main/docs/Sensor_Innen.jpg)

### Lüfterempfehlung

Für eine effektive Kellerlüftung sind leistungsstarke Ventilatoren erforderlich. Normale PC-Lüfter reichen in der Regel nicht aus.

**Empfehlung:**

- Verwende **Wandventilatoren** oder **Rohrventilatoren**, z. B. aus dem Bereich der Bad- oder Kellerlüftung
- Achte auf ausreichenden **Volumenstrom** (mind. 100–150 m³/h für kleine Kellerräume)
- Ideal: **Abluftventilator** mit Rückschlagklappe
- Optional: **Zuluftventilator** oder passive Zuluftöffnung (mit Insektenschutzgitter)
- Alternativ zum Relaisausgang kann die Lüftung auch über eine per MQTT schaltbare Steckdose gesteuert werden – z. B. eine WLAN-Steckdose, die über ein Smart-Home-System (wie Home Assistant) eingebunden ist.

| Komponente                | Preis (ca.) | Hinweis                                                     |
| -------------------------- | ------------ | ------------------------------------------------------------ |
| Rohrventilator 100 mm       | 25–40 €      | z. B. VENTS, Maico, oder günstige Modelle aus dem Baumarkt   |
| Rückschlagklappe            | 5–10 €       | verhindert Rückströmung                                      |
| Wandgitter / Tellerventil   | 5–10 €       | schützt Öffnungen vor Schmutz und Tieren                     |

**Montagehinweise:**

- Abluft sollte möglichst **nah an der Kellerdecke** montiert werden, wo sich warme, feuchte Luft sammelt
- Zuluftöffnung idealerweise **bodennah gegenüberliegend**, um einen sinnvollen Luftstrom durch den Raum zu erzeugen
- Abluft nach draußen führen – **nicht** in andere Räume
- Zuluft idealerweise aus einem trockeneren Raum oder über eine Außenöffnung mit Insektenschutzgitter
- Mehrere Ventilatoren (z. B. ein Zuluft- und ein Abluftventilator) können **parallel am gleichen Relaisausgang** betrieben werden, sofern die Gesamtlast das Relais nicht übersteigt (max. ca. 2 A bei 230 V bei handelsüblichen Relaismodulen)
- Bei höheren Lasten ggf. **ein externes Leistungsrelais oder Schütz** verwenden
- **Spannungsversorgung des ESP32** nicht am selben Stromkreis wie eine große Motorlast betreiben, sofern vermeidbar – beim Schalten induktiver Lasten (Lüftermotoren) können kurze Netzstörungen über einfache AC/DC-Module (z. B. HLK-PM01) auf die 5V-Versorgung durchschlagen und den ESP32 per Brownout-Reset neu starten lassen. Abhilfe schafft ein zusätzlicher Pufferkondensator direkt an der Versorgung des ESP32 (5V bzw. 3.3V gegen GND, so nah wie möglich an den Versorgungspins), am wirksamsten als Kombination aus zwei Kondensatoren parallel: ein Elektrolytkondensator (470–1000 µF) gegen kurze Spannungseinbrüche sowie zusätzlich ein Keramikkondensator (100 nF) gegen kurze, hochfrequente Störspitzen – der Elko allein fängt die hochfrequenten Anteile nur unzureichend ab.

### Home Assistant Integration (MQTT)

- Unterstützt Auto-Discovery
- MQTT Topics sind vollständig konfigurierbar im Webinterface
- Verfügbarkeitsstatus über `availability_topic`
- Bestehende Home-Assistant-Sensoren lassen sich per Automation als Sensorquelle einspeisen (siehe [Anleitung: HA-Sensoren via MQTT](https://github.com/mallewski/TaupunktLueftung/blob/main/docs/Anleitung_HA_Sensoren_via_MQTT.md))

**Hinweis:** Falls MQTT Discovery nicht funktioniert, siehe [Debug-Hinweise](https://github.com/mallewski/TaupunktLueftung/blob/main/debug_hinweise.md)

## Firmware selbst bauen (GitHub Actions)

Bei jedem veröffentlichten [Release](https://github.com/mallewski/TaupunktLueftung/releases) baut eine GitHub Action automatisch beide Firmware-Varianten (DHT22 und SHT31) und hängt sie als `.bin`-Dateien an – ganz ohne lokale Arduino-IDE-Installation. Wer selbst Änderungen am Quellcode testen möchte, kompiliert weiterhin lokal über die Arduino IDE mit den unter [Schnellstart](https://github.com/mallewski/TaupunktLueftung/blob/main/quick_start.md) beschriebenen Bibliotheken.

## Unterstütze das Projekt

Wenn dir dieses Projekt gefällt oder du es nützlich findest, kannst du mir einen Kaffee spendieren:

[![Buy Me A Coffee](https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png)](https://www.buymeacoffee.com/mallewski)

Made with ESP32 and Love!
