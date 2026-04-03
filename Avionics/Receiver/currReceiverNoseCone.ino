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

SPIClass spi_bus(VSPI);

// ============================================================
// TELEMETRY STRUCT
// Matches simplified transmitter packet:
// T,<ms>,<gValid>,<lat>,<lon>,<gAlt>,<heading>,<fix>,<sats>,
//   <iValid>,<ax>,<ay>,<az>,<gx>,<gy>,<gz>,<temp>,
//   <a1Valid>,<a1Pressure>,<a1Temp>,<a1Alt>,
//   <a2Valid>,<a2Pressure>,<a2Temp>,<a2Alt>
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
  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;
  float imuTempC;

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
void printTelemetry(const Telemetry &t) {
  Serial.println("----- Parsed Telemetry -----");

  Serial.print("ms: ");
  Serial.println(t.ms);

  Serial.print("GPS valid: ");
  Serial.println(t.gpsValid);
  Serial.print("lat: ");
  Serial.println(t.lat);
  Serial.print("lon: ");
  Serial.println(t.lon);
  Serial.print("gpsAltMm: ");
  Serial.println(t.gpsAltMm);
  Serial.print("heading: ");
  Serial.println(t.heading);
  Serial.print("fixType: ");
  Serial.println(t.fixType);
  Serial.print("sats: ");
  Serial.println(t.sats);

  Serial.print("IMU valid: ");
  Serial.println(t.imuValid);
  Serial.print("ax: ");
  Serial.println(t.ax, 3);
  Serial.print("ay: ");
  Serial.println(t.ay, 3);
  Serial.print("az: ");
  Serial.println(t.az, 3);
  Serial.print("gx: ");
  Serial.println(t.gx, 3);
  Serial.print("gy: ");
  Serial.println(t.gy, 3);
  Serial.print("gz: ");
  Serial.println(t.gz, 3);
  Serial.print("imuTempC: ");
  Serial.println(t.imuTempC, 2);

  Serial.print("ALT1 valid: ");
  Serial.println(t.alt1Valid);
  Serial.print("alt1Pressure: ");
  Serial.println(t.alt1Pressure, 2);
  Serial.print("alt1TempC: ");
  Serial.println(t.alt1TempC, 2);
  Serial.print("alt1AltM: ");
  Serial.println(t.alt1AltM, 2);

  Serial.print("ALT2 valid: ");
  Serial.println(t.alt2Valid);
  Serial.print("alt2Pressure: ");
  Serial.println(t.alt2Pressure, 2);
  Serial.print("alt2TempC: ");
  Serial.println(t.alt2TempC, 2);
  Serial.print("alt2AltM: ");
  Serial.println(t.alt2AltM, 2);

  Serial.println("----------------------------");
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

  Serial.println();
  Serial.println("========== PACKET RECEIVED ==========");
  Serial.print("Raw: ");
  Serial.println(packet);
  Serial.print("Length: ");
  Serial.println(index);
  Serial.print("RSSI: ");
  Serial.println(LoRa.packetRssi());
  Serial.print("SNR: ");
  Serial.println(LoRa.packetSnr());

  // Make a copy because strtok modifies the buffer
  char parseBuffer[RX_PACKET_MAX];
  strncpy(parseBuffer, packet, RX_PACKET_MAX);
  parseBuffer[RX_PACKET_MAX - 1] = '\0';

  Telemetry t;
  if (parseTelemetryPacket(parseBuffer, t)) {
    printTelemetry(t);
  } else {
    Serial.println("Telemetry parse failed.");
  }
}
