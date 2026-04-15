#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

#include <math.h>

#include <SparkFun_u-blox_GNSS_v3.h>
#include <Adafruit_ISM330DHCX.h>
#include <Adafruit_Sensor.h>
#include <LoRa.h>

#include <Arduino.h>

// ============================================================
// PINS - NOSE CONE BOARD
// ============================================================

// GPS
#define GPS_SDA 17
#define GPS_SCL 16
#define GNSS_ADDRESS 0x42
#define RST_GPS 5

// SPI BUS A: RADIO
#define SCLK_A_PIN 9
#define MISO_A_PIN 10
#define MOSI_A_PIN 11

#define RADIO_CS 21
#define RST_RADIO 14
#define RADIO_DIO1 12
#define TXEN 47
#define RXEN 48

// SPI BUS B: IMU + ALTIMETERS
#define SCLK_B 35
#define MISO_B 37
#define MOSI_B 36

#define IMU_CS 41
#define IMU_INT 42

#define ALT_CS1 39
#define ALT_CS2 38



// =======================
// BUZZER INIT
// =======================
int buzzerPin = 15;

const int buzzerChannel = 0; //conforming to older style buzzer API

#define NOTE_C4 262
#define NOTE_C5 523

// ============================================================
// CONFIG
// ============================================================
#define LOOP_DELAY_MS 100
#define SEA_LEVEL_PRESSURE_HPA 1013.25f
#define RADIO_PACKET_MAX 220

// IMPORTANT: make these match receiver exactly
#define LORA_FREQ              915E6
#define LORA_SPREADING_FACTOR  8
#define LORA_SIGNAL_BANDWIDTH  250E3
#define LORA_CODING_RATE       5
#define LORA_TX_POWER          20

#define SD_CS 8  // SD card CS pin

// MS5607 commands
static const uint8_t MS5607_CMD_RESET = 0x1E;
static const uint8_t MS5607_CMD_ADC_READ = 0x00;
static const uint8_t MS5607_CMD_PROM_READ_BASE = 0xA0;
static const uint8_t MS5607_CMD_CONV_D1 = 0x48;  // OSR 4096
static const uint8_t MS5607_CMD_CONV_D2 = 0x58;  // OSR 4096

// ============================================================
// GLOBALS
// ============================================================
SPIClass spiA(FSPI);
SPIClass spiB(HSPI);

TwoWire gnssWire = TwoWire(0);
SFE_UBLOX_GNSS gnss;
Adafruit_ISM330DHCX imu;

bool gpsReady = false;
bool imuReady = false;
bool radioReady = false;
bool alt1Ready = false;
bool alt2Ready = false;

String logFilename;
bool sdReady = false;

double ALTITUDE_OFFSET = -580.0; //MOJAVE DESERT SEA LEVEL ALTITUDE OFFSET

// ============================================================
// DATA STRUCTS
// ============================================================
struct GpsData {
  bool valid;
  int32_t lat;
  int32_t lon;
  int32_t alt_mm;
  int32_t heading;
  uint8_t fixType;
  uint8_t sats;
};

struct ImuData {
  bool valid;
  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;
  float tempC;
};

struct AltData {
  bool valid;
  float pressure_mbar;
  float tempC;
  float altitude_m;
};

struct Telemetry {
  uint32_t ms;
  GpsData gps;
  ImuData imu;
  AltData alt1;
  AltData alt2;
};

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

  bool read(AltData &out) {
    out.valid = false;
    out.pressure_mbar = NAN;
    out.tempC = NAN;
    out.altitude_m = NAN;

    if (!initialized) return false;

    uint32_t D1 = convertAndRead(MS5607_CMD_CONV_D1);
    uint32_t D2 = convertAndRead(MS5607_CMD_CONV_D2);

    if (D1 == 0 || D2 == 0) {
      return false;
    }

    int32_t dT = (int32_t)D2 - ((int32_t)C[5] << 8);
    int64_t TEMP = 2000LL + ((int64_t)dT * C[6]) / 8388608LL;
    int64_t OFF = ((int64_t)C[2] << 17) + (((int64_t)C[4] * dT) >> 6);
    int64_t SENS = ((int64_t)C[1] << 16) + (((int64_t)C[3] * dT) >> 7);

    int64_t T2 = 0;
    int64_t OFF2 = 0;
    int64_t SENS2 = 0;

    if (TEMP < 2000) {
      T2 = ((int64_t)dT * dT) >> 31;
      OFF2 = 5LL * (TEMP - 2000LL) * (TEMP - 2000LL) / 2LL;
      SENS2 = 5LL * (TEMP - 2000LL) * (TEMP - 2000LL) / 4LL;

      if (TEMP < -1500) {
        OFF2 += 7LL * (TEMP + 1500LL) * (TEMP + 1500LL);
        SENS2 += 11LL * (TEMP + 1500LL) * (TEMP + 1500LL) / 2LL;
      }
    }

    TEMP -= T2;
    OFF -= OFF2;
    SENS -= SENS2;

    int64_t P = ((((int64_t)D1 * SENS) >> 21) - OFF) >> 15;

    out.tempC = TEMP / 100.0f;
    out.pressure_mbar = P / 100.0f;

    if (out.pressure_mbar > 0.1f) {
      out.altitude_m = 44330.0f * (1.0f - pow(out.pressure_mbar / SEA_LEVEL_PRESSURE_HPA, 0.19029495718f));
    } else {
      out.altitude_m = NAN;
    }

    out.valid = true;
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

MS5607 altimeter1;
MS5607 altimeter2;

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
bool initGPS() {
  gnssWire.begin(GPS_SDA, GPS_SCL);

  for (int i = 0; i < 10; i++) {
    if (gnss.begin(gnssWire, GNSS_ADDRESS)) {
      gnss.setI2COutput(COM_TYPE_UBX);
      ledcWriteTone(buzzerPin, NOTE_C5);
      delay(1000);
      Serial.println("[GPS] OK");
      ledcWriteTone(buzzerPin, 0);
      delay(2000);
      return true;
    }
    delay(300);
  }

  ledcWriteTone(buzzerPin, NOTE_C4);
  delay(1000);
  Serial.println("[GPS] FAIL");
  ledcWriteTone(buzzerPin, 0);
  delay(2000);

  return false;
}

bool initIMU() {
  pinMode(IMU_CS, OUTPUT);
  digitalWrite(IMU_CS, HIGH);

  if (!imu.begin_SPI(IMU_CS, &spiB)) {
    ledcWriteTone(buzzerPin, NOTE_C4);
    delay(1000);
    Serial.println("[IMU] FAIL");
    ledcWriteTone(buzzerPin, 0);
    delay(2000);
    return false;
  }

  imu.setAccelRange(LSM6DS_ACCEL_RANGE_16_G);
  imu.setGyroRange(LSM6DS_GYRO_RANGE_2000_DPS);
  imu.setAccelDataRate(LSM6DS_RATE_104_HZ);
  imu.setGyroDataRate(LSM6DS_RATE_104_HZ);

  ledcWriteTone(buzzerPin, NOTE_C5);
  delay(1000);
  ledcWriteTone(buzzerPin, NOTE_C5);
  Serial.println("[IMU] OK");
  delay(1000);
  ledcWriteTone(buzzerPin, 0);

  return true;
}

bool initAltimeters() {
  alt1Ready = altimeter1.begin(&spiB, ALT_CS1);
  alt2Ready = altimeter2.begin(&spiB, ALT_CS2);

  Serial.print("[ALT1] ");
  Serial.println(alt1Ready ? "OK" : "FAIL");
  Serial.print("[ALT2] ");
  Serial.println(alt2Ready ? "OK" : "FAIL");

  return alt1Ready || alt2Ready;
}

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
// READ FUNCTIONS
// ============================================================
void readGPS(GpsData &g) {
  g.valid = false;
  g.lat = 0;
  g.lon = 0;
  g.alt_mm = 0;
  g.heading = 0;
  g.fixType = 0;
  g.sats = 0;

  if (!gpsReady) return;
  if (!gnss.getPVT()) return;

  g.lat = gnss.getLatitude();
  g.lon = gnss.getLongitude();
  g.alt_mm = gnss.getAltitudeMSL();
  g.heading = gnss.getHeading();
  g.fixType = gnss.getFixType();
  g.sats = gnss.getSIV();
  g.valid = true;
}

void readIMU(ImuData &i) {
  i.valid = false;
  i.ax = i.ay = i.az = NAN;
  i.gx = i.gy = i.gz = NAN;
  i.tempC = NAN;

  if (!imuReady) return;

  sensors_event_t accel, gyro, temp;
  imu.getEvent(&accel, &gyro, &temp);

  i.ax = accel.acceleration.x;
  i.ay = accel.acceleration.y;
  i.az = accel.acceleration.z;

  i.gx = gyro.gyro.x;
  i.gy = gyro.gyro.y;
  i.gz = gyro.gyro.z;

  i.tempC = temp.temperature;
  i.valid = true;
}

void readAltimeters(AltData &a1, AltData &a2) {
  a1.valid = false;
  a2.valid = false;

  if (alt1Ready) altimeter1.read(a1);
  if (alt2Ready) altimeter2.read(a2);
}

Telemetry readTelemetry() {
  Telemetry t;
  t.ms = millis();

  readGPS(t.gps);
  readIMU(t.imu);
  readAltimeters(t.alt1, t.alt2);

  return t;
}

// ============================================================
// DEBUG PRINT
// ============================================================
void printTelemetry(const Telemetry &t) {
  Serial.print("T=");
  Serial.print(t.ms);

  Serial.print(" GPS[");
  Serial.print(t.gps.valid);
  Serial.print("] lat=");
  Serial.print(t.gps.lat);
  Serial.print(" lon=");
  Serial.print(t.gps.lon);
  Serial.print(" alt=");
  Serial.print(t.gps.alt_mm);

  Serial.print(" IMU[");
  Serial.print(t.imu.valid);
  Serial.print("] ax=");
  Serial.print(t.imu.ax, 3);
  Serial.print(" ay=");
  Serial.print(t.imu.ay, 3);
  Serial.print(" az=");
  Serial.print(t.imu.az, 3);

  Serial.print(" ALT1[");
  Serial.print(t.alt1.valid);
  Serial.print("] ");
  Serial.print(t.alt1.altitude_m + ALTITUDE_OFFSET, 2);

  Serial.print(" ALT2[");
  Serial.print(t.alt2.valid);
  Serial.print("] ");
  Serial.println(t.alt2.altitude_m + ALTITUDE_OFFSET, 2); //debug check
}

// ============================================================
// PACKET BUILD / TX
// ============================================================
void sendTelemetry(const Telemetry &t) {
  if (!radioReady) return;

  char packet[RADIO_PACKET_MAX];

  int len = snprintf(
    packet, sizeof(packet),
    "T,%lu,%d,%ld,%ld,%ld,%ld,%u,%u,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%d,%.2f,%.2f,%.2f,%d,%.2f,%.2f,%.2f",
    (unsigned long)t.ms,
    t.gps.valid,
    (long)t.gps.lat,
    (long)t.gps.lon,
    (long)t.gps.alt_mm,
    (long)t.gps.heading,
    (unsigned)t.gps.fixType,
    (unsigned)t.gps.sats,
    t.imu.valid,
    t.imu.ax, t.imu.ay, t.imu.az,
    t.imu.gx, t.imu.gy, t.imu.gz,
    t.imu.tempC,
    t.alt1.valid,
    t.alt1.pressure_mbar,
    t.alt1.tempC,
    t.alt1.altitude_m + ALTITUDE_OFFSET,
    t.alt2.valid,
    t.alt2.pressure_mbar,
    t.alt2.tempC,
    t.alt2.altitude_m + ALTITUDE_OFFSET
  );

  if (len <= 0) {
    Serial.println("[TX FAIL] snprintf error");
    return;
  }

  if (len >= (int)sizeof(packet)) {
    Serial.println("[TX FAIL] packet truncated");
    return;
  }

  setTX();

  LoRa.beginPacket();
  LoRa.write((const uint8_t*)packet, len);

  int state = LoRa.endPacket(false);

  setRX();

  if (state == 1) {
    Serial.print("[TX ");
    Serial.print(len);
    Serial.print("B] ");
    Serial.println(packet);
  } else {
    Serial.println("[TX FAIL]");
  }
}

// ============================================================
// SD CARD LOGGING
// ============================================================
bool initSD() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(10);

  if (!SD.begin(SD_CS, spiA, 4000000)) {
    Serial.println("[SD] FAIL");
    return false;
  }

  // Find a unique log file name each boot
  int n = 0;
  do {
    logFilename = "/log" + String(n++) + ".csv";
  } while (SD.exists(logFilename.c_str()) && n < 1000);

  // Write header matching packet order
  File f = SD.open(logFilename.c_str(), FILE_WRITE);
  if (!f) {
    Serial.println("[SD] Failed to create log file");
    return false;
  }

  f.println("type,ms,gps_valid,lat,lon,gps_alt_mm,heading,fix_type,sats,"
            "imu_valid,ax,ay,az,gx,gy,gz,imu_temp_c,"
            "alt1_valid,alt1_pressure_mbar,alt1_temp_c,alt1_alt_m,"
            "alt2_valid,alt2_pressure_mbar,alt2_temp_c,alt2_alt_m");
  f.close();

  Serial.print("[SD] OK, logging to ");
  Serial.println(logFilename);
  return true;
}

void logTelemetryToSD(const Telemetry &t) {
  if (!sdReady) return;

  File f = SD.open(logFilename.c_str(), FILE_APPEND);
  if (!f) {
    Serial.println("[SD] Failed to open log file for append");
    return;
  }

  Serial.println("Writing");

  char row[256];
  int len = snprintf(
    row, sizeof(row),
    "T,%lu,%d,%ld,%ld,%ld,%ld,%u,%u,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%d,%.2f,%.2f,%.2f,%d,%.2f,%.2f,%.2f",
    (unsigned long)t.ms,
    t.gps.valid,
    (long)t.gps.lat,
    (long)t.gps.lon,
    (long)t.gps.alt_mm,
    (long)t.gps.heading,
    (unsigned)t.gps.fixType,
    (unsigned)t.gps.sats,
    t.imu.valid,
    t.imu.ax, t.imu.ay, t.imu.az,
    t.imu.gx, t.imu.gy, t.imu.gz,
    t.imu.tempC,
    t.alt1.valid,
    t.alt1.pressure_mbar,
    t.alt1.tempC,
    t.alt1.altitude_m + ALTITUDE_OFFSET,
    t.alt2.valid,
    t.alt2.pressure_mbar,
    t.alt2.tempC,
    t.alt2.altitude_m + ALTITUDE_OFFSET
  );

  if (len <= 0) {
    Serial.println("[SD] snprintf error");
    f.close();
    return;
  }

  if (len >= (int)sizeof(row)) {
    Serial.println("[SD] row truncated");
    f.close();
    return;
  }

  f.println(row);
  f.close();

  Serial.println("After writing");

}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("BOOT 1");

  pinMode(RST_GPS, OUTPUT);
  digitalWrite(RST_GPS, HIGH);

  spiA.begin(SCLK_A_PIN, MISO_A_PIN, MOSI_A_PIN, RADIO_CS);
  Serial.println("BOOT 2");

  spiB.begin(SCLK_B, MISO_B, MOSI_B, -1);
  Serial.println("BOOT 3");

  Serial.println("BEFORE GPS");
  gpsReady = initGPS();
  Serial.println("AFTER GPS");

  Serial.println("BEFORE IMU");
  imuReady = initIMU();
  Serial.println("AFTER IMU");

  Serial.println("BEFORE ALT");
  initAltimeters();
  Serial.println("AFTER ALT");

  Serial.println("BEFORE RADIO");
  radioReady = initRadio();
  Serial.println("AFTER RADIO");

  Serial.println("BEFORE SD");
  sdReady = initSD();
  Serial.println("AFTER SD");

  ledcSetup(buzzerChannel, 2000, 8); //conforming to older style version > ledcAttach (<---- that's newer style api for buzzer)
  ledcAttachPin(buzzerPin, buzzerChannel);
  //ledcAttach(buzzerPin, 2000, 8);


  if (!gpsReady || !imuReady || !alt1Ready || !alt2Ready || !radioReady || !sdReady) {
    Serial.println("[INIT] One or more subsystems failed -- buzzer ON");
    ledcWriteTone(buzzerChannel, NOTE_C4); //replaced buzzerPin with buzzerChannel
    delay(5000);
    ledcWriteTone(buzzerChannel,0); //replaced buzzerPin with buzzerChannel
    
  }
}

void loop() {
  Telemetry t = readTelemetry();
  printTelemetry(t);
  sendTelemetry(t);
  logTelemetryToSD(t);
  delay(LOOP_DELAY_MS);
}