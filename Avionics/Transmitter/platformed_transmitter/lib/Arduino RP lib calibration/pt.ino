// this is the code for the PT, take note that you multiply the calibrated voltage by 0.5 because of there was some inconsistency with the original datasheet

#include <Arduino.h>
#include "ADS8688.h"

ADS8688 adc;

// CORRECT PINS
#define MISO 35
#define SCLK 48
#define MOSI 34
#define CS_1 36

void setup() {
    // Initialize Serial
    Serial.begin(115200);

    delay(5);

    Serial.println("Ready!");
    
    // Initialize ADCs
    // NOTE: Change pins later, change back to 0x05 and correct if reading is not correct
    adc.begin(MISO, SCLK, MOSI, CS_1, 4.1, 0x05);
    adc.setInputRange(CS_1, 0x05);

    Serial.println("End Setup");
}

void loop() {
    Serial.println("HI");
    float voltages[8]; // Contains 8 Pressure Readings
    float calibratedVoltages[8]; // Store calibrated values

    // Read PTs from ADC (CS39)
    adc.readAllChannels(CS_1, true, voltages);

    // Apply calibration to PTs
    for (int i = 0; i < 8; i++) {
        calibratedVoltages[i] = 0.5f * voltages[i];
        // calibratedVoltages[i] = voltages[i];
    }

    // Print all 8 values in one line
    for (int i = 0; i < 7; i++) {
        Serial.print(String(calibratedVoltages[i],2) + ",");
    }
    Serial.println(String(calibratedVoltages[7],2)); // Last value without trailing comma
    delay(100);
}