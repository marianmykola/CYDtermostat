#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <DHT.h>
#include <time.h>
#include <vector>
#include <String>
#include <TFT_eSPI.h>
#include <XPT2046_Bitbang.h>
#include <HTTPClient.h>


TFT_eSPI tft = TFT_eSPI();

// Переменные для интерфейса экрана (против мерцания)
String oldTime = "";
float oldTemp = 23;
float oldTarget = 18;
float oldHumidity = -100;
bool oldHeating = true;
bool oldWifi = false;
bool oldMqttConnected = false;

String weatherTemp = "";
String weatherDesc = "";
String oldWeatherTemp = "";
String oldWeatherDesc = "";

WiFiManager wm;
WebServer server(80); // Веб-сервер на 80 порту
bool isConnected = false;

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// CYD Тач-пины
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_SCLK 25

#define DHT11_PIN 22

// EEPROM Адреса памяти
#define TARGET_TEMP_ADDR 200
#define BRIGHTNESS_ADDR  210 // Адрес для сохранения яркости

// Настройки ШИМ для подсветки дисплея (Пин 21 на CYD)
#define LEDC_CHANNEL 0
#define LEDC_RESOLUTION 8
#define LEDC_FREQ 5000

// OpenWeather Config
#define WEATHER_API_KEY "5b09c7a00a36ece6308bbc11c96c50a6"
#define WEATHER_CITY "Valdice"

XPT2046_Bitbang ts(TOUCH_MOSI, TOUCH_MISO, TOUCH_SCLK, TOUCH_CS);

// Цветовая палитра экрана
#define DARK_BG      0x10A2  
#define CARD_BG      0x2124  
#define BORDER_COLOR 0x4228  
#define TXT_ORANGE   0xFDA0  

// Настройки MQTT (HiveMQ TLS)
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

const char* mqttServer = "67d990d328564142ada404280806eb3b.s1.eu.hivemq.cloud";
const int mqttPort = 8883;
const char* mqttUser = "esp32";
const char* mqttPassword = "Energy654.";

const char* tempTopic = "esp32/temperature";
const char* humTopic = "esp32/humidity";
const char* timeTopic = "esp32/time";
const char* targetTempTopic = "esp32/targetTemp";
const char* relayStatusTopic = "esp32/rele";

unsigned long lastPublish = 0;
const unsigned long publishInterval = 5000; 

// Текущее состояние термостата
float currentTemp = 0.0;
float targetTemp = 22.0;
float currentHumidity = 0.0;
bool heatingOn = false;
int displayBrightness = 255; // Яркость по умолчанию (макс)

// NTP Время
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

DHT dht(DHT11_PIN, DHT11);

// Прототипы функций
void drawHomeScreen();
void checkTouch();
void handleHomeTouch(int x, int y);
void setupWiFi();
void readTemperature();
void saveTargetTemp();
void loadTargetTemp();
void saveBrightness();
void loadBrightness();
void setDisplayBrightness(int value);
void setupTime();
String getCurrentTime();
void fetchWeather();
void checkHeating();
String translateWeatherToCzech(String englishDesc);
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool mqttConnect();
void setupWebServer();

// HTML + JS код веб-интерфейса
const char* webPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1" charset="UTF-8">
<title>CYD Смарт Термостат</title>
<style>
  :root{--bg:#0d1117;--card:#161b22;--accent:#238636;--text:#e6eef8;--border:#30363d}
  body{margin:0;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--text);display:flex;flex-direction:column;align-items:center;padding:20px}
  .card{width:100%;max-width:440px;background:var(--card);border:1px solid var(--border);border-radius:16px;padding:20px;box-shadow:0 8px 24px rgba(0,0,0,0.5);margin-bottom:15px}
  .title{font-size:22px;font-weight:600;text-align:center;margin-bottom:20px;color:#58a6ff}
  .bigtemp{font-size:56px;font-weight:700;text-align:center;margin:10px 0;color:#58a6ff}
  .target{font-size:22px;text-align:center;color:#c9d1d9;margin-bottom:20px}
  .row{display:flex;justify-content:center;gap:15px;margin-top:10px}
  .btn{flex:1;background:var(--accent);border:none;color:white;padding:14px;border-radius:10px;font-size:18px;font-weight:600;cursor:pointer;transition:0.2s}
  .btn:hover{opacity:0.9;transform:scale(1.02)}
  .btn.blue{background:#1f6feb}
  .slider-container{margin:25px 0 15px 0}
  .slider-label{display:flex;justify-content:space-between;font-size:14px;color:#8b949e;margin-bottom:8px}
  input[type=range]{width:100%;height:6px;background:#30363d;border-radius:5px;outline:none;-webkit-appearance:none}
  input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;background:#58a6ff;border-radius:50%;cursor:pointer}
  .info-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;font-size:14px;color:#8b949e;border-top:1px solid var(--border);padding-top:15px;margin-top:15px}
  .status-val{color:#fff;font-weight:500;text-align:right}
</style>
</head>
<body>
  <div class="card">
    <div class="title" id="time">--:--:--</div>
    <div class="bigtemp" id="temp">--.- °C</div>
    <div class="target">Цель: <span id="target">--.-</span> °C</div>
    
    <div class="row">
      <button class="btn blue" onclick="changeTemp('inc')">+0.5°C</button>
      <button class="btn" onclick="changeTemp('dec')">-0.5°C</button>
    </div>

    <div class="slider-container">
      <div class="slider-label">
        <span>Яркость дисплея</span>
        <span id="bright-val">100%</span>
      </div>
      <input type="range" id="brightness" min="10" max="255" value="255" onchange="updateBrightness(this.value)">
    </div>

    <div class="info-grid">
      <div>Влажность:</div><div class="status-val" id="hum">-- %</div>
      <div>Статус реле:</div><div class="status-val" id="relay">--</div>
      <div>Wi-Fi IP:</div><div class="status-val">192.168.0.115</div>
      <div>MQTT Broker:</div><div class="status-val" id="mqtt">--</div>
    </div>
  </div>

<script>
async function fetchStatus(){
  try{
    const res = await fetch('/status');
    const j = await res.json();
    document.getElementById('temp').innerText = j.temp.toFixed(1) + ' °C';
    document.getElementById('target').innerText = j.target.toFixed(1);
    document.getElementById('hum').innerText = j.humidity + ' %';
    document.getElementById('time').innerText = j.time;
    document.getElementById('relay').innerText = j.heating ? 'ОТПЛЕНИЕ ВКЛ' : 'ОЖИДАНИЕ';
    document.getElementById('mqtt').innerText = j.mqtt ? 'CONNECTED' : 'DISCONNECTED';
    document.getElementById('brightness').value = j.brightness;
    document.getElementById('bright-val').innerText = Math.round((j.brightness/255)*100) + '%';
  }catch(e){console.error(e);}
}

async function changeTemp(action){
  await fetch('/' + action, {method:'POST'});
  fetchStatus();
}

async function updateBrightness(val){
  await fetch('/set_brightness?value=' + val, {method:'POST'});
  document.getElementById('bright-val').innerText = Math.round((val/255)*100) + '%';
}

setInterval(fetchStatus, 2000);
window.onload = fetchStatus;
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial.println("=== CYD THERMOSTAT SMART UI + WEB SERVER ===");
  
  // Настройка ШИМ для плавной регулировки яркости экрана
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RESOLUTION);
  ledcAttachPin(TFT_BL, LEDC_CHANNEL);
  
  if (!EEPROM.begin(512)) {
    Serial.println("EEPROM Init Failed");
  }
  loadTargetTemp();
  loadBrightness();
  setDisplayBrightness(displayBrightness); // Применяем сохраненную яркость
  
  dht.begin();
  readTemperature();
  
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(DARK_BG);
  
  ts.begin();
  setupWiFi();
  setupTime();
  setupWebServer(); // Запуск нашего веб-сервера
  
  espClient.setInsecure(); 
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
  
  fetchWeather();
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", webPage);
  });
  
  server.on("/status", HTTP_GET, []() {
    String t = getCurrentTime();
    String payload = "{\"temp\":" + String(currentTemp, 1) + 
                     ",\"target\":" + String(targetTemp, 1) +
                     ",\"humidity\":" + String(currentHumidity, 0) +
                     ",\"time\":\"" + t + "\"" +
                     ",\"brightness\":" + String(displayBrightness) +
                     ",\"heating\":" + (heatingOn ? "true" : "false") +
                     ",\"mqtt\":" + (mqttClient.connected() ? "true" : "false") + "}";
    server.send(200, "application/json", payload);
  });
  
  server.on("/inc", HTTP_POST, []() {
    targetTemp += 0.5;
    if (targetTemp > 35.0) targetTemp = 35.0;
    saveTargetTemp();
    if (mqttClient.connected()) mqttClient.publish(targetTempTopic, String(targetTemp, 1).c_str(), true);
    server.send(200, "text/plain", "ok");
  });
  
  server.on("/dec", HTTP_POST, []() {
    targetTemp -= 0.5;
    if (targetTemp < 5.0) targetTemp = 5.0;
    saveTargetTemp();
    if (mqttClient.connected()) mqttClient.publish(targetTempTopic, String(targetTemp, 1).c_str(), true);
    server.send(200, "text/plain", "ok");
  });
  
  server.on("/set_brightness", HTTP_POST, []() {
    if (server.hasArg("value")) {
      int val = server.arg("value").toInt();
      if (val >= 10 && val <= 255) {
        displayBrightness = val;
        setDisplayBrightness(displayBrightness);
        saveBrightness();
      }
    }
    server.send(200, "text/plain", "ok");
  });

  server.begin();
  Serial.println("HTTP Web Server started on port 80!");
}

void setDisplayBrightness(int value) {
  ledcWrite(LEDC_CHANNEL, value); // ШИМ управление пином подсветки
}

void saveBrightness() {
  EEPROM.put(BRIGHTNESS_ADDR, displayBrightness);
  EEPROM.commit();
}

void loadBrightness() {
  EEPROM.get(BRIGHTNESS_ADDR, displayBrightness);
  if (displayBrightness < 10 || displayBrightness > 255) {
    displayBrightness = 255; // По умолчанию на максимум
  }
}

// -------- Графика экрана (без изменений) --------
void drawHomeScreen() {
  static bool firstRun = true;
  if (firstRun) {
    tft.fillScreen(DARK_BG);
    tft.fillRoundRect(10, 10, 300, 50, 6, CARD_BG);
    tft.drawRoundRect(10, 10, 300, 50, 6, BORDER_COLOR);
    tft.fillRoundRect(10, 68, 300, 124, 6, CARD_BG);
    tft.drawRoundRect(10, 68, 300, 124, 6, BORDER_COLOR);
    
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.drawString("ROOM", 25, 76);
    tft.drawString("TARGET", 135, 76);
    tft.drawString("HUMIDITY", 25, 134);
    tft.drawString("STATUS", 135, 134);
    
    tft.fillRoundRect(240, 72, 65, 65, 8, TFT_RED);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("+", 264, 92);
    
    tft.fillRoundRect(240, 142, 65, 65, 8, TFT_BLUE);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("-", 264, 162);

    tft.fillRoundRect(10, 200, 210, 32, 6, CARD_BG);
    tft.drawRoundRect(10, 200, 210, 32, 6, BORDER_COLOR);
    tft.fillRoundRect(230, 200, 80, 32, 6, 0x03E0); 
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Update", 244, 211);
    firstRun = false;
  }

  String timeStr = getCurrentTime();
  if (timeStr != oldTime) {
    tft.fillRect(75, 16, 170, 38, CARD_BG);
    tft.setFreeFont(&FreeSansBold24pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(timeStr.substring(0, 5), 100, 18); 
    oldTime = timeStr;
  }

  if (abs(currentTemp - oldTemp) > 0.05) {
    tft.fillRect(25, 96, 85, 28, CARD_BG);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_CYAN);
    tft.drawString(String(currentTemp, 1) + " C", 25, 96);
    oldTemp = currentTemp;
  }

  if (abs(targetTemp - oldTarget) > 0.05) {
    tft.fillRect(135, 96, 95, 28, CARD_BG);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(String(targetTemp, 1) + " C", 135, 96);
    oldTarget = targetTemp;
  }

  if (abs(currentHumidity - oldHumidity) > 0.5) {
    tft.fillRect(25, 154, 85, 31, CARD_BG);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_MAGENTA);
    tft.drawString(String(currentHumidity, 0) + " %", 30, 158);
    oldHumidity = currentHumidity;
  }

  if (heatingOn != oldHeating) {
    tft.fillRect(135, 154, 95, 28, CARD_BG);
    tft.setFreeFont(&FreeSansBold12pt7b);
    
    tft.fillCircle(168, 170, 15, heatingOn ? TFT_GREEN : TFT_RED); 
   
    oldHeating = heatingOn;
  }

  if (isConnected != oldWifi) {
    tft.fillCircle(275, 35, 5, isConnected ? TFT_GREEN : TFT_RED); 
    oldWifi = isConnected;
  }
  
  bool mqttOk = mqttClient.connected();
  if (mqttOk != oldMqttConnected) {
    tft.fillCircle(292, 35, 5, mqttOk ? TFT_BLUE : TFT_DARKGREY); 
    oldMqttConnected = mqttOk;
  }
  
  if (weatherTemp != oldWeatherTemp || weatherDesc != oldWeatherDesc) {
    tft.fillRect(16, 206, 198, 20, CARD_BG);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(weatherTemp + "  " + weatherDesc, 20, 211);
    oldWeatherTemp = weatherTemp;
    oldWeatherDesc = weatherDesc;
  }
}

void checkTouch() {
  TouchPoint p = ts.getTouch();
  if (p.x != 0 || p.y != 0) { 
    int displayX = map(p.x, 300, 30, 0, 320);
    int displayY = map(p.y, 220, 20, 0, 240);
    if(displayX > 0 && displayX < 320 && displayY > 0 && displayY < 240) {
      handleHomeTouch(displayX, displayY);
      delay(200); 
    }
  }
}

void handleHomeTouch(int x, int y) {
  if (x >= 240 && x <= 305 && y >= 72 && y <= 137) {
    targetTemp += 0.5;
    if (targetTemp > 35.0) targetTemp = 35.0;
    saveTargetTemp();
    if (mqttClient.connected()) mqttClient.publish(targetTempTopic, String(targetTemp, 1).c_str(), true);
  }
  if (x >= 240 && x <= 305 && y >= 142 && y <= 207) {
    targetTemp -= 0.5;
    if (targetTemp < 5.0) targetTemp = 5.0;
    saveTargetTemp();
    if (mqttClient.connected()) mqttClient.publish(targetTempTopic, String(targetTemp, 1).c_str(), true);
  }
  if (x >= 230 && x <= 310 && y >= 200 && y <= 232) {
    fetchWeather();
  }
}

void checkHeating() {
  heatingOn = currentTemp < targetTemp;
}

void setupWiFi() {
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Connecting to WiFi...", 20, 30);
  
  wm.setConfigPortalTimeout(120);
  
  IPAddress local_IP(192, 168, 0, 115);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(8, 8, 8, 8);
  
  wm.setSTAStaticIPConfig(local_IP, gateway, subnet, dns);
  wm.setAPStaticIPConfig(local_IP, gateway, subnet);
  
  if (wm.autoConnect("ESP32_Thermostat_AP", "12345678")) {
    isConnected = true;
  } else {
    delay(1000);
    ESP.restart();
  }
}

void readTemperature() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  if (!isnan(temp) && !isnan(humidity)) {
    currentTemp = temp;
    currentHumidity = humidity;
  }
}

void saveTargetTemp() {
  EEPROM.put(TARGET_TEMP_ADDR, targetTemp);
  EEPROM.commit();
}

void loadTargetTemp() {
  EEPROM.get(TARGET_TEMP_ADDR, targetTemp);
  if (isnan(targetTemp) || targetTemp < 5.0 || targetTemp > 35.0) {
    targetTemp = 22.0;
  }
}

void setupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--:--:--";
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
  return String(timeString);
}

void fetchWeather() {
  if (!isConnected) return;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(WEATHER_CITY) + "&appid=" + String(WEATHER_API_KEY) + "&units=metric&lang=cs";
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument* doc = new DynamicJsonDocument(1500); 
    deserializeJson(*doc, payload);
    float temp = (*doc)["main"]["temp"];
    const char* description = (*doc)["weather"][0]["description"];
    weatherTemp = String(temp, 1) + "C";
    weatherDesc = translateWeatherToCzech(String(description));
    delete doc;
  }
  http.end();
}

String translateWeatherToCzech(String englishDesc) {
  englishDesc.toLowerCase();
  if (englishDesc == "clear sky") return "jasno";
  if (englishDesc == "few clouds") return "mirne oblacno";
  if (englishDesc == "scattered clouds") return "polojasno";
  if (englishDesc == "broken clouds") return "oblacno";
  if (englishDesc == "overcast clouds") return "zatazeno";
  if (englishDesc == "light rain") return "lehky dest";
  if (englishDesc == "moderate rain") return "dest";
  if (englishDesc == "heavy rain") return "silny dest";
  return englishDesc;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, targetTempTopic) == 0) {
    char buf[length + 1];
    memcpy(buf, payload, length);
    buf[length] = '\0';
    float val = atof(buf);
    if (!isnan(val) && val >= 5.0 && val <= 35.0) {
      targetTemp = val;
      saveTargetTemp();
    }
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
  drawHomeScreen();
  server.handleClient(); // Обработка запросов веб-сервера
  
  if (isConnected) {
    if (!mqttClient.connected()) {
      static unsigned long lastReconnectAttempt = 0;
      if (millis() - lastReconnectAttempt > 10000) { 
        lastReconnectAttempt = millis();
        if (mqttConnect()) lastReconnectAttempt = 0;
      }
    } else {
      mqttClient.loop();
    }
  }

  static unsigned long lastTempRead = 0;
  if (millis() - lastTempRead > 5000) {
    readTemperature();
    checkHeating();
    lastTempRead = millis();
  }
  
  unsigned long now = millis();
  if (now - lastPublish >= publishInterval) {
    lastPublish = now;
    if (mqttClient.connected()) {
      mqttClient.publish(tempTopic, String(currentTemp, 1).c_str(), true);
      mqttClient.publish(humTopic, String(currentHumidity, 0).c_str(), true);
      mqttClient.publish(relayStatusTopic, heatingOn ? "ON" : "OFF", true);
      mqttClient.publish(timeTopic, getCurrentTime().c_str(), true);
      mqttClient.publish(targetTempTopic, String(targetTemp, 1).c_str(), true);
    }
  }
  
  static unsigned long lastWeatherFetch = 0;
  if (millis() - lastWeatherFetch > 600000) {
    fetchWeather();
    lastWeatherFetch = millis();
  }
  
  delay(10);
}