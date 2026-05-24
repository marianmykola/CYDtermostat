#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
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

// =====================================================
// GLOBAL
// =====================================================

TFT_eSPI tft = TFT_eSPI();

String oldTime = "";
float oldTemp = -100;
float oldTarget = -100;
bool oldHeating = false;
bool oldWifi = false;

// Weather data
String weatherTemp = "";
String weatherDesc = "";
String oldWeatherTemp = "";
String oldWeatherDesc = "";

// WiFi Manager
WiFiManager wm;

// WiFi status
bool isConnected = false;

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Touch screen pins
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_SCLK 25

// DHT11 temperature sensor pin
#define DHT11_PIN 22

// EEPROM addresses
#define TARGET_TEMP_ADDR 200
#define CURRENT_MODE_ADDR 210

// Weather API
#define WEATHER_API_KEY "5b09c7a00a36ece6308bbc11c96c50a6"
#define WEATHER_CITY "Valdice"

// Touch screen
XPT2046_Bitbang ts(TOUCH_MOSI, TOUCH_MISO, TOUCH_SCLK, TOUCH_CS);

// Calibration settings
#define RERUN_CALIBRATE true

// Screen enum
enum Screen { HOME };

// Current screen
Screen currentScreen = HOME;

// WiFi configuration
IPAddress local_IP(192, 168, 0, 116);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// Temperature control
float currentTemp = 0.0;
float targetTemp = 22.0;
bool heatingOn = false;

// Time data
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
bool timeInitialized = false;

// Temperature sensor
DHT dht(DHT11_PIN, DHT11);

// Function declarations
void drawHomeScreen();
void drawStaticUI();
void checkTouch();
void handleHomeTouch(int x, int y);
void loadCustomFonts();

// WiFi Functions
void setupWiFi();

// Temperature Functions
void readTemperature();
void saveTargetTemp();
void loadTargetTemp();

// Time Functions
void setupTime();
String getCurrentTime();

// Weather Functions
void fetchWeather();
String translateWeatherToCzech(String englishDesc);

void setup() {
  Serial.begin(115200);
  Serial.println("=== SMART THERMOSTAT IMPROVED ===");
  
  // Initialize EEPROM
  EEPROM.begin(512);
  loadTargetTemp();
  
  // Initialize DHT11 sensor
  dht.begin();
  Serial.println("DHT11 sensor initialized");
  
  // Read initial temperature
  readTemperature();
  
  // Initialize TFT display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  // Turn on backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // Initialize touch screen
  ts.begin();
  Serial.println("Touch screen initialized");
  
    
  // Load custom fonts
  loadCustomFonts();
  Serial.println("Custom fonts loaded");
  
  // Setup WiFi
  setupWiFi();
  
  // Initialize time
  setupTime();
  
  Serial.println("UI initialized");


}

void loadCustomFonts() {
  // Load custom smooth fonts if available
  // For now, use built-in smooth font
  tft.setTextFont(0);
}

void drawHomeScreen() {
  // Clear screen completely only once at start
  static bool firstRun = true;
  if (firstRun) {
    tft.fillScreen(TFT_BLACK);
    firstRun = false;
  }

  // ===== CLOCK =====
  String timeStr = getCurrentTime();
  if (timeStr != oldTime) {
    // Clear ONLY clock area
    tft.fillRect(60, 10, 250, 50, TFT_BLACK);
    
    tft.setFreeFont(&FreeSansBold24pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(timeStr, SCREEN_WIDTH/2 - 80, 20);
    
    oldTime = timeStr;
  }

  // ===== TEMPERATURE DISPLAY =====
  // Current temperature
  if (abs(currentTemp - oldTemp) > 0.05) {
    // Clear ONLY temperature area
    tft.fillRect(60, 60, 90, 40, TFT_BLACK);
    
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_CYAN);
    tft.drawString(String(currentTemp, 1) + "C", SCREEN_WIDTH/2 - 70, 70);
    
    oldTemp = currentTemp;
  }

  // Target temperature
  if (abs(targetTemp - oldTarget) > 0.05) {
    // Clear ONLY target area
    tft.fillRect(170, 60, 120, 40, TFT_BLACK);
    
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.drawString("->" + String(targetTemp, 1) + "C", SCREEN_WIDTH/2 , 70);
    
    oldTarget = targetTemp;
  }

  // ===== BUTTONS =====
  // Draw buttons only once (they don't change)
  static bool buttonsDrawn = false;
  if (!buttonsDrawn) {
    // Plus button (red)
    tft.fillRect((int32_t)80, (int32_t)110, (int32_t)70, (int32_t)60, TFT_RED);
    tft.setFreeFont(&FreeSansBold24pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("+", 102, 115);

    // Minus button (blue)
    tft.fillRect((int32_t)190, (int32_t)110, (int32_t)70, (int32_t)60, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("-", 218, 115);
    
    buttonsDrawn = true;
  }

  // ===== HEATING STATUS =====
  if (heatingOn != oldHeating) {
    // Clear ONLY heating status area
    tft.fillRect(120, 175, 80, 60, TFT_BLACK);
    
    if (heatingOn) {
      // Active heating - orange flame
      tft.fillCircle((int32_t)(SCREEN_WIDTH/2), (int32_t)200, (int32_t)15, TFT_ORANGE);
      tft.fillCircle((int32_t)(SCREEN_WIDTH/2), (int32_t)200, (int32_t)10, TFT_RED);
      tft.fillCircle((int32_t)(SCREEN_WIDTH/2), (int32_t)200, (int32_t)5, TFT_YELLOW);
     
    } else {
      // Idle - blue circle
      tft.fillCircle((int32_t)(300), (int32_t)10, (int32_t)15, TFT_BLUE);
      tft.fillCircle((int32_t)(300), (int32_t)10, (int32_t)10, TFT_DARKGREY);

    }
    
    oldHeating = heatingOn;
  }

  // ===== WIFI STATUS =====
  if (isConnected != oldWifi) {
    // Clear ONLY WiFi area
    tft.fillRect(0, 0, 130, 30, TFT_BLACK);
    
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(isConnected ? TFT_GREEN : TFT_RED);
    tft.drawString(isConnected ? "WIFI" : "NO WIFI", 10, 10);
    
    oldWifi = isConnected;
  }
  
  // ===== WEATHER DISPLAY =====
  if (weatherTemp != oldWeatherTemp || weatherDesc != oldWeatherDesc) {
    // Clear ONLY weather area
    tft.fillRect(20, 200, 180, 20, TFT_BLACK);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(weatherTemp + " " + weatherDesc, 30, 200);
    
    oldWeatherTemp = weatherTemp;
    oldWeatherDesc = weatherDesc;
  }

  // ===== WEATHER REFRESH BUTTON =====
  static bool weatherButtonDrawn = false;
  if (!weatherButtonDrawn) {
    tft.fillRect(230, 180, 80, 30, TFT_GREEN);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(TFT_BLACK);
    tft.drawString("Refresh", 240, 190);
    weatherButtonDrawn = true;
  }
}

void checkTouch() {
  TouchPoint p = ts.getTouch();
  
  // Display touches that aren't at the coordinate (0,0)
  if (p.x != 0 || p.y != 0) {
    // Apply working coordinate transformation from memory
    // X = map(p.y, 50, 250, 0, SCREEN_WIDTH)
    // Y = map(319 - p.x, 100, 320, 0, SCREEN_HEIGHT)
    int displayX = map(p.x, 300, 30, 0, 320);
    int displayY = map(p.y, 220, 20, 0, 240);
    
    Serial.print("Raw touch: X=");
    Serial.print(p.x);
    Serial.print(", Y=");
    Serial.print(p.y);
    Serial.print(" -> Display: X=");
    Serial.print(displayX);
    Serial.print(", Y=");
    Serial.println(displayY);
    
    handleHomeTouch(displayX, displayY);
    delay(100);
  }
}

void handleHomeTouch(int x, int y) {

  // ===== PLUS BUTTON =====
  int plusX = 80;
  int plusY = 110;
  int plusW = 70;
  int plusH = 60;

  if (x >= plusX && x <= (plusX + plusW) &&
      y >= plusY && y <= (plusY + plusH)) {

    Serial.println("Plus button pressed!");

    targetTemp += 0.5;

    if (targetTemp > 35.0)
      targetTemp = 35.0;

    saveTargetTemp();
    oldTarget = -100;
  }

  // ===== MINUS BUTTON =====
  int minusX = 190;
  int minusY = 110;
  int minusW = 70;
  int minusH = 60;

  if (x >= minusX && x <= (minusX + minusW) &&
      y >= minusY && y <= (minusY + minusH)) {

    Serial.println("Minus button pressed!");

    targetTemp -= 0.5;

    if (targetTemp < 5.0)
      targetTemp = 5.0;

    saveTargetTemp();
    oldTarget = -100;
  }

  // ===== WEATHER REFRESH BUTTON =====
  
  int weatherX = 230;
  int weatherY = 180;
  int weatherW = 80;
  int weatherH = 30;

  if (x >= weatherX && x <= (weatherX + weatherW) &&
      y >= weatherY && y <= (weatherY + weatherH)) {

    Serial.println("Weather refresh button pressed!");
    fetchWeather();
  }
}
void checkHeating() {
  // Simple heating control logic
  heatingOn = currentTemp < targetTemp;
}


void setupWiFi() {
  Serial.println("=== WiFi Setup Starting ===");
  
  wm.setConfigPortalTimeout(300);
  wm.setAPStaticIPConfig(local_IP, gateway, subnet);
  wm.setDebugOutput(true);
  
  Serial.println("Starting WiFiManager...");
  
  if (wm.autoConnect("ESP32_Config", "12345678")) {
    Serial.println("=== WiFi Connected Successfully ===");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    
    isConnected = true;
  } else {
    Serial.println("Failed to connect, starting config portal...");
    
    if (wm.startConfigPortal("ESP32_Config", "12345678")) {
      Serial.println("=== WiFi Connected via Portal ===");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      
      isConnected = true;
    } else {
      Serial.println("Failed to connect or timed out");
      Serial.println("Restarting ESP32...");
      delay(1000);
      ESP.restart();
    }
  }
}

void readTemperature() {
  float temp = dht.readTemperature();
  if (isnan(temp)) {
    Serial.println("DHT11 sensor read error!");
    currentTemp = 0.0;
  } else {
    currentTemp = temp;
  }
}

void saveTargetTemp() {
  EEPROM.put(TARGET_TEMP_ADDR, targetTemp);
  EEPROM.commit();
}

void loadTargetTemp() {
  EEPROM.get(TARGET_TEMP_ADDR, targetTemp);
  if (isnan(targetTemp) || targetTemp < 10 || targetTemp > 35) {
    targetTemp = 22.0;
  }
}

void setupTime() {
  Serial.println("Initializing NTP time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    Serial.print("Waiting for NTP time...");
    delay(1000);
    attempts++;
  }
  
  if (getLocalTime(&timeinfo)) {
    Serial.println("NTP time synchronized!");
    timeInitialized = true;
  } else {
    Serial.println("Failed to obtain NTP time");
  }
}


String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time Error";
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
  return String(timeString);
}

void fetchWeather() {
  if (!isConnected) {
    return;
  }

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(WEATHER_CITY) + "&appid=" + String(WEATHER_API_KEY) + "&units=metric&lang=cs";
  
  Serial.println("Fetching weather: " + url);
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("API Response: " + payload);
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    float temp = doc["main"]["temp"];
    const char* description = doc["weather"][0]["description"];
    
    weatherTemp = String(temp, 1) + "C";
    weatherDesc = translateWeatherToCzech(String(description));
    
    Serial.println("Weather updated: " + weatherTemp + " " + weatherDesc);
  } else {
    Serial.println("Weather API error: " + String(httpCode));
  }
  
  http.end();
}

String translateWeatherToCzech(String englishDesc) {
  englishDesc.toLowerCase();
  
  if (englishDesc == "clear sky") return "jasno";
  if (englishDesc == "few clouds") return "mírně oblacno";
  if (englishDesc == "scattered clouds") return "polojasno";
  if (englishDesc == "broken clouds") return "oblacno";
  if (englishDesc == "overcast clouds") return "zatazeno";
  if (englishDesc == "light rain") return "lehky dest";
  if (englishDesc == "moderate rain") return "dest";
  if (englishDesc == "heavy rain") return "silny dest";
  if (englishDesc == "shower rain") return "prehaanky";
  if (englishDesc == "thunderstorm") return "bourka";
  if (englishDesc == "snow") return "snih";
  if (englishDesc == "mist") return "mlha";
  if (englishDesc == "fog") return "husta mlha";
  
  return englishDesc;
}

void loop() {
  checkTouch();
  
  // Update display every 100ms for smooth updates
  drawHomeScreen();
  
  // Read temperature every 5 seconds
  static unsigned long lastTempRead = 0;
  if (millis() - lastTempRead > 5000) {
    readTemperature();
    lastTempRead = millis();
    Serial.println("Current temp: " + String(currentTemp) + "°C, Target: " + String(targetTemp) + "°C");
  }
  
  // Fetch weather every 10 minutes
  static unsigned long lastWeatherFetch = 0;
  if (millis() - lastWeatherFetch > 600000) {
    fetchWeather();
    lastWeatherFetch = millis();
  }
  
  // Check heating control
  checkHeating();
  
  // WiFiManager handles its own web server
  wm.process();
  
  delay(50);
}
