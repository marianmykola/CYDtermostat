#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== Arduino IDE Display Test ===");
  
  // Turn backlight on
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  
  // Initialize display
  tft.init();
  tft.setRotation(0);
  
  Serial.printf("Display: %dx%d\n", tft.width(), tft.height());
  
  // Clear screen
  tft.fillScreen(TFT_BLACK);
  
  // Draw test pattern
  tft.fillRect(0, 0, 80, 320, TFT_RED);
  tft.fillRect(80, 0, 80, 320, TFT_GREEN);
  tft.fillRect(160, 0, 80, 320, TFT_BLUE);
  
  // Add text
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("ARDUINO", 50, 10);
  tft.drawString("TEST", 70, 40);
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("WORKING!", 50, 80);
  
  Serial.println("Test completed!");
}

void loop() {
  // Blink LED
  static bool led = false;
  led = !led;
  digitalWrite(2, led);
  
  delay(1000);
}
