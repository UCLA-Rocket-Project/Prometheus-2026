#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================
// RECEIVER BOARD PINS
// ============================================================
#define LORA_MISO  35
#define LORA_MOSI  25
#define LORA_SCK   32
#define LORA_CS    33

#define LORA_D0    14
#define LORA_RST   27
#define LORA_TX_EN 17
#define LORA_RX_EN 5

// ============================================================
// LORA SETTINGS
// Must match transmitter exactly
// ============================================================
#define LORA_FREQ               915E6
#define LORA_SPREADING_FACTOR   8
#define LORA_SIGNAL_BANDWIDTH   250E3
#define LORA_CODING_RATE        5

#define RX_PACKET_MAX 256
#define EXPECTED_FIELDS 25

SPIClass spi_bus; //used to be spi_bus(VSPI)

// ============================================================
// TELEMETRY STRUCT
// ============================================================
struct Telemetry {
  unsigned long ms;

  int gpsValid;
  long lat;
  long lon;
  long gpsAltMm;
  long heading;
  unsigned int fixType;
  unsigned int sats;

  int imuValid;
  float ax; //accel
  float ay;
  float az;
  float gx;
  float gy;
  float gz;
  float imuTempC;

//altitude sensors
  int alt1Valid;
  float alt1Pressure;
  float alt1TempC;
  float alt1AltM;

  int alt2Valid;
  float alt2Pressure;
  float alt2TempC;
  float alt2AltM;
};

// ============================================================
// PARSER
// ============================================================
bool parseTelemetryPacket(char *packet, Telemetry &t) {
  char *fields[EXPECTED_FIELDS];
  int fieldCount = 0;

  char *saveptr = nullptr;
  char *token = strtok_r(packet, ",", &saveptr);

  while (token != nullptr && fieldCount < EXPECTED_FIELDS) {
    fields[fieldCount++] = token;
    token = strtok_r(nullptr, ",", &saveptr);
  }

  if (fieldCount != EXPECTED_FIELDS) {
    Serial.print("Parse fail: expected ");
    Serial.print(EXPECTED_FIELDS);
    Serial.print(" fields, got ");
    Serial.println(fieldCount);
    return false;
  }

  if (strcmp(fields[0], "T") != 0) {
    Serial.println("Parse fail: packet does not start with T");
    return false;
  }

  t.ms           = strtoul(fields[1],  nullptr, 10);

  t.gpsValid     = atoi(fields[2]);
  t.lat          = atol(fields[3]);
  t.lon          = atol(fields[4]);
  t.gpsAltMm     = atol(fields[5]);
  t.heading      = atol(fields[6]);
  t.fixType      = (unsigned int)strtoul(fields[7],  nullptr, 10);
  t.sats         = (unsigned int)strtoul(fields[8],  nullptr, 10);

  t.imuValid     = atoi(fields[9]);
  t.ax           = atof(fields[10]);
  t.ay           = atof(fields[11]);
  t.az           = atof(fields[12]);
  t.gx           = atof(fields[13]);
  t.gy           = atof(fields[14]);
  t.gz           = atof(fields[15]);
  t.imuTempC     = atof(fields[16]);

  t.alt1Valid    = atoi(fields[17]);
  t.alt1Pressure = atof(fields[18]);
  t.alt1TempC    = atof(fields[19]);
  t.alt1AltM     = atof(fields[20]);

  t.alt2Valid    = atoi(fields[21]);
  t.alt2Pressure = atof(fields[22]);
  t.alt2TempC    = atof(fields[23]);
  t.alt2AltM     = atof(fields[24]);

  return true;
}

// ============================================================
// PRINT PARSED TELEMETRY
// ============================================================
void printTelemetry(const Telemetry &t, int rssi, float snr) {
  Serial.print("SENDING: nosecone_data ");
  Serial.print("baro_alt=");    Serial.print(t.alt1AltM, 2);
  Serial.print(",ax=");         Serial.print(t.ax, 2);
  Serial.print(",ay=");         Serial.print(t.ay, 2);
  Serial.print(",az=");         Serial.print(t.az, 2);
  Serial.print(",gx=");         Serial.print(t.gx, 2);
  Serial.print(",gy=");         Serial.print(t.gy, 2);
  Serial.print(",gz=");         Serial.print(t.gz, 2);
  Serial.print(",lat=");        Serial.print(t.lat / 10000000.0, 4);
  Serial.print(",lon=");        Serial.print(t.lon / 10000000.0, 4);
  Serial.print(",gps_alt=");    Serial.print(t.gpsAltMm / 1000.0, 1);
  Serial.print(",gps_fix=");    Serial.print(t.fixType);
  Serial.println();

  Serial.print("RSSI: ");
  Serial.print(rssi);
  Serial.print(" SNR: ");
  Serial.println(snr);
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  spi_bus.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);

  LoRa.setSPI(spi_bus);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_D0);

  int lora_init = LoRa.begin(LORA_FREQ);
  if (!lora_init) {
    Serial.println("ERROR: LoRa init failed. Check wiring and SPI pins.");
    while (true) {
      delay(1000);
    }
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);

  pinMode(LORA_RX_EN, OUTPUT);
  pinMode(LORA_TX_EN, OUTPUT);

  // Set receiver board to RX mode
  digitalWrite(LORA_RX_EN, HIGH);
  digitalWrite(LORA_TX_EN, LOW);

  Serial.println("LoRa telemetry receiver ready!");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) {
    return;
  }

  if (packetSize >= RX_PACKET_MAX) {
    Serial.print("Packet too large: ");
    Serial.println(packetSize);

    while (LoRa.available()) {
      LoRa.read();
    }
    return;
  }

  char packet[RX_PACKET_MAX];
  int index = 0;

  while (LoRa.available() && index < RX_PACKET_MAX - 1) {
    packet[index++] = (char)LoRa.read();
  }
  packet[index] = '\0';

  int rssi = LoRa.packetRssi();
  float snr = LoRa.packetSnr();

  char parseBuffer[RX_PACKET_MAX];
  strncpy(parseBuffer, packet, RX_PACKET_MAX);
  parseBuffer[RX_PACKET_MAX - 1] = '\0';

  Telemetry t;
  if (parseTelemetryPacket(parseBuffer, t)) {
    printTelemetry(t, rssi, snr);
  } else {
    Serial.println("Telemetry parse failed.");
  }
}