#include <Arduino.h>
#include <ADS1256.h>
#include "ADS8688.h"
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Load Cell (ADS1256) SPI Pins
#define ADS1256_MISO 35
#define ADS1256_SCLK 48
#define ADS1256_MOSI 34
#define ADS1256_CS   7
#define ADS1256_DRDY 4

// PT (ADS8688) SPI Pin
#define ADS8688_CS 36

// LED indicator pin
#define LED 38

// WiFi + MQTT credentials
const char* ssid = "ILAY";
const char* password = "lebronpookie123";
const char* mqtt_server = "192.168.0.103";
WiFiClient espClient;
// PubSubClient client(espClient);

// SPI bus shared between both ADCs
SPIClass sharedSPI(FSPI);

// ADS1256 instance
ADS1256 loadCellADC(&sharedSPI, ADS1256_DRDY, ADS1256_CS, 2.5);

// ADS8688 instance
ADS8688 pressureADC;

// Calibration coefficients for load cell
float calibrationA1 = -57938.14;
float calibrationB1 = 1.16948;
// Convert voltage to weight
float convertToWeightLC1(float voltage) {
  return (calibrationA1 * voltage) + calibrationB1;
}

float calibrationA2 = -401428.57;
float calibrationB2 = -3.46143;
float convertToWeightLC2(float voltage) {
  return (calibrationA2 * voltage) + calibrationB2;
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, password);

  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.println("still connecting...");
  // }

  // Serial.println("WiFi connected");
  // Serial.print("IP address: ");
  // Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  // Start custom SPI bus
  sharedSPI.begin(ADS1256_SCLK, ADS1256_MISO, ADS1256_MOSI, -1);

  // Initialize ADS1256 (Load Cell)
  loadCellADC.InitializeADC();
  loadCellADC.setPGA(PGA_64);
  // loadCellADC.setMUX(DIFF_0_1);
  loadCellADC.setDRATE(DRATE_1000SPS);

  // Initialize ADS8688 (PTs)
  pressureADC.begin(ADS1256_MISO, ADS1256_SCLK, ADS1256_MOSI, ADS8688_CS, 4.1, 0x05);
  pressureADC.setInputRange(ADS8688_CS, 0x05);

  // // WiFi + MQTT setup
  // setup_wifi();
  // client.setServer(mqtt_server, 1883);

  // Serial.println("Setup complete");
}

void loop() {
  // --- PT Measurements (8 channels) ---
  float ptVoltages[8];
  float ptCalibrated[8];
  pressureADC.readAllChannels(ADS8688_CS, true, ptVoltages);
  for (int i = 0; i < 8; i++) {
    ptCalibrated[i] = 0.5f * ptVoltages[i];  // placeholder calibration
  }

  for (int i = 0; i < 8; ++i) {
    Serial.printf("%4.10f,", ptCalibrated[i]);
  }
  Serial.println();

  delay(200);
}