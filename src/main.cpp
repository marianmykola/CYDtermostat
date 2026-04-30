#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  // backlight ON
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  tft.init();
  tft.setRotation(0);
  tft.invertDisplay(true);  

  // clear screen
  tft.fillScreen(TFT_BLACK);

  // test colors
  delay(500);
  tft.fillScreen(TFT_RED);
  delay(500);
  tft.fillScreen(TFT_GREEN);
  delay(500);
  tft.fillScreen(TFT_BLUE);
  delay(500);

  // draw shapes
  tft.fillScreen(TFT_BLACK);

  tft.fillRect(10, 10, 100, 50, TFT_RED);
  tft.fillRect(10, 70, 100, 50, TFT_GREEN);
  tft.fillRect(10, 130, 100, 50, TFT_BLUE);

  // draw lines
  tft.drawLine(0, 0, 240, 320, TFT_WHITE);
  tft.drawLine(240, 0, 0, 320, TFT_YELLOW);

  // text
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("TFT OK", 70, 200);
}

void loop() {
  // nothing here
}