#ifndef PINS_H
#define PINS_H

// =========================
// GPS (I2C)
// =========================
#define GPS_SDA 17
#define GPS_SCL 16
#define gnssAddress 0x42

// TODO : Not using ICM
// ======================
// USE ISM330 INSTEAD
//======================
#define IMU_CS 41

// // =========================
// // ICM20948 (SPI)
// // =========================
// #define ICM_CS 17   // USING IMU_CS
// #define ICM_SCK 18  // USING IMU_SCK
// #define ICM_MISO 13 // USING IMU_MISO
// #define ICM_MOSI 23 // using IMU_MOSI

// TODO : Not using bmp
//=======================
// MS560 INSTEAD OF THIS
//=======================

#define ALT_CS1 39
#define ALT_CS2 38
#define MOSI_B 36
#define MISO_B 37
#define SCLK_B 35

// // =========================
// // BMP390 (SPI)
// // =========================
// #define BMP_CS 16   // using ALT_CS1 and ALT_CS2 INSTEAD
// #define BMP_SCK 18  // USING SCLK_B instead
// #define BMP_MISO 13 // using MISO_B instead
// #define BMP_MOSI 23 // using MOSI_B instead

// CHANGE THE SD CARDS

// =========================
// SD Card (SPI)
// =========================
#define SD_CS 4
#define SD_SCK 18
#define SD_MISO 13
#define SD_MOSI 23

// LORA
#define SCLK_A_PIN 9
#define MISO_A_PIN 10
#define MOSI_A_PIN 11
#define RADIO_CS 21
#define RST_RADIO 14
#define BOOT 0
#define RXEN 48
#define TXEN 47
#define RST_GPS 5

#endif