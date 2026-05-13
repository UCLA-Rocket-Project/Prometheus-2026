#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <SparkFun_u-blox_GNSS_v3.h>

// ============================================================
// PINS
// ============================================================
#define GPS_SDA 17
#define GPS_SCL 16
#define GNSS_ADDRESS 0x42
#define RST_GPS 5

#define SCLK_A_PIN 9
#define MISO_A_PIN 10
#define MOSI_A_PIN 11
#define RADIO_CS 21
#define RST_RADIO 14
#define RADIO_DIO1 12
#define TXEN 47
#define RXEN 48

// ============================================================
// CONFIG
// ============================================================
#define LORA_FREQ 915E6
#define LORA_SPREADING_FACTOR 8
#define LORA_SIGNAL_BANDWIDTH 250E3
#define LORA_CODING_RATE 5
#define LORA_TX_POWER 20

#define TX_INTERVAL_MS 500 // how often to send GPS when transmitting
#define RX_WINDOW_MS 200   // how long to listen between transmits

// ============================================================
// GLOBALS
// ============================================================
SPIClass spiA(FSPI);
TwoWire gnssWire = TwoWire(0);
SFE_UBLOX_GNSS gnss;

bool gpsReady = false;
bool transmitting = true; // flipped to true when "T" received

unsigned long lastTxTime = 0;

// ============================================================
// RADIO
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
  digitalWrite(RST_RADIO, LOW);
  delay(10);
  digitalWrite(RST_RADIO, HIGH);
  delay(20);

  spiA.begin(SCLK_A_PIN, MISO_A_PIN, MOSI_A_PIN, RADIO_CS);
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

bool initGPS()
{
  gnssWire.begin(GPS_SDA, GPS_SCL);

  for (int i = 0; i < 10; i++)
  {
    if (gnss.begin(gnssWire, GNSS_ADDRESS))
    {
      gnss.setI2COutput(COM_TYPE_UBX);
      Serial.println("[GPS] OK");
      return true;
    }
    delay(300);
  }

  Serial.println("[GPS] FAIL");
  return false;
}

// ============================================================
// GPS TX
// ============================================================
void sendGPS()
{
  if (!gpsReady)
  {
    Serial.println("[TX] GPS not ready");
    return;
  }

  if (!gnss.getPVT())
  {
    Serial.println("[TX] No PVT");
    return;
  }

  int32_t lat = gnss.getLatitude();
  int32_t lon = gnss.getLongitude();
  int32_t alt_mm = abs((gnss.getAltitudeMSL()));
  uint8_t fixType = gnss.getFixType();
  uint8_t sats = gnss.getSIV();

  char packet[128];
  int len = snprintf(packet, sizeof(packet),
                     "G,%ld,%ld,%ld,%u,%u",
                     (long)lat, (long)lon, (long)alt_mm,
                     (unsigned)fixType, (unsigned)sats);

  setTX();
  LoRa.beginPacket();
  LoRa.write((const uint8_t *)packet, len);
  LoRa.endPacket(false);
  setRX();

  Serial.print("[TX] ");
  Serial.println(packet);
}

// ============================================================
// RECEIVE
// ============================================================
void checkIncoming()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0)
    return;

  String msg = "";
  while (LoRa.available())
  {
    msg += (char)LoRa.read();
  }
  msg.trim();

  Serial.print("[RX] ");
  Serial.println(msg);

  if (msg == "launch" || msg == "Launch" || msg == "LAUNCH")
  {
    //this needs to do SD card saving 
    //Serial.println("[CMD] Starting GPS transmission");
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
  bool radioOK = initRadio();
  Serial.println("BOOT 2");
  gpsReady = initGPS();
  Serial.println("BOOT 3");

  if (!radioOK)
  {
    Serial.println("[INIT] Radio failed - halting");
    while (1)
      delay(1000);
  }

  LoRa.receive();
  Serial.println("Ready. Send 'T' to start GPS TX, 'S' to stop.");
}

void loop()
{
  checkIncoming();

  if (transmitting && (millis() - lastTxTime >= TX_INTERVAL_MS))
  {
    sendGPS();
    lastTxTime = millis();
    LoRa.receive(); // re-arm RX mode after each TX
  }
}