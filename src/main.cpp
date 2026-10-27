#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include <vector>
#include <String>
#include <TFT_eSPI.h>
#include <XPT2046_Bitbang.h>
#include <HTTPClient.h>

// Подключаем шрифты Ubuntu
#include "FreeSansBold32pt7b.h"
#include "FreeSansBold36pt7b.h"
#include "Ubuntu_Bold7pt7b.h"
#include "Ubuntu_Bold9pt7b.h"
#include "Ubuntu_Bold12pt7b.h"
#include "Ubuntu_Bold24pt7b.h"
#include "Ubuntu_Regular12pt7b.h"

TFT_eSPI tft = TFT_eSPI();

// Текущий экран: 0 - Главный, 1 - Настройки
int currentScreen = 0;
int settingsPage = 0; // 0=категории, 1=температура, 2=экран, 3=диагностика, 4=сеть, 5=сервис
bool redrawScreen = true;
bool sensorValid = false;
float calibrationOffset = 0.0f;
float targetStep = 0.5f;
int dimTimeoutSeconds = 30;
int nightBrightness = 35;
bool nightModeEnabled = true;
int appliedBrightness = -1;
unsigned long lastTouchMillis = 0;
unsigned long screenWakeUntil = 0;
unsigned long bootMillis = 0;
bool relayTestActive = false;
unsigned long relayTestUntil = 0;
bool relayStateBeforeTest = false;

// Touch calibration is deliberately separate from thermostat settings.
// If calibration is never completed, the original mapping remains in use.
bool touchCalibrationValid = false;
bool touchCalibrationActive = false;
bool touchCalibrationTouchLatched = false;
int touchCalibrationStep = 0;
int touchCalRawX[4] = {0, 0, 0, 0};
int touchCalRawY[4] = {0, 0, 0, 0};
String touchCalibrationMessage = "";

// Переменные интерфейса
String oldTime = "";
float oldTemp = -100;
float oldTarget = -100;
bool oldHeating = false;
bool oldWifi = false;
bool oldMqttConnected = false;
bool oldSensorValid = false;

String weatherTemp = "";
String weatherDesc = "";
String oldWeatherTemp = "";
String oldWeatherDesc = "";
uint8_t weatherIconType = 0; // 0=unknown, 1=sun, 2=cloud, 3=rain, 4=snow, 5=storm, 6=fog
uint8_t oldWeatherIconType = 255;
uint8_t timeMode = 0; // 0=automatic Czech DST, 1=winter time, 2=summer time

WiFiManager wm;
WebServer server(80);
bool isConnected = false;

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// CYD Тач-пины
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_SCLK 25
#define ONE_WIRE_BUS 22

OneWire oneWire(ONE_WIRE_BUS);  
DallasTemperature sensors(&oneWire);

// EEPROM Адреса
#define TARGET_TEMP_ADDR 200
#define BRIGHTNESS_ADDR  210
#define MQTT_CONFIG_ADDR 220
#define SETTINGS_CONFIG_ADDR 300
#define MQTT_FULL_CONFIG_ADDR 360
#define TOUCH_CALIBRATION_ADDR 496  // reserved gap after MQTT settings; fits within EEPROM(512)
#define TIME_MODE_ADDR 508          // one byte: 0=automatic, 1=winter, 2=summer

// ШИМ подсветки
#define LEDC_CHANNEL 0
#define LEDC_RESOLUTION 8
#define LEDC_FREQ 5000

// OpenWeather
#define WEATHER_API_KEY "5b09c7a00a36ece6308bbc11c96c50a6"
#define WEATHER_CITY "Valdice"

XPT2046_Bitbang ts(TOUCH_MOSI, TOUCH_MISO, TOUCH_SCLK, TOUCH_CS);

struct TouchCalibrationSettings {
  uint16_t magic;
  int16_t rawLeft;
  int16_t rawRight;
  int16_t rawTop;
  int16_t rawBottom;
};
TouchCalibrationSettings touchCal = {0, 300, 30, 220, 20};
const int TOUCH_CAL_X[4] = {25, 295, 295, 25};
const int TOUCH_CAL_Y[4] = {25, 25, 215, 215};

// Палитра (Modern Dark)
#define DARK_BG      0x0841  
#define CARD_BG      0x18E3  
#define BORDER_COLOR 0x31A6  
#define TXT_ORANGE   0xFDA0  

#define RELAY_PIN 27

WiFiClient espClient;
PubSubClient mqttClient(espClient);

char mqttServer[64] = "192.168.0.111";
int mqttPort = 1883;
char mqttUser[32] = "esp32";
char mqttPassword[32] = "Energy654.";

struct ThermostatSettings {
  uint32_t magic;
  float hysteresisLow;
  float hysteresisHigh;
  float calibrationOffset;
  int32_t dimTimeoutSeconds;
  int32_t nightBrightness;
  uint8_t nightModeEnabled;
  uint8_t targetStepTenths;
};

struct MqttSettings {
  char server[64];
  int32_t port;
  char user[32];
  char password[32];
};

const char* tempTopic = "esp32/temperature";
const char* targetTempTopic = "esp32/targetTemp";
const char* relayStatusTopic = "esp32/rele";

float hysteresisLow = 0.2;
float hysteresisHigh = 0.3;

unsigned long lastPublish = 0;
const unsigned long publishInterval = 5000; 

float currentTemp = 0.0;
float targetTemp = 22.0;
bool heatingOn = false;
int displayBrightness = 255; 

// NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600; // летнее время для Чехии

// Прототипы
void drawHomeScreen();
void drawSettingsScreen();
void checkTouch();
void handleHomeTouch(int x, int y);
void handleSettingsTouch(int x, int y);
void setupWiFi();
void readTemperature();
void saveTargetTemp();
void loadTargetTemp();
void saveBrightness();
void loadBrightness();
void saveMqttConfig();
void loadMqttConfig();
void setDisplayBrightness(int value);
void setupTime();
String getCurrentTime();
void fetchWeather();
String removeDiacritics(String str);
void checkHeating();
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool mqttConnect();
void setupWebServer();
void saveThermostatSettings();
void loadThermostatSettings();
void saveFullMqttConfig();
void loadFullMqttConfig();
void updateBrightnessMode();
void drawSettingsPage();
void startRelayTest();
void loadTouchCalibration();
void saveTouchCalibration();
void startTouchCalibration();
void processTouchCalibration(TouchPoint p);

// Веб-интерфейс со слайдером яркости
const char* webPage = R"rawliteral(
<!DOCTYPE html>
<html lang="cs">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1" charset="UTF-8">
<title>CYD Smart Termostat</title>
<style>
:root{--bg:#0d1117;--card:#161b22;--accent:#238636;--text:#e6eef8;--border:#30363d;--blue:#1f6feb}
*{box-sizing:border-box}body{margin:0;font-family:Arial,sans-serif;background:var(--bg);color:var(--text);padding:14px;display:flex;flex-direction:column;align-items:center}
.card{width:100%;max-width:520px;background:var(--card);border:1px solid var(--border);border-radius:16px;padding:18px;margin-bottom:14px;box-shadow:0 6px 18px #0005}
.title{font-size:21px;font-weight:700;text-align:center;margin:2px 0 16px;color:#58a6ff}.bigtemp{font-size:50px;font-weight:700;text-align:center;color:#58a6ff;margin:8px 0}.target{text-align:center;font-size:21px;margin:8px 0 16px}.status{text-align:center;color:#9da7b3;font-size:14px;line-height:1.7}
.row{display:flex;gap:10px;margin:10px 0}.btn{flex:1;border:0;border-radius:10px;padding:13px 10px;background:var(--accent);color:#fff;font-size:16px;font-weight:700;cursor:pointer}.blue{background:var(--blue)}.red{background:#b62324}.field{margin:12px 0}label{display:block;font-size:13px;color:#9da7b3;margin-bottom:5px}input[type=text],input[type=password],input[type=number]{width:100%;padding:11px;background:#0d1117;border:1px solid var(--border);color:white;border-radius:8px;font-size:16px}input[type=range]{width:100%;accent-color:#58a6ff}.rangehead{display:flex;justify-content:space-between;color:#9da7b3;font-size:14px;margin-bottom:8px}.checkrow{display:flex;align-items:center;gap:10px;margin:14px 0}.checkrow input{width:20px;height:20px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}.note{font-size:13px;color:#9da7b3;line-height:1.5}.msg{font-size:14px;min-height:20px;color:#7ee787;line-height:1.5}
</style>
</head>
<body>
  <div class="card">
    <div class="title" id="time">--:--:--</div>
    <div class="bigtemp" id="temp">--.- °C</div>
    <div class="target">Cílová teplota: <b><span id="target">--.-</span> °C</b></div>
    <div class="status" id="weather">🌤️ Načítám počasí…</div>
    <div class="row"><button class="btn blue" onclick="changeTemp('inc')">+0,5 °C</button><button class="btn" onclick="changeTemp('dec')">−0,5 °C</button></div>
    <div class="status" id="liveStatus">Načítám stav…</div>
  </div>

  <div class="card">
    <div class="title">Nastavení termostatu</div>
    <div class="field"><label>Hystereze zapnutí (°C)</label><input id="hLow" type="number" min="0.1" max="3" step="0.1"></div>
    <div class="field"><label>Hystereze vypnutí (°C)</label><input id="hHigh" type="number" min="0.1" max="3" step="0.1"></div>
    <div class="field"><label>Kalibrace teplotního čidla DS18B20 (°C, od −5 do +5)</label><input id="offset" type="number" min="-5" max="5" step="0.1"></div>
    <div class="field"><div class="rangehead"><span>Jas displeje</span><b id="brightVal">100%</b></div><input id="brightness" type="range" min="10" max="255" value="255" oninput="document.getElementById('brightVal').textContent=Math.round(this.value/255*100)+'%'"></div>
    <div class="checkrow"><input id="nightMode" type="checkbox"><label for="nightMode">Noční režim (22:00–06:00)</label></div>
    <div class="field"><div class="rangehead"><span>Noční jas</span><b id="nightVal">14%</b></div><input id="nightBrightness" type="range" min="10" max="255" value="35" oninput="document.getElementById('nightVal').textContent=Math.round(this.value/255*100)+'%'"></div>
    <div class="field"><label>Letní / zimní čas</label><select id="timeMode"><option value="0">Automaticky podle Česka (doporučeno)</option><option value="1">Vždy zimní čas (UTC+1)</option><option value="2">Vždy letní čas (UTC+2)</option></select></div>
    <p class="note">Automatický režim přepíná čas podle platných pravidel pro Českou republiku.</p>
    <div class="field"><label>Automatické ztlumení po nečinnosti (sekundy, 5–600)</label><input id="dimTimeout" type="number" min="5" max="600" step="5"></div>
    <p class="note">Krok změny cílové teploty je pevně nastaven na 0,5 °C.</p>
    <button class="btn" onclick="saveSettings()">Uložit nastavení</button><div class="msg" id="settingsMsg"></div>
  </div>

  <div class="card">
    <div class="title">Diagnostika a servis</div>
    <div class="status" id="diagnostics">Načítám diagnostiku…</div>
    <div class="row"><button class="btn blue" onclick="relayTest()">Test relé (3 s)</button></div>
    <div class="msg" id="serviceMsg"></div>
  </div>

  <div class="card">
    <div class="title">Konfigurace MQTT</div>
    <form id="mqttForm">
      <div class="field"><label>Server / IP</label><input name="server" id="mqttServer" type="text" maxlength="63" required></div>
      <div class="grid"><div class="field"><label>Port</label><input name="port" id="mqttPort" type="number" min="1" max="65535" required></div><div class="field"><label>Uživatel</label><input name="user" id="mqttUser" type="text" maxlength="31"></div></div>
      <div class="field"><label>Heslo (prázdné = ponechat současné)</label><input name="password" id="mqttPassword" type="password" maxlength="31" autocomplete="new-password"></div>
      <button class="btn" type="submit">Uložit MQTT</button>
    </form><div class="msg" id="netMsg"></div>
    <p class="note">Wi-Fi zařízení: <span id="ip">—</span></p>
    <button class="btn red" onclick="resetWifi()">Znovu nastavit Wi-Fi</button>
  </div>
<script>
let loaded=false;
async function fetchStatus(){try{const r=await fetch('/status',{cache:'no-store'});const j=await r.json();
 document.getElementById('temp').textContent=(j.sensorOk?Number(j.temp).toFixed(1):'--.-')+' °C';document.getElementById('target').textContent=Number(j.target).toFixed(1);document.getElementById('time').textContent=j.time;document.getElementById('weather').textContent=j.weatherEmoji+' '+j.weatherTemp+' '+j.weatherDesc;
 document.getElementById('liveStatus').textContent='Topení: '+(j.relay==='ON'?'ZAPNUTO':'VYPNUTO')+' · Čidlo: '+(j.sensorOk?'OK':'CHYBA')+' · Wi-Fi: '+(j.wifi?'připojena':'odpojena')+' · MQTT: '+(j.mqtt?'připojeno':'odpojeno');
 document.getElementById('diagnostics').innerHTML='Čidlo: <b>'+(j.sensorOk?'OK':'CHYBA')+'</b><br>Relé: <b>'+j.relay+'</b><br>Wi-Fi: '+(j.wifi?'připojena':'odpojena')+' · RSSI: '+j.rssi+' dBm<br>MQTT: '+(j.mqtt?'připojeno':'odpojeno')+'<br>IP: '+j.ip+'<br>Doba běhu: '+j.uptime+' min';document.getElementById('ip').textContent=j.ip;
 if(!loaded){document.getElementById('hLow').value=j.hLow;document.getElementById('hHigh').value=j.hHigh;document.getElementById('offset').value=j.offset;document.getElementById('brightness').value=j.brightness;document.getElementById('brightVal').textContent=Math.round(j.brightness/255*100)+'%';document.getElementById('nightMode').checked=j.nightMode;document.getElementById('nightBrightness').value=j.nightBrightness;document.getElementById('nightVal').textContent=Math.round(j.nightBrightness/255*100)+'%';document.getElementById('dimTimeout').value=j.dimTimeout;document.getElementById('timeMode').value=j.timeMode;document.getElementById('mqttServer').value=j.mqttServer;document.getElementById('mqttPort').value=j.mqttPort;document.getElementById('mqttUser').value=j.mqttUser;loaded=true;}
 }catch(e){document.getElementById('liveStatus').textContent='Nepodařilo se načíst stav zařízení.'}}
async function changeTemp(action){await fetch('/'+action,{method:'POST'});fetchStatus()}
async function saveSettings(){const data=new URLSearchParams({hLow:document.getElementById('hLow').value,hHigh:document.getElementById('hHigh').value,offset:document.getElementById('offset').value,brightness:document.getElementById('brightness').value,nightMode:document.getElementById('nightMode').checked?'1':'0',nightBrightness:document.getElementById('nightBrightness').value,dimTimeout:document.getElementById('dimTimeout').value,timeMode:document.getElementById('timeMode').value});const r=await fetch('/save-settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data});document.getElementById('settingsMsg').textContent=r.ok?'Nastavení uloženo.':'Chyba: zkontrolujte zadané hodnoty.';if(r.ok){loaded=false;fetchStatus()}}
async function relayTest(){const r=await fetch('/relay-test',{method:'POST'});document.getElementById('serviceMsg').textContent=r.ok?'Test relé spuštěn na 3 sekundy.':await r.text();fetchStatus()}
document.getElementById('mqttForm').addEventListener('submit',async e=>{e.preventDefault();const data=new URLSearchParams(new FormData(e.target));const r=await fetch('/mqtt-config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data});document.getElementById('netMsg').textContent=r.ok?'MQTT nastavení uloženo.':'Chyba ukládání MQTT.';if(r.ok){document.getElementById('mqttPassword').value='';loaded=false;fetchStatus()}});
async function resetWifi(){if(confirm('Zařízení se restartuje a znovu otevře Wi-Fi konfigurační portál. Pokračovat?')){await fetch('/wifi-reset',{method:'POST'});document.getElementById('netMsg').textContent='Zařízení se restartuje.'}}
setInterval(fetchStatus,2500);window.onload=fetchStatus;
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  if (!EEPROM.begin(512)) Serial.println("EEPROM Failed");
  loadTargetTemp();
  loadBrightness();
  loadMqttConfig();
  loadThermostatSettings();
  loadFullMqttConfig();
  loadTouchCalibration();
  uint8_t storedTimeMode = EEPROM.read(TIME_MODE_ADDR);
  timeMode = (storedTimeMode <= 2) ? storedTimeMode : 0;
  bootMillis = millis();
  lastTouchMillis = millis();
  
  sensors.begin();
  readTemperature();
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  heatingOn = false;

  tft.init();
  tft.setRotation(1);
  tft.setAttribute(UTF8_SWITCH, true); 
  tft.fillScreen(DARK_BG);
  
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RESOLUTION);
  ledcAttachPin(TFT_BL, LEDC_CHANNEL);
  setDisplayBrightness(displayBrightness);
  appliedBrightness = displayBrightness; 
  
  ts.begin();
  setupWiFi();
  setupTime();
  setupWebServer(); 
   
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
  
  fetchWeather();
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() { server.send(200, "text/html; charset=utf-8", webPage); });
  server.on("/status", HTTP_GET, []() {
    String ip = isConnected ? WiFi.localIP().toString() : String("—");
    long rssi = isConnected ? WiFi.RSSI() : 0;
    String payload = "{\"temp\":" + String(currentTemp, 1) +
      ",\"sensorOk\":" + String(sensorValid ? "true" : "false") +
      ",\"relay\":\"" + String(heatingOn ? "ON" : "OFF") + "\"" +
      ",\"target\":" + String(targetTemp, 1) +
      ",\"time\":\"" + getCurrentTime() + "\"" +
      ",\"timeMode\":" + String(timeMode) +
      ",\"weatherEmoji\":\"" + String(weatherIconType == 1 ? "☀️" : weatherIconType == 2 ? "☁️" : weatherIconType == 3 ? "🌧️" : weatherIconType == 4 ? "❄️" : weatherIconType == 5 ? "⛈️" : weatherIconType == 6 ? "🌫️" : "🌦️") + "\"" +
      ",\"weatherTemp\":\"" + weatherTemp + "\"" +
      ",\"weatherDesc\":\"" + weatherDesc + "\"" +
      ",\"brightness\":" + String(displayBrightness) +
      ",\"hLow\":" + String(hysteresisLow, 1) +
      ",\"hHigh\":" + String(hysteresisHigh, 1) +
      ",\"offset\":" + String(calibrationOffset, 1) +
      ",\"nightMode\":" + String(nightModeEnabled ? "true" : "false") +
      ",\"nightBrightness\":" + String(nightBrightness) +
      ",\"dimTimeout\":" + String(dimTimeoutSeconds) +
      ",\"wifi\":" + String(isConnected && WiFi.status() == WL_CONNECTED ? "true" : "false") +
      ",\"mqtt\":" + String(mqttClient.connected() ? "true" : "false") +
      ",\"rssi\":" + String(rssi) +
      ",\"ip\":\"" + ip + "\"" +
      ",\"uptime\":" + String(millis() / 60000UL) +
      ",\"mqttServer\":\"" + String(mqttServer) + "\"" +
      ",\"mqttPort\":" + String(mqttPort) +
      ",\"mqttUser\":\"" + String(mqttUser) + "\"}";
    server.send(200, "application/json", payload);
  });
  server.on("/inc", HTTP_POST, []() {
    targetTemp = min(35.0f, targetTemp + 0.5f);
    saveTargetTemp();
    if (mqttClient.connected()) mqttClient.publish(targetTempTopic, String(targetTemp, 1).c_str(), true);
    server.send(200, "text/plain", "ok");
  });
  server.on("/dec", HTTP_POST, []() {
    targetTemp = max(5.0f, targetTemp - 0.5f);
    saveTargetTemp();
    if (mqttClient.connected()) mqttClient.publish(targetTempTopic, String(targetTemp, 1).c_str(), true);
    server.send(200, "text/plain", "ok");
  });
  server.on("/set_brightness", HTTP_POST, []() {
    if (server.hasArg("value")) {
      int val = server.arg("value").toInt();
      if (val >= 10 && val <= 255) {
        displayBrightness = val; saveBrightness(); updateBrightnessMode();
      }
    }
    server.send(200, "text/plain", "ok");
  });
  server.on("/save-settings", HTTP_POST, []() {
    if (!server.hasArg("hLow") || !server.hasArg("hHigh") || !server.hasArg("offset") ||
        !server.hasArg("brightness") || !server.hasArg("nightBrightness") || !server.hasArg("dimTimeout") || !server.hasArg("timeMode")) {
      server.send(400, "text/plain", "Chybí povinné hodnoty"); return;
    }
    float low = server.arg("hLow").toFloat();
    float high = server.arg("hHigh").toFloat();
    float offset = server.arg("offset").toFloat();
    int bright = server.arg("brightness").toInt();
    int nightBright = server.arg("nightBrightness").toInt();
    int dim = server.arg("dimTimeout").toInt();
    int requestedTimeMode = server.arg("timeMode").toInt();
    if (!isfinite(low) || low < 0.1f || low > 3.0f || !isfinite(high) || high < 0.1f || high > 3.0f ||
        !isfinite(offset) || offset < -5.0f || offset > 5.0f || bright < 10 || bright > 255 ||
        nightBright < 10 || nightBright > 255 || dim < 5 || dim > 600 || requestedTimeMode < 0 || requestedTimeMode > 2) {
      server.send(400, "text/plain", "Hodnoty mimo povolený rozsah"); return;
    }
    hysteresisLow = low; hysteresisHigh = high; calibrationOffset = offset;
    displayBrightness = bright; nightBrightness = nightBright; dimTimeoutSeconds = dim;
    nightModeEnabled = server.hasArg("nightMode") && server.arg("nightMode") == "1";
    timeMode = (uint8_t)requestedTimeMode;
    EEPROM.write(TIME_MODE_ADDR, timeMode); EEPROM.commit();
    setupTime();
    targetStep = 0.5f; // Fixed step: always half a degree.
    saveBrightness(); saveThermostatSettings(); readTemperature(); updateBrightnessMode();
    server.send(200, "text/plain", "ok");
  });
  server.on("/relay-test", HTTP_POST, []() {
    if (!sensorValid) { server.send(409, "text/plain", "Test relé nelze spustit: teplotní čidlo není v pořádku."); return; }
    if (relayTestActive) { server.send(409, "text/plain", "Test relé už probíhá."); return; }
    startRelayTest();
    server.send(200, "text/plain", "ok");
  });
  server.on("/mqtt-config", HTTP_POST, []() {
    if (server.hasArg("server") && server.arg("server").length() > 0) {
      String v = server.arg("server"); v.trim();
      if (v.length() >= sizeof(mqttServer)) { server.send(400, "text/plain", "Adresa MQTT serveru je příliš dlouhá"); return; }
      v.toCharArray(mqttServer, sizeof(mqttServer));
    }
    if (server.hasArg("port") && server.arg("port").length() > 0) {
      int portValue = server.arg("port").toInt();
      if (portValue < 1 || portValue > 65535) { server.send(400, "text/plain", "Neplatný MQTT port"); return; }
      mqttPort = portValue;
    }
    if (server.hasArg("user") && server.arg("user").length() > 0) {
      String v = server.arg("user");
      if (v.length() >= sizeof(mqttUser)) { server.send(400, "text/plain", "Uživatelské jméno je příliš dlouhé"); return; }
      v.toCharArray(mqttUser, sizeof(mqttUser));
    }
    if (server.hasArg("password") && server.arg("password").length() > 0) {
      String v = server.arg("password");
      if (v.length() >= sizeof(mqttPassword)) { server.send(400, "text/plain", "Heslo je příliš dlouhé"); return; }
      v.toCharArray(mqttPassword, sizeof(mqttPassword));
    }
    saveFullMqttConfig(); mqttClient.disconnect(); mqttClient.setServer(mqttServer, mqttPort);
    server.send(200, "text/plain", "ok");
  });
  server.on("/wifi-reset", HTTP_POST, []() {
    server.send(200, "text/plain", "restarting");
    delay(300); wm.resetSettings(); ESP.restart();
  });
  server.begin();
}
void setDisplayBrightness(int value) {
  value = constrain(value, 0, 255);
  ledcWrite(LEDC_CHANNEL, value);
  appliedBrightness = value;
}
void saveBrightness() { EEPROM.put(BRIGHTNESS_ADDR, displayBrightness); EEPROM.commit(); }
void loadBrightness() { EEPROM.get(BRIGHTNESS_ADDR, displayBrightness); if (displayBrightness < 10 || displayBrightness > 255) displayBrightness = 255; }
void saveMqttConfig() { EEPROM.put(MQTT_CONFIG_ADDR, mqttServer); EEPROM.commit(); }
void loadMqttConfig() {
  char stored[64] = {0};
  EEPROM.get(MQTT_CONFIG_ADDR, stored);
  if (stored[0] != '\0' && strnlen(stored, sizeof(stored)) < sizeof(stored)) {
    strncpy(mqttServer, stored, sizeof(mqttServer) - 1);
    mqttServer[sizeof(mqttServer) - 1] = '\0';
  }
}
void saveThermostatSettings() {
  ThermostatSettings cfg = {0x43594431UL, hysteresisLow, hysteresisHigh, calibrationOffset,
    dimTimeoutSeconds, nightBrightness, (uint8_t)nightModeEnabled,
    (uint8_t)round(targetStep * 10.0f)};
  EEPROM.put(SETTINGS_CONFIG_ADDR, cfg);
  EEPROM.commit();
}
void loadThermostatSettings() {
  ThermostatSettings cfg;
  EEPROM.get(SETTINGS_CONFIG_ADDR, cfg);
  if (cfg.magic != 0x43594431UL || !isfinite(cfg.hysteresisLow) || cfg.hysteresisLow < 0.1f || cfg.hysteresisLow > 3.0f ||
      !isfinite(cfg.hysteresisHigh) || cfg.hysteresisHigh < 0.1f || cfg.hysteresisHigh > 3.0f ||
      !isfinite(cfg.calibrationOffset) || cfg.calibrationOffset < -5.0f || cfg.calibrationOffset > 5.0f ||
      cfg.dimTimeoutSeconds < 5 || cfg.dimTimeoutSeconds > 600 || cfg.nightBrightness < 10 || cfg.nightBrightness > 255 || cfg.targetStepTenths < 1 || cfg.targetStepTenths > 10) {
    hysteresisLow = 0.2f; hysteresisHigh = 0.3f; calibrationOffset = 0.0f;
    dimTimeoutSeconds = 30; nightBrightness = 35; nightModeEnabled = true; targetStep = 0.5f;
    saveThermostatSettings();
    return;
  }
  hysteresisLow = cfg.hysteresisLow; hysteresisHigh = cfg.hysteresisHigh;
  calibrationOffset = cfg.calibrationOffset; dimTimeoutSeconds = cfg.dimTimeoutSeconds;
  nightBrightness = cfg.nightBrightness; nightModeEnabled = cfg.nightModeEnabled != 0;
  targetStep = 0.5f; // Fixed target adjustment step: always 0.5 C.
}
void saveFullMqttConfig() {
  MqttSettings cfg = {};
  strncpy(cfg.server, mqttServer, sizeof(cfg.server)-1);
  cfg.port = mqttPort;
  strncpy(cfg.user, mqttUser, sizeof(cfg.user)-1);
  strncpy(cfg.password, mqttPassword, sizeof(cfg.password)-1);
  EEPROM.put(MQTT_FULL_CONFIG_ADDR, cfg);
  EEPROM.commit();
}
void loadFullMqttConfig() {
  MqttSettings cfg;
  EEPROM.get(MQTT_FULL_CONFIG_ADDR, cfg);
  if (cfg.server[0] == '\0' || strnlen(cfg.server, sizeof(cfg.server)) >= sizeof(cfg.server) || cfg.port < 1 || cfg.port > 65535) return;
  strncpy(mqttServer, cfg.server, sizeof(mqttServer)-1); mqttServer[sizeof(mqttServer)-1] = '\0';
  mqttPort = cfg.port;
  if (strnlen(cfg.user, sizeof(cfg.user)) < sizeof(cfg.user)) { strncpy(mqttUser, cfg.user, sizeof(mqttUser)-1); mqttUser[sizeof(mqttUser)-1]='\0'; }
  if (strnlen(cfg.password, sizeof(cfg.password)) < sizeof(cfg.password)) { strncpy(mqttPassword, cfg.password, sizeof(mqttPassword)-1); mqttPassword[sizeof(mqttPassword)-1]='\0'; }
}
void updateBrightnessMode() {
  int wanted = displayBrightness;
  unsigned long now = millis();
  // После касания на 10 секунд возвращаем обычную яркость, в том числе ночью.
  if (screenWakeUntil != 0 && (long)(screenWakeUntil - now) > 0) {
    wanted = displayBrightness;
  } else {
    struct tm ti;
    bool isNight = false;
    if (nightModeEnabled && getLocalTime(&ti, 10)) isNight = (ti.tm_hour >= 22 || ti.tm_hour < 6);
    if (isNight) wanted = min(wanted, nightBrightness);
    else if (dimTimeoutSeconds > 0 && now - lastTouchMillis >= (unsigned long)dimTimeoutSeconds * 1000UL)
      wanted = min(wanted, max(10, displayBrightness / 5));
  }
  if (wanted != appliedBrightness) setDisplayBrightness(wanted);
}
void startRelayTest() {
  if (relayTestActive || !sensorValid) return;
  relayStateBeforeTest = heatingOn;
  relayTestActive = true;
  relayTestUntil = millis() + 3000UL;
  heatingOn = !relayStateBeforeTest;
  digitalWrite(RELAY_PIN, heatingOn ? LOW : HIGH);
}

void drawWeatherIcon(int cx, int cy, uint8_t type) {
  // Compact pixel-art icons sized for the weather card on the 320x240 CYD.
  const uint16_t sun = 0xFFE0, cloud = 0xC618, rain = 0x07FF, snow = 0xFFFF, storm = 0xFD20;
  if (type == 1) { // clear / sun
    tft.fillCircle(cx, cy, 7, sun);
    for (int a = 0; a < 8; ++a) {
      float ang = a * 0.785398f;
      int x1 = cx + (int)(10 * cosf(ang)), y1 = cy + (int)(10 * sinf(ang));
      int x2 = cx + (int)(13 * cosf(ang)), y2 = cy + (int)(13 * sinf(ang));
      tft.drawLine(x1, y1, x2, y2, sun);
    }
  } else if (type == 2 || type == 3 || type == 4 || type == 5 || type == 6) {
    // Cloud body
    tft.fillCircle(cx - 5, cy - 1, 6, cloud);
    tft.fillCircle(cx + 2, cy - 5, 8, cloud);
    tft.fillCircle(cx + 9, cy, 5, cloud);
    tft.fillRoundRect(cx - 9, cy - 1, 21, 8, 4, cloud);
    if (type == 3) {
      tft.drawLine(cx - 5, cy + 9, cx - 7, cy + 13, rain);
      tft.drawLine(cx + 2, cy + 9, cx, cy + 13, rain);
      tft.drawLine(cx + 9, cy + 9, cx + 7, cy + 13, rain);
    } else if (type == 4) {
      tft.drawLine(cx - 5, cy + 9, cx - 5, cy + 14, snow); tft.drawLine(cx - 8, cy + 11, cx - 2, cy + 12, snow);
      tft.drawLine(cx + 4, cy + 9, cx + 4, cy + 14, snow); tft.drawLine(cx + 1, cy + 11, cx + 7, cy + 12, snow);
    } else if (type == 5) {
      tft.fillTriangle(cx + 1, cy + 6, cx - 3, cy + 13, cx + 1, cy + 12, storm);
      tft.fillTriangle(cx + 1, cy + 12, cx + 6, cy + 7, cx + 2, cy + 8, storm);
    } else if (type == 6) {
      tft.drawFastHLine(cx - 12, cy + 10, 24, cloud);
      tft.drawFastHLine(cx - 10, cy + 13, 20, cloud);
    }
  } else { // unknown / drizzle: simple cloud with drops
    tft.fillCircle(cx - 5, cy - 1, 6, cloud); tft.fillCircle(cx + 2, cy - 5, 8, cloud);
    tft.fillCircle(cx + 9, cy, 5, cloud); tft.fillRoundRect(cx - 9, cy - 1, 21, 8, 4, cloud);
    tft.drawLine(cx - 4, cy + 9, cx - 6, cy + 12, rain);
    tft.drawLine(cx + 4, cy + 9, cx + 2, cy + 12, rain);
  }
}

void drawHomeScreen() {
  if (redrawScreen) {
    tft.fillScreen(DARK_BG);

    // Большая верхняя панель часов. Остальные информационные блоки опущены ниже.
    tft.fillRoundRect(10, 5, 225, 70, 10, CARD_BG);
    tft.drawRoundRect(10, 5, 225, 70, 10, BORDER_COLOR);

    // SET оставляем в прежнем положении.
    tft.fillRoundRect(245, 8, 65, 54, 8, 0x3186);
    tft.drawRoundRect(245, 8, 65, 54, 8, BORDER_COLOR);
    tft.setFreeFont(&Ubuntu_Bold12pt7b);
    tft.setTextColor(TFT_WHITE, 0x3186);
    tft.drawCentreString("SET", 277, 25, 1);

    // Температурные карточки перенесены ниже увеличенных часов.
    tft.fillRoundRect(10, 81, 105, 61, 8, CARD_BG);
    tft.drawRoundRect(10, 81, 105, 61, 8, BORDER_COLOR);
    tft.fillRoundRect(122, 81, 113, 61, 8, CARD_BG);
    tft.drawRoundRect(122, 81, 113, 61, 8, BORDER_COLOR);

    tft.setFreeFont(&Ubuntu_Bold7pt7b);
    tft.setTextColor(TFT_LIGHTGREY, CARD_BG);
    tft.drawCentreString("POKOJ", 62, 86, 1);
    tft.drawCentreString("CILOVA", 178, 86, 1);

    // Статус отопления — ниже температур.
    tft.fillRoundRect(10, 148, 225, 30, 8, CARD_BG);
    tft.drawRoundRect(10, 148, 225, 30, 8, BORDER_COLOR);

    // КНОПКИ + И -: положение, размер, цвет и touch-зоны не изменять.
    tft.fillRoundRect(245, 68, 65, 58, 8, 0xF800);
    tft.drawRoundRect(245, 68, 65, 58, 8, BORDER_COLOR);
    tft.fillRoundRect(245, 133, 65, 58, 8, 0x001F);
    tft.drawRoundRect(245, 133, 65, 58, 8, BORDER_COLOR);
    tft.setFreeFont(&Ubuntu_Bold24pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("+", 267, 78);
    tft.drawString("-", 271, 143);

    // Погода внизу.
    tft.fillRoundRect(10, 185, 225, 38, 8, CARD_BG);
    tft.drawRoundRect(10, 185, 225, 38, 8, BORDER_COLOR);

    oldTime = "";
    oldTemp = -100;
    oldTarget = -100;
    oldHeating = !heatingOn;
    oldWifi = !isConnected;
    oldMqttConnected = !mqttClient.connected();
    oldWeatherTemp = "";
    oldWeatherDesc = "";
    redrawScreen = false;
  }

  // Большие часы: рисуем HH:MM целиком одним шрифтом, без отдельного двоеточия.
  String timeStr = getCurrentTime();
  String clockText = timeStr.substring(0, 5);
  if (clockText != oldTime) {
    // Область очистки не доходит до индикаторов справа.
    tft.fillRect(15, 9, 184, 61, CARD_BG);
    tft.setFreeFont(&FreeSansBold36pt7b);
    tft.setTextColor(TFT_WHITE, CARD_BG);
    tft.drawCentreString(clockText, 107, 8, 1);
    oldTime = clockText;
  }

  // Комнатная температура.
  if (abs(currentTemp - oldTemp) > 0.05) {
    tft.fillRect(15, 104, 95, 34, CARD_BG);
    tft.setFreeFont(&Ubuntu_Bold12pt7b);
    tft.setTextColor(0x07FF, CARD_BG);
    tft.drawCentreString(String(currentTemp, 1) + " C", 62, 108, 1);
    oldTemp = currentTemp;
  }

  // Целевая температура.
  if (abs(targetTemp - oldTarget) > 0.05) {
    tft.fillRect(127, 104, 103, 34, CARD_BG);
    tft.setFreeFont(&Ubuntu_Bold12pt7b);
    tft.setTextColor(0xFFE0, CARD_BG);
    tft.drawCentreString(String(targetTemp, 1) + " C", 178, 108, 1);
    oldTarget = targetTemp;
  }

  // Статус отопления.
  if (heatingOn != oldHeating) {
    tft.fillRoundRect(13, 151, 219, 24, 5, CARD_BG);
    tft.setFreeFont(&Ubuntu_Bold9pt7b);
    if (heatingOn) {
      tft.setTextColor(TXT_ORANGE, CARD_BG);
      tft.drawString("TOPENI  ZAPNUTO", 20, 154);
    } else {
      tft.setTextColor(0x07E0, CARD_BG);
      tft.drawString("TOPENI  VYPNUTO", 20, 154);
    }
    oldHeating = heatingOn;
  }

  // Wi-Fi и MQTT остаются в верхней панели справа от часов.
  if (isConnected != oldWifi) {
    tft.fillCircle(211, 23, 5, isConnected ? 0x07E0 : 0xF800);
    oldWifi = isConnected;
  }

  bool mqttOk = mqttClient.connected();
  if (mqttOk != oldMqttConnected) {
    tft.drawCircle(211, 23, 8, mqttOk ? 0x001F : 0x7BEF);
    tft.drawCircle(211, 23, 9, mqttOk ? 0x001F : 0x7BEF);
    oldMqttConnected = mqttOk;
  }

  // Погода с пиктограммой, соответствующей коду OpenWeather.
  if (weatherTemp != oldWeatherTemp || weatherDesc != oldWeatherDesc || weatherIconType != oldWeatherIconType) {
    tft.fillRect(13, 187, 219, 33, CARD_BG);
    drawWeatherIcon(32, 202, weatherIconType);
    tft.setFreeFont(&Ubuntu_Bold9pt7b);
    tft.setTextColor(TFT_WHITE, CARD_BG);
    String weatherLine = weatherTemp + "  " + weatherDesc;
    if (weatherLine.length() > 24) weatherLine = weatherLine.substring(0, 23) + "…";
    tft.drawCentreString(weatherLine, 140, 196, 1);
    oldWeatherTemp = weatherTemp;
    oldWeatherDesc = weatherDesc;
    oldWeatherIconType = weatherIconType;
  }
}
void drawSettingsScreen() {
  if (redrawScreen) {
    tft.fillScreen(DARK_BG);
    // The back button is hidden during calibration because the first target
    // deliberately occupies the top-left corner.
    if (settingsPage != 6) {
      tft.fillRoundRect(10, 10, 50, 45, 6, 0x8400);
      tft.drawRoundRect(10, 10, 50, 45, 6, BORDER_COLOR);
      tft.setFreeFont(&Ubuntu_Bold12pt7b);
      tft.setTextColor(TFT_WHITE, DARK_BG);
      tft.drawString("<-", 22, 22);
    }
    if (settingsPage == 0) {
      tft.drawString("NASTAVENI", 80, 22);
      const char* labels[] = {"TEPLOTA", "DISPLEJ", "DIAGNOSTIKA", "SIT / MQTT", "SERVIS"};
      const int xs[] = {10,165,10,165,10};
      const int ys[] = {65,65,125,125,185};
      const int ws[] = {145,145,145,145,300};
      const int hs[] = {50,50,50,50,43};
      for (int i=0;i<5;i++) {
        tft.fillRoundRect(xs[i],ys[i],ws[i],hs[i],7,CARD_BG);
        tft.drawRoundRect(xs[i],ys[i],ws[i],hs[i],7,BORDER_COLOR);
        tft.setFreeFont(&Ubuntu_Bold9pt7b);
        tft.setTextColor(i==4 ? TXT_ORANGE : TFT_WHITE,CARD_BG);
        tft.drawCentreString(labels[i],xs[i]+ws[i]/2,ys[i]+(hs[i]-15)/2,1);
      }
    } else {
      const char* title = settingsPage==1 ? "TEPLOTA" : settingsPage==2 ? "DISPLEJ" : settingsPage==3 ? "DIAGNOSTIKA" : settingsPage==4 ? "SIT / MQTT" : settingsPage==5 ? "SERVIS" : "KALIBRACE DOTYKU";
      tft.drawString(title, 80, 22);
      tft.setFreeFont(&Ubuntu_Bold9pt7b);
      tft.setTextColor(TFT_WHITE,DARK_BG);
      if (settingsPage == 1) {
        tft.drawString("Cil: " + String(targetTemp,1) + " C  krok " + String(targetStep,1), 18, 70);
        tft.drawString("Hyst. zap: " + String(hysteresisLow,1) + " C", 18, 105);
        tft.drawString("Hyst. vyp: " + String(hysteresisHigh,1) + " C", 18, 140);
        tft.drawString("Kalibrace: " + String(calibrationOffset,1) + " C", 18, 175);
        tft.drawString("Dotyk vlevo = minus, vpravo = plus", 18, 212);
      } else if (settingsPage == 2) {
        tft.drawString("Jas: " + String(displayBrightness*100/255) + "%", 18, 70);
        tft.drawString("Nocni rezim: " + String(nightModeEnabled ? "ZAP" : "VYP"), 18, 105);
        tft.drawString("Nocni jas: " + String(nightBrightness*100/255) + "%", 18, 140);
        tft.drawString("Ztmaveni: " + String(dimTimeoutSeconds) + " s", 18, 175);
        tft.setTextColor(0x07FF,DARK_BG);
        tft.drawString(touchCalibrationMessage.length() ? touchCalibrationMessage : "Kalibrace dotyku: stiskni zde", 18, 212);
      } else if (settingsPage == 3) {
        tft.drawString("DS18B20: " + String(sensorValid ? "OK" : "CHYBA"), 18, 67);
        tft.drawString("Teplota: " + String(sensorValid ? currentTemp : -127.0f,1) + " C", 18, 91);
        tft.drawString("Rele: " + String(heatingOn ? "ZAPNUTO" : "VYPNUTO"), 18, 115);
        tft.drawString("Wi-Fi: " + String(WiFi.status()==WL_CONNECTED ? "OK" : "ODPOJENO"), 18, 139);
        tft.drawString("MQTT: " + String(mqttClient.connected() ? "OK" : "ODPOJENO"), 18, 163);
        tft.drawString("IP: " + WiFi.localIP().toString(), 18, 187);
        tft.drawString("Uptime: " + String((millis()-bootMillis)/60000UL) + " min", 18, 211);
      } else if (settingsPage == 4) {
        tft.drawString("Wi-Fi: " + String(WiFi.status()==WL_CONNECTED ? "PRIPOJENO" : "ODPOJENO"), 18, 70);
        tft.drawString("IP: " + WiFi.localIP().toString(), 18, 100);
        tft.drawString("MQTT: " + String(mqttServer), 18, 130);
        tft.drawString("Port: " + String(mqttPort), 18, 155);
        tft.drawString("Web nastaveni: http://" + WiFi.localIP().toString(), 18, 185);
        tft.drawString("Wi-Fi reset pres webove rozhrani", 18, 212);
      } else if (settingsPage == 5) {
        tft.drawString("Test rele na 3 sekundy", 18, 75);
        tft.drawString("Pouzivej jen pri bezpecnem servisu", 18, 100);
        tft.drawString("Teplota senzoru: " + String(sensorValid ? "OK" : "CHYBA"), 18, 135);
        tft.fillRoundRect(20, 165, 280, 45, 7, 0x7800);
        tft.drawRoundRect(20, 165, 280, 45, 7, BORDER_COLOR);
        tft.setTextColor(TFT_WHITE,0x7800);
        tft.drawCentreString("TEST RELE",160,179,1);
      } else if (settingsPage == 6) {
        tft.setFreeFont(&Ubuntu_Bold9pt7b);
        tft.setTextColor(TFT_WHITE, DARK_BG);
        if (touchCalibrationActive) {
          tft.drawCentreString("Dotkni se krize a pak uvolni", 160, 58, 1);
          tft.drawCentreString(String("Bod ") + String(touchCalibrationStep + 1) + " / 4", 160, 82, 1);
          tft.drawCentreString("Nemen nastaveni pri kalibraci", 160, 103, 1);
          int cx = TOUCH_CAL_X[touchCalibrationStep];
          int cy = TOUCH_CAL_Y[touchCalibrationStep];
          tft.drawLine(cx - 12, cy, cx + 12, cy, TFT_CYAN);
          tft.drawLine(cx, cy - 12, cx, cy + 12, TFT_CYAN);
          tft.drawCircle(cx, cy, 5, TFT_WHITE);
        } else {
          tft.drawCentreString(touchCalibrationMessage.length() ? touchCalibrationMessage : "Kalibrace dotyku", 160, 95, 1);
          tft.drawCentreString("Tap pro opakovani", 160, 125, 1);
        }
      }
      if (settingsPage==1 || settingsPage==2) {
        tft.setTextColor(0x07FF,DARK_BG);
        tft.drawString("stisk vlevo - / vpravo +", 18, 226);
      }
    }
    redrawScreen = false;
  }
  // обновляем живые данные на странице диагностики без перерисовки интерфейса целиком
  static unsigned long lastDiagRefresh = 0;
  if (settingsPage == 3 && !redrawScreen && millis() - lastDiagRefresh >= 1000UL) {
    lastDiagRefresh = millis();
    tft.fillRect(15,60,225,165,CARD_BG);
    tft.setFreeFont(&Ubuntu_Bold9pt7b); tft.setTextColor(TFT_WHITE,CARD_BG);
    tft.drawString("DS18B20: " + String(sensorValid ? "OK" : "CHYBA"),18,67);
    tft.drawString("Teplota: " + String(sensorValid ? currentTemp : -127.0f,1) + " C",18,91);
    tft.drawString("Rele: " + String(heatingOn ? "ZAPNUTO" : "VYPNUTO"),18,115);
    tft.drawString("Wi-Fi: " + String(WiFi.status()==WL_CONNECTED ? "OK" : "ODPOJENO"),18,139);
    tft.drawString("MQTT: " + String(mqttClient.connected() ? "OK" : "ODPOJENO"),18,163);
    tft.drawString("IP: " + WiFi.localIP().toString(),18,187);
    tft.drawString("Uptime: " + String((millis()-bootMillis)/60000UL) + " min",18,211);
  }
}

void checkTouch() {
  TouchPoint p = ts.getTouch();

  // Calibration has its own raw-touch path. Do not pass calibration taps to
  // normal UI hit-testing, so the + / - buttons and existing hit zones cannot move.
  if (touchCalibrationActive) {
    if (p.x == 0 && p.y == 0) {
      touchCalibrationTouchLatched = false;
    } else if (!touchCalibrationTouchLatched) {
      touchCalibrationTouchLatched = true;
      lastTouchMillis = millis();
      screenWakeUntil = lastTouchMillis + 10000UL;
      processTouchCalibration(p);
      delay(180);
    }
    return;
  }

  if (p.x != 0 || p.y != 0) {
    lastTouchMillis = millis();
    screenWakeUntil = lastTouchMillis + 10000UL;
    updateBrightnessMode();
    int displayX;
    int displayY;
    if (touchCalibrationValid) {
      displayX = map(p.x, touchCal.rawLeft, touchCal.rawRight, 25, 295);
      displayY = map(p.y, touchCal.rawTop, touchCal.rawBottom, 25, 215);
    } else {
      // Original factory mapping preserved exactly until user completes calibration.
      displayX = map(p.x, 300, 30, 0, 320);
      displayY = map(p.y, 220, 20, 0, 240);
    }
    if(displayX > 0 && displayX < 320 && displayY > 0 && displayY < 240) {
      if (currentScreen == 0) handleHomeTouch(displayX, displayY);
      else handleSettingsTouch(displayX, displayY);
      delay(200);
    }
  }
}

void handleHomeTouch(int x, int y) {
  // Кнопка SET
  if (x >= 245 && x <= 310 && y >= 8 && y <= 62) {
    currentScreen = 1;
    redrawScreen = true;
    return;
  }
  // Кнопка Плюс (+) - НЕ ТРОНУТА
  if (x >= 245 && x <= 310 && y >= 68 && y <= 126) {
    targetTemp = min(35.0f, targetTemp + targetStep);
    saveTargetTemp();
    if (mqttClient.connected()) mqttClient.publish(targetTempTopic, String(targetTemp, 1).c_str(), true);
  }
  // Кнопка Минус (-) - НЕ ТРОНУТА
  if (x >= 245 && x <= 310 && y >= 133 && y <= 191) {
    targetTemp = max(5.0f, targetTemp - targetStep);
    saveTargetTemp();
    if (mqttClient.connected()) mqttClient.publish(targetTempTopic, String(targetTemp, 1).c_str(), true);
  }
}

void handleSettingsTouch(int x, int y) {
  if (x >= 10 && x <= 60 && y >= 10 && y <= 55) {
    if (settingsPage != 0) settingsPage = 0;
    else { currentScreen = 0; oldTime=""; oldTemp=-100; oldTarget=-100; oldHeating=!heatingOn; oldWeatherTemp=""; }
    redrawScreen = true;
    return;
  }
  if (settingsPage == 0) {
    if (y >= 65 && y <= 115 && x >= 10 && x <= 155) settingsPage = 1;
    else if (y >= 65 && y <= 115 && x >= 165 && x <= 310) settingsPage = 2;
    else if (y >= 125 && y <= 175 && x >= 10 && x <= 155) settingsPage = 3;
    else if (y >= 125 && y <= 175 && x >= 165 && x <= 310) settingsPage = 4;
    else if (y >= 185 && x >= 10 && x <= 310) settingsPage = 5;
    redrawScreen = true;
    return;
  }
  if (settingsPage == 1) {
    float d = (x < 160) ? -0.1f : 0.1f;
    if (y >= 60 && y < 91) {
      targetTemp = constrain(targetTemp + ((x < 160) ? -targetStep : targetStep), 5.0f, 35.0f);
      saveTargetTemp();
      if (mqttClient.connected()) mqttClient.publish(targetTempTopic, String(targetTemp, 1).c_str(), true);
    } else if (y >= 91 && y < 121) {
      hysteresisLow = constrain(hysteresisLow + d, 0.1f, 3.0f);
    } else if (y >= 121 && y < 151) {
      hysteresisHigh = constrain(hysteresisHigh + d, 0.1f, 3.0f);
    } else if (y >= 151 && y < 181) {
      calibrationOffset = constrain(calibrationOffset + d, -5.0f, 5.0f);
    } else if (y >= 181 && y <= 230) {
      targetStep = 0.5f; // Fixed step; no user-selectable step.
    } else return;
    saveThermostatSettings(); redrawScreen=true; return;
  }
  if (settingsPage == 2) {
    if (y >= 60 && y < 91) displayBrightness = constrain(displayBrightness + (x < 160 ? -15 : 15), 10, 255);
    else if (y >= 91 && y < 121) nightModeEnabled = !nightModeEnabled;
    else if (y >= 121 && y < 151) nightBrightness = constrain(nightBrightness + (x < 160 ? -10 : 10), 10, 255);
    else if (y >= 151 && y < 181) dimTimeoutSeconds = (dimTimeoutSeconds >= 60) ? 15 : dimTimeoutSeconds + 15;
    else if (y >= 190 && y <= 235) { startTouchCalibration(); return; }
    else return;
    saveBrightness(); saveThermostatSettings(); updateBrightnessMode(); redrawScreen=true; return;
  }
  if (settingsPage == 6 && !touchCalibrationActive) {
    startTouchCalibration();
    return;
  }
  if (settingsPage == 5 && y >= 160 && y <= 215) { startRelayTest(); redrawScreen=true; return; }
  if (settingsPage == 4 && y >= 185) {
    // Reset Wi-Fi is available on the web interface to avoid accidental reset from a tap.
    redrawScreen=true;
  }
}

void checkHeating() {
  // При недостоверном датчике отключаем отопление; тест реле ограничен по времени.
  if (!sensorValid) {
    relayTestActive = false;
    heatingOn = false;
    digitalWrite(RELAY_PIN, HIGH);
    return;
  }
  if (relayTestActive) {
    if ((long)(millis() - relayTestUntil) >= 0) {
      relayTestActive = false;
      heatingOn = relayStateBeforeTest;
      digitalWrite(RELAY_PIN, heatingOn ? LOW : HIGH);
    }
    return;
  }
  if (!heatingOn && currentTemp <= targetTemp - hysteresisLow) {
    heatingOn = true; digitalWrite(RELAY_PIN, LOW);
  } else if (heatingOn && currentTemp >= targetTemp + hysteresisHigh) {
    heatingOn = false; digitalWrite(RELAY_PIN, HIGH);
  }
}

void setupWiFi() {
  wm.setConfigPortalTimeout(120);
  IPAddress local_IP(192, 168, 0, 115);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(8, 8, 8, 8);
  wm.setSTAStaticIPConfig(local_IP, gateway, subnet, dns);
  if (wm.autoConnect("ESP32_Thermostat_AP", "12345678")) isConnected = true;
  else { delay(1000); ESP.restart(); }
}

void readTemperature() {
  sensors.requestTemperatures();
  float raw = sensors.getTempCByIndex(0);
  sensorValid = isfinite(raw) && raw != DEVICE_DISCONNECTED_C && raw >= -55.0f && raw <= 125.0f;
  if (sensorValid) currentTemp = raw + calibrationOffset;
  else {
    heatingOn = false;
    digitalWrite(RELAY_PIN, HIGH);
  }
}

void loadTouchCalibration() {
  TouchCalibrationSettings stored;
  EEPROM.get(TOUCH_CALIBRATION_ADDR, stored);
  // Magic + plausible span check prevents uninitialized EEPROM bytes being used.
  if (stored.magic == 0x5443 && stored.rawLeft > stored.rawRight && stored.rawTop > stored.rawBottom &&
      ((int)stored.rawLeft - (int)stored.rawRight) >= 100 &&
      ((int)stored.rawTop - (int)stored.rawBottom) >= 100 &&
      stored.rawLeft >= 0 && stored.rawLeft <= 4095 && stored.rawRight >= 0 && stored.rawRight <= 4095 &&
      stored.rawTop >= 0 && stored.rawTop <= 4095 && stored.rawBottom >= 0 && stored.rawBottom <= 4095) {
    touchCal = stored;
    touchCalibrationValid = true;
    Serial.println("Touch calibration loaded from EEPROM");
  } else {
    touchCal = {0, 300, 30, 220, 20};
    touchCalibrationValid = false;
    Serial.println("Using original touch coordinates; no valid calibration stored");
  }
}

void saveTouchCalibration() {
  touchCal.magic = 0x5443;
  EEPROM.put(TOUCH_CALIBRATION_ADDR, touchCal);
  EEPROM.commit();
  touchCalibrationValid = true;
}

void startTouchCalibration() {
  touchCalibrationActive = true;
  // The tap that opened calibration must be released before the first target
  // can be captured; otherwise that menu tap could become point #1.
  touchCalibrationTouchLatched = true;
  touchCalibrationStep = 0;
  touchCalibrationMessage = "";
  currentScreen = 1;
  settingsPage = 6;
  redrawScreen = true;
  Serial.println("Touch calibration started; existing calibration will remain until all 4 points pass validation");
}

void processTouchCalibration(TouchPoint p) {
  // Average a few readings from the same press to reduce noise.
  long sumX = 0, sumY = 0;
  int samples = 0;
  for (int i = 0; i < 7; ++i) {
    TouchPoint q = (i == 0) ? p : ts.getTouch();
    if (q.x != 0 || q.y != 0) {
      sumX += q.x;
      sumY += q.y;
      samples++;
    }
    delay(8);
  }
  if (samples < 3) return;

  touchCalRawX[touchCalibrationStep] = (int)(sumX / samples);
  touchCalRawY[touchCalibrationStep] = (int)(sumY / samples);
  touchCalibrationStep++;

  if (touchCalibrationStep < 4) {
    redrawScreen = true;
    return;
  }

  TouchCalibrationSettings candidate = {};
  candidate.magic = 0x5443;
  candidate.rawLeft = (int16_t)((touchCalRawX[0] + touchCalRawX[3]) / 2);
  candidate.rawRight = (int16_t)((touchCalRawX[1] + touchCalRawX[2]) / 2);
  candidate.rawTop = (int16_t)((touchCalRawY[0] + touchCalRawY[1]) / 2);
  candidate.rawBottom = (int16_t)((touchCalRawY[2] + touchCalRawY[3]) / 2);

  // Preserve the current panel orientation: the known-good mapping is
  // decreasing on both raw axes. Reject reversed taps and suspiciously tiny spans.
  bool valid = candidate.rawLeft > candidate.rawRight &&
               candidate.rawTop > candidate.rawBottom &&
               ((int)candidate.rawLeft - (int)candidate.rawRight) >= 100 &&
               ((int)candidate.rawTop - (int)candidate.rawBottom) >= 100 &&
               candidate.rawLeft >= 0 && candidate.rawLeft <= 4095 &&
               candidate.rawRight >= 0 && candidate.rawRight <= 4095 &&
               candidate.rawTop >= 0 && candidate.rawTop <= 4095 &&
               candidate.rawBottom >= 0 && candidate.rawBottom <= 4095;

  touchCalibrationActive = false;
  touchCalibrationTouchLatched = false;
  if (valid) {
    touchCal = candidate;
    saveTouchCalibration();
    touchCalibrationMessage = "Kalibrace ulozena";
    settingsPage = 2;
    Serial.printf("Touch calibration saved: X %d -> %d, Y %d -> %d\n", touchCal.rawLeft, touchCal.rawRight, touchCal.rawTop, touchCal.rawBottom);
  } else {
    // Keep the previous calibration (or original mapping) intact on bad readings.
    touchCalibrationMessage = "Kalibrace selhala";
    settingsPage = 6;
    Serial.println("Touch calibration rejected; previous coordinates preserved");
  }
  redrawScreen = true;
}

void saveTargetTemp() { EEPROM.put(TARGET_TEMP_ADDR, targetTemp); EEPROM.commit(); }
void loadTargetTemp() { EEPROM.get(TARGET_TEMP_ADDR, targetTemp); if (isnan(targetTemp) || targetTemp < 5.0 || targetTemp > 35.0) targetTemp = 22.0; }
void setupTime() {
  // POSIX TZ rules for Czechia: CET/CEST changes automatically on the last Sundays of March and October.
  const char* tz = "CET-1CEST,M3.5.0/2,M10.5.0/3";
  if (timeMode == 1) tz = "CET-1";       // fixed UTC+1
  else if (timeMode == 2) tz = "CEST-2"; // fixed UTC+2
  setenv("TZ", tz, 1);
  tzset();
  configTime(0, 0, ntpServer);
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--:--:--";
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
  return String(timeString);
}

String removeDiacritics(String str) {
  str.replace("áč", "ac"); str.replace("é", "e"); str.replace("ě", "e");
  str.replace("í", "i"); str.replace("ó", "o"); str.replace("ř", "r");
  str.replace("š", "s"); str.replace("ť", "t"); str.replace("ú", "u");
  str.replace("ů", "u"); str.replace("ý", "y"); str.replace("ž", "z");
  str.replace("Á", "Ac"); str.replace("É", "E"); str.replace("Ě", "E");
  str.replace("Í", "I"); str.replace("Ó", "O"); str.replace("Ř", "R");
  str.replace("Š", "S"); str.replace("Ť", "T"); str.replace("Ú", "U");
  str.replace("Ů", "U"); str.replace("Ý", "Y"); str.replace("Ž", "Z");
  return str;
}

void fetchWeather() {
  if (!isConnected) return;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(WEATHER_CITY) + "&appid=" + String(WEATHER_API_KEY) + "&units=metric&lang=cz";
  http.begin(url);
  if (http.GET() == 200) {
    DynamicJsonDocument doc(1500);
    deserializeJson(doc, http.getString());
    float temp = doc["main"]["temp"] | 0.0f;
    int weatherId = doc["weather"][0]["id"] | 0;
    const char* desc = doc["weather"][0]["description"] | "pocasi";
    weatherTemp = String(temp, 1) + "C";
    weatherDesc = removeDiacritics(String(desc));
    if (weatherId == 800) weatherIconType = 1;                 // jasno
    else if (weatherId == 801) weatherIconType = 2;            // skoro jasno
    else if (weatherId >= 802 && weatherId <= 804) weatherIconType = 2; // oblacno
    else if (weatherId >= 200 && weatherId < 300) weatherIconType = 5;  // bourky
    else if ((weatherId >= 300 && weatherId < 400) || (weatherId >= 500 && weatherId < 600)) weatherIconType = 3; // dest
    else if (weatherId >= 600 && weatherId < 700) weatherIconType = 4; // snih
    else if (weatherId >= 700 && weatherId < 800) weatherIconType = 6; // mlha
    else weatherIconType = 0;
  }
  http.end();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, targetTempTopic) == 0) {
    char buf[length + 1];
    memcpy(buf, payload, length);
    buf[length] = '\0';
    float val = atof(buf);
    if (!isnan(val) && val >= 5.0 && val <= 35.0) { targetTemp = val; saveTargetTemp(); }
  }
}

bool mqttConnect() {
  if (mqttClient.connected()) return true;
  String clientId = "ESP32-CYD-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (mqttClient.connect(clientId.c_str(), mqttUser, mqttPassword)) {
    mqttClient.subscribe(targetTempTopic);
    return true;
  }
  return false;
}

void loop() {
  checkTouch();
  updateBrightnessMode();
  if (relayTestActive && (long)(millis() - relayTestUntil) >= 0) checkHeating();
  if (currentScreen == 0) drawHomeScreen();
  else drawSettingsScreen();

  server.handleClient(); 
  
  if (isConnected && !mqttClient.connected()) {
    static unsigned long lastRec = 0;
    if (millis() - lastRec > 10000) { lastRec = millis(); mqttConnect(); }
  } else if (isConnected) {
    mqttClient.loop();
  }

  static unsigned long lastTempRead = 0;
  if (millis() - lastTempRead > 5000) {
    readTemperature();
    checkHeating();
    lastTempRead = millis();
  }
  
  if (millis() - lastPublish >= publishInterval) {
    lastPublish = millis();
    if (mqttClient.connected()) {
      mqttClient.publish(tempTopic, sensorValid ? String(currentTemp, 1).c_str() : "ERROR", true);
      mqttClient.publish(relayStatusTopic, heatingOn ? "ON" : "OFF", true);
    }
  }
  
  static unsigned long lastWeatherFetch = 0;
  if (millis() - lastWeatherFetch > 600000) {
    fetchWeather();
    lastWeatherFetch = millis();
  }
  
  delay(10);
}