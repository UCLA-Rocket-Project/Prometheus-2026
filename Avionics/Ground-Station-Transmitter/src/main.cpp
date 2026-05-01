//code derived from nosecone transmitter board 

#include <Wire.h>
#include <SPI.h>
#include <FS.h>

#include <math.h>

#include <SparkFun_u-blox_GNSS_v3.h>
#include <LoRa.h>

#include <Arduino.h>

// ============================================================
// PINS - used from NOSE CONE RECEIVER BOARD --> use esp32dev since its wroom esp32
// ============================================================

// SPI BUS A: RADIO
#define SCLK_A_PIN 32
#define MISO_A_PIN 35
#define MOSI_A_PIN 25

#define RADIO_CS 33
#define RST_RADIO 27
#define RADIO_DIO1 14
#define TXEN 17
#define RXEN 5

// ============================================================
// CONFIG
// ============================================================
#define LOOP_DELAY_MS 100
#define RADIO_PACKET_MAX 220

// IMPORTANT: make these match receiver exactly
#define LORA_FREQ              915E6
#define LORA_SPREADING_FACTOR  8
#define LORA_SIGNAL_BANDWIDTH  250E3
#define LORA_CODING_RATE       5
#define LORA_TX_POWER          20

// MS5607 commands
static const uint8_t MS5607_CMD_RESET = 0x1E;
static const uint8_t MS5607_CMD_ADC_READ = 0x00;
static const uint8_t MS5607_CMD_PROM_READ_BASE = 0xA0;
static const uint8_t MS5607_CMD_CONV_D1 = 0x48;  // OSR 4096
static const uint8_t MS5607_CMD_CONV_D2 = 0x58;  // OSR 4096

// ============================================================
// GLOBALS
// ============================================================
SPIClass spiA(VSPI);

TwoWire gnssWire = TwoWire(0);
SFE_UBLOX_GNSS gnss;

bool radioReady = false;

// ============================================================
// MS5607 DRIVER
// ============================================================
class MS5607 {
public:
  MS5607() : spi(nullptr), cs(-1), initialized(false) {
    for (int i = 0; i < 8; i++) C[i] = 0;
  }

  bool begin(SPIClass *bus, int csPin) {
    spi = bus;
    cs = csPin;

    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);

    reset();
    delay(10);

    for (int i = 0; i < 8; i++) {
      C[i] = readPROM(i);
    }

    if (C[1] == 0 || C[1] == 0xFFFF || C[2] == 0 || C[2] == 0xFFFF) {
      initialized = false;
      return false;
    }

    initialized = true;
    return true;
  }

private:
  SPIClass *spi;
  int cs;
  bool initialized;
  uint16_t C[8];

  void beginTx() {
    spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(cs, LOW);
  }

  void endTx() {
    digitalWrite(cs, HIGH);
    spi->endTransaction();
  }

  void reset() {
    beginTx();
    spi->transfer(MS5607_CMD_RESET);
    endTx();
  }

  uint16_t readPROM(uint8_t index) {
    uint8_t cmd = MS5607_CMD_PROM_READ_BASE + (index * 2);
    beginTx();
    spi->transfer(cmd);
    uint8_t msb = spi->transfer(0x00);
    uint8_t lsb = spi->transfer(0x00);
    endTx();
    return ((uint16_t)msb << 8) | lsb;
  }

  uint32_t convertAndRead(uint8_t cmd) {
    beginTx();
    spi->transfer(cmd);
    endTx();

    delayMicroseconds(10000);

    beginTx();
    spi->transfer(MS5607_CMD_ADC_READ);
    uint8_t b1 = spi->transfer(0x00);
    uint8_t b2 = spi->transfer(0x00);
    uint8_t b3 = spi->transfer(0x00);
    endTx();

    return ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | b3;
  }
};

// ============================================================
// RADIO CONTROL
// ============================================================
#define LORA_CS   RADIO_CS
#define LORA_RST  RST_RADIO
#define LORA_DIO0 RADIO_DIO1

void setTX() {
  digitalWrite(RXEN, LOW);
  digitalWrite(TXEN, HIGH);
  delayMicroseconds(100);
}

void setRX() {
  digitalWrite(TXEN, LOW);
  digitalWrite(RXEN, HIGH);
  delayMicroseconds(100);
}

// ============================================================
// INIT
// ============================================================

bool initRadio() {
  pinMode(TXEN, OUTPUT);
  pinMode(RXEN, OUTPUT);
  setRX();

  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);
  delay(20);

  LoRa.setSPI(spiA);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("[LORA] FAIL");
    return false;
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setTxPower(LORA_TX_POWER);

  Serial.println("[LORA] OK");
  return true;
}

// ============================================================
// PACKET BUILD / TX
// ============================================================

void sendPacket(const String &instruct) {
  if (!radioReady)
    return;

  if (instruct.length() == 0) {
    Serial.println("[TX FAIL] empty message");
    return;
  }

  if (instruct.length() >= RADIO_PACKET_MAX) {
    Serial.println("[TX FAIL] message too long");
    return;
  }

  setTX();

  LoRa.beginPacket();
  LoRa.print(instruct);
  int state = LoRa.endPacket(false);

  setRX();

  if (state == 1) {
    Serial.print("[TX ");
    Serial.print(instruct.length());
    Serial.print("B] ");
    Serial.println(instruct);
  }
  else {
    Serial.println(" [TX FAIL]");
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("BOOT 1");
  spiA.begin(SCLK_A_PIN, MISO_A_PIN, MOSI_A_PIN, RADIO_CS);
  Serial.println("BOOT 2");
  radioReady = initRadio();

}


void loop() {
  Serial.println("Enter instruction:");
  while (Serial.available() == 0) {
  }

  String instruction = Serial.readString();
  instruction.trim();

  sendPacket(instruction);

  delay(LOOP_DELAY_MS);
}