#ifndef PINS_H
#define PINS_H

// =========================
// GPS (I2C)
// =========================
#define GPS_SDA 10
#define GPS_SCL 21
#define gnssAddress 0x42

// =========================
// ICM20948 (SPI)
// =========================
#define ICM_CS 17
#define ICM_SCK 18
#define ICM_MISO 13
#define ICM_MOSI 23

// =========================
// BMP390 (SPI)
// =========================
#define BMP_CS 16
#define BMP_SCK 18
#define BMP_MISO 13
#define BMP_MOSI 23

// =========================
// SD Card (SPI)
// =========================
#define SD_CS 4
#define SD_SCK 18
#define SD_MISO 13
#define SD_MOSI 23

// LORA
#define SCK_PIN 32
#define MISO_PIN 35
#define MOSI_PIN 25
#define CS_PIN 33
#define RESET_PIN 27
#define G0_PIN 14
#define RX_ENABLE 12
#define TX_ENABLE 26

#endif