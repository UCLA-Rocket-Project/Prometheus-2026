// THIS IS TRANSMITTER CODE
#include <ADS1256.h>
#include <Adafruit_ADS1X15.h>
#include <Arduino.h>
#include <HardwareSerial.h>
#include <SPI.h>

#include <string>

#include "ADS8688.h"

// SPI
#define MISO 35
#define MOSI 34
#define SCLK 48

// Load Cell SPI Pins
#define ADS1256_CS 7
#define ADS1256_DRDY 4

// PT SPI Pins
#define ADS8688_CS 36

// RS Pins
#define RO_PIN 44
#define DI_PIN 43
#define DE_RE_PIN 41

int logicalToLibraryPtIndex[8] = {
    4,
    5,
    6,
    7,
    0,
    1,
    2,
    3};

HardwareSerial rs485Serial(2);

SPIClass sharedSPI(FSPI);
ADS1256 loadCellADC(&sharedSPI, ADS1256_DRDY, ADS1256_CS, 2.5);
ADS8688 pressureADC;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Serial Started.");

    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, HIGH);

    rs485Serial.begin(115200, SERIAL_8N1, RO_PIN, DI_PIN);

    // Start custom SPI bus
    sharedSPI.begin(SCLK, MISO, MOSI, -1);

    // Initialize ADS1256 (Load Cell)
    loadCellADC.InitializeADC();
    loadCellADC.setPGA(PGA_64);
    loadCellADC.setDRATE(DRATE_1000SPS);

    // Initialize ADS8688 (PTs)
    pressureADC.begin(MISO, SCLK, MOSI, ADS8688_CS, 4.1, 0x05);
    pressureADC.setInputRange(ADS8688_CS, 0x05);

    Serial.println("Setup complete");
}
float getCalibratedValue(float m, float b, float raw) {
    return raw * m + b;
}
float mValues[8] = {1, 1, 1, 1, 1, 1, 1, 1};
float bValues[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void loop() {
    // --- PT Measurements (8 channels) ---
    float ptVoltages[8];
    float ptCalibrated[8];
    float logicalPtVoltageOrdering[8];

    pressureADC.readAllChannels(ADS8688_CS, true, ptVoltages);

    // Not the most clean
    for (int i = 0; i < 8; i++) {
        logicalPtVoltageOrdering[i] = ptVoltages[logicalToLibraryPtIndex[i]];
    }
    for (int i = 0; i < 8; i++) {
        ptCalibrated[i] = getCalibratedValue(mValues[i], bValues[i], logicalPtVoltageOrdering[i]);
    }

    float loadCell[2] = {-1, -1};
    loadCell[0] = loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_0_1));
    loadCell[1] = loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_2_3));
    char finalStr[400];
    snprintf(
        finalStr,
        sizeof(finalStr),
        "rocket_data pt0=%.4f,pt1=%.4f,pt2=%.4f,pt3=%.4f,pt4=%.4f,pt5=%.4f,pt6=%.4f,pt7=%.4f,lc0=%.4f,lc1=%.4f,uptime_ms=%lu",
        ptCalibrated[0],
        ptCalibrated[1],
        ptCalibrated[2],
        ptCalibrated[3],
        ptCalibrated[4],
        ptCalibrated[5],
        ptCalibrated[6],
        ptCalibrated[7],
        loadCell[0],
        loadCell[1],
        millis());

    Serial.println(finalStr);
    
    rs485Serial.println(finalStr);
    delay(10);
}
