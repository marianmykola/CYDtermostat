#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Bitbang.h>

TFT_eSPI tft = TFT_eSPI();

// Screen dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// Touch screen pins (CYD VSPI configuration)
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_SCLK 25
XPT2046_Bitbang ts(TOUCH_MOSI, TOUCH_MISO, TOUCH_SCLK, TOUCH_CS, SCREEN_WIDTH, SCREEN_HEIGHT);

// Touch zones for rectangles
struct TouchZone {
  int x, y, width, height;
  uint16_t color;
  const char* name;
};

TouchZone zones[] = {
  {10, 10, 100, 50, TFT_RED, "RED"},      // Top-left
  {130, 10, 100, 50, TFT_BLUE, "BLUE"},    // Top-right
  {10, 80, 100, 50, TFT_GREEN, "GREEN"},  // Middle-left
  {130, 80, 100, 50, TFT_YELLOW, "YELLOW"}, // Middle-right
  {10, 150, 100, 50, TFT_CYAN, "CYAN"},  // Bottom-left
  {130, 150, 100, 50, TFT_MAGENTA, "MAGENTA"} // Bottom-right
};

int numZones = 6;
bool fullScreenMode = false;
uint16_t fullScreenColor = TFT_BLACK;

// Function declarations
void drawMainScreen();
void drawFullScreen(uint16_t color);
void checkTouch();
void setup();

void setup() {
  Serial.begin(115200);
  Serial.println("=== HYBRID TOUCH DISPLAY ===");
  Serial.println("TFT_eSPI for display, XPT2046_Bitbang for touch");
  
  // Initialize display
  tft.init();
  tft.setRotation(0);
  tft.invertDisplay(false);
  tft.fillScreen(TFT_BLACK);
  
  // Turn on backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // Initialize touch screen
  ts.begin();
  Serial.println("Touch screen initialized");
  Serial.printf("Display size: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
  
  // Draw main screen
  drawMainScreen();
}

void drawMainScreen() {
  fullScreenMode = false;
  
  tft.fillScreen(TFT_BLACK);
  
  // Draw fine grid for precise coordinate checking
  tft.setTextColor(TFT_DARKGREY);
  for (int i = 0; i <= SCREEN_WIDTH; i += 25) {
    tft.drawLine(i, 0, i, SCREEN_HEIGHT, TFT_DARKGREY);
  }
  for (int i = 0; i <= SCREEN_HEIGHT; i += 25) {
    tft.drawLine(0, i, SCREEN_WIDTH, i, TFT_DARKGREY);
  }
  
  // Draw colored rectangles with detailed borders
  for (int i = 0; i < numZones; i++) {
    tft.fillRect(zones[i].x, zones[i].y, zones[i].width, zones[i].height, zones[i].color);
    tft.drawRect(zones[i].x, zones[i].y, zones[i].width, zones[i].height, TFT_WHITE);
    
    // Draw corner markers for precise boundaries
    tft.drawPixel(zones[i].x, zones[i].y, TFT_WHITE);
    tft.drawPixel(zones[i].x + zones[i].width - 1, zones[i].y, TFT_WHITE);
    tft.drawPixel(zones[i].x, zones[i].y + zones[i].height - 1, TFT_WHITE);
    tft.drawPixel(zones[i].x + zones[i].width - 1, zones[i].y + zones[i].height - 1, TFT_WHITE);
    
    // Add text labels with coordinates
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.drawString(zones[i].name, zones[i].x + 25, zones[i].y + 20);
    tft.setTextSize(0);
    tft.drawString(String(zones[i].x) + "," + String(zones[i].y), zones[i].x + 5, zones[i].y + 35);
    
    // Show zone boundaries
    tft.setTextSize(0);
    String bounds = String(zones[i].x + zones[i].width - 1) + "," + String(zones[i].y + zones[i].height - 1);
    tft.drawString(bounds, zones[i].x + 5, zones[i].y + zones[i].height - 10);
  }
  
  // Title
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("HYBRID TOUCH", 60, 200);
  
  tft.setTextSize(1);
  tft.drawString("TFT_eSPI + XPT2046", 60, 230);
  tft.drawString("Touch rectangles", 70, 250);
}

void drawFullScreen(uint16_t color) {
  fullScreenMode = true;
  fullScreenColor = color;
  
  tft.fillScreen(color);
  
  // Instructions
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("FULL SCREEN", 50, 100);
  
  tft.setTextSize(1);
  tft.drawString("Touch anywhere", 70, 150);
  tft.drawString("to return", 90, 170);
  
  // Show color name
  const char* colorName = "UNKNOWN";
  for (int i = 0; i < numZones; i++) {
    if (zones[i].color == color) {
      colorName = zones[i].name;
      break;
    }
  }
  tft.drawString(colorName, 90, 200);
}

void checkTouch() {
  TouchPoint p = ts.getTouch();
  
  if (p.x != -1 && p.y != -1) { // Touch detected
    // Filter out invalid touches near (0,0)
    if (p.x < 5 || p.y < 5) {
      return;
    }
    
    // Show RAW touch coordinates
    Serial.printf("RAW: %d, %d\n", p.x, p.y);
    
    // Apply working transformation: "Swapped + Y flipped" with exact ranges
    int swappedX = p.y;                   // Swapped: X becomes Y
    int swappedY = 319 - p.x;             // Y flipped: 319 - original X
    
    // Map to screen coordinates with proven ranges
    int displayX = map(swappedX, 50, 250, 0, SCREEN_WIDTH);
    int displayY = map(swappedY, 100, 320, 0, SCREEN_HEIGHT);
    
    Serial.printf("DISPLAY: %d, %d\n", displayX, displayY);
    
    // Show coordinates on screen
    tft.fillRect(0, 280, 240, 40, TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.drawString("RAW:" + String(p.x) + "," + String(p.y), 5, 285);
    tft.drawString("DISP:" + String(displayX) + "," + String(displayY), 5, 300);
    
    // Check which zone was touched
    if (fullScreenMode) {
      // Return to main screen
      drawMainScreen();
      Serial.println("Returned to main screen");
    } else {
      bool zoneFound = false;
      for (int i = 0; i < numZones; i++) {
        int x1 = zones[i].x;
        int y1 = zones[i].y;
        int x2 = zones[i].x + zones[i].width - 1;
        int y2 = zones[i].y + zones[i].height - 1;
        
        bool inZone = (displayX >= x1 && displayX <= x2 && displayY >= y1 && displayY <= y2);
        
        if (inZone) {
          Serial.printf("Zone %s touched! [%d,%d to %d,%d]\n", 
                       zones[i].name, x1, y1, x2, y2);
          drawFullScreen(zones[i].color);
          zoneFound = true;
          break;
        }
      }
      
      if (!zoneFound) {
        Serial.println("No zone hit - touch in empty area");
      }
    }
    
    delay(300); // Debounce
  }
}

void loop() {
  // Check touch input
  checkTouch();
  delay(50);
}
