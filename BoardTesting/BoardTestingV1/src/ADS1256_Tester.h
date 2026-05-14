
#include <Arduino.h>
#include <SPI.h>
#include <ADS1256.h>

int miso;
int sclk;
int mosi;
int cs;
int drdy;

SPIClass sharedSPI(FSPI);
ADS1256 loadCellADC(&sharedSPI, drdy, cs, 2.5);

/// @brief Requires user confirmation for selected pin
void confirmPins() {
    // Displays given pins and asks for confirmation.
    // Returns on confirmation
    // Stalls on negation

    while (!Serial) delay(100);
    Serial.println("Confirm Pins:");
    Serial.print("MISO: ");
    Serial.print(miso);
    Serial.print(" MOSI: ");
    Serial.print(mosi);
    Serial.print(" SCLK: ");
    Serial.print(sclk);
    Serial.print(" CS: ");
    Serial.print(cs);
    Serial.print(" DRDY: ");
    Serial.println(drdy);

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
    sharedSPI.begin(sclk, miso, mosi, cs);
    Serial.println(" Done.");
}

void beginADC() {
    Serial.print("Starting ADC...");
    loadCellADC.InitializeADC();
    Serial.print(" Initialized.");
    loadCellADC.setPGA(PGA_64);
    loadCellADC.setMUX(DIFF_0_1);
    loadCellADC.setDRATE(DRATE_1000SPS);
    Serial.println(" Configured.");
}

void readValues(){
    const int multiplier = 100000;
    Serial.print("Reading values (multiplied by ");
    Serial.print(multiplier);
    Serial.print("): ");

    float channel0 = multiplier * loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_0_1));
    float channel1 = multiplier * loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_2_3));
    float channel2 = multiplier * loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_4_5));
    Serial.print(channel0);
    Serial.print(" ");
    Serial.print(channel1);
    Serial.print(" ");
    Serial.println(channel2);
}

void runADS1256Test(int misoP, int mosiP, int sclkP, int csP, int drdyP){
    while(!Serial)delay(100);
    delay(1000);

    Serial.println("Starting 1256 Test.");

    confirmPins();

    beginSPI();

    beginADC();

    Serial.println("Reading 10 values.");
    for(int i = 0; i < 10; i++){
        readValues();
    }
}