// daq
#include <ADS1256.h>
#include <Arduino.h>
#include <SPI.h>

#include "ADS8688.h"
#include "lcpt_calibration.h"
// Load Cell (ADS1256) SPI Pins
#define ADS1256_MISO 35
#define ADS1256_SCLK 48
#define ADS1256_MOSI 34
#define ADS1256_CS 7
#define ADS1256_DRDY 4
// PT (ADS8688) SPI Pin
#define ADS8688_CS 36
// LED indicator pin
#define LED 38

static const int logicalToLibraryPtIndex[8] = {
    4,
    5,
    6,
    7,
    0,
    1,
    2,
    3};

SPIClass sharedSPI(FSPI);
// ADS1256 instance
ADS1256 loadCellADC(&sharedSPI, ADS1256_DRDY, ADS1256_CS, 2.5);
// ADS8688 instance
ADS8688 pressureADC;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Serial Started.");
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    Serial.println("led");
    // Start custom SPI bus
    sharedSPI.begin(ADS1256_SCLK, ADS1256_MISO, ADS1256_MOSI, ADS1256_CS);
    // Initialize ADS1256 (Load Cell)
    loadCellADC.InitializeADC();
    loadCellADC.setPGA(PGA_64);
    loadCellADC.setMUX(DIFF_0_1);
    loadCellADC.setDRATE(DRATE_1000SPS);
    // Initialize ADS8688 (PTs)
    pressureADC.begin(ADS1256_MISO, ADS1256_SCLK, ADS1256_MOSI, ADS8688_CS, 4.1, 0x05);
    pressureADC.setInputRange(ADS8688_CS, 0x05);
    Serial.println("Setup Completed.");
    startCalibration();
}

float getLCValue(int channel) {
    if (channel == 1) {
        return loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_2_3));
    }
    return loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_0_1));
}

float getPTValue(int correctChannel) {
    float voltages[8];
    pressureADC.readAllChannels(ADS8688_CS, true, voltages);
    if (correctChannel < 0 || correctChannel > 7) return 0.0f;
    return voltages[logicalToLibraryPtIndex[correctChannel]];
}

void loop() {
    delay(10000000);
}
