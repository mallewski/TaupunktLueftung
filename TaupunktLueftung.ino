// TaupunktLueftung v2.0
// Vollständige Version mit Chart-Update via AJAX, MQTT-Setup, LED-Steuerung, Webinterface

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_SHT31.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <time.h>
#include <Update.h>
#include "secrets.h"

//Parameter
#define NAME "TaupunktLueftung"
#define FIRMWARE_VERSION "v2.0"
#define RELAY_LED_PIN 16
#define STATUS_GREEN_PIN 2
#define STATUS_RED_PIN 18
#define STATUS_YELLOW_PIN 19
#define DHTPIN 17
#define DHTTYPE DHT22
#define MAX_POINTS 720
#define SENSORZYKLUS_MS 5000

//Debugging
bool debugMQTT = false; // Debug für MQTT Discovery

Preferences prefs;
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Adafruit_SHT31 shtInnen = Adafruit_SHT31();
DHT dht(DHTPIN, DHTTYPE);

char ssid[32] = WIFI_SSID;
char password[64] = WIFI_PASSWORD;

const char* configPassword = CONFIG_PASSWORD;

char mqttServer[64] = MQTT_SERVER;
int mqttPort = MQTT_PORT;
char mqttUser[32] = MQTT_USER;
char mqttPassword[64] = MQTT_PASSWORD;

String mqttTempInnen = "sensors/temp_innen";
String mqttHygroInnen = "sensors/hygro_innen";
String mqttTempAussen = "sensors/temp_aussen";
String mqttHygroAussen = "sensors/hygro_aussen";
String mqttPublishPrefix = "taupunktlueftung/";
String mqttDiscoveryPrefix = "homeassistant/";

bool mqttAktiv = true;
String modus_innen = "hardware";
String modus_aussen = "hardware";
bool updateModeActive = false;
bool schutzVorAuskuehlungAktiv = false;
float minTempInnen = 12.0; // °C – Beispielwert
float taupunktDifferenzSchwellwert = 4.0;

float t_in = NAN, rh_in = NAN, td_in = NAN;
float t_out = NAN, rh_out = NAN, td_out = NAN;

float mqtt_t_in = NAN, mqtt_rh_in = NAN;
float mqtt_t_out = NAN, mqtt_rh_out = NAN;

float td_in_history[MAX_POINTS];
float td_out_history[MAX_POINTS];
float td_diff_history[MAX_POINTS];
float rh_in_history[MAX_POINTS];
float rh_out_history[MAX_POINTS];
bool status_history[MAX_POINTS]; 
int history_index = 0;

String statusText = "Unbekannt";
bool lueftungAktiv = false;
String logEintrag = "";
String letzteUhrzeit = "--:--:--";

String getUhrzeit() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--:--:--";
  char buf[20];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

void logEvent(String msg) {
  letzteUhrzeit = getUhrzeit();
  logEintrag = letzteUhrzeit + " - " + msg;
  Serial.println(logEintrag);
}

void setLEDs(bool gruen, bool rot, bool gelb) {
  digitalWrite(STATUS_GREEN_PIN, gruen);
  digitalWrite(STATUS_RED_PIN, rot);
  digitalWrite(STATUS_YELLOW_PIN, gelb);
}

//Taupunktberechnung
float berechneTaupunkt(float T, float RH) {
  float a = (T >= 0) ? 7.5 : 7.6;
  float b = (T >= 0) ? 237.3 : 240.7;
  float sdd = 6.1078 * pow(10, (a * T) / (b + T));
  float dd = sdd * RH / 100.0;
  float v = log10(dd / 6.1078);
  return (b * v) / (a - v);
}

void aktualisiereSensoren() {
  t_in = (modus_innen == "mqtt" && mqttAktiv) ? mqtt_t_in : shtInnen.readTemperature();
  rh_in = (modus_innen == "mqtt" && mqttAktiv) ? mqtt_rh_in : shtInnen.readHumidity();
  t_out = (modus_aussen == "mqtt" && mqttAktiv) ? mqtt_t_out : dht.readTemperature();
  rh_out = (modus_aussen == "mqtt" && mqttAktiv) ? mqtt_rh_out : dht.readHumidity();

  if (!isnan(t_in) && !isnan(rh_in)) td_in = berechneTaupunkt(t_in, rh_in);
  if (!isnan(t_out) && !isnan(rh_out)) td_out = berechneTaupunkt(t_out, rh_out);

  publishAllStates();
}

void steuerlogik() {
  if (isnan(td_in) || isnan(td_out)) return;
  float diff = td_in - td_out;

  if (diff >= taupunktDifferenzSchwellwert) {
    if (!lueftungAktiv) {
      digitalWrite(RELAY_LED_PIN, HIGH);
      lueftungAktiv = true;
      logEvent("Lüftung aktiviert");
    }
    setLEDs(true, false, false);
    statusText = "Trocknend - Lüftung aktiv";
  } else {
    if (lueftungAktiv) {
      digitalWrite(RELAY_LED_PIN, LOW);
      lueftungAktiv = false;
      logEvent("Lüftung deaktiviert");
    }
    if (diff <= -taupunktDifferenzSchwellwert) {
      setLEDs(false, true, false);
      statusText = "Befeuchtend - Lüftung aus";
    } else {
      setLEDs(false, false, true);
      statusText = "Neutral - Lüftung aus";
    }
    publishAllStates();
  }

  td_in_history[history_index] = td_in;
  td_out_history[history_index] = td_out;
  td_diff_history[history_index] = diff;
  rh_in_history[history_index] = rh_in;
  rh_out_history[history_index] = rh_out;
  status_history[history_index] = lueftungAktiv;
  history_index = (history_index + 1) % MAX_POINTS;
}

//MQTT Empfang
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String val;
  for (unsigned int i = 0; i < length; i++) val += (char)payload[i];
  float fval = val.toFloat();
  if (String(topic) == mqttTempInnen) mqtt_t_in = fval;
  if (String(topic) == mqttHygroInnen) mqtt_rh_in = fval;
  if (String(topic) == mqttTempAussen) mqtt_t_out = fval;
  if (String(topic) == mqttHygroAussen) mqtt_rh_out = fval;
}

//MQTT Senden
void publishAllStates() {
  if (!mqttAktiv || !mqttClient.connected()) return;

  // Innen
  mqttClient.publish((mqttPublishPrefix + "temp_innen").c_str(), String(t_in, 1).c_str(), true);
  mqttClient.publish((mqttPublishPrefix + "hygro_innen").c_str(), String(rh_in, 1).c_str(), true);
  mqttClient.publish((mqttPublishPrefix + "taupunkt_innen").c_str(), String(td_in, 1).c_str(), true);

  // Außen
  mqttClient.publish((mqttPublishPrefix + "temp_aussen").c_str(), String(t_out, 1).c_str(), true);
  mqttClient.publish((mqttPublishPrefix + "hygro_aussen").c_str(), String(rh_out, 1).c_str(), true);
  mqttClient.publish((mqttPublishPrefix + "taupunkt_aussen").c_str(), String(td_out, 1).c_str(), true);

  // Differenz
  mqttClient.publish((mqttPublishPrefix + "diff").c_str(), String(td_in - td_out, 1).c_str(), true);

  // Lüftung an/aus
  mqttClient.publish((mqttPublishPrefix + "status").c_str(), lueftungAktiv ? "1" : "0", true);

  // Availability (optional bei reconnect)
  mqttClient.publish((mqttPublishPrefix + "availability").c_str(), "online", true);
}

void publishMQTTDiscovery() {
  if (!mqttClient.connected()) return;
  String deviceID = NAME; // einheitlich für alle Sensoren

  struct Sensor {
    String id;
    String name;
    String unit;
    String devclass;
    String topic;
    bool binary; // true für binary_sensor
  };

  Sensor sensoren[] = {
    {"tin", "Temperatur Innen", "°C", "temperature", mqttPublishPrefix + "temp_innen", false},
    {"hin", "Luftfeuchte Innen", "%", "humidity", mqttPublishPrefix + "hygro_innen", false},
    {"tdin", "Taupunkt Innen", "°C", "temperature", mqttPublishPrefix + "taupunkt_innen", false},

    {"tout", "Temperatur Außen", "°C", "temperature", mqttPublishPrefix + "temp_aussen", false},
    {"hout", "Luftfeuchte Außen", "%", "humidity", mqttPublishPrefix + "hygro_aussen", false},
    {"tdout", "Taupunkt Außen", "°C", "temperature", mqttPublishPrefix + "taupunkt_aussen", false}, // ggf. anpassen

    {"diff", "Taupunkt-Differenz", "°C", "temperature", mqttPublishPrefix + "diff", false},
    
    {"lueftung", "Lüftung aktiv", "", "", mqttPublishPrefix + "status", true}
  };

  for (Sensor s : sensoren) {
    String type = s.binary ? "binary_sensor" : "sensor";
    String configTopic = mqttDiscoveryPrefix + type + "/" + deviceID + "_" + s.id + "/config";

    String payload = "{";
    payload += "\"name\":\"" + s.name + "\",";
    payload += "\"state_topic\":\"" + s.topic + "\",";
    payload += "\"availability_topic\":\"" + mqttPublishPrefix + "availability\",";
    payload += "\"payload_available\":\"online\",\"payload_not_available\":\"offline\",";
    if (!s.unit.isEmpty()) payload += "\"unit_of_measurement\":\"" + s.unit + "\",";
    if (!s.devclass.isEmpty()) payload += "\"device_class\":\"" + s.devclass + "\",";
    if (s.binary) payload += "\"payload_on\":\"1\",\"payload_off\":\"0\",";
    payload += "\"unique_id\":\"" + deviceID + "_" + s.id + "\",";
    payload += "\"device\":{";
    payload += "\"identifiers\":[\"" + deviceID + "\"],";
    payload += "\"name\":\"TaupunktLueftung\",";
    payload += "\"model\":\"ESP32\",";
    payload += "\"manufacturer\":\"DIY\"}";
    payload += "}";
    bool ok = mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    if (debugMQTT) {
      Serial.println("Sende Discovery an Topic: " + configTopic);
      Serial.println("Payload: " + payload);
      Serial.println(ok ? "✔️ Publish erfolgreich" : "❌ Publish FEHLGESCHLAGEN");
    }
  }
  if (debugMQTT) {
    mqttClient.publish("homeassistant/sensor/testsensor/config", 
    "{\"name\":\"TestSensor\",\"state_topic\":\"testsensor/value\",\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\"}", 
    true);
    mqttClient.publish("testsensor/value", "22.1", true);
  }
}

void loadMQTTSettings() {
  prefs.begin("config", true);
  String serverStr = prefs.getString("mqtt_server", mqttServer);
  String user = prefs.getString("mqtt_user", mqttUser);
  String pass = prefs.getString("mqtt_pass", mqttPassword);
  mqttPort = prefs.getInt("mqtt_port", mqttPort);
  prefs.end();

  strncpy(mqttServer, serverStr.c_str(), sizeof(mqttServer));
  strncpy(mqttUser, user.c_str(), sizeof(mqttUser));
  strncpy(mqttPassword, pass.c_str(), sizeof(mqttPassword));
  mqttClient.setServer(mqttServer, mqttPort);
}

void saveMQTTSettings() {
  prefs.begin("config", false);
  prefs.putString("mqtt_server", mqttServer);
  prefs.putInt("mqtt_port", mqttPort);
  prefs.putString("mqtt_user", mqttUser);
  prefs.putString("mqtt_pass", mqttPassword);
  prefs.end();
}

void loadMQTTTopics() {
  prefs.begin("config", true);
  mqttTempInnen = prefs.getString("mqtt_temp_in", mqttTempInnen);
  mqttHygroInnen = prefs.getString("mqtt_rh_in", mqttHygroInnen);
  mqttTempAussen = prefs.getString("mqtt_temp_out", mqttTempAussen);
  mqttHygroAussen = prefs.getString("mqtt_rh_out", mqttHygroAussen);
  mqttPublishPrefix = prefs.getString("mqtt_pub_prefix", mqttPublishPrefix);
  mqttDiscoveryPrefix = prefs.getString("mqtt_discovery_prefix", mqttDiscoveryPrefix);
  prefs.end();
}

void saveMQTTTopics() {
  prefs.begin("config", false);
  prefs.putString("mqtt_temp_in", mqttTempInnen);
  prefs.putString("mqtt_rh_in", mqttHygroInnen);
  prefs.putString("mqtt_temp_out", mqttTempAussen);
  prefs.putString("mqtt_rh_out", mqttHygroAussen);
  prefs.putString("mqtt_pub_prefix", mqttPublishPrefix);
  prefs.putString("mqtt_discovery_prefix", mqttDiscoveryPrefix);
  prefs.end();
}

void resubscribeMQTTTopics() {
  if (!mqttClient.connected()) return;
  mqttClient.subscribe(mqttTempInnen.c_str());
  mqttClient.subscribe(mqttHygroInnen.c_str());
  mqttClient.subscribe(mqttTempAussen.c_str());
  mqttClient.subscribe(mqttHygroAussen.c_str());
}

void reconnectMQTT() {
  if (!mqttAktiv) return;
  while (!mqttClient.connected()) {
    Serial.print("MQTT verbinden mit: "); Serial.println(mqttServer);
    if (mqttClient.connect(NAME, mqttUser, mqttPassword)) {
      delay(500);
      resubscribeMQTTTopics();
      Serial.println("MQTT verbunden.");
      delay(500);
      publishMQTTDiscovery();
      mqttClient.publish("homeassistant/status", "online", true);
    } else {
      Serial.print("MQTT-Verbindung fehlgeschlagen. Code: ");
      Serial.println(mqttClient.state());
      break;
    }
  }
  publishAllStates();
}

void handleChartData() {
  String json = "[";
  for (int i = 0; i < MAX_POINTS; i++) {
    int idx = (history_index + i) % MAX_POINTS;
    json += "{";
    auto f2 = [](float val) {
      return isnan(val) || isinf(val) ? "null" : String(val, 2);
    };
    auto f1 = [](float val) {
      return isnan(val) || isinf(val) ? "null" : String(val, 1);
    };

    json += "\"td_in\":" + f2(td_in_history[idx]) + ",";
    json += "\"td_out\":" + f2(td_out_history[idx]) + ",";
    json += "\"diff\":" + f2(td_diff_history[idx]) + ",";
    json += "\"rh_in\":" + f1(rh_in_history[idx]) + ",";
    json += "\"rh_out\":" + f1(rh_out_history[idx]) + ",";
    json += "\"status\":" + String(status_history[idx] ? 1 : 0);
    json += "}";
    if (i < MAX_POINTS - 1) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleLiveData() {
  String sichereUhrzeit = getUhrzeit();
  sichereUhrzeit.replace("\"", "'");
  String sichererStatus = statusText;
  sichererStatus.replace("\"", "'");

  String json = "{";
  json += "\"t_in\":" + String(t_in, 1) + ",";
  json += "\"rh_in\":" + String(rh_in, 1) + ",";
  json += "\"t_out\":" + String(t_out, 1) + ",";
  json += "\"rh_out\":" + String(rh_out, 1) + ",";
  json += "\"td_in\":" + String(td_in, 1) + ",";
  json += "\"td_out\":" + String(td_out, 1) + ",";
  json += "\"zeit\":\"" + sichereUhrzeit + "\",";
  json += "\"status\":\"" + sichererStatus + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void redirectToSettings() {
  server.send(200, "text/html", R"rawliteral(
    <html><head><meta charset='UTF-8'>
      <script>
        localStorage.setItem("stayOnSettings", "true");
        window.location.href = "/";
      </script>
    </head><body></body></html>
  )rawliteral");
}

// --- Dashboard --->
//JS-Script
String getMainScripts() {
  return R"rawliteral(
    <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
    <script>
      // ===== Globale Konstanten =====
      const SCHWELLWERT = %SCHWELLE%;
      const COLOR_TD_IN = 'green';
      const COLOR_TD_OUT = 'blue';
      const COLOR_DIFF = 'orange';
      const COLOR_SCHWELL = 'gray';
      const COLOR_STATUS_ON = 'rgba(0,200,0,0.6)';
      const COLOR_STATUS_OFF = 'rgba(0,0,0,0)';
      let chart, chart_humidity, chart_status;

      // ===== Chart-Updates =====
      let chartInitialized = false;

      async function updateChart() {
        try {
          const r = await fetch('/chartdata');
          const d = await r.json();
          const range = parseFloat(document.getElementById('rangeSelector').value);
          const totalPoints = Math.floor(range * (3600 / 5));
          const recent = d.slice(-totalPoints);

          const l = recent.map((_, i) => i);
          const tdIn = recent.map(p => p.td_in);
          const tdOut = recent.map(p => p.td_out);
          const diff = recent.map(p => p.diff);
          const rhIn = recent.map(p => p.rh_in);
          const rhOut = recent.map(p => p.rh_out);
          const status = recent.map(p => p.status);

          if (!chartInitialized) {
            // Taupunkt-Chart
            chart = new Chart(document.getElementById('chart'), {
              type: 'line',
              data: {
                labels: l,
                datasets: [
                  { label: 'Taupunkt Innen', data: tdIn, borderColor: COLOR_TD_IN, borderWidth: 2, fill: false, pointStyle: 'circle', pointRadius: 0},
                  { label: 'Taupunkt Außen', data: tdOut, borderColor: COLOR_TD_OUT, borderWidth: 2, fill: false, pointStyle: 'circle', pointRadius: 0},
                  { label: 'Differenz', data: diff, borderColor: COLOR_DIFF, borderWidth: 2, fill: false, pointStyle: 'circle', pointRadius: 0},
                  { label: 'Schwellwert +', data: Array(l.length).fill(SCHWELLWERT), borderDash: [5, 5], borderColor: COLOR_SCHWELL, borderWidth: 1, fill: false, pointStyle: 'circle', pointRadius: 0},
                  { label: 'Schwellwert -', data: Array(l.length).fill(-SCHWELLWERT), borderDash: [5, 5], borderColor: COLOR_SCHWELL, borderWidth: 1, fill: false, pointStyle: 'circle', pointRadius: 0}
                ]
              },
              options: {
                responsive: true,
                plugins: {
                  legend: {
                    labels: {
                      usePointStyle: true,
                      pointStyle: 'line'
                    }
                  }
                }
              }
            });

            // Feuchte-Chart
            chart_humidity = new Chart(document.getElementById('chart_humidity'), {
              type: 'line',
              data: {
                labels: l,
                datasets: [
                  { label: 'RH Innen', data: rhIn, borderColor: 'teal', borderWidth: 2, fill: false, pointStyle: 'circle', pointRadius: 0},
                  { label: 'RH Außen', data: rhOut, borderColor: 'purple', borderWidth: 2, fill: false, pointStyle: 'circle', pointRadius: 0}
                ]
              },
              options: {
                responsive: true,
                plugins: {
                  legend: {
                    labels: {
                      usePointStyle: true,
                      pointStyle: 'line'
                    }
                  }
                }
              }
            });

            // Status-Balken
            chart_status = new Chart(document.getElementById('chart_status'), {
              type: 'bar',
              data: {
                labels: l,
                datasets: [{
                  label: 'Lüftung',
                  data: status,
                  backgroundColor: status.map(s => s === 1 ? COLOR_STATUS_ON : COLOR_STATUS_OFF)
                }]
              },
              options: {
                responsive: true,
                scales: { y: { beginAtZero: true, max: 1 } }
              }
            });

            chartInitialized = true;

          } else {
            chart.data.labels = l;
            chart.data.datasets[0].data = tdIn;
            chart.data.datasets[1].data = tdOut;
            chart.data.datasets[2].data = diff;
            chart.data.datasets[3].data = Array(l.length).fill(SCHWELLWERT);
            chart.data.datasets[4].data = Array(l.length).fill(-SCHWELLWERT);
            chart.update();

            chart_humidity.data.labels = l;
            chart_humidity.data.datasets[0].data = rhIn;
            chart_humidity.data.datasets[1].data = rhOut;
            chart_humidity.update();

            chart_status.data.labels = l;
            chart_status.data.datasets[0].data = status;
            chart_status.data.datasets[0].backgroundColor = status.map(s => s === 1 ? COLOR_STATUS_ON : COLOR_STATUS_OFF);
            chart_status.update();
          }
        } catch (e) {
          console.error("Chart-Update-Fehler:", e);
        }
      }

      // ===== Live-Daten =====
      function updateElementValue(id, value, unit = "") {
        const el = document.getElementById(id);
        if (!el) return;

        const isValid = typeof value === "number" && !isNaN(value);

        if (isValid) {
          el.textContent = value.toFixed(1) + unit;
          el.style.color = "";
          el.title = "";
        } else {
          el.textContent = "⚠️";
          el.style.color = "red";
          el.title = "Sensorwert ungültig oder nicht verfügbar";
        }
      }
      async function updateLiveData() {
        try {
          const res = await fetch('/livedata');
          const data = await res.json();
          document.getElementById('zeit').textContent = data.zeit;
          document.getElementById('status_text').textContent = data.status;
          updateElementValue('t_in', data.t_in, '°C');
          updateElementValue('rh_in', data.rh_in, '%');
          updateElementValue('t_out', data.t_out, '°C');
          updateElementValue('rh_out', data.rh_out, '%');
          updateElementValue('td_in', data.td_in, '°C');
          updateElementValue('td_out', data.td_out, '°C');
        } catch (e) {
          console.error("Live-Daten-Fehler:", e);
        }
      }

      // ===== Tabs =====
      function showTab(tab) {
        document.getElementById('dashboardTab').classList.add('hidden');
        document.getElementById('settingsTab').classList.add('hidden');
        document.getElementById(tab + 'Tab').classList.remove('hidden');
        localStorage.setItem("activeTab", tab);
        if (tab === 'dashboard') {
          updateLiveData();
          updateChart();
        }
        if (tab === 'settings' && localStorage.getItem('manualSettingsClick') === 'true') {
          openFirmwareModalUI();
          localStorage.removeItem('manualSettingsClick');
        }
      }

      // ===== MQTT Umschalter =====
      function toggleMQTT(el) {
        const form = el.closest("form");
        const hidden = form.querySelector("input[name='mqtt']");
        const aktivieren = el.checked;
        hidden.value = aktivieren ? "MQTT aktivieren" : "MQTT deaktivieren";
        
        // Sende AJAX-Request
        fetch('/setMQTT', {
          method: "POST",
          body: new URLSearchParams(new FormData(form))
        }).then(() => {
          // UI sofort aktualisieren ohne Reload:
          const disabled = !aktivieren;

          // Hinweistext zeigen/verstecken
          const info = document.getElementById("mqttHinweis");
          if (info) info.style.display = disabled ? "block" : "none";

          // Dropdowns und Submit-Button aktivieren/deaktivieren
          document.querySelectorAll("select[name='modus_innen'], select[name='modus_aussen']").forEach(sel => {
            sel.disabled = disabled;
          });
          document.querySelector("input[value='Modus speichern']").disabled = disabled;
        });
      }

      // ===== Modal Logik =====
      function openFirmwareModalUI() {
        const modal = document.getElementById("firmwareModal");
        modal.classList.remove("hidden");
        if (!openFirmwareModalUI.listenerAdded) {
          document.addEventListener("keydown", escCloseModal);
          document.addEventListener("click", outsideClickModal);
          openFirmwareModalUI.listenerAdded = true;
        }
      }

      function closeFirmwareModal() {
        const modal = document.getElementById("firmwareModal");
        modal.classList.add("hidden");
        if (openFirmwareModalUI.listenerAdded) {
          document.removeEventListener("keydown", escCloseModal);
          document.removeEventListener("click", outsideClickModal);
          openFirmwareModalUI.listenerAdded = false;
        }
      }

      function escCloseModal(e) {
        if (e.key === "Escape") closeFirmwareModal();
      }

      function outsideClickModal(e) {
        const modal = document.getElementById("firmwareModal");
        const modalContent = document.querySelector(".modal-content");
        if (!modal.classList.contains("hidden") && !modalContent.contains(e.target)) {
          closeFirmwareModal();
        }
      }

      function confirmFirmwareUpdate() {
        const go = confirm("⚠️ Firmware-Update vorbereiten?\n\n- MQTT wird getrennt\n- Sensorlogik pausiert\n\nJetzt fortfahren?");
        if (go) {
          closeFirmwareModal();
          return true;
        }
        return false;
      }

      // ===== Init =====
      window.onload = () => {
        openFirmwareModalUI.listenerAdded = false;

        const savedTab = localStorage.getItem("stayOnSettings") === "true"
          ? "settings"
          : "dashboard";

        localStorage.removeItem("stayOnSettings");
        showTab(savedTab);

        // Live-Daten + Chart dauerhaft starten – egal welcher Tab
        setInterval(updateLiveData, 5000);
        setInterval(updateChart, 5000);
        ajaxFormHandler("tempschutzForm", "Temperaturschutz gespeichert.");
        ajaxFormHandler("schwelleForm", "Schwellenwert gespeichert.");
        ajaxFormHandler("modusForm", "Sensor-Modus gespeichert.");
        ajaxFormHandler("mqttConfigForm", "MQTT-Verbindung gespeichert.");
        ajaxFormHandler("mqttTopicsForm", "MQTT Topics gespeichert.");
        ajaxFormHandler("discoveryForm", "MQTT Discovery gesendet.");
        ajaxFormHandler("discoveryPrefixForm", "Discovery Prefix gespeichert.");
      };

      // ===== AJAX Hilfsfunktion =====
      function ajaxFormHandler(formId, successMessage = "Gespeichert!") {
        const form = document.getElementById(formId);
        if (!form) return;

        form.addEventListener("submit", function (e) {
          e.preventDefault();
          const data = new FormData(form);
          fetch(form.action, {
            method: "POST",
            body: new URLSearchParams(data)
          }).then(() => {
            alert(successMessage);
          }).catch(err => {
            console.error("AJAX-Fehler:", err);
          });
        });
      }
    </script>
  )rawliteral";
}
//CSS
void handleCSS() {
  String css = R"rawliteral(
    body {
      font-family: Arial, sans-serif;
      background: #f8f9fa;
      margin: 20px;
      color: #2c5777; /* neue dunklere Textfarbe */
    }
    h1 {
      color: #2c5777;
    }
    form {
      margin: 20px 0;
    }
    canvas {
      background: white;
      border: 1px solid #ccc;
      margin-bottom: 20px;
    }
    .hidden {
      display: none !important;
    }

    button,
    input[type='submit'],
    input[type='button'],
    .button-link {
      background-color: #e1ecf4;
      border-radius: 3px;
      border: 1px solid #7aa7c7;
      box-shadow: rgba(255, 255, 255, .7) 0 1px 0 0 inset;
      box-sizing: border-box;
      color: #39739d;
      cursor: pointer;
      display: inline-block;
      font-family: -apple-system,system-ui,"Segoe UI","Liberation Sans",sans-serif;
      font-size: 13px;
      font-weight: 400;
      line-height: 1.15385;
      margin: 5px 5px 5px 0;
      outline: none;
      padding: 8px .8em;
      position: relative;
      text-align: center;
      text-decoration: none;
      user-select: none;
      vertical-align: baseline;
      white-space: nowrap;
    }

    button:hover,
    input[type='submit']:hover,
    input[type='button']:hover,
    .button-link:hover {
      background-color: #b3d3ea;
      color: #2c5777;
    }

    button:focus,
    input[type='submit']:focus,
    input[type='button']:focus {
      box-shadow: 0 0 0 4px rgba(0, 149, 255, .15);
    }

    button:active,
    input[type='submit']:active,
    input[type='button']:active {
      background-color: #a0c7e4;
      box-shadow: none;
      color: #2c5777;
    }

    a.button-link {
      text-align: center;
    }

    select {
      padding: 5px 10px;
      font-size: 1em;
      border: 1px solid #ccc;
      border-radius: 5px;
      background-color: white;
      margin: 5px 0;
      appearance: none;
      -webkit-appearance: none;
      -moz-appearance: none;
    }
    select:focus {
      outline: none;
      border-color: #007bff;
      box-shadow: 0 0 3px #007bff55;
    }

    .switch {
      position: relative;
      display: inline-block;
      width: 50px;
      height: 24px;
      margin-left: 10px;
    }
    .switch input {
      display: none;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      background-color: #ccc;
      border-radius: 24px;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      transition: 0.4s;
    }
    .slider:before {
      content: "";
      position: absolute;
      height: 18px;
      width: 18px;
      left: 3px;
      bottom: 3px;
      background-color: white;
      border-radius: 50%;
      transition: 0.4s;
    }
    input:checked + .slider {
      background-color: #007bff;
    }
    input:checked + .slider:before {
      transform: translateX(26px);
    }

    .modal {
      position: fixed;
      top: 0; left: 0;
      width: 100%; height: 100%;
      z-index: 1000;
      background: rgba(0,0,0,0.5);
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .modal.hidden {
      display: none !important;
    }
    .modal-content {
      background: #fff;
      padding: 20px;
      border-radius: 10px;
      max-width: 400px;
      width: 90%;
      box-shadow: 0 0 10px #000;
      position: relative;
    }
    .close {
      position: absolute;
      top: 10px;
      right: 15px;
      font-size: 1.5em;
      cursor: pointer;
    }

    fieldset {
      border: 1px solid #ccc;
      border-radius: 8px;
      padding: 10px 15px;
      margin-bottom: 20px;
      background-color: #ffffffcc;
    }
    legend {
      font-weight: bold;
      padding: 0 5px;
      color: #2c5777;
    }
    input[type='text'],
    input[type='number'],
    input[type='password'],
    select,
    textarea {
      color: #2c5777;
    }
  )rawliteral";
  server.send(200, "text/css", css);
}

//Root
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>" + String(NAME) + "</title>";
  
  html += "<link rel='stylesheet' href='/style.css'>";

  html += "</head><body>";
  html += "<h1>" + String(NAME) + " " + String(FIRMWARE_VERSION) + " Interface</h1>";

  // Tabs oben
  html += "<p><button onclick=\"showTab('dashboard')\">Dashboard</button>";
  html += "<button onclick=\"localStorage.setItem('manualSettingsClick','true'); showTab('settings')\">Einstellungen</button>";

  // Eingefügte UI-Blöcke:
  html += getDashboardHtml();
  html += getSettingsHtml();
  html += getFirmwareModalHtml();

  String script = getMainScripts();
  script.replace("%SCHWELLE%", String(taupunktDifferenzSchwellwert));
  html += script;

  html += "</body></html>";

  server.send(200, "text/html", html);
}
//Dashboard
String getDashboardHtml() {
  String html;
  html += "<div id='dashboardTab'>";
  html += "<div id='live'><p><strong>Zeit:</strong> <span id='zeit'></span><br>";
  html += "<strong>Innen:</strong> <span id='t_in'></span>, <span id='rh_in'></span><br>";
  html += "<strong>Außen:</strong> <span id='t_out'></span>, <span id='rh_out'></span><br>";
  html += "<strong>Taupunkt innen:</strong> <span id='td_in'></span><br>";
  html += "<strong>Taupunkt außen:</strong> <span id='td_out'></span></p></div>";
  html += "<p><strong>Status:</strong> <span id='status_text'></span></p>";
  html += "<p><strong>Letztes Ereignis:</strong> " + logEintrag + "</p>";
  html += "<form id='rangeForm' onsubmit='return false;'>"
          "<label><strong>Zeitraum:</strong></label> "
          "<select id='rangeSelector' onchange='updateChart()'>"
          "<option value='0.1'>10 Minuten</option>"
          "<option value='0.5'>30 Minuten</option>"
          "<option value='1'>1 Stunden</option>"
          "</select>"
          "</form>";
  html += "<canvas id='chart' width='400' height='100'></canvas>";
  html += "<canvas id='chart_humidity' width='400' height='70'></canvas>";
  html += "<canvas id='chart_status' width='400' height='30'></canvas>";
  html += "</div>";
  return html;
}
//Settings
String getSettingsHtml() {
  String html;
  html += "<div id='settingsTab' class='hidden'>";
  html += "<h2>Einstellungen</h2>";

  // Temperaturschutz
  html += "<fieldset><legend>Temperaturschutz</legend>";
  html += "<form id='tempschutzForm' method='POST' action='/tempschutz'>";
  html += "<label><input type='checkbox' name='aktiv'";
  if (schutzVorAuskuehlungAktiv) html += " checked";
  html += "> Aktivieren</label><br>";
  html += "Mindest-Innentemperatur (°C): <input type='number' step='0.1' name='min_temp' value='" + String(minTempInnen, 1) + "'><br>";
  html += "<input type='submit' value='Speichern'></form></fieldset>";

  // Taupunktdifferenz-Schwellenwert
  html += "<fieldset><legend>Schwellenwert</legend>";
  html += "<form id='schwelleForm' method='POST' action='/setSchwelle'>";
  html += "Taupunkt-Differenz (°C), ab der gelüftet wird:<br>";
  html += "<input type='number' step='0.1' name='schwelle' value='" + String(taupunktDifferenzSchwellwert, 1) + "' "
          "title='Empfohlener Wert: 4,0 °C\n\nDie Außenluft muss mindestens so viel \"trockener\" sein (Taupunkt-Differenz), damit gelüftet wird.\n\nTipp: Höher = vorsichtiger, niedriger = aggressiver lüften.'><br>";
  html += "<input type='submit' value='Schwellenwert speichern'>";
  html += "</form></fieldset>";

  // Sensorquelle
  bool disabled = !mqttAktiv;
  html += "<fieldset><legend>Sensorquelle</legend>";
  if (disabled) html += "<p id='mqttHinweis' style='color:gray;'>MQTT ist deaktiviert – Auswahl gesperrt.</p>";
  else html += "<p id='mqttHinweis' style='display:none;'></p>";
  html += "<form id='modusForm' method='POST' action='/setModus'>";
  html += "Modus innen: <select name='modus_innen'" + String(disabled ? " disabled" : "") + ">";
  html += "<option value='hardware'" + String(modus_innen == "hardware" ? " selected" : "") + ">Hardware</option>";
  html += "<option value='mqtt'" + String(modus_innen == "mqtt" ? " selected" : "") + ">MQTT</option>";
  html += "</select><br>";
  html += "Modus außen: <select name='modus_aussen'" + String(disabled ? " disabled" : "") + ">";
  html += "<option value='hardware'" + String(modus_aussen == "hardware" ? " selected" : "") + ">Hardware</option>";
  html += "<option value='mqtt'" + String(modus_aussen == "mqtt" ? " selected" : "") + ">MQTT</option>";
  html += "</select><br>";
  html += "<input type='submit' value='Modus speichern'" + String(disabled ? " disabled" : "") + ">";
  html += "</form></fieldset>";

  // MQTT Einstellungen
  html += "<fieldset><legend>MQTT</legend>";
  // Verbindungsdaten
  html += "<form id='mqttConfigForm' method='POST' action='/mqttconfig'>";
  html += "<p><strong>MQTT-Server</strong> Zugangsdaten:</p>";
  html += "Server: <input name='server' value='" + String(mqttServer) + "' "
          "title='Hostname oder IP-Adresse deines MQTT-Brokers, z. B. 192.168.1.10 oder mqtt.local'><br>";
  html += "Port: <input name='port' value='" + String(mqttPort) + "' "
          "title='Standardmäßig 1883. Passe den Port an, falls dein Broker einen anderen verwendet.'><br>";
  html += "Benutzer: <input name='user' value='" + String(mqttUser) + "' "
          "title='Benutzername für die Verbindung zum MQTT-Server (optional)'><br>";
  html += "Passwort: <input type='password' name='pass' value='" + String(mqttPassword) + "' "
          "title='Passwort für den oben angegebenen Benutzer (optional)'><br>";
  html += "<input type='submit' value='MQTT-Verbindung speichern'></form>";
  // Topics & Prefix
  // Discovery-Prefix
  html += "<form id='discoveryPrefixForm' method='POST' action='/mqttdiscoveryprefix'>";
  html += "<p><strong>MQTT Discovery Prefix</strong>:</p>";
  html += "<input name='mqtt_discovery_prefix' value='" + mqttDiscoveryPrefix + "' "
          "title='Prefix für MQTT Auto-Discovery, z. B. homeassistant/'><br>";
  html += "<input type='submit' value='Discovery-Prefix speichern'>";
  html += "</form>";
  html += "<form id='mqttTopicsForm' method='POST' action='/mqtttopics'>";
  html += "<p><strong>MQTT-Prefix</strong> für alle ausgehenden Nachrichten:</p>";
  html += "Publish-Prefix: <input name='mqtt_pub_prefix' value='" + mqttPublishPrefix + "' "
          "title='Dieses Präfix wird für alle automatisch gesendeten MQTT-Nachrichten verwendet, z. B. esp32/innen/. Achte auf einen abschließenden Slash!'><br>";
  html += "<p><strong>Abonnierte MQTT-Topics</strong> für empfangene Sensordaten:</p>";
  html += "Innen Temperatur: <input name='temp_innen' value='" + mqttTempInnen + "' "
          "title='MQTT-Topic, von dem Temperaturwerte (in °C) für den Innenraum empfangen werden. Nur bei MQTT-Modus aktiv.'><br>";
  html += "Innen Feuchte: <input name='hygro_innen' value='" + mqttHygroInnen + "' "
          "title='MQTT-Topic, von dem Luftfeuchtigkeit (0–100 %) für den Innenraum empfangen wird.'><br>";
  html += "Außen Temperatur: <input name='temp_aussen' value='" + mqttTempAussen + "' "
          "title='MQTT-Topic, von dem Temperaturwerte für den Außenbereich empfangen werden.'><br>";
  html += "Außen Feuchte: <input name='hygro_aussen' value='" + mqttHygroAussen + "' "
          "title='MQTT-Topic, von dem Luftfeuchtigkeit für den Außenbereich empfangen wird.'><br>";
  html += "<input type='submit' value='MQTT Topics speichern'></form>";
  // Umschalter MQTT ein/aus
  html += "<form method='POST' action='/setMQTT'>";
  html += "<label for='mqtt_toggle'>MQTT aktiv:</label>";
  html += "<input type='hidden' name='mqtt' value=''>"; // WICHTIG!
  html += "<label class='switch'>";
  html += "<input type='checkbox' name='mqtt_toggle' id='mqtt_toggle' ";
  html += mqttAktiv ? "checked " : "";
  html += "onchange='toggleMQTT(this)'>";
  html += "<span class='slider round'></span>";
  html += "</label></form>";
  // Manuelle Discovery-Auslösung
  html += "<form id='discoveryForm' method='POST' action='/mqttdiscovery'>";
  html += "<input type='submit' value='MQTT Discovery erneut senden'>";
  html += "</form>";
  html += "</fieldset>";

  // Firmware-Button
  html += "<fieldset><legend>Firmware</legend>";
  html += "<p><button type='button' onclick=\"showTab('settings'); setTimeout(openFirmwareModalUI, 100);\">Firmware-Update</button></p>";
  html += "</fieldset>";

  html += "<p align='center'>";
  html += "<a href='https://github.com/mallewski/TaupunktLueftung' target='_blank' "
        "style='display:inline-block;text-decoration:none;padding:6px 12px;"
        "background:#24292e;color:white;border-radius:5px;font-weight:bold;'>"
        "<svg height='16' width='16' viewBox='0 0 16 16' fill='white' "
        "style='vertical-align:middle;margin-right:6px;'>"
        "<path d='M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38"
        " 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13"
        "-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07"
        "-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08"
        "-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27s1.36.09 2 .27"
        "c1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15"
        " 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2"
        " 0 .21.15.46.55.38A8.013 8.013 0 0 0 16 8c0-4.42-3.58-8-8-8z'/>"
        "</svg> Projekt auf GitHub</a>";
  html += "</p>";
  html += "<p align='center'>Wenn du dieses Projekt nützlich findest, kannst du mir einen Kaffee spendieren:</p>";
  html += "<p align='center'><a href='https://www.buymeacoffee.com/mallewski' target='_blank'><img src='https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png' alt='Buy Me A Coffee' height='60' width='217'></a></p>";

  html += "</div>"; // settingsTab
  return html;
}
//Firmware
String getFirmwareModalHtml() {
  String html = R"rawliteral(
    <div id="firmwareModal" class="modal hidden">
      <div class="modal-content">
        <span class="close" onclick="closeFirmwareModal()">&times;</span>
        <h3>Firmware-Update durchführen</h3>
  )rawliteral";

  // Hier dynamisch einfügen:
  html += "<p>Installierte Firmware-Version: " + String(FIRMWARE_VERSION) + "</p>";

  html += R"rawliteral(
        <form method="POST" action="/update" enctype="multipart/form-data" onsubmit="return confirmFirmwareUpdate();">
          <input type="file" name="firmware" required><br><br>
          <input type="submit" value="Upload & Update">
        </form>
      </div>
    </div>
  )rawliteral";

  return html;
}

//Handler
//Temperaturschutz
void handleTempSchutz() {
  if (server.hasArg("aktiv")) {
    schutzVorAuskuehlungAktiv = true;
  } else {
    schutzVorAuskuehlungAktiv = false;
  }
  if (server.hasArg("min_temp")) {
    minTempInnen = server.arg("min_temp").toFloat();
  }

  prefs.begin("config", false);
  prefs.putBool("tempschutz", schutzVorAuskuehlungAktiv);
  prefs.putFloat("min_temp", minTempInnen);
  prefs.end();

  redirectToSettings();
}
//Schwellenwert
void handleSetSchwelle() {
  if (server.hasArg("schwelle")) {
    taupunktDifferenzSchwellwert = server.arg("schwelle").toFloat();
    prefs.begin("config", false);
    prefs.putFloat("schwelle", taupunktDifferenzSchwellwert);
    prefs.end();
  }
  redirectToSettings();
}
//Modus (eigene Sensoren oder MQTT)
void handleSetModus() {
  if (server.hasArg("modus_innen")) modus_innen = server.arg("modus_innen");
  if (server.hasArg("modus_aussen")) modus_aussen = server.arg("modus_aussen");
  prefs.begin("config", false);
  prefs.putString("modus_innen", modus_innen);
  prefs.putString("modus_aussen", modus_aussen);
  prefs.end();
  redirectToSettings();
}
//MQTT
void handleSetMQTT() {
  if (server.hasArg("mqtt")) {
    if (debugMQTT) {
      Serial.println("MQTT-Toggle empfangen: " + server.arg("mqtt"));
    }
    mqttAktiv = (server.arg("mqtt") == "MQTT aktivieren");
    prefs.begin("config", false);
    prefs.putBool("mqtt", mqttAktiv);
    prefs.end();
    if (mqttAktiv) {
      reconnectMQTT();
    }
  }
  redirectToSettings();
}
void handleMQTTConfig() {
  if (server.hasArg("server")) strncpy(mqttServer, server.arg("server").c_str(), sizeof(mqttServer));
  if (server.hasArg("port")) mqttPort = server.arg("port").toInt();
  if (server.hasArg("user")) strncpy(mqttUser, server.arg("user").c_str(), sizeof(mqttUser));
  if (server.hasArg("pass")) strncpy(mqttPassword, server.arg("pass").c_str(), sizeof(mqttPassword));

  saveMQTTSettings();
  mqttClient.setServer(mqttServer, mqttPort);
  reconnectMQTT();

  redirectToSettings();
}
void handleMQTTTopics() {
  mqttTempInnen   = server.arg("temp_innen");
  mqttHygroInnen  = server.arg("hygro_innen");
  mqttTempAussen  = server.arg("temp_aussen");
  mqttHygroAussen = server.arg("hygro_aussen");

  if (server.hasArg("mqtt_pub_prefix")) {
    mqttPublishPrefix = server.arg("mqtt_pub_prefix");
    if (!mqttPublishPrefix.endsWith("/")) mqttPublishPrefix += "/";
  }

  saveMQTTTopics();
  if (mqttClient.connected()) {
    resubscribeMQTTTopics();
    publishMQTTDiscovery();
  }

  redirectToSettings();
}
void handleMQTTDiscovery() {
  if (mqttClient.connected()) {
    publishMQTTDiscovery();
    Serial.println("🟢 MQTT ist verbunden. Sende Discovery...");
  } else {
    Serial.println("🔴 MQTT NICHT verbunden – keine Discovery gesendet.");
  }
  redirectToSettings();
}

//Firmware
void handleFirmwareUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    prepareForFirmwareUpdate(); // <<< Hier aktivieren wir den Stop-Modus

    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin()) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update abgeschlossen: %u Bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}
void prepareForFirmwareUpdate() {
  mqttClient.disconnect();         // MQTT sicher trennen
  mqttAktiv = false;
  // Optionale Flags:
  updateModeActive = true;        // Setze in der loop() oder beim Lesen Checks wie: if (updateModeActive) return;
  logEvent("Firmware-Update vorbereitet. Dienste deaktiviert.");
}

// --- Setup --->
//Setup Wifi
void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("WLAN verbinden");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[OK] WLAN verbunden: " + WiFi.localIP().toString());
}
//Setup Sensoren
void setupSensoren() {
  pinMode(RELAY_LED_PIN, OUTPUT);
  pinMode(STATUS_GREEN_PIN, OUTPUT);
  pinMode(STATUS_RED_PIN, OUTPUT);
  pinMode(STATUS_YELLOW_PIN, OUTPUT);
  setLEDs(false, false, false);

  shtInnen.begin(0x44);
  dht.begin();
}
//setup Preferences
void setupPreferences() {
  prefs.begin("config", true);
  taupunktDifferenzSchwellwert = prefs.getFloat("schwelle", 4.0);
  modus_innen = prefs.getString("modus_innen", "hardware");
  modus_aussen = prefs.getString("modus_aussen", "hardware");
  mqttAktiv = prefs.getBool("mqtt", true);
  schutzVorAuskuehlungAktiv = prefs.getBool("tempschutz", false);
  minTempInnen = prefs.getFloat("min_temp", 12.0);
  prefs.end();
}
//Setup MQTT
void setupMQTT() {
  loadMQTTSettings();
  loadMQTTTopics();
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setKeepAlive(60);
  mqttClient.setCallback(mqttCallback);
  if (mqttAktiv) reconnectMQTT();
}
//Setup Web Server
void setupWebServer() {
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");

  server.on("/", handleRoot);
  server.on("/tempschutz", HTTP_POST, handleTempSchutz);
  server.on("/setSchwelle", HTTP_POST, handleSetSchwelle);
  server.on("/mqttconfig", HTTP_POST, handleMQTTConfig);
  server.on("/mqtttopics", HTTP_POST, handleMQTTTopics);
  server.on("/setMQTT", handleSetMQTT);
  server.on("/setModus", handleSetModus);
  server.on("/chartdata", handleChartData);
  server.on("/livedata", handleLiveData);
  server.on("/style.css", handleCSS);
  server.on("/mqttdiscovery", HTTP_POST, handleMQTTDiscovery);
  server.on("/mqttdiscoveryprefix", HTTP_POST, []() {
    if (server.hasArg("mqtt_discovery_prefix")) {
      mqttDiscoveryPrefix = server.arg("mqtt_discovery_prefix");
      if (!mqttDiscoveryPrefix.endsWith("/")) mqttDiscoveryPrefix += "/";
      
      prefs.begin("config", false);
      prefs.putString("mqtt_discovery_prefix", mqttDiscoveryPrefix);
      prefs.end();

      publishMQTTDiscovery();

      server.send(200, "text/plain", "OK"); // Nur als Feedback für AJAX
    } else {
      server.send(400, "text/plain", "Missing prefix");
    }
  });
  server.on("/rediscovery", []() {
    if (mqttClient.connected()) {
      publishMQTTDiscovery();
      mqttClient.publish("homeassistant/status", "online", true);
      server.send(200, "text/plain", "Discovery gesendet.");
    } else {
      server.send(500, "text/plain", "MQTT nicht verbunden.");
    }
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", R"rawliteral(
      <html><head><meta charset='UTF-8'><title>Update</title><style>
        body { font-family: sans-serif; background: #f8f9fa; text-align: center; padding: 50px; }
        .status { font-size: 1.5em; color: #007bff; }
      </style></head><body>
      <p class='status'>Firmware-Update erfolgreich.<br>Neustart in wenigen Sekunden...</p>
      <script>
        setTimeout(() => window.location.href = "/", 10000);
      </script>
      </body></html>
    )rawliteral");
    delay(1000);
    ESP.restart();
  }, handleFirmwareUpload);

  server.begin();
  Serial.println("[OK] Webserver gestartet.");
}

// >>> SETUP
void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  setupWiFi();
  setupSensoren();
  setupPreferences();
  setupMQTT();
  setupWebServer();

  Serial.println("[OK] Setup abgeschlossen.");
}

//--- Loop ---->
//loop MQTT
void handleMQTT() {
  if (!mqttAktiv) return;
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();
}
//Loop Web Server
void handleWebServer() {
  server.handleClient();
}
//Loop Sensoren
void handleSensorzyklus() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= SENSORZYKLUS_MS) {
    lastUpdate = millis();
    aktualisiereSensoren();
    steuerlogik();
  }
}

// >>> LOOP
void loop() {
  if (updateModeActive) {
    server.handleClient();
    return;
  }

  handleMQTT();
  handleWebServer();
  handleSensorzyklus();
}


