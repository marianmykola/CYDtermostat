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

// Подключаем встроенные красивые шрифты

TFT_eSPI tft = TFT_eSPI();

// Переменные для отслеживания изменений (чтобы экран не мерцал)
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

// Цветовая палитра (Modern Dark)
#define DARK_BG      0x10A2  // Очень темный серый/синий для фона
#define CARD_BG      0x2124  // Светло-серый фон для блоков
#define BORDER_COLOR 0x4228  // Серый для рамок
#define TXT_ORANGE   0xFDA0  // Оранжевый для отопления

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
  Serial.println("=== SMART THERMOSTAT UI IMPROVED ===");
  
  if (!EEPROM.begin(512)) {
    Serial.println("EEPROM Init Failed");
  }
  loadTargetTemp();
  
  dht.begin();
  readTemperature();
  
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(DARK_BG);
  
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  ts.begin();
  Serial.println("Touch screen initialized");
  
  setupWiFi();
  setupTime();
  fetchWeather();
}

void drawHomeScreen() {
  static bool firstRun = true;
  
  // При первом запуске рисуем статичную красивую подложку (интерфейс в виде карточек)
  if (firstRun) {
    tft.fillScreen(DARK_BG);
    
    // Карточка времени (Верхняя)
    tft.fillRoundRect(10, 10, 300, 50, 6, CARD_BG);
    tft.drawRoundRect(10, 10, 300, 50, 6, BORDER_COLOR);
    
    // Карточка температур (Основная по центру)
    tft.fillRoundRect(10, 68, 300, 124, 6, CARD_BG);
    tft.drawRoundRect(10, 68, 300, 124, 6, BORDER_COLOR);
    
    // Статичные надписи мелким красивым шрифтом
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.drawString("ROOM", 25, 76);
    tft.drawString("TARGET", 135, 76);
    tft.drawString("HUMIDITY", 25, 134);
    tft.drawString("STATUS", 135, 134);
    
    // Кнопка "+" (Справа сверху в блоке температуры)
    tft.fillRoundRect(240, 76, 60, 48, 6, TFT_RED);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("+", 262, 92);
    
    // Кнопка "-" (Справа снизу в блоке температуры)
    tft.fillRoundRect(240, 134, 60, 48, 6, TFT_BLUE);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("-", 264, 150);

    // Нижняя плашка погоды
    tft.fillRoundRect(10, 200, 210, 32, 6, CARD_BG);
    tft.drawRoundRect(10, 200, 210, 32, 6, BORDER_COLOR);
    
    // Кнопка погоды "Refresh"
    tft.fillRoundRect(230, 200, 80, 32, 6, 0x03E0); // Темно-зеленый
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Update", 244, 211);
    
    firstRun = false;
  }

  // ===== ЧАСЫ (Крупный сглаженный шрифт) =====
  String timeStr = getCurrentTime();
  if (timeStr != oldTime) {
    // Стираем старое время цветом карточки
    tft.fillRect(75, 16, 170, 38, CARD_BG);
    tft.setFreeFont(&FreeSansBold24pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(timeStr.substring(0, 5), 105, 20); // Выводим только ЧЧ:ММ для красоты
    oldTime = timeStr;
  }

  // ===== ТЕКУЩАЯ ТЕМПЕРАТУРА РОМ =====
  if (abs(currentTemp - oldTemp) > 0.05) {
    tft.fillRect(25, 96, 85, 28, CARD_BG);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_CYAN);
    tft.drawString(String(currentTemp, 1) + " C", 25, 96);
    oldTemp = currentTemp;
  }

  // ===== ЦЕЛЕВАЯ ТЕМПЕРАТУРА =====
  if (abs(targetTemp - oldTarget) > 0.05) {
    tft.fillRect(135, 96, 95, 28, CARD_BG);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(String(targetTemp, 1) + " C", 135, 96);
    oldTarget = targetTemp;
  }

  // ===== ВЛАЖНОСТЬ =====
  if (abs(currentHumidity - oldHumidity) > 0.5) {
    tft.fillRect(25, 154, 85, 28, CARD_BG);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_MAGENTA);
    tft.drawString(String(currentHumidity, 0) + " %", 25, 154);
    oldHumidity = currentHumidity;
  }

  // ===== СТАТУС ОТОПЛЕНИЯ =====
  if (heatingOn != oldHeating) {
    tft.fillRect(135, 154, 95, 28, CARD_BG);
    tft.setFreeFont(&FreeSansBold12pt7b);
    if (heatingOn) {
      tft.setTextColor(TXT_ORANGE);
      tft.drawString("HEAT ON", 135, 154);
    } else {
      tft.setTextColor(TFT_GREEN);
      tft.drawString("IDLE", 135, 154);
    }
    oldHeating = heatingOn;
  }

  // ===== СТАТУС WIFI (Иконка-индикатор в углу часов) =====
  if (isConnected != oldWifi) {
    tft.fillCircle(290, 35, 6, isConnected ? TFT_GREEN : TFT_RED);
    oldWifi = isConnected;
  }
  
  // ===== ПОГОДА =====
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
    // Применяем твою точную рабочую коррекцию координат!
    int displayX = map(p.x, 300, 30, 0, 320);
    int displayY = map(p.y, 220, 20, 0, 240);
    
    if(displayX > 0 && displayX < 320 && displayY > 0 && displayY < 240) {
      Serial.printf("Touch: X=%d, Y=%d\n", displayX, displayY);
      handleHomeTouch(displayX, displayY);
      delay(200); // Антидребезг
    }
  }
}

void handleHomeTouch(int x, int y) {
  // Координаты кнопки "+" (240, 76, ширина 60, высота 48)
  if (x >= 240 && x <= 300 && y >= 76 && y <= 124) {
    Serial.println("Plus pressed");
    targetTemp += 0.5;
    if (targetTemp > 35.0) targetTemp = 35.0;
    saveTargetTemp();
  }

  // Координаты кнопки "-" (240, 134, ширина 60, высота 48)
  if (x >= 240 && x <= 300 && y >= 134 && y <= 182) {
    Serial.println("Minus pressed");
    targetTemp -= 0.5;
    if (targetTemp < 5.0) targetTemp = 5.0;
    saveTargetTemp();
  }

  // Координаты кнопки обновления погоды "Update" (230, 200, ширина 80, высота 32)
  if (x >= 230 && x <= 310 && y >= 200 && y <= 232) {
    Serial.println("Weather update requested");
    fetchWeather();
  }
}

void checkHeating() {
  heatingOn = currentTemp < targetTemp;
}

void setupWiFi() {
  Serial.println("=== WiFi Setup ===");
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Connecting to WiFi...", 20, 30);
  
  wm.setConfigPortalTimeout(120);
  
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
    Serial.println("DHT11 read error!");
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
  }
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--:--";
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
    Serial.println("Weather sync OK");
  } else {
    Serial.printf("Weather HTTP error: %d\n", httpCode);
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