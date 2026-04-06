// TFT_eSPI configuration for Freenove ESP32-S3 Display (FNK0104)
// ST7789 240×320 IPS TFT + FT6336U capacitive touch

#define USER_SETUP_ID 99

#define ST7789_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Freenove FNK0104 SPI pins
#define TFT_CS   10
#define TFT_DC   46
#define TFT_RST  -1
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_MISO 13

// ESP32-S3 must use FSPI (the default SPI bus for user peripherals)
#define USE_FSPI_PORT

#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

#define SMOOTH_FONT
