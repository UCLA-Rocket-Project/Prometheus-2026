#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <string.h>

// ===== YAGI PINS =====
#define LORA_MISO 38
#define LORA_MOSI 36
#define LORA_SCK 37
#define LORA_CS 13
#define LORA_RST 35
#define LORA_TX_EN 5
#define LORA_RX_EN 6
// =====================

// ===== DATA STRUCTS (MUST MATCH TRANSMITTER EXACTLY) =====
struct GpsData
{
    int32_t latitude;
    int32_t longitude;
    int32_t altitude;
    int32_t heading;
};

struct ICMData
{
    float accelX;
    float accelY;
    float accelZ;

    float gyroX;
    float gyroY;
    float gyroZ;

    float magX;
    float magY;
    float magZ;

    float icmTemp;
};

struct BMPData
{
    double bmpTemp;
    double pressure;
    double altitude;
};
// ========================================================

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println("LoRa Receiver Starting...");

    pinMode(LORA_TX_EN, OUTPUT);
    pinMode(LORA_RX_EN, OUTPUT);

    digitalWrite(LORA_TX_EN, LOW);
    digitalWrite(LORA_RX_EN, HIGH);

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setSPI(SPI);
    LoRa.setPins(LORA_CS, LORA_RST, -1);

    if (!LoRa.begin(915E6))
    {
        Serial.println("LoRa.begin FAILED");
        while (1)
            delay(1000);
    }

    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(250E3);
    LoRa.disableCrc();

    Serial.println("LoRa Receiver Ready");
}

void loop()
{
    int packetSize = LoRa.parsePacket();

    if (!packetSize)
    {
        delay(10);
        return;
    }

    uint8_t buf[128];
    int i = 0;

    while (LoRa.available() && i < (int)sizeof(buf))
    {
        buf[i++] = (uint8_t)LoRa.read();
    }

    if (i != 88)
    {
        Serial.print("Wrong packet size: ");
        Serial.println(i);
        return;
    }

    // check markers "A " and " Z"
    if (buf[0] != 'A' || buf[1] != ' ' || buf[86] != ' ' || buf[87] != 'Z')
    {
        Serial.println("Bad packet markers");
        return;
    }

    GpsData gps;
    ICMData icm;
    BMPData bmp;
    unsigned long timestamp;

    int index = 2;

    memcpy(&gps, &buf[index], sizeof(GpsData));
    index += sizeof(GpsData);

    memcpy(&icm, &buf[index], sizeof(ICMData));
    index += sizeof(ICMData);

    memcpy(&bmp, &buf[index], sizeof(BMPData));
    index += sizeof(BMPData);

    memcpy(&timestamp, &buf[index], sizeof(unsigned long));

    Serial.println();
    Serial.println("===== DECODED PACKET =====");

    Serial.print("RSSI: ");
    Serial.println(LoRa.packetRssi());

    Serial.print("SNR: ");
    Serial.println(LoRa.packetSnr());

    Serial.print("Timestamp: ");
    Serial.println(timestamp);

    Serial.println("--- GPS ---");
    Serial.print("Lat: ");
    Serial.println(gps.latitude);
    Serial.print("Lon: ");
    Serial.println(gps.longitude);
    Serial.print("Alt (mm): ");
    Serial.println(gps.altitude);
    Serial.print("Heading: ");
    Serial.println(gps.heading);

    Serial.println("--- ICM ---");
    Serial.print("Accel: ");
    Serial.print(icm.accelX);
    Serial.print(", ");
    Serial.print(icm.accelY);
    Serial.print(", ");
    Serial.println(icm.accelZ);

    Serial.print("Gyro: ");
    Serial.print(icm.gyroX);
    Serial.print(", ");
    Serial.print(icm.gyroY);
    Serial.print(", ");
    Serial.println(icm.gyroZ);

    Serial.print("Mag: ");
    Serial.print(icm.magX);
    Serial.print(", ");
    Serial.print(icm.magY);
    Serial.print(", ");
    Serial.println(icm.magZ);

    Serial.print("Temp: ");
    Serial.println(icm.icmTemp);

    Serial.println("--- BMP ---");
    Serial.print("Temp: ");
    Serial.println(bmp.bmpTemp);

    Serial.print("Pressure: ");
    Serial.println(bmp.pressure);

    Serial.print("Altitude: ");
    Serial.println(bmp.altitude);

    Serial.println("==========================");
}