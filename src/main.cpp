#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Pin definitions
#define RELAY_PIN 4
#define DS18B20_PIN 16
#define LED_PIN 2

// Display and touch
TFT_eSPI tft = TFT_eSPI();
TFT_eSPI_Button buttons[6];

// Temperature sensor
OneWire oneWire(DS18B20_PIN);
DallasTemperature tempSensor(&oneWire);

// WiFi and Web server
WebServer server(80);
Preferences preferences;

// Thermostat settings
struct ThermostatSettings {
  float targetTemp = 22.0;
  float hysteresis = 0.5;
  float currentTemp = 20.0;
  bool heatingOn = false;
  bool manualMode = false;
  bool relayState = false;
  unsigned long lastTempRead = 0;
  unsigned long lastScreenUpdate = 0;
  unsigned long lastWifiCheck = 0;
};

ThermostatSettings settings;

// WiFi credentials
String wifiSSID = "";
String wifiPassword = "";

// Screen dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// Button areas
#define TEMP_UP_X 20
#define TEMP_UP_Y 100
#define TEMP_UP_W 100
#define TEMP_UP_H 40

#define TEMP_DOWN_X 130
#define TEMP_DOWN_Y 100
#define TEMP_DOWN_W 100
#define TEMP_DOWN_H 40

#define MODE_X 20
#define MODE_Y 200
#define MODE_W 200
#define MODE_H 40

#define WIFI_X 20
#define WIFI_Y 250
#define WIFI_W 200
#define WIFI_H 40

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // Initialize backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // Initialize temperature sensor
  tempSensor.begin();
  tempSensor.setResolution(12);
  
  // Load settings from flash
  loadSettings();
  
  // Initialize WiFi
  setupWiFi();
  
  // Initialize web server
  setupWebServer();
  
  // Initialize buttons
  setupButtons();
  
  // Initial screen draw
  drawScreen();
  
  Serial.println("Thermostat initialized");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Read temperature every 2 seconds
  if (currentMillis - settings.lastTempRead >= 2000) {
    readTemperature();
    settings.lastTempRead = currentMillis;
    updateRelay();
  }
  
  // Update screen every 500ms
  if (currentMillis - settings.lastScreenUpdate >= 500) {
    drawScreen();
    settings.lastScreenUpdate = currentMillis;
  }
  
  // Check WiFi connection every 30 seconds
  if (currentMillis - settings.lastWifiCheck >= 30000) {
    checkWiFi();
    settings.lastWifiCheck = currentMillis;
  }
  
  // Handle web requests
  server.handleClient();
  
  // Handle touch
  handleTouch();
  
  delay(10);
}

void readTemperature() {
  tempSensor.requestTemperatures();
  settings.currentTemp = tempSensor.getTempCByIndex(0);
  
  if (settings.currentTemp == -127.0) {
    Serial.println("Temperature sensor error!");
    settings.currentTemp = settings.targetTemp; // Fallback
  }
}

void updateRelay() {
  bool shouldHeat = false;
  
  if (settings.manualMode) {
    shouldHeat = settings.heatingOn;
  } else {
    shouldHeat = settings.currentTemp < (settings.targetTemp - settings.hysteresis);
  }
  
  if (shouldHeat != settings.relayState) {
    settings.relayState = shouldHeat;
    digitalWrite(RELAY_PIN, shouldHeat ? HIGH : LOW);
    digitalWrite(LED_PIN, shouldHeat ? HIGH : LOW);
    Serial.printf("Relay %s\n", shouldHeat ? "ON" : "OFF");
  }
}

void drawScreen() {
  tft.fillScreen(TFT_BLACK);
  
  // Title
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("CYD Thermostat", 10, 10);
  
  // Current temperature
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Current:", 10, 40);
  tft.setTextColor(settings.relayState ? TFT_RED : TFT_GREEN, TFT_BLACK);
  tft.drawFloat(settings.currentTemp, 1, 150, 40);
  tft.drawString("C", 220, 40);
  
  // Target temperature
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Target:", 10, 70);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawFloat(settings.targetTemp, 1, 150, 70);
  tft.drawString("C", 220, 70);
  
  // Draw buttons
  drawButtons();
  
  // Status
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Mode: " + String(settings.manualMode ? "Manual" : "Auto"), 10, 160);
  tft.drawString("Heating: " + String(settings.relayState ? "ON" : "OFF"), 10, 180);
  
  // WiFi status
  tft.setTextColor(WiFi.status() == WL_CONNECTED ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.drawString("WiFi: " + (WiFi.status() == WL_CONNECTED ? wifiSSID : "Disconnected"), 10, 300);
  
  // Draw temperature buttons
  buttons[0].drawButton();
  buttons[1].drawButton();
  buttons[2].drawButton();
  buttons[3].drawButton();
}

void setupButtons() {
  // Temperature up button
  buttons[0].initButton(&tft, TEMP_UP_X + TEMP_UP_W/2, TEMP_UP_Y + TEMP_UP_H/2, 
                       TEMP_UP_W, TEMP_UP_H, TFT_WHITE, TFT_BLUE, TFT_WHITE, 
                       "Temp +", 2);
  
  // Temperature down button  
  buttons[1].initButton(&tft, TEMP_DOWN_X + TEMP_DOWN_W/2, TEMP_DOWN_Y + TEMP_DOWN_H/2,
                       TEMP_DOWN_W, TEMP_DOWN_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                       "Temp -", 2);
  
  // Mode button
  buttons[2].initButton(&tft, MODE_X + MODE_W/2, MODE_Y + MODE_H/2,
                       MODE_W, MODE_H, TFT_WHITE, TFT_PURPLE, TFT_WHITE,
                       "Mode", 2);
  
  // Manual heating button
  buttons[3].initButton(&tft, MODE_X + MODE_W/2, MODE_Y + MODE_H + 20,
                       MODE_W, MODE_H, TFT_WHITE, settings.heatingOn ? TFT_RED : TFT_DARKGREY, TFT_WHITE,
                       settings.heatingOn ? "Heat ON" : "Heat OFF", 2);
}

void drawButtons() {
  setupButtons();
}

void handleTouch() {
  uint16_t x, y;
  
  if (tft.getTouch(&x, &y)) {
    // Check temperature up button
    if (x > TEMP_UP_X && x < TEMP_UP_X + TEMP_UP_W && 
        y > TEMP_UP_Y && y < TEMP_UP_Y + TEMP_UP_H) {
      settings.targetTemp += 0.5;
      if (settings.targetTemp > 35.0) settings.targetTemp = 35.0;
      saveSettings();
      delay(200);
    }
    
    // Check temperature down button
    if (x > TEMP_DOWN_X && x < TEMP_DOWN_X + TEMP_DOWN_W && 
        y > TEMP_DOWN_Y && y < TEMP_DOWN_Y + TEMP_DOWN_H) {
      settings.targetTemp -= 0.5;
      if (settings.targetTemp < 10.0) settings.targetTemp = 10.0;
      saveSettings();
      delay(200);
    }
    
    // Check mode button
    if (x > MODE_X && x < MODE_X + MODE_W && 
        y > MODE_Y && y < MODE_Y + MODE_H) {
      settings.manualMode = !settings.manualMode;
      saveSettings();
      delay(200);
    }
    
    // Check manual heating button
    if (x > MODE_X && x < MODE_X + MODE_W && 
        y > MODE_Y + MODE_H + 20 && y < MODE_Y + MODE_H * 2 + 20) {
      settings.heatingOn = !settings.heatingOn;
      saveSettings();
      delay(200);
    }
  }
}

void setupWiFi() {
  preferences.begin("thermostat", false);
  wifiSSID = preferences.getString("wifiSSID", "");
  wifiPassword = preferences.getString("wifiPassword", "");
  preferences.end();
  
  if (wifiSSID.length() > 0) {
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    }
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED && wifiSSID.length() > 0) {
    Serial.println("WiFi disconnected, reconnecting...");
    setupWiFi();
  }
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handleSetSettings);
  server.on("/api/wifi", HTTP_POST, handleSetWiFi);
  
  server.begin();
  Serial.println("HTTP server started");
}

void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>CYD Thermostat</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 20px; }
        .container { max-width: 400px; }
        .temp { font-size: 2em; margin: 10px 0; }
        .button { padding: 10px; margin: 5px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; }
        .button:hover { background: #0056b3; }
        input { padding: 5px; margin: 5px; width: 100%; }
    </style>
</head>
<body>
    <div class="container">
        <h1>CYD Thermostat</h1>
        <div class="temp">Current: <span id="current">--</span>°C</div>
        <div class="temp">Target: <span id="target">--</span>°C</div>
        <div>Status: <span id="status">--</span></div>
        
        <h2>Controls</h2>
        <button class="button" onclick="adjustTemp(0.5)">Temp +0.5°C</button>
        <button class="button" onclick="adjustTemp(-0.5)">Temp -0.5°C</button>
        <button class="button" onclick="toggleMode()">Toggle Mode</button>
        
        <h2>WiFi Settings</h2>
        <input type="text" id="ssid" placeholder="SSID">
        <input type="password" id="password" placeholder="Password">
        <button class="button" onclick="setWiFi()">Save WiFi</button>
    </div>
    
    <script>
        function updateData() {
            fetch('/api/settings')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('current').textContent = data.currentTemp.toFixed(1);
                    document.getElementById('target').textContent = data.targetTemp.toFixed(1);
                    document.getElementById('status').textContent = data.relayState ? 'Heating' : 'Idle';
                });
        }
        
        function adjustTemp(delta) {
            fetch('/api/settings', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({targetTemp: parseFloat(document.getElementById('target').textContent) + delta})
            }).then(updateData);
        }
        
        function toggleMode() {
            fetch('/api/settings', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({manualMode: true})
            }).then(updateData);
        }
        
        function setWiFi() {
            fetch('/api/wifi', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({
                    ssid: document.getElementById('ssid').value,
                    password: document.getElementById('password').value
                })
            }).then(() => alert('WiFi settings saved!'));
        }
        
        setInterval(updateData, 2000);
        updateData();
    </script>
</body>
</html>
)";
  
  server.send(200, "text/html", html);
}

void handleGetSettings() {
  StaticJsonDocument<200> doc;
  doc["currentTemp"] = settings.currentTemp;
  doc["targetTemp"] = settings.targetTemp;
  doc["manualMode"] = settings.manualMode;
  doc["heatingOn"] = settings.heatingOn;
  doc["relayState"] = settings.relayState;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSetSettings() {
  String body = server.arg("plain");
  StaticJsonDocument<200> doc;
  
  if (deserializeJson(doc, body) == DeserializationError::Ok) {
    if (doc.containsKey("targetTemp")) {
      settings.targetTemp = doc["targetTemp"];
    }
    if (doc.containsKey("manualMode")) {
      settings.manualMode = doc["manualMode"];
    }
    if (doc.containsKey("heatingOn")) {
      settings.heatingOn = doc["heatingOn"];
    }
    
    saveSettings();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\"}");
  }
}

void handleSetWiFi() {
  String body = server.arg("plain");
  StaticJsonDocument<200> doc;
  
  if (deserializeJson(doc, body) == DeserializationError::Ok) {
    wifiSSID = doc["ssid"].as<String>();
    wifiPassword = doc["password"].as<String>();
    
    preferences.begin("thermostat", false);
    preferences.putString("wifiSSID", wifiSSID);
    preferences.putString("wifiPassword", wifiPassword);
    preferences.end();
    
    server.send(200, "application/json", "{\"status\":\"ok\"}");
    
    // Reconnect WiFi
    setupWiFi();
  } else {
    server.send(400, "application/json", "{\"status\":\"error\"}");
  }
}

void saveSettings() {
  preferences.begin("thermostat", false);
  preferences.putFloat("targetTemp", settings.targetTemp);
  preferences.putFloat("hysteresis", settings.hysteresis);
  preferences.putBool("manualMode", settings.manualMode);
  preferences.putBool("heatingOn", settings.heatingOn);
  preferences.end();
}

void loadSettings() {
  preferences.begin("thermostat", true);
  settings.targetTemp = preferences.getFloat("targetTemp", 22.0);
  settings.hysteresis = preferences.getFloat("hysteresis", 0.5);
  settings.manualMode = preferences.getBool("manualMode", false);
  settings.heatingOn = preferences.getBool("heatingOn", false);
  preferences.end();
}
