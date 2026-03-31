#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <math.h>

#include "pins.h"

#include <SparkFun_u-blox_GNSS_v3.h>
#include <Adafruit_ISM330DHCX.h>
#include <Adafruit_Sensor.h>
#include <RadioLib.h>

// ============================================================
// SPI BUS A (SD + RADIO)
// ============================================================
#define SPIA_SCK SCLK_A_PIN
#define SPIA_MISO MISO_A_PIN
#define SPIA_MOSI MOSI_A_PIN

// SPI BUS B (SENSORS)
#define SPIB_SCK SCLK_B
#define SPIB_MISO MISO_B
#define SPIB_MOSI MOSI_B

// ============================================================
// CONFIG
// ============================================================
#define LOOP_DELAY_MS 100
#define SEA_LEVEL_PRESSURE_HPA 1013.25

#define RADIO_PACKET_MAX 128

// ============================================================
// GLOBALS
// ============================================================
SPIClass spiA(FSPI);
SPIClass spiB(HSPI);

TwoWire gnssWire = TwoWire(0);
SFE_UBLOX_GNSS gnss;

Adafruit_ISM330DHCX imu;

// SX1262
SX1262 radio = new Module(RADIO_CS, RADIO_DIO1, RST_RADIO, RADIO_BUSY, spiA);

// ============================================================
// DATA STRUCTS
// ============================================================
struct Telemetry
{
    uint32_t ms;
    int32_t lat;
    int32_t lon;
    int32_t alt;
    float ax, ay, az;
    float alt1;
    float alt2;
};

// ============================================================
// ALTIMETER (MINIMAL DRIVER)
// ============================================================
class MS5607
{
public:
    void begin(SPIClass *s, int cs)
    {
        spi = s;
        csPin = cs;
        pinMode(csPin, OUTPUT);
        digitalWrite(csPin, HIGH);
    }

    float readAltitude()
    {
        // Minimal safe dummy read for testing stability
        return random(100, 120); // placeholder altitude
    }

private:
    SPIClass *spi;
    int csPin;
};

MS5607 alt1, alt2;

// ============================================================
// FLAGS
// ============================================================
bool gpsReady = false;
bool imuReady = false;
bool radioReady = false;

// ============================================================
// RADIO CONTROL
// ============================================================
void setTX()
{
    digitalWrite(RXEN, LOW);
    digitalWrite(TXEN, HIGH);
}

void setRX()
{
    digitalWrite(TXEN, LOW);
    digitalWrite(RXEN, HIGH);
}

// ============================================================
// INIT
// ============================================================
bool initGPS()
{
    gnssWire.begin(GPS_SDA, GPS_SCL);

    if (!gnss.begin(gnssWire, GNSS_ADDRESS))
    {
        Serial.println("GPS FAIL");
        return false;
    }

    gnss.setI2COutput(COM_TYPE_UBX);
    return true;
}

bool initIMU()
{
    if (!imu.begin_SPI(IMU_CS, &spiB))
    {
        Serial.println("IMU FAIL");
        return false;
    }
    return true;
}

bool initRadio()
{
    pinMode(TXEN, OUTPUT);
    pinMode(RXEN, OUTPUT);
    pinMode(RST_RADIO, OUTPUT);

    // proper reset
    digitalWrite(RST_RADIO, LOW);
    delay(10);
    digitalWrite(RST_RADIO, HIGH);
    delay(10);

    setRX();

    int state = radio.begin(915.0, 250.0, 7, 5, 0x12, 22, 8);

    if (state != RADIOLIB_ERR_NONE)
    {
        Serial.print("RADIO FAIL: ");
        Serial.println(state);
        return false;
    }

    radio.setCRC(true);

    Serial.println("RADIO OK");
    return true;
}

// ============================================================
// READ
// ============================================================
Telemetry readData()
{
    Telemetry t = {0};
    t.ms = millis();

    if (gpsReady && gnss.getPVT())
    {
        t.lat = gnss.getLatitude();
        t.lon = gnss.getLongitude();
        t.alt = gnss.getAltitudeMSL();
    }

    if (imuReady)
    {
        sensors_event_t a, g, temp;
        imu.getEvent(&a, &g, &temp);

        t.ax = a.acceleration.x;
        t.ay = a.acceleration.y;
        t.az = a.acceleration.z;
    }

    t.alt1 = alt1.readAltitude();
    t.alt2 = alt2.readAltitude();

    return t;
}

// ============================================================
// RADIO SEND
// ============================================================
void sendTelemetry(Telemetry &t)
{
    if (!radioReady)
        return;

    char packet[RADIO_PACKET_MAX];

    snprintf(packet, sizeof(packet),
             "A,ms=%lu,lat=%ld,lon=%ld,a1=%.2f,a2=%.2f,Z",
             (unsigned long)t.ms,
             (long)t.lat,
             (long)t.lon,
             t.alt1,
             t.alt2);

    setTX();
    int state = radio.transmit(packet);
    setRX();

    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(packet);
    }
    else
    {
        Serial.print("TX FAIL: ");
        Serial.println(state);
    }
}

// == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==
//     SETUP == == == == == == == == == == == == == == == == == == == == == == == == == == == == == ==
void setup()
{
    Serial.begin(115200);
    delay(1000);

    // INIT SPI FIRST
    spiA.begin(SPIA_SCK, SPIA_MISO, SPIA_MOSI, -1);
    spiB.begin(SPIB_SCK, SPIB_MISO, SPIB_MOSI, -1);

    gpsReady = initGPS();
    imuReady = initIMU();
    radioReady = initRadio();

    alt1.begin(&spiB, ALT_CS1);
    alt2.begin(&spiB, ALT_CS2);

    Serial.println("SYSTEM READY 🚀");
}

// ============================================================
// LOOP
// ============================================================
void loop()
{
    Telemetry t = readData();

    sendTelemetry(t);

    delay(LOOP_DELAY_MS);
}