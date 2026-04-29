// ARES EGSE 2024-2025 Load Cell System Code
// ESP32-S3 Documentation: https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf
// ADS1256 Documentation: https://www.ti.com/lit/gpn/ads1256
// ADS1256 Library Information: https://github.com/CuriousScientist0/ADS1256/tree/main

#include <Arduino.h>
#include <ADS1256.h>

// This is correct pins @Euan
// Define custom SPI pins for ADS1256 Module
#define SPI_MISO 35
#define SPI_SCLK 48
#define SPI_MOSI 34
#define SPI_CS  7

#define DRDY 4

// Define LED indicator pin
#define LED 38

SPIClass spi_ads(FSPI); // Create custom SPI instance
ADS1256 ADC(&spi_ads, DRDY, SPI_CS, 2.5); // Instantiate ADS1256

// Calibration coefficients y=Ax+B
float calibrationA = -195.25664;
float calibrationB = -91.19858;

// convertToWeight function
// Returns measured force (lbf)
float convertToWeight(float voltage) {
  return (calibrationA * voltage) + calibrationB;
}

void setup()
{
    // Initialize Serial
    Serial.begin(115200);
    delay(100);

    // Configure SPI pins
    pinMode(SPI_MISO, INPUT_PULLUP);
    pinMode(SPI_SCLK, OUTPUT);
    pinMode(SPI_MOSI, OUTPUT);

    // Configure LED pin and write HIGH
    pinMode(LED, OUTPUT); digitalWrite(LED, HIGH);

    // Initialize the SPI bus with custom pins
    spi_ads.begin(SPI_SCLK, SPI_MISO, SPI_MOSI, SPI_CS);

    // ADC Setup
    ADC.InitializeADC(); 

    // Set Gain (PGA)
    ADC.setPGA(PGA_64);  // Maximum gain for small signals

    //Set Mux
    ADC.setMUX(DIFF_0_1);

    // Set Sampling Rate
    ADC.setDRATE(DRATE_1000SPS);  

    delay(100);
}

void loop() {
  float voltages[2]; // Store two differential voltages

  // ----- Read AIN0-AIN1 -----
  ADC.setMUX(DIFF_0_1);         // Set MUX to AIN0-AIN1
  voltages[0] = ADC.convertToVoltage(ADC.readSingle()); // Read value and convert to voltage

  // ----- Read AIN2-AIN3 -----
  ADC.setMUX(DIFF_2_3);         // Set MUX to AIN2-AIN3
  voltages[1] = ADC.convertToVoltage(ADC.readSingle());

  // ----- Print Results -----
  Serial.print("AIN0-1: ");
  Serial.print(voltages[0], 6);
  Serial.print(" V\t");

  Serial.print("AIN2-3: ");
  Serial.print(voltages[1], 6);
  Serial.println(" V");
}
