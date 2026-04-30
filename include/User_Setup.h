#define USER_SETUP_INFO "ESP32-2432S028"

#define ILI9341_DRIVER   // ← ВОТ ЭТО КЛЮЧЕВОЕ
#define ST7789_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define TFT_INVERSION_ON

#define TOUCH_CS 33

#define SPI_FREQUENCY  40000000