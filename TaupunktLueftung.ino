// TaupunktLueftungBuddy v1.6
// Vollständige Version mit Chart-Update via AJAX, MQTT-Setup, LED-Steuerung, Webinterface

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_SHT31.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <time.h>

#define FIRMWARE_VERSION "v1.6"
#define RELAY_LED_PIN 16
#define STATUS_GREEN_PIN 2
#define STATUS_RED_PIN 18
#define STATUS_YELLOW_PIN 19
#define DHTPIN 17
#define DHTTYPE DHT22
#define MAX_POINTS 50

Preferences prefs;
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

Adafruit_SHT31 shtInnen = Adafruit_SHT31();
DHT dht(DHTPIN, DHTTYPE);

#include "secrets.h"

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
String mqttPublishPrefix = "esp32/innen/";

String modus_innen = "hardware";
String modus_aussen = "hardware";
bool mqttAktiv = true;

float t_in = NAN, rh_in = NAN, td_in = NAN;
float t_out = NAN, rh_out = NAN, td_out = NAN;
float mqtt_t_in = NAN, mqtt_rh_in = NAN;
float mqtt_t_out = NAN, mqtt_rh_out = NAN;
float td_in_history[MAX_POINTS];
float td_out_history[MAX_POINTS];
float td_diff_history[MAX_POINTS];
float rh_in_history[MAX_POINTS];
float rh_out_history[MAX_POINTS];
int history_index = 0;

String statusText = "Unbekannt";
bool lueftungAktiv = false;
String logEintrag = "";
String letzteUhrzeit = "--:--:--";
float taupunktDifferenzSchwellwert = 4.0;
bool status_history[MAX_POINTS]; 

float berechneTaupunkt(float T, float RH) {
  float a = (T >= 0) ? 7.5 : 7.6;
  float b = (T >= 0) ? 237.3 : 240.7;
  float sdd = 6.1078 * pow(10, (a * T) / (b + T));
  float dd = sdd * RH / 100.0;
  float v = log10(dd / 6.1078);
  return (b * v) / (a - v);
}

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

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String val;
  for (unsigned int i = 0; i < length; i++) val += (char)payload[i];
  float fval = val.toFloat();
  if (String(topic) == mqttTempInnen) mqtt_t_in = fval;
  if (String(topic) == mqttHygroInnen) mqtt_rh_in = fval;
  if (String(topic) == mqttTempAussen) mqtt_t_out = fval;
  if (String(topic) == mqttHygroAussen) mqtt_rh_out = fval;
}

void publishSensorwerte() {
  if (!mqttAktiv) return;
  mqttClient.publish((mqttPublishPrefix + "temp").c_str(), String(t_in, 1).c_str(), true);
  mqttClient.publish((mqttPublishPrefix + "hygro").c_str(), String(rh_in, 1).c_str(), true);
  mqttClient.publish((mqttPublishPrefix + "taupunkt").c_str(), String(td_in, 1).c_str(), true);
}

void aktualisiereSensoren() {
  t_in = (modus_innen == "mqtt" && mqttAktiv) ? mqtt_t_in : shtInnen.readTemperature();
  rh_in = (modus_innen == "mqtt" && mqttAktiv) ? mqtt_rh_in : shtInnen.readHumidity();
  t_out = (modus_aussen == "mqtt" && mqttAktiv) ? mqtt_t_out : dht.readTemperature();
  rh_out = (modus_aussen == "mqtt" && mqttAktiv) ? mqtt_rh_out : dht.readHumidity();

  if (!isnan(t_in) && !isnan(rh_in)) td_in = berechneTaupunkt(t_in, rh_in);
  if (!isnan(t_out) && !isnan(rh_out)) td_out = berechneTaupunkt(t_out, rh_out);

  publishSensorwerte();
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
  }

  td_in_history[history_index] = td_in;
  td_out_history[history_index] = td_out;
  td_diff_history[history_index] = diff;
  rh_in_history[history_index] = rh_in;
  rh_out_history[history_index] = rh_out;
  status_history[history_index] = lueftungAktiv;
  history_index = (history_index + 1) % MAX_POINTS;

}

void handleChartData() {
  String json = "[";
  for (int i = 0; i < MAX_POINTS; i++) {
    int idx = (history_index + i) % MAX_POINTS;
    json += "{";
    json += "\"td_in\":" + String(td_in_history[idx], 2) + ",";
    json += "\"td_out\":" + String(td_out_history[idx], 2) + ",";
    json += "\"diff\":" + String(td_diff_history[idx], 2) + ",";
    json += "\"rh_in\":" + String(rh_in_history[idx], 1) + ",";
    json += "\"rh_out\":" + String(rh_out_history[idx], 1) + ","; 
    json += "\"status\":" + String(status_history[idx] ? 1 : 0);
    json += "}";
    if (i < MAX_POINTS - 1) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleLiveData() {
  String json = "{";
  json += "\"t_in\":" + String(t_in, 1) + ",";
  json += "\"rh_in\":" + String(rh_in, 1) + ",";
  json += "\"t_out\":" + String(t_out, 1) + ",";
  json += "\"rh_out\":" + String(rh_out, 1) + ",";
  json += "\"td_in\":" + String(td_in, 1) + ",";
  json += "\"td_out\":" + String(td_out, 1) + ",";
  json += "\"zeit\":\"" + getUhrzeit() + "\"";
  json += ",\"status\":\"" + statusText + "\"";
  json += "}";
  server.send(200, "application/json", json);
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
  prefs.end();
}

void saveMQTTTopics() {
  prefs.begin("config", false);
  prefs.putString("mqtt_temp_in", mqttTempInnen);
  prefs.putString("mqtt_rh_in", mqttHygroInnen);
  prefs.putString("mqtt_temp_out", mqttTempAussen);
  prefs.putString("mqtt_rh_out", mqttHygroAussen);
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
    if (mqttClient.connect("TaupunktBuddy", mqttUser, mqttPassword)) {
      resubscribeMQTTTopics();
      Serial.println("MQTT verbunden.");
    } else {
      Serial.print("MQTT-Verbindung fehlgeschlagen. Code: ");
      Serial.println(mqttClient.state());
      break;
    }
  }
}
//JS-Script
String getChartScript() {
  return R"rawliteral(
    <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
    <script>
      const SCHWELLWERT = %SCHWELLE%;
      let chart, chart_humidity, chart_status;
      async function updateChart() {
        let r = await fetch('/chartdata');
        let d = await r.json();
        let l = d.map((_,i)=>i);
        let tdIn = d.map(p=>p.td_in), tdOut = d.map(p=>p.td_out), diff = d.map(p=>p.diff);
        let rhIn = d.map(p=>p.rh_in), rhOut = d.map(p=>p.rh_out);
        let status = d.map(p=>p.status);

        if (!chart) {
          chart = new Chart(document.getElementById('chart'), {
            type: 'line',
            data: {
              labels: l,
              datasets: [
                {label:'Taupunkt Innen', data:tdIn, borderColor:'green', borderWidth: 2, fill: false, pointStyle: 'line'},
                {label:'Taupunkt Außen', data:tdOut, borderColor:'blue', borderWidth: 2, fill: false, pointStyle: 'line},
                {label:'Differenz', data:diff, borderColor:'orange', borderWidth: 2, fill: false, pointStyle: 'line},
                {label:'Schwellwert', data:Array(l.length).fill(SCHWELLWERT), borderDash:[5,5], borderColor:'grey', borderWidth: 2, fill: false, pointStyle: 'line}
              ]
            },
            options: {responsive:true}
          });

          chart_humidity = new Chart(document.getElementById('chart_humidity'), {
            type: 'line',
            data: {
              labels: l,
              datasets: [
                {label:'RH Innen', data:rhIn, borderColor:'teal', borderWidth: 2, fill: false, pointStyle: 'line},
                {label:'RH Außen', data:rhOut, borderColor:'purple', borderWidth: 2, fill: false, pointStyle: 'line}
              ]
            },
            options: {responsive:true}
          });

          chart_status = new Chart(document.getElementById('chart_status'), {
            type: 'bar',
            data: {
              labels: l,
              datasets: [{
                label: 'Lüftung',
                data: status,
                backgroundColor: status.map(s => s===1 ? 'rgba(0,200,0,0.6)' : 'rgba(200,0,0,0.3)')
              }]
            },
            options: {responsive:true, scales:{y:{beginAtZero:true,max:1}}}
          });
        } else {
          chart.data.labels = l;
          chart.data.datasets[0].data = tdIn;
          chart.data.datasets[1].data = tdOut;
          chart.data.datasets[2].data = diff;
          chart.data.datasets[3].data = Array(l.length).fill(SCHWELLWERT);
          chart.update();

          chart_humidity.data.labels = l;
          chart_humidity.data.datasets[0].data = rhIn;
          chart_humidity.data.datasets[1].data = rhOut;
          chart_humidity.update();

          chart_status.data.labels = l;
          chart_status.data.datasets[0].data = status;
          chart_status.data.datasets[0].backgroundColor = status.map(s => s===1 ? 'rgba(0,200,0,0.6)' : 'rgba(200,0,0,0.3)');
          chart_status.update();
        }
      }
      setInterval(updateLiveData, 1000); // jede Sekunde
      setInterval(updateChart, 5000);    // alle 5 Sekunden

      window.onload = () => {
        updateChart();
        updateLiveData();
      };
      async function updateLiveData() {
        try {
          const res = await fetch('/livedata');
          const data = await res.json();

          document.getElementById('zeit').textContent = data.zeit;
          document.getElementById('status_text').textContent = data.status;
          document.getElementById('t_in').textContent = data.t_in.toFixed(1);
          document.getElementById('rh_in').textContent = data.rh_in.toFixed(1);
          document.getElementById('t_out').textContent = data.t_out.toFixed(1);
          document.getElementById('rh_out').textContent = data.rh_out.toFixed(1);
          document.getElementById('td_in').textContent = data.td_in.toFixed(1);
          document.getElementById('td_out').textContent = data.td_out.toFixed(1);
        } catch (e) {
          console.error("Fehler beim Live-Datenabruf:", e);
        }
      }
    </script>
  )rawliteral";
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>TaupunktBuddy</title>";
  String script = getChartScript();
  script.replace("%SCHWELLE%", String(taupunktDifferenzSchwellwert));
  html += script;
  html += "<h1>TaupunktLüftungBuddy</h1>";
  html += "<div id='live'><p><strong>Zeit:</strong> <span id='zeit'></span><br>";
  html += "<strong>Innen:</strong> <span id='t_in'></span>°C, <span id='rh_in'></span>%<br>";
  html += "<strong>Außen:</strong> <span id='t_out'></span>°C, <span id='rh_out'></span>%<br>";
  html += "<strong>Taupunkt innen:</strong> <span id='td_in'></span>°C<br>";
  html += "<strong>Taupunkt außen:</strong> <span id='td_out'></span>°C</p></div>";
  html += "<p><strong>Status:</strong> <span id='status_text'></span></p>";
  html += "<p><strong>Letztes Ereignis:</strong> " + logEintrag + "</p>";
  html += "<canvas id='chart' width='400' height='100'></canvas>";
  html += "<canvas id='chart_humidity' width='400' height='100'></canvas>";
  html += "<canvas id='chart_status' width='400' height='30'></canvas>";
  html += "<form action='/setMQTT' method='POST'><input type='submit' name='mqtt' value='MQTT aktivieren'> <input type='submit' name='mqtt' value='MQTT deaktivieren'></form>";
  html += "<form action='/setModus' method='POST'>";
  html += String("Modus innen: <select name='modus_innen'><option value='hardware'") +
         (modus_innen == "hardware" ? " selected" : "") +
         ">Hardware</option><option value='mqtt'" +
         (modus_innen == "mqtt" ? " selected" : "") +
         ">MQTT</option></select><br>";
  html += String("Modus außen: <select name='modus_aussen'><option value='hardware'") +
         (modus_aussen == "hardware" ? " selected" : "") +
         ">Hardware</option><option value='mqtt'" +
         (modus_aussen == "mqtt" ? " selected" : "") +
         ">MQTT</option></select><br>";
  html += "<input type='submit' value='Modus speichern'></form>";
  html += "<p><a href='/mqttconfig'>MQTT-Konfiguration ändern</a></p>";
  html += "<p><a href='/mqtttopics'>MQTT-Topic-Zuweisung ändern</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleMQTTConfig() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("server")) strncpy(mqttServer, server.arg("server").c_str(), sizeof(mqttServer));
    if (server.hasArg("port")) mqttPort = server.arg("port").toInt();
    if (server.hasArg("user")) strncpy(mqttUser, server.arg("user").c_str(), sizeof(mqttUser));
    if (server.hasArg("pass")) strncpy(mqttPassword, server.arg("pass").c_str(), sizeof(mqttPassword));
    saveMQTTSettings(); loadMQTTSettings(); reconnectMQTT();
    server.sendHeader("Location", "/"); server.send(303); return;
  }
  String html = "<html><head><meta charset='UTF-8'><title>MQTT Setup</title></head><body><h2>MQTT Setup</h2><form method='POST'>";
  html += "Server: <input name='server' value='" + String(mqttServer) + "'><br>";
  html += "Port: <input name='port' value='" + String(mqttPort) + "'><br>";
  html += "Benutzer: <input name='user' value='" + String(mqttUser) + "'><br>";
  html += "Passwort: <input type='password' name='pass' value='" + String(mqttPassword) + "'><br>";
  html += "<input type='submit' value='Speichern'></form><a href='/'>Zurück</a></body></html>";
  server.send(200, "text/html", html);
}

void handleMQTTTopics() {
  if (server.method() == HTTP_POST) {
    mqttTempInnen = server.arg("temp_innen");
    mqttHygroInnen = server.arg("hygro_innen");
    mqttTempAussen = server.arg("temp_aussen");
    mqttHygroAussen = server.arg("hygro_aussen");
    saveMQTTTopics(); resubscribeMQTTTopics();
    server.sendHeader("Location", "/"); server.send(303); return;
  }
  String html = "<html><head><meta charset='UTF-8'><title>MQTT Topics</title></head><body><h2>MQTT Topic-Zuweisung</h2><form method='POST'>";
  html += "Innen Temperatur: <input name='temp_innen' value='" + mqttTempInnen + "'><br>";
  html += "Innen Feuchte: <input name='hygro_innen' value='" + mqttHygroInnen + "'><br>";
  html += "Außen Temperatur: <input name='temp_aussen' value='" + mqttTempAussen + "'><br>";
  html += "Außen Feuchte: <input name='hygro_aussen' value='" + mqttHygroAussen + "'><br>";
  html += "<input type='submit' value='Speichern'></form><a href='/'>Zurück</a></body></html>";
  server.send(200, "text/html", html);
}

void handleSetModus() {
  if (server.hasArg("modus_innen")) modus_innen = server.arg("modus_innen");
  if (server.hasArg("modus_aussen")) modus_aussen = server.arg("modus_aussen");
  prefs.begin("config", false);
  prefs.putString("modus_innen", modus_innen);
  prefs.putString("modus_aussen", modus_aussen);
  prefs.end();
  server.sendHeader("Location", "/"); server.send(303);
}

void handleSetMQTT() {
  if (server.hasArg("mqtt")) {
    mqttAktiv = (server.arg("mqtt") == "MQTT aktivieren");
    prefs.begin("config", false);
    prefs.putBool("mqtt", mqttAktiv);
    prefs.end();
    if (mqttAktiv) reconnectMQTT();
  }
  server.sendHeader("Location", "/"); server.send(303);
}

void setup() {
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n[OK] WLAN verbunden: " + WiFi.localIP().toString());

  pinMode(RELAY_LED_PIN, OUTPUT);
  pinMode(STATUS_GREEN_PIN, OUTPUT);
  pinMode(STATUS_RED_PIN, OUTPUT);
  pinMode(STATUS_YELLOW_PIN, OUTPUT);
  setLEDs(false, false, false);

  shtInnen.begin(0x44);
  dht.begin();

  prefs.begin("config", true);
  taupunktDifferenzSchwellwert = prefs.getFloat("schwelle", 4.0);
  modus_innen = prefs.getString("modus_innen", "hardware");
  modus_aussen = prefs.getString("modus_aussen", "hardware");
  mqttAktiv = prefs.getBool("mqtt", true);
  prefs.end();

  loadMQTTSettings(); loadMQTTTopics();
  server.on("/mqtttopics", handleMQTTTopics);
  mqttClient.setCallback(mqttCallback);
  if (mqttAktiv) reconnectMQTT();

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
  server.on("/", handleRoot);
  server.on("/setMQTT", handleSetMQTT);
  server.on("/setModus", handleSetModus);
  server.on("/mqttconfig", handleMQTTConfig);
  server.on("/chartdata", handleChartData);
  server.on("/livedata", handleLiveData);
  server.begin();
  Serial.println("[OK] Webserver gestartet.");
}

void loop() {
  if (mqttAktiv && !mqttClient.connected()) reconnectMQTT();
  if (mqttAktiv) mqttClient.loop();
  server.handleClient();

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) { // alle 2 Sekunden
    lastUpdate = millis();
    aktualisiereSensoren();
    steuerlogik();
  }
}

