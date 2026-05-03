#pragma once

// Driver
#define ST7789_DRIVER

// Resolution
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Display pins (your original pins)
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

// Backlight
#define TFT_BL 21
#define TFT_BACKLIGHT_ON HIGH

// SPI speed
#define SPI_FREQUENCY  40000000

// Color order
#define TFT_RGB_ORDER TFT_BGR

// Inversion (try if colors are wrong)
#define TFT_INVERSION_ON

// Touch (only CS pin needed)
#define TOUCH_CS 33
