#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Bitbang.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include <vector>
#include <String>

TFT_eSPI tft = TFT_eSPI();

// Screen dimensions (landscape mode)
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Touch screen pins (VSPI configuration)
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_SCLK 25

// DS18B20 temperature sensor pin
#define DS18B20_PIN 4

// EEPROM addresses
#define TARGET_TEMP_ADDR 200
#define CURRENT_MODE_ADDR 210

// Weather API
#define WEATHER_API_KEY "YOUR_API_KEY_HERE"
#define WEATHER_CITY "Kyiv"

XPT2046_Bitbang ts(TOUCH_MOSI, TOUCH_MISO, TOUCH_SCLK, TOUCH_CS, 240, 320);

// UI Elements
struct Button {
  int x, y, width, height;
  uint16_t color;
  const char* text;
  void (*action)();
};

// UI State
enum Screen { HOME, TEMP, SYSTEM, SETTINGS };
Screen currentScreen = HOME;

// WiFi Settings
bool isConnected = false;

// WiFi Manager
WiFiManager wm;

// WiFi Configuration
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Temperature control
float currentTemp = 0.0;
float targetTemp = 22.0;
bool heatingOn = false;

// Weather data
float outdoorTemp = 0.0;
String weatherDescription = "";
String weatherIcon = "";
unsigned long lastWeatherUpdate = 0;

// Time data
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200; // GMT+2 (Kyiv time)
const int daylightOffset_sec = 3600;
bool timeInitialized = false;

// DS18B20 sensor
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// UI Elements
std::vector<Button> homeButtons;

// Function declarations
void drawHomeScreen();
void drawStatusBar();
void checkTouch();
void handleHomeTouch(int x, int y);
void handleTempTouch(int x, int y);
void handleSystemTouch(int x, int y);
void handleSettingsTouch(int x, int y);
void initUI();
void drawTempScreen();
void drawSystemScreen();
void drawSettingsScreen();

// WiFi Functions
void setupWiFi();

// Temperature Functions
void readTemperature();
void updateTemperatureDisplay();
void saveTargetTemp();
void loadTargetTemp();

// Weather Functions
void updateWeather();
void drawWeatherIcon(int x, int y, String icon);

// Temperature Control Functions
void increaseTargetTemp();
void decreaseTargetTemp();

// Time Functions
void setupTime();
void updateTimeDisplay();
String getCurrentTime();

void setup() {
  Serial.begin(115200);
  Serial.println("=== SMART THERMOSTAT UI ===");
  
  // Initialize EEPROM
  EEPROM.begin(512);
  loadTargetTemp();
  
  // Initialize DS18B20 sensor
  sensors.begin();
  sensors.setResolution(12); // 12-bit resolution
  Serial.println("DS18B20 sensor initialized");
  
  // Read initial temperature
  readTemperature();
  
  // Initialize weather
  updateWeather();
  Serial.println("Weather initialized");
  
  // Initialize display
  tft.init();
  tft.setRotation(1); // Landscape mode
  tft.invertDisplay(false);
  tft.fillScreen(TFT_BLACK);
  
  // Turn on backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // Initialize touch screen
  ts.begin();
  Serial.println("Touch screen initialized");
  
  // Setup WiFi with WiFiManager
  setupWiFi();
  
  // Initialize time after WiFi is connected
  setupTime();
  
  // Initialize UI
  initUI();
  
  // Draw home screen
  drawHomeScreen();
  
  Serial.println("UI initialized");
}

void initUI() {
  // Clear all buttons
  homeButtons.clear();
  
  // Add settings button
  homeButtons.push_back({240, 200, 70, 35, TFT_CYAN, "Settings", [](){ currentScreen = SETTINGS; drawSettingsScreen(); }});
}

void drawStatusBar() {
  // Status bar at top
  tft.fillRect(0, 0, SCREEN_WIDTH, 25, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  
  // WiFi status
  if (isConnected) {
    tft.drawString("WiFi: OK", 10, 8);
  } else {
    tft.drawString("WiFi: --", 10, 8);
  }
  
  // Real time from NTP
  String currentTime = getCurrentTime();
  tft.drawString(currentTime, SCREEN_WIDTH - 60, 8);
}

void drawHomeScreen() {
  tft.fillScreen(TFT_BLACK);
  drawStatusBar();
  
  // Title
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("Smart Thermostat", 70, 10);
  
  // Current temperature display with better label
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(4);
  tft.drawString(String(currentTemp, 1) + "C", 100, 50);
  
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.drawString("Inside", 140, 60);
  
  // Target temperature display
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  tft.drawString("Target: " + String(targetTemp, 1) + "C", 20, 110);
  
  // Temperature control buttons
  tft.fillRect(220, 100, 40, 30, TFT_BLUE);
  tft.fillRect(270, 100, 40, 30, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("+", 235, 108);
  tft.drawString("-", 285, 108);
  
  // Weather section
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.drawString("Outdoor", 20, 150);
  
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.drawString(String(outdoorTemp, 1) + "C", 20, 170);
  
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.drawString(weatherDescription, 100, 175);
  
  // Draw weather icon
  drawWeatherIcon(250, 160, weatherIcon);
  
  // Draw settings button
  for (const auto& btn : homeButtons) {
    tft.fillRect(btn.x, btn.y, btn.width, btn.height, btn.color);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.drawString(btn.text, btn.x + 5, btn.y + btn.height/2 - 8);
  }
}

void drawTempScreen() {
  tft.fillScreen(TFT_BLACK);
  drawStatusBar();
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("Temperature Control", 70, 30);
  
  // Current temperature
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.drawString("Current:", 20, 70);
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(3);
  tft.drawString(String(currentTemp, 1) + "C", 100, 85);
  
  // Target temperature
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.drawString("Target:", 20, 130);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(3);
  tft.drawString(String(targetTemp, 1) + "C", 100, 145);
  
  // Heating status
  tft.setTextColor(heatingOn ? TFT_RED : TFT_BLUE);
  tft.setTextSize(1);
  tft.drawString(heatingOn ? "HEATING ON" : "HEATING OFF", 200, 100);
  
  // Back button
  tft.fillRect(20, 190, 80, 35, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.drawString("Back", 45, 202);
}

void drawSystemScreen() {
  tft.fillScreen(TFT_BLACK);
  drawStatusBar();
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("System Info", 90, 30);
  
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.drawString("ESP32 Thermostat", 20, 80);
  tft.drawString("Version: 1.0", 20, 100);
  tft.drawString("WiFi: " + String(isConnected ? "Connected" : "Disconnected"), 20, 120);
  tft.drawString("IP: " + WiFi.localIP().toString(), 20, 140);
  
  // Back button
  tft.fillRect(20, 190, 80, 35, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.drawString("Back", 45, 202);
}

void drawSettingsScreen() {
  tft.fillScreen(TFT_BLACK);
  drawStatusBar();
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("Settings", 110, 30);
  
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.drawString("Temperature Unit: Celsius", 20, 80);
  tft.drawString("Display Brightness: High", 20, 100);
  tft.drawString("Auto Update: Enabled", 20, 120);
  
  // Back button
  tft.fillRect(20, 190, 80, 35, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.drawString("Back", 45, 202);
}

void checkTouch() {
  TouchPoint p = ts.getTouch();
  
  if (p.x != -1 && p.y != -1) {
    // Apply corrected transformation - fix Y mapping
    int displayX = map(p.x, 220, 20, 0, 320);
    int displayY = map(p.y, 295, 50, 0, 240);
    
    switch (currentScreen) {
      case HOME:
        handleHomeTouch(displayX, displayY);
        break;
      case TEMP:
        handleTempTouch(displayX, displayY);
        break;
      case SYSTEM:
        handleSystemTouch(displayX, displayY);
        break;
      case SETTINGS:
        handleSettingsTouch(displayX, displayY);
        break;
    }
    
    delay(200);
  }
}

void handleHomeTouch(int x, int y) {
  // Check temperature control buttons first
  if (x >= 220 && x <= 260 && y >= 100 && y <= 130) {
    increaseTargetTemp();
    return;
  }
  
  if (x >= 270 && x <= 310 && y >= 100 && y <= 130) {
    decreaseTargetTemp();
    return;
  }
}

void handleTempTouch(int x, int y) {
  // Back button
  if (x >= 20 && x <= 100 && y >= 190 && y <= 225) {
    currentScreen = HOME;
    drawHomeScreen();
  }
}

void handleSystemTouch(int x, int y) {
  // Back button
  if (x >= 20 && x <= 100 && y >= 190 && y <= 225) {
    currentScreen = HOME;
    drawHomeScreen();
  }
}

void handleSettingsTouch(int x, int y) {
  // Back button
  if (x >= 20 && x <= 100 && y >= 190 && y <= 225) {
    currentScreen = HOME;
    drawHomeScreen();
  }
}

void setupWiFi() {
  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  
  // Connect to WiFi using WiFiManager
  Serial.println("Starting WiFiManager...");
  
  // Set custom AP name and password
  wm.setConfigPortalTimeout(300); // 5 minutes timeout
  
  if (!wm.autoConnect("ESP32_Config", "12345678")) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
    delay(1000);
  }
  
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  isConnected = true;
}

void readTemperature() {
  sensors.requestTemperatures();
  currentTemp = sensors.getTempCByIndex(0);
  if (currentTemp == DEVICE_DISCONNECTED_C) {
    Serial.println("DS18B20 sensor disconnected!");
    currentTemp = 0.0;
  }
}

void updateTemperatureDisplay() {
  // Update temperature display on current screen
  if (currentScreen == TEMP) {
    drawTempScreen();
  }
}

void saveTargetTemp() {
  EEPROM.put(TARGET_TEMP_ADDR, targetTemp);
  EEPROM.commit();
}

void loadTargetTemp() {
  EEPROM.get(TARGET_TEMP_ADDR, targetTemp);
  if (isnan(targetTemp) || targetTemp < 10 || targetTemp > 35) {
    targetTemp = 22.0; // Default temperature
  }
}

void increaseTargetTemp() {
  targetTemp += 0.5;
  if (targetTemp > 35) targetTemp = 35;
  saveTargetTemp();
  drawHomeScreen();
  Serial.println("Target temp: " + String(targetTemp));
}

void decreaseTargetTemp() {
  targetTemp -= 0.5;
  if (targetTemp < 10) targetTemp = 10;
  saveTargetTemp();
  drawHomeScreen();
  Serial.println("Target temp: " + String(targetTemp));
}

void updateWeather() {
  // Update weather every 30 minutes
  if (millis() - lastWeatherUpdate > 1800000 || lastWeatherUpdate == 0) {
    // For now, simulate weather data
    // TODO: Implement OpenWeatherMap API
    outdoorTemp = 18.5;
    weatherDescription = "Partly Cloudy";
    weatherIcon = "cloud";
    lastWeatherUpdate = millis();
    
    if (currentScreen == HOME) {
      drawHomeScreen();
    }
  }
}

void drawWeatherIcon(int x, int y, String icon) {
  // Simple weather icons
  if (icon == "sun") {
    tft.fillCircle(x + 15, y + 15, 12, TFT_YELLOW);
  } else if (icon == "cloud") {
    tft.fillCircle(x + 15, y + 15, 12, TFT_LIGHTGREY);
    tft.fillCircle(x + 18, y + 12, 8, TFT_WHITE);
  } else if (icon == "rain") {
    tft.fillCircle(x + 15, y + 10, 10, TFT_DARKGREY);
    // Rain drops
    for (int i = 0; i < 3; i++) {
      tft.drawLine(x + 8 + i*8, y + 20, x + 8 + i*8, y + 25, TFT_BLUE);
    }
  } else {
    // Default icon
    tft.drawRect(x + 5, y + 5, 20, 20, TFT_WHITE);
  }
}

void setupTime() {
  Serial.println("Initializing NTP time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Wait for time to be set
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

void updateTimeDisplay() {
  if (timeInitialized && currentScreen == HOME) {
    drawStatusBar(); // Update status bar with current time
  }
}

String getCurrentTime() {
  if (!timeInitialized) {
    return "--:--";
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "--:--";
  }
  
  char timeString[6];
  strftime(timeString, sizeof(timeString), "%H:%M", &timeinfo);
  return String(timeString);
}

void loop() {
  checkTouch();
  
  // Read temperature every 5 seconds
  static unsigned long lastTempRead = 0;
  if (millis() - lastTempRead > 5000) {
    readTemperature();
    updateTemperatureDisplay();
    lastTempRead = millis();
    Serial.println("Current temp: " + String(currentTemp) + "°C, Target: " + String(targetTemp) + "°C");
  }
  
  // Update weather every 30 minutes
  updateWeather();
  
  // Update time display every minute
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate > 60000) {
    updateTimeDisplay();
    lastTimeUpdate = millis();
  }
  
  // WiFiManager handles its own web server
  wm.process();
  
  delay(50);
}
