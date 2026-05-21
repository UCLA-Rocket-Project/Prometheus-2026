//daq
#include <Arduino.h>
#include <ADS1256.h>
#include "ADS8688.h"
#include <SPI.h>
#include "lcpt_calibration.h"
// Load Cell (ADS1256) SPI Pins
#define ADS1256_MISO 16
#define ADS1256_SCLK 17
#define ADS1256_MOSI 15
// #define ADS1256_CS 6
// #define ADS1256_DRDY 4
// PT (ADS8688) SPI Pin
#define ADS8688_CS 6
// LED indicator pin
// #define LED 38

SPIClass sharedSPI(FSPI);
// ADS1256 instance
// ADS1256 loadCellADC(&sharedSPI, ADS1256_DRDY, ADS1256_CS, 2.5);
// ADS8688 instance
ADS8688 pressureADC;

void setup() {

Serial.begin(115200);
delay(1000);

Serial.println("Serial Started.");

sharedSPI.begin(ADS1256_SCLK, ADS1256_MISO, ADS1256_MOSI, ADS8688_CS);

pressureADC.begin(ADS1256_MISO, ADS1256_SCLK, ADS1256_MOSI, ADS8688_CS, 4.1, 0x05);
pressureADC.setInputRange(ADS8688_CS, 0x05);

Serial.println("Setup Completed.");
startCalibration();
}

float getLCValue(int channel) {
  // if (channel == 1) {
  //   return loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_2_3)) * 100000.0f;
  // }
  // return loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_0_1)) * 100000.0f;
  return 0;
}

float getPTValue(int correctChannel) {
  float voltages[8];
  pressureADC.readAllChannels(ADS8688_CS, true, voltages);
  if (correctChannel < 2 || correctChannel > 4) return 0.0f;
  return voltages[correctChannel];
}

void loop() {
delay(10000000);
}
