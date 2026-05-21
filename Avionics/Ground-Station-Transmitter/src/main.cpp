#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <Arduino.h>
#include <LoRa.h>

// ============================================================
// PINS - GROUND STATION
// ============================================================
#define SCLK_A_PIN 37
#define MISO_A_PIN 38
#define MOSI_A_PIN 36
#define RADIO_CS 13
#define RST_RADIO 35
#define RADIO_DIO0 10
#define TXEN 5
#define RXEN 6

// ============================================================
// CONFIG
// ============================================================
#define LORA_FREQ 910E6
#define LORA_SPREADING_FACTOR 8
#define LORA_SIGNAL_BANDWIDTH 250E3
#define LORA_CODING_RATE 5
#define LORA_TX_POWER 20
#define RADIO_PACKET_MAX 250
#define DEBUG_PRINT 1

// ============================================================
// GLOBALS
// ============================================================
SPIClass spiA(FSPI);
bool radioReady = false;

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

bool initRadio()
{
  pinMode(TXEN, OUTPUT);
  pinMode(RXEN, OUTPUT);
  setRX();

  pinMode(RST_RADIO, OUTPUT);
  digitalWrite(RST_RADIO, HIGH);
  delay(20);

  LoRa.setSPI(spiA);
  LoRa.setPins(RADIO_CS, RST_RADIO, RADIO_DIO0);

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
// PARSE + DISPLAY
// T,ms,gps_valid,lat,lon,alt_mm,heading,fixType,sats,
//   imu_valid,ax,ay,az,gx,gy,gz,tempC,
//   alt1_valid,pressure,altTempC,altitude,
//   pt_valid,pt0,pt1,pt2
// ============================================================
void parseTelemetry(char *raw, int rssi, float snr)
{
  char *fields[26];
  int count = 0;

  char *saveptr = nullptr;
  char *token = strtok_r(raw, ",", &saveptr);
  while (token != nullptr && count < 26)
  {
    fields[count++] = token;
    token = strtok_r(nullptr, ",", &saveptr);
  }

  if (count < 25 || strcmp(fields[0], "T") != 0)
  {
    Serial.print("[RX RAW] ");
    Serial.println(raw);
    return;
  }

  unsigned long ms   = strtoul(fields[1], nullptr, 10);
  int gps_valid      = atoi(fields[2]);
  long lat           = atol(fields[3]);
  long lon           = atol(fields[4]);
  long alt_mm        = atol(fields[5]);
  long heading       = atol(fields[6]);
  unsigned fixType   = (unsigned)atoi(fields[7]);
  unsigned sats      = (unsigned)atoi(fields[8]);
  int imu_valid      = atoi(fields[9]);
  float ax           = atof(fields[10]);
  float ay           = atof(fields[11]);
  float az           = atof(fields[12]);
  float gx           = atof(fields[13]);
  float gy           = atof(fields[14]);
  float gz           = atof(fields[15]);
  float imu_tempC    = atof(fields[16]);
  int alt1_valid     = atoi(fields[17]);
  float pressure     = atof(fields[18]);
  float alt_tempC    = atof(fields[19]);
  float altitude     = atof(fields[20]);
  int pt_valid       = atoi(fields[21]);
  float pt0          = atof(fields[22]);
  float pt1          = atof(fields[23]);
  float pt2          = atof(fields[24]);

#if DEBUG_PRINT
  Serial.println("-----------------------------");
  Serial.print("T=");       Serial.print(ms);
  Serial.print("ms RSSI="); Serial.print(rssi);
  Serial.print(" SNR=");    Serial.println(snr, 1);

  Serial.print("GPS[");     Serial.print(gps_valid);
  Serial.print("] lat=");   Serial.print(lat / 1e7, 6);
  Serial.print(" lon=");    Serial.print(lon / 1e7, 6);
  Serial.print(" alt=");    Serial.print(alt_mm / 1000.0, 1);
  Serial.print("m hdg=");   Serial.print(heading / 1e5, 1);
  Serial.print(" fix=");    Serial.print(fixType);
  Serial.print(" sats=");   Serial.println(sats);

  Serial.print("IMU[");     Serial.print(imu_valid);
  Serial.print("] ax=");    Serial.print(ax, 3);
  Serial.print(" ay=");     Serial.print(ay, 3);
  Serial.print(" az=");     Serial.print(az, 3);
  Serial.print(" gx=");     Serial.print(gx, 3);
  Serial.print(" gy=");     Serial.print(gy, 3);
  Serial.print(" gz=");     Serial.print(gz, 3);
  Serial.print(" temp=");   Serial.println(imu_tempC, 2);

  Serial.print("ALT1[");    Serial.print(alt1_valid);
  Serial.print("] pres=");  Serial.print(pressure, 2);
  Serial.print("mbar temp="); Serial.print(alt_tempC, 2);
  Serial.print("C alt=");   Serial.print(altitude, 2);
  Serial.println("m");

  Serial.print("PT[");      Serial.print(pt_valid);
  Serial.print("] pt0=");   Serial.print(pt0, 2);
  Serial.print(" pt1=");    Serial.print(pt1, 2);
  Serial.print(" pt2=");    Serial.println(pt2, 2);
  Serial.println("-----------------------------");
#endif
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  spiA.begin(SCLK_A_PIN, MISO_A_PIN, MOSI_A_PIN, RADIO_CS);
  radioReady = initRadio();

  if (!radioReady)
  {
    Serial.println("[HALT] Radio failed");
    while (1);
  }

  LoRa.receive();
  Serial.println("[RX] Listening for telemetry...");
}

void loop()
{
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
    parseTelemetry(parseBuf, rssi, snr);
  }
}