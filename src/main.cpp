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

TFT_eSPI tft = TFT_eSPI();

String oldTime = "";
float oldTemp = -100;
float oldTarget = -100;
float oldHumidity = -100;
bool oldHeating = false;
bool oldWifi = false;

String weatherTemp = "";
String weatherDesc = "";
String oldWeatherTemp = "";
String oldWeatherDesc = "";

WiFiManager wm;
bool isConnected = false;

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// CYD Touch Pins
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_SCLK 25

#define DHT11_PIN 22

#define TARGET_TEMP_ADDR 200

#define WEATHER_API_KEY "5b09c7a00a36ece6308bbc11c96c50a6"
#define WEATHER_CITY "Valdice"

XPT2046_Bitbang ts(TOUCH_MOSI, TOUCH_MISO, TOUCH_SCLK, TOUCH_CS);

enum Screen { HOME };
Screen currentScreen = HOME;

IPAddress local_IP(192, 168, 0, 116);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

float currentTemp = 0.0;
float targetTemp = 22.0;
float currentHumidity = 0.0;
bool heatingOn = false;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
bool timeInitialized = false;

DHT dht(DHT11_PIN, DHT11);

void drawHomeScreen();
void checkTouch();
void handleHomeTouch(int x, int y);
void setupWiFi();
void readTemperature();
void saveTargetTemp();
void loadTargetTemp();
void setupTime();
String getCurrentTime();
void fetchWeather();
void checkHeating();
String translateWeatherToCzech(String englishDesc);

void setup() {
  Serial.begin(115200);
  Serial.println("=== SMART THERMOSTAT IMPROVED ===");
  
  if (!EEPROM.begin(512)) {
    Serial.println("EEPROM Init Failed");
  }
  loadTargetTemp();
  
  dht.begin();
  readTemperature();
  
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  ts.begin(); // Инициализация тачскрина
  Serial.println("Touch screen initialized");
  
  setupWiFi();
  setupTime();
  fetchWeather(); // Сразу запросим погоду при старте
  
  Serial.println("UI initialized");
}

void drawHomeScreen() {
  static bool firstRun = true;
  if (firstRun) {
    tft.fillScreen(TFT_BLACK);
    firstRun = false;
  }

  // ===== CLOCK =====
  String timeStr = getCurrentTime();
  if (timeStr != oldTime) {
    tft.fillRect(60, 10, 200, 40, TFT_BLACK);
    tft.setTextSize(3); // Стандартный крупный шрифт (чтобы не упало без .h файлов)
    tft.setTextColor(TFT_WHITE);
    tft.drawString(timeStr, SCREEN_WIDTH/2 - 70, 15);
    oldTime = timeStr;
  }

  // ===== TEMPERATURE =====
  if (abs(currentTemp - oldTemp) > 0.05) {
    tft.fillRect(40, 60, 100, 30, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN);
    tft.drawString(String(currentTemp, 1) + "C", SCREEN_WIDTH/2 - 110, 65);
    oldTemp = currentTemp;
  }

  if (abs(targetTemp - oldTarget) > 0.05) {
    tft.fillRect(170, 60, 120, 30, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.drawString("->" + String(targetTemp, 1) + "C", SCREEN_WIDTH/2 + 10, 65);
    oldTarget = targetTemp;
  }

  // ===== BUTTONS =====
  static bool buttonsDrawn = false;
  if (!buttonsDrawn) {
    tft.fillRect(60, 110, 80, 50, TFT_RED);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("+", 92, 122);

    tft.fillRect(180, 110, 80, 50, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("-", 212, 122);
    
    buttonsDrawn = true;
  }

  // ===== HUMIDITY =====
  if (abs(currentHumidity - oldHumidity) > 0.5) {
    tft.fillRect(100, 170, 120, 30, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_MAGENTA);
    tft.drawString(String(currentHumidity, 0) + "% RH", SCREEN_WIDTH/2 - 40, 175);
    oldHumidity = currentHumidity;
  }

  // ===== WIFI =====
  if (isConnected != oldWifi) {
    tft.fillRect(0, 0, 100, 20, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(isConnected ? TFT_GREEN : TFT_RED);
    tft.drawString(isConnected ? "WIFI OK" : "NO WIFI", 10, 5);
    oldWifi = isConnected;
  }
  
  // ===== WEATHER =====
  if (weatherTemp != oldWeatherTemp || weatherDesc != oldWeatherDesc) {
    tft.fillRect(10, 210, 200, 25, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(weatherTemp + " " + weatherDesc, 15, 215);
    oldWeatherTemp = weatherTemp;
    oldWeatherDesc = weatherDesc;
  }

  static bool weatherButtonDrawn = false;
  if (!weatherButtonDrawn) {
    tft.fillRect(230, 205, 80, 30, TFT_GREEN);
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK);
    tft.drawString("Refresh", 245, 215);
    weatherButtonDrawn = true;
  }
}

void checkTouch() {
  TouchPoint p = ts.getTouch();
  
  // Проверяем, что координаты не нулевые (экран действительно нажат)
  if (p.x != 0 || p.y != 0) { 
    
    // Инвертируем карту координат под поворот экрана tft.setRotation(1)

    int displayX = map(p.x, 300, 30, 0, 320);
    int displayY = map(p.y, 220, 20, 0, 240);
    
    // Проверяем, что координаты входят в рамки разрешения экрана
    if(displayX > 0 && displayX < 320 && displayY > 0 && displayY < 240) {
      Serial.printf("Touch: X=%d, Y=%d\n", displayX, displayY);
      handleHomeTouch(displayX, displayY);
      
      // Задержка (антидребезг), чтобы одно нажатие не засчитывалось по 100 раз
      delay(250); 
    }
  }
}
void handleHomeTouch(int x, int y) {
  // Координаты кнопок подправлены под новые размеры отрисовки
  if (x >= 60 && x <= 140 && y >= 110 && y <= 160) {
    Serial.println("Plus pressed");
    targetTemp += 0.5;
    if (targetTemp > 35.0) targetTemp = 35.0;
    saveTargetTemp();
  }

  if (x >= 180 && x <= 260 && y >= 110 && y <= 160) {
    Serial.println("Minus pressed");
    targetTemp -= 0.5;
    if (targetTemp < 5.0) targetTemp = 5.0;
    saveTargetTemp();
  }

  if (x >= 230 && x <= 310 && y >= 205 && y <= 235) {
    Serial.println("Weather refresh pressed");
    fetchWeather();
  }
}

void checkHeating() {
  heatingOn = currentTemp < targetTemp;
  // Тут можно добавить управление пином реле котла, если оно подключено
}

void setupWiFi() {
  Serial.println("=== WiFi Setup ===");
  wm.setConfigPortalTimeout(180);
  wm.setAPStaticIPConfig(local_IP, gateway, subnet);
  
  if (wm.autoConnect("ESP32_Thermostat_AP", "12345678")) {
    Serial.println("Connected!");
    isConnected = true;
  } else {
    Serial.println("Portal timeout, restarting...");
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
  } else {
    Serial.println("DHT11 error!");
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
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    timeInitialized = true;
    Serial.println("Time sync OK");
  }
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
    
    // Безопасное выделение памяти в куче (Heap), чтобы не уронить стек
    DynamicJsonDocument* doc = new DynamicJsonDocument(1500); 
    deserializeJson(*doc, payload);
    
    float temp = (*doc)["main"]["temp"];
    const char* description = (*doc)["weather"][0]["description"];
    
    weatherTemp = String(temp, 1) + "C";
    weatherDesc = translateWeatherToCzech(String(description));
    
    delete doc; // Обязательно освобождаем память!
    Serial.println("Weather updated!");
  } else {
    Serial.printf("Weather error: %d\n", httpCode);
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

void loop() {
  checkTouch();
  drawHomeScreen();
  
  static unsigned long lastTempRead = 0;
  if (millis() - lastTempRead > 5000) {
    readTemperature();
    checkHeating();
    lastTempRead = millis();
  }
  
  static unsigned long lastWeatherFetch = 0;
  if (millis() - lastWeatherFetch > 600000) {
    fetchWeather();
    lastWeatherFetch = millis();
  }
  
  delay(30);
}