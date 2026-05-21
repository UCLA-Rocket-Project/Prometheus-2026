#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <Arduino.h>
#include <LoRa.h>

// ============================================================
// PINS - GROUND STATION (WROOM ESP32)
// ============================================================
#define SCLK_A_PIN 37
#define MISO_A_PIN 39
#define MOSI_A_PIN 36
#define RADIO_CS 13
#define RST_RADIO 35
#define RADIO_DIO1 11 //otherwise try 10
#define TXEN 5
#define RXEN 6

// ============================================================
// CONFIG
// ============================================================
#define LORA_FREQ 915E6
#define LORA_SPREADING_FACTOR 8
#define LORA_SIGNAL_BANDWIDTH 250E3
#define LORA_CODING_RATE 5
#define LORA_TX_POWER 20
#define RADIO_PACKET_MAX 220
#define LOOP_DELAY_MS 100

// ============================================================
// GLOBALS
// ============================================================
SPIClass spiA(VSPI);

bool radioReady = false;
bool receiving = false;

// ============================================================
// RADIO CONTROL
// ============================================================
void setTX()
{
  digitalWrite(RXEN, LOW);
  digitalWrite(TXEN, HIGH);
  delayMicroseconds(100);
}

void setRX()
{
  digitalWrite(TXEN, LOW);
  digitalWrite(RXEN, HIGH);
  delayMicroseconds(100);
}

// ============================================================
// INIT
// ============================================================
bool initRadio()
{
  pinMode(TXEN, OUTPUT);
  pinMode(RXEN, OUTPUT);
  setRX();

  pinMode(RST_RADIO, OUTPUT);
  digitalWrite(RST_RADIO, HIGH);
  delay(20);

  LoRa.setSPI(spiA);
  LoRa.setPins(RADIO_CS, RST_RADIO, RADIO_DIO1);

  if (!LoRa.begin(LORA_FREQ))
  {
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
// SEND
// ============================================================
void sendPacket(const String &msg)
{
  if (!radioReady)
    return;
  if (msg.length() == 0)
  {
    Serial.println("[TX FAIL] empty");
    return;
  }
  if (msg.length() >= RADIO_PACKET_MAX)
  {
    Serial.println("[TX FAIL] too long");
    return;
  }

  setTX();
  LoRa.beginPacket();
  LoRa.print(msg);
  int state = LoRa.endPacket(false);
  setRX();

  if (state == 1)
  {
    Serial.print("[TX ");
    Serial.print(msg.length());
    Serial.print("B] ");
    Serial.println(msg);
  }
  else
  {
    Serial.println("[TX FAIL]");
  }
}

// ============================================================
// PARSE + PRINT GPS PACKET
// format: G,lat,lon,alt_mm,fixType,sats
// ============================================================
void parseAndPrintGPS(char *packet, int rssi, float snr)
{
  char *fields[6];
  int count = 0;

  char *saveptr = nullptr;
  char *token = strtok_r(packet, ",", &saveptr);
  while (token != nullptr && count < 6)
  {
    fields[count++] = token;
    token = strtok_r(nullptr, ",", &saveptr);
  }

  if (count != 6 || strcmp(fields[0], "G") != 0)
  {
    Serial.print("[RX RAW] ");
    Serial.println(packet);
    return;
  }

  long lat = atol(fields[1]);
  long lon = atol(fields[2]);
  long alt_mm = atol(fields[3]);
  int fixType = atoi(fields[4]);
  int sats = atoi(fields[5]);

  Serial.print("[GPS] lat=");
  Serial.print(lat / 10000000.0, 6);
  Serial.print(" lon=");
  Serial.print(lon / 10000000.0, 6);
  Serial.print(" alt=");
  Serial.print(alt_mm / 1000.0, 1);
  Serial.print("m fix=");
  Serial.print(fixType);
  Serial.print(" sats=");
  Serial.print(sats);
  Serial.print(" RSSI=");
  Serial.print(rssi);
  Serial.print(" SNR=");
  Serial.println(snr, 1);
}

// ============================================================
// RECEIVE LOOP
// ============================================================
void receiveLoop()
{
  Serial.println("[RX MODE] Listening for GPS... press X to stop");
  LoRa.receive();

  while (true)
  {
    // check for X to exit
    if (Serial.available())
    {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      if (cmd == "X" || cmd == "x")
      {
        Serial.println("[RX MODE] Stopped. Back to send mode.");
        receiving = false;
        return;
      }
    }

    int packetSize = LoRa.parsePacket();
    if (packetSize > 0 && packetSize < RADIO_PACKET_MAX)
    {
      char packet[RADIO_PACKET_MAX];
      int index = 0;
      while (LoRa.available() && index < RADIO_PACKET_MAX - 1)
      {
        packet[index++] = (char)LoRa.read();
      }
      packet[index] = '\0';

      int rssi = LoRa.packetRssi();
      float snr = LoRa.packetSnr();

      char parseBuf[RADIO_PACKET_MAX];
      strncpy(parseBuf, packet, RADIO_PACKET_MAX);
      parseAndPrintGPS(parseBuf, rssi, snr);
    }
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup()
{
  Serial.begin(115200);
  delay(3000);

  Serial.println("BOOT 1");
  spiA.begin(SCLK_A_PIN, MISO_A_PIN, MOSI_A_PIN, RADIO_CS);
  Serial.println("BOOT 2");
  radioReady = initRadio();

  Serial.println("Ready. Receiving Data, or 'R' to send command.");
}

void loop()
{
  Serial.println("Enter instruction (R command mode):");
  while (Serial.available() == 0)
  {
  }

  String instruction = Serial.readStringUntil('\n');
  instruction.trim();

  if (instruction == "Launch" || instruction == "launch" || instruction == "LAUNCH")
  {
    sendPacket(instruction);
  }
  else
  {
    receiving = true;
    // sendPacket(""); // tell the nosecone to start transmitting
    receiveLoop();
  }

  delay(LOOP_DELAY_MS);
}