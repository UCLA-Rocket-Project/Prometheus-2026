#include <Arduino.h>
#include <SPI.h>
#include <ADS1256.h>
#include <SD.h>
#include <FS.h>
#include <Wire.h>
#include "pins.h"

// --------------------------------------------------
// SPI buses
// ADS1256 on FSPI (SPI4 bus)
// SD card on HSPI (SPI2 bus)
// --------------------------------------------------
SPIClass adcSpi(FSPI);
SPIClass sdSpi(HSPI);

// ADS1256(SPI*, DRDY, CS, VREF)
ADS1256 adc(&adcSpi, DRDY, SPI4_CS, 2.500);

// --------------------------------------------------
// Globals
// --------------------------------------------------
float baseline = 0.0;
bool baselineSet = false;

String fileName;

// --------------------------------------------------
// File helpers
// --------------------------------------------------
String makeFile() {
  int index = 0;
  String path;
  while (true) {
    path = "/launch" + String(index) + ".csv";
    if (!SD.exists(path)) {
      break;
    }
    index++;
  }
  return path;
}


void appendFile(fs::FS &fs, const char *path, const char *message) {
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (!file.print(message)) {
    Serial.println("Append failed");
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (!file.print(message)) {
    Serial.println("Write failed");
  }
  file.close();
}

// --------------------------------------------------
// Helpers
// --------------------------------------------------
static void printBanner() {
  Serial.println();
  Serial.println("======================================");
  Serial.println("=== BODY TUBE BOARD ===");
  Serial.println("ADS1256 + SD Card Logger");
  Serial.println("ADS1256: FSPI  SCK=17 MISO=16 MOSI=15");
  Serial.println("SD Card: HSPI  SCK=39 MISO=37 MOSI=38");
  Serial.println("======================================");
}

static void initBoardPins() {
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(BUZZ, OUTPUT);

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(BUZZ, LOW);

  // Drive all CS lines high before SPI init
  pinMode(SPI4_CS, OUTPUT);
  digitalWrite(SPI4_CS, HIGH);
  pinMode(SPI4_CS2, OUTPUT);
  digitalWrite(SPI4_CS2, HIGH);
  pinMode(SPI2_CS, OUTPUT);
  digitalWrite(SPI2_CS, HIGH);

  pinMode(DRDY, INPUT);
}

float calibrate(float num) {
  return 2.20462 * (-15588 * num + 1.66);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  initBoardPins();
  printBanner();

  // --------------------------------------------------
  // SD card on SPI2 (HSPI)  SCK=39 MISO=37 MOSI=38 CS=40
  // --------------------------------------------------
  sdSpi.begin(SPI2_SCK, SPI2_MISO, SPI2_MOSI, SPI2_CS);

  bool sd_init = false;
  while (!sd_init) {
    sd_init = SD.begin(SPI2_CS, sdSpi);
    if (!sd_init) {
      Serial.println("Waiting for SD card... Insert card now.");
      delay(1000);
    }
  }
  Serial.println("SD initialization done.");

  fileName = makeFile();
  writeFile(SD, fileName.c_str(), "LC_RAW,VOLTAGE\n");
  // Read back the header to confirm file was created
  File f = SD.open(fileName);
  if (f) {
    Serial.print("File contents: ");
    while (f.available()) {
      Serial.write(f.read());
    }
    f.close();
  } else {
    Serial.println("Could not open file to verify!");
  }
  Serial.print("Logging to: ");
  Serial.println(fileName);

  // --------------------------------------------------
  // ADS1256 on FSPI  SCK=17 MISO=16 MOSI=15 CS=9
  // --------------------------------------------------
  adcSpi.begin(SPI4_SCK, SPI4_MISO, SPI4_MOSI, SPI4_CS);

  Serial.println("Initializing ADS1256...");
  adc.InitializeADC();
  adc.writeRegister(STATUS_REG, 0x00);  // disable BUFEN and ACAL
  adc.setPGA(PGA_64);
  adc.setMUX(DIFF_0_1);
  adc.setDRATE(DRATE_1000SPS);

  uint8_t status = adc.readRegister(STATUS_REG);
  Serial.print("STATUS register: 0x");
  Serial.println(status, HEX);

  Serial.print("PGA:   ");
  Serial.println(adc.getPGA());
  Serial.print("MUX:   ");
  Serial.println(adc.readRegister(MUX_REG));
  Serial.print("DRATE: ");
  Serial.println(adc.readRegister(DRATE_REG));

  // Blink LED1 to signal ready
  digitalWrite(LED1, HIGH);
  delay(500);
  digitalWrite(LED1, LOW);

  Serial.println("Ready.");
}

bool waitDRDY(uint32_t timeout_us = 5000) {
  uint32_t start = micros();
  while (digitalRead(DRDY)) {
    if (micros() - start > timeout_us) {
      return false;  // timed out, skip sample
    }
  }
  return true;
}

void loop() {
  if (waitDRDY()) {
    long lc = adc.readDifferentialFaster(DIFF_0_1);

    Serial.print("RAW: ");
    Serial.println(lc);

    float voltage = calibrate(adc.convertToVoltage(lc));

    String message = String(lc) + ","
                     + String(voltage, 6) +  "\n";

    Serial.print(message);
    appendFile(SD, fileName.c_str(), message.c_str());
  } else {
    Serial.println("DRDY timeout");
  }
}
