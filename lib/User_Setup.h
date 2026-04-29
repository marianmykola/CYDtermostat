// =======================
//  DISPLAY DRIVER
// =======================
#define ST7789_DRIVER

// Rozlišení panelu (DŮLEŽITÉ)
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// =======================
//  SPI PINS (DISPLAY)
// =======================
// TY NECH, pokud ti už funguje TFT
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1   // často není zapojen

// =======================
//  BACKLIGHT
// =======================
#define TFT_BL 21
#define TFT_BACKLIGHT_ON HIGH

// =======================
//  SPI SPEED
// =======================
#define SPI_FREQUENCY  40000000

// =======================
//  TOUCH (XPT2046)
// =======================
// tvoje reálné zapojení:
#define TOUCH_CS 33

// =======================
//  OPTIONAL TOUCH SPI PINS
// (NEPOUŽÍVÁ SE explicitně v TFT_eSPI,
// ale dokumentačně)
#define XPT2046_CLK 25
#define XPT2046_MOSI 32
#define XPT2046_MISO 39

// =======================
//  COLOR ORDER (důležité pro ST789P3 klony)
// =======================
#define TFT_RGB_ORDER TFT_BGR

// =======================
//  INVERSION (některé panely potřebují)
// =======================
// pokud budeš mít invert barvy, odkomentuj:
// #define TFT_INVERSION_ON

// =======================
//  FONT SELECTION
// =======================
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH
#define LOAD_GFXFF  // FreeFonts. Include access to FreeFonts

#define SMOOTH_FONT

// =======================
//  TOUCH SETTINGS
// =======================
#define TOUCH_CALIBRATION_X0 200
#define TOUCH_CALIBRATION_X1 3700
#define TOUCH_CALIBRATION_Y0 200
#define TOUCH_CALIBRATION_Y1 3700
