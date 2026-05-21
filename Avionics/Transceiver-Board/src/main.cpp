#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <SD.h>
#include <FS.h>

#include <math.h>

#include <SparkFun_u-blox_GNSS_v3.h>
#include <Adafruit_ISM330DHCX.h>
#include <Adafruit_Sensor.h>
// ============================================================
// PINS
// ============================================================
#define GPS_RXD 18
#define GPS_TXD 8

#define SCLK_A_PIN 48
#define MISO_A_PIN 35
#define MOSI_A_PIN 36
#define RADIO_CS 37
#define RST_RADIO 42
#define RADIO_DIO1 41
#define TXEN 2
#define RXEN 1

// SPI BUS B: IMU + ALTIMETERS
#define SCLK_B 14
#define MISO_B 12
#define MOSI_B 13

#define IMU_CS 21
#define IMU_INT 38

#define ALT_CS1 39

// ============================================================
// CONFIG
// ============================================================
#define SEA_LEVEL_PRESSURE_HPA 1013.25f
#define LOOP_DELAY_MS 100

#define LORA_FREQ 910E6
#define LORA_SPREADING_FACTOR 8
#define LORA_SIGNAL_BANDWIDTH 250E3
#define LORA_CODING_RATE 5
#define LORA_TX_POWER 20

// MS5607 commands
static const uint8_t MS5607_CMD_RESET = 0x1E;
static const uint8_t MS5607_CMD_ADC_READ = 0x00;
static const uint8_t MS5607_CMD_PROM_READ_BASE = 0xA0;
static const uint8_t MS5607_CMD_CONV_D1 = 0x48; // OSR 4096
static const uint8_t MS5607_CMD_CONV_D2 = 0x58; // OSR 4096

// ============================================================
// GLOBALS
// ============================================================

TwoWire gnssWire = TwoWire(0);
SFE_UBLOX_GNSS gnss;
Adafruit_ISM330DHCX imu;

bool imuReady = false;
bool radioReady = false;
bool alt1Ready = false;
// int ALT_ELEVATION = 0;
String logFilename;
bool sdReady = false;

double ALTITUDE_OFFSET = 48.0; // MOJAVE DESERT SEA LEVEL ALTITUDE OFFSET

// DATA STRUCTS

struct GpsData
{
  bool valid;
  int32_t lat;     // degrees * 1e7
  int32_t lon;     // degrees * 1e7
  int32_t alt_mm;  // mm
  int32_t heading; // degrees * 1e5
  uint8_t fixType;
  uint8_t sats;
  float hdop;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};
struct ImuData
{
  bool valid;
  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;
  float tempC;
};

struct AltData
{
  bool valid;
  float pressure_mbar;
  float tempC;
  float altitude_m;
};

struct Telemetry
{
  uint32_t ms;
  GpsData gps;
  ImuData imu;
  AltData alt1;
};

#define RADIO_PACKET_MAX 200

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
SPIClass spiA(FSPI);
SPIClass spiB(HSPI);

// ============================================================
// MS5607 DRIVER
// ============================================================
class MS5607
{
public:
  MS5607() : spi(nullptr), cs(-1), initialized(false)
  {
    for (int i = 0; i < 8; i++)
      C[i] = 0;
  }

  bool begin(SPIClass *bus, int csPin)
  {
    spi = bus;
    cs = csPin;

    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);

    reset();
    delay(10);

    for (int i = 0; i < 8; i++)
    {
      C[i] = readPROM(i);
    }

    if (C[1] == 0 || C[1] == 0xFFFF || C[2] == 0 || C[2] == 0xFFFF)
    {
      initialized = false;
      return false;
    }

    initialized = true;
    return true;
  }

  bool read(AltData &out)
  {
    out.valid = false;
    out.pressure_mbar = NAN;
    out.tempC = NAN;
    out.altitude_m = NAN;

    if (!initialized)
      return false;

    uint32_t D1 = convertAndRead(MS5607_CMD_CONV_D1);
    uint32_t D2 = convertAndRead(MS5607_CMD_CONV_D2);

    if (D1 == 0 || D2 == 0)
    {
      return false;
    }

    int32_t dT = (int32_t)D2 - ((int32_t)C[5] << 8);
    int64_t TEMP = 2000LL + ((int64_t)dT * C[6]) / 8388608LL;
    int64_t OFF = ((int64_t)C[2] << 17) + (((int64_t)C[4] * dT) >> 6);
    int64_t SENS = ((int64_t)C[1] << 16) + (((int64_t)C[3] * dT) >> 7);

    int64_t T2 = 0;
    int64_t OFF2 = 0;
    int64_t SENS2 = 0;

    if (TEMP < 2000)
    {
      T2 = ((int64_t)dT * dT) >> 31;
      OFF2 = 5LL * (TEMP - 2000LL) * (TEMP - 2000LL) / 2LL;
      SENS2 = 5LL * (TEMP - 2000LL) * (TEMP - 2000LL) / 4LL;

      if (TEMP < -1500)
      {
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

    if (out.pressure_mbar > 0.1f)
    {
      out.altitude_m = 44330.0f * (1.0f - pow(out.pressure_mbar / SEA_LEVEL_PRESSURE_HPA, 0.19029495718f));
    }
    else
    {
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

  void beginTx()
  {
    spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(cs, LOW);
  }

  void endTx()
  {
    digitalWrite(cs, HIGH);
    spi->endTransaction();
  }

  void reset()
  {
    beginTx();
    spi->transfer(MS5607_CMD_RESET);
    endTx();
  }

  uint16_t readPROM(uint8_t index)
  {
    uint8_t cmd = MS5607_CMD_PROM_READ_BASE + (index * 2);
    beginTx();
    spi->transfer(cmd);
    uint8_t msb = spi->transfer(0x00);
    uint8_t lsb = spi->transfer(0x00);
    endTx();
    return ((uint16_t)msb << 8) | lsb;
  }

  uint32_t convertAndRead(uint8_t cmd)
  {
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

// INIT

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

  // spiA.begin(SCLK_A_PIN, MISO_A_PIN, MOSI_A_PIN, RADIO_CS);
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

// IMU

bool initIMU()
{
  pinMode(IMU_CS, OUTPUT);
  digitalWrite(IMU_CS, HIGH);

  if (!imu.begin_SPI(IMU_CS, &spiB))
  {
    Serial.println("[IMU] FAIL");
    delay(200);
    delay(2000);
    return false;
  }

  imu.setAccelRange(LSM6DS_ACCEL_RANGE_16_G);
  imu.setGyroRange(LSM6DS_GYRO_RANGE_2000_DPS);
  imu.setAccelDataRate(LSM6DS_RATE_104_HZ);
  imu.setGyroDataRate(LSM6DS_RATE_104_HZ);

  Serial.println("[IMU] OK");
  return true;
}

// altimeter

bool initAltimeters()
{
  alt1Ready = altimeter1.begin(&spiB, ALT_CS1);

  Serial.print("[ALT1] ");
  Serial.println(alt1Ready ? "OK" : "FAIL");

  return alt1Ready;
}
// ============================================================
// GPS
// ============================================================
void initGPS()
{
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RXD, GPS_TXD);
}

// void readGPS()
// {
//   if (!gps.location.isValid())
//   {
//     Serial.println("[TX] No GPS fix");
//     return;
//   }

//   char packet[128];
//   int len = snprintf(packet, sizeof(packet),
//                      "G,%.6f,%.6f,%.1f,%u,%u,%02d:%02d:%02d",
//                      gps.location.lat(),
//                      gps.location.lng(),
//                      gps.altitude.meters(),
//                      gps.satellites.value(),
//                      (unsigned)gps.hdop.hdop(),
//                      gps.time.hour(),
//                      gps.time.minute(),
//                      gps.time.second());

//   setTX();
//   LoRa.beginPacket();
//   LoRa.write((const uint8_t *)packet, len);
//   LoRa.endPacket(false);
//   setRX();

//   Serial.print("[TX] ");
//   Serial.println(packet);
// }

void readGPS(GpsData &g)
{
  while (gpsSerial.available())
    gps.encode(gpsSerial.read());

  g.valid = gps.location.isValid();
  g.lat = (int32_t)(gps.location.lat() * 1e7);
  g.lon = (int32_t)(gps.location.lng() * 1e7);
  g.alt_mm = (int32_t)(gps.altitude.meters() * 1000.0f);
  g.heading = (int32_t)(gps.course.deg() * 1e5);
  g.fixType = gps.location.isValid() ? 3 : 0;
  g.sats = gps.satellites.value();
  g.hdop = gps.hdop.hdop();
  g.hour = gps.time.hour();
  g.minute = gps.time.minute();
  g.second = gps.time.second();
}

void readIMU(ImuData &i)
{
  i.valid = false;
  i.ax = i.ay = i.az = NAN;
  i.gx = i.gy = i.gz = NAN;
  i.tempC = NAN;

  if (!imuReady)
    return;

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

void readAltimeters(AltData &a1)
{
  a1.valid = false;

  if (alt1Ready)
    altimeter1.read(a1);
}
Telemetry readTelemetry()
{
  Telemetry t;
  t.ms = millis();

  readGPS(t.gps);
  readIMU(t.imu);
  readAltimeters(t.alt1);

  return t;
}
// ============================================================
// DEBUG PRINT
// ============================================================
void printTelemetry(const Telemetry &t)
{
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
  Serial.println(t.alt1.altitude_m + ALTITUDE_OFFSET, 2);
}

// ============================================================
// PACKET BUILD / TX
// ============================================================
void sendTelemetry(const Telemetry &t)
{
  if (!radioReady)
    return;

  char packet[RADIO_PACKET_MAX];

  int len = snprintf(
      packet, sizeof(packet),
      "T,%lu,%d,%ld,%ld,%ld,%ld,%u,%u,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%d,%.2f,%.2f,%.2f",
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
      t.alt1.altitude_m + ALTITUDE_OFFSET);

  if (len <= 0)
  {
    Serial.println("[TX FAIL] snprintf error");
    return;
  }

  if (len >= (int)sizeof(packet))
  {
    Serial.println("[TX FAIL] packet truncated");
    return;
  }

  setTX();
  LoRa.beginPacket();
  LoRa.write((const uint8_t *)packet, len);
  int state = LoRa.endPacket(false);
  setRX();

  if (state == 1)
  {
    Serial.print("[TX ");
    Serial.print(len);
    Serial.print("B] ");
    Serial.println(packet);
  }
  else
  {
    Serial.println("[TX FAIL]");
  }
}
// ============================================================
// SETUP / LOOP
// ============================================================
// void setup()
// {
//   Serial.begin(115200);
//   delay(1000);

//   initGPS();
//   Serial.println("[GPS] Ready");

//   if (!initRadio())
//   {
//     Serial.println("Halting.");
//     while (1)
//       ;
//   }
// }

// void loop()
// {
//   while (gpsSerial.available())
//   {
//     gps.encode(gpsSerial.read());
//   }

//   if (gps.location.isUpdated())
//   {
//     Serial.println("=============================");
//     Serial.print("LAT:  ");
//     Serial.println(gps.location.lat(), 6);
//     Serial.print("LNG:  ");
//     Serial.println(gps.location.lng(), 6);
//     Serial.print("ALT:  ");
//     Serial.print(gps.altitude.meters());
//     Serial.println(" m");
//     Serial.print("SATS: ");
//     Serial.println(gps.satellites.value());
//     Serial.print("HDOP: ");
//     Serial.println(gps.hdop.hdop());
//     Serial.print("TIME: ");
//     Serial.print(gps.time.hour());
//     Serial.print(":");
//     Serial.print(gps.time.minute());
//     Serial.print(":");
//     Serial.println(gps.time.second());

//     readGPS();

//     Serial.println("[TX] Packet sent");
//     Serial.println("=============================");
//   }
// }

// void setup()
// {
//   Serial.begin(115200);
//   delay(1000);

//   initGPS();
//   Serial.println("[GPS] Ready");

//   imuReady = initIMU();
//   alt1Ready = initAltimeters();
//   radioReady = initRadio();

//   if (!radioReady)
//   {
//     Serial.println("[HALT] Radio failed");
//     while (1)
//       ;
//   }

//   Serial.println("[SYS] All init done");
// }
void setup()
{
  Serial.begin(115200);
  delay(1000);

  spiA.begin(SCLK_A_PIN, MISO_A_PIN, MOSI_A_PIN, RADIO_CS);
  spiB.begin(SCLK_B, MISO_B, MOSI_B, IMU_CS);

  initGPS();
  Serial.println("[GPS] Ready");

  imuReady = initIMU();
  alt1Ready = initAltimeters();
  radioReady = initRadio();

  if (!radioReady)
    Serial.println("[WARN] Radio not ready, continuing anyway");

  if (!imuReady)
    Serial.println("[WARN] IMU not ready, continuing anyway");

  if (!alt1Ready)
    Serial.println("[WARN] ALT1 not ready, continuing anyway");

  Serial.println("[SYS] Init done");
  Serial.println("=============================");
  Serial.print("[GPS]  ");
  Serial.println("OK");
  Serial.print("[LORA] ");
  Serial.println(radioReady ? "OK" : "FAIL");
  Serial.print("[IMU]  ");
  Serial.println(imuReady ? "OK" : "FAIL");
  Serial.print("[ALT1] ");
  Serial.println(alt1Ready ? "OK" : "FAIL");
  Serial.println("=============================");
}
void loop()
{
  Telemetry t = readTelemetry();

  Serial.println("-----------------------------");
  printTelemetry(t);
  sendTelemetry(t);
  Serial.println("-----------------------------");

  delay(500);
}