//Tests ADS8688
//Intended public interface:
//void runTest
//  handles all testing, logs all results
//  at bottom of file

#include <Arduino.h>
#include <SPI.h>
#include <ADS8688.h>

#define MISO 35
#define SCLK 48
#define MOSI 34
#define CS 36

SPIClass sharedSPI(FSPI);
ADS8688 pressureADC;

/// @brief Requires user confirmation for selected pin
void confirmPins() {
    // Displays given pins and asks for confirmation.
    // Returns on confirmation
    // Stalls on negation

    while (!Serial) delay(100);
    Serial.println("Confirm Pins:");
    Serial.print("MISO: ");
    Serial.print(MISO);
    Serial.print(" MOSI: ");
    Serial.print(MOSI);
    Serial.print(" SCLK: ");
    Serial.print(SCLK);
    Serial.print(" CS: ");
    Serial.println(CS);

    Serial.print("Correct? (y/n): ");
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toLowerCase();

    if (input.equals("y") || input.equals("yes")) {
        return;  // success
    } else {
        Serial.println("Negative response. Enter \"yes\" or \"y\" if pins are correct.\nReset to try again");
        while (true) delay(100);
    }
}

void beginSPI() {
    Serial.print("Starting SPI...");
    sharedSPI.begin(SCLK, MISO, MOSI, CS);
    Serial.println(" Done.");
}

void beginADC() {
    Serial.print("Starting ADC...");
    pressureADC.begin(MISO, SCLK, MOSI, CS, 4.1, 0x01);
    Serial.println(" Done.");
}

void readValues() {
    Serial.print("Reading values: ");
    float rawValues[8];
    pressureADC.readAllChannels(CS, true, rawValues);
    for(int i = 0; i < 8; i++){
        Serial.print(rawValues[i]);
        Serial.print(" ");
    }
    Serial.println();
}

void runADS8688Test() {
    while (!Serial) delay(100);
    Serial.println("Starting ADS8688 Test.");

    confirmPins();

    beginSPI();

    beginADC();

    Serial.println("Reading 10 values:");
    for(int i = 0; i < 10; i++){
        readValues();
    }
}