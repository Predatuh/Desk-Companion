// TFT_eSPI configuration for Freenove ESP32-S3 Display (FNK0104)
// ST7789 240×320 IPS TFT + FT6336U capacitive touch
// This file must be copied into the TFT_eSPI library folder
// OR referenced via User_Setup_Select.h

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

#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

// Use PSRAM for sprite buffers
#define USE_DMA_TO_TFT
