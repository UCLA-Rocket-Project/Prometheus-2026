// #include <Arduino.h>
// #include <SPI.h>
// #include <LoRa.h>
// #include <string.h>

// // ===== YAGI PINS =====
// #define LORA_MISO 38
// #define LORA_MOSI 36
// #define LORA_SCK 37
// #define LORA_CS 13
// #define LORA_RST 35
// #define LORA_TX_EN 5
// #define LORA_RX_EN 6
// // =====================

// // ===== DATA STRUCTS (MUST MATCH TRANSMITTER EXACTLY) =====
// // struct GpsData
// // {
// //     int32_t latitude;
// //     int32_t longitude;
// //     int32_t altitude;
// //     int32_t heading;
// // };

// // struct ICMData
// // {
// //     float accelX;
// //     float accelY;
// //     float accelZ;

// //     float gyroX;
// //     float gyroY;
// //     float gyroZ;

// //     float magX;
// //     float magY;
// //     float magZ;

// //     float icmTemp;
// // };

// // struct BMPData
// // {
// //     double bmpTemp;
// //     double pressure;
// //     double altitude;
// // };
// // ========================================================

// void setup()
// {
//     Serial.begin(115200);
//     delay(1500);

//     Serial.println("LoRa Receiver Starting...");

//     pinMode(LORA_TX_EN, OUTPUT);
//     pinMode(LORA_RX_EN, OUTPUT);

//     digitalWrite(LORA_TX_EN, LOW);
//     digitalWrite(LORA_RX_EN, HIGH);

//     SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
//     LoRa.setSPI(SPI);
//     LoRa.setPins(LORA_CS, LORA_RST, -1);

//     if (!LoRa.begin(915E6))
//     {
//         Serial.println("LoRa.begin FAILED");
//         while (1)
//             delay(1000);
//     }

//     LoRa.setSpreadingFactor(7);
//     LoRa.setSignalBandwidth(250E3);
//     LoRa.setCodingRate4(5);
//     LoRa.disableCrc();

//     Serial.println("LoRa Receiver Ready");
// }

// void loop()
// {
//     int packetSize = LoRa.parsePacket();
//     if (!packetSize)
//     {
//         delay(10);
//         return;
//     }

//     String msg = "";
//     while (LoRa.available())
//     {
//         msg += (char)LoRa.read();
//     }

//     Serial.println();
//     Serial.println("===== RECEIVED PACKET =====");
//     Serial.print("RSSI: ");
//     Serial.println(LoRa.packetRssi());
//     Serial.print("SNR: ");
//     Serial.println(LoRa.packetSnr());
//     Serial.print("DATA: ");
//     Serial.println(msg);
//     Serial.println("==========================");
// }

#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
// DAQ perfboard
#define LORA_MISO  35 // 38 //35 c
#define LORA_MOSI  25 // 36 //25 c
#define LORA_SCK   32 // 37 //32 c
#define LORA_CS    33 // 13 //33 c

// Control Pins
#define LORA_D0    14 // 10 //14 c
#define LORA_D1    11 //
#define LORA_RST   27 // 35 //27 c
#define LORA_TX_EN 17 // 5 //26 c
#define LORA_RX_EN 5  // 6 //12 c

/* Other settings */
#define LORA_FREQ             915E6
#define LORA_SPREADING_FACTOR 7
#define LORA_SIGNAL_BANDWIDTH 250E3

SPIClass spi_bus(VSPI);



void setup() {
//     Serial.begin(115200);
//     delay(2000);

//     Serial.println("Initializing...");

//     spi_bus.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);

//     LoRa.setSPI(spi_bus);
//     LoRa.setPins(LORA_CS, LORA_RST, LORA_D0);

//     int lora_init = LoRa.begin(LORA_FREQ);
//     configASSERT(lora_init != 0 && "LoRa cannot start up");

// #ifdef UPLINK_MODE
//     LoRa.setCodingRate4(8);
//     LoRa.setSignalBandwidth(62500);
//     LoRa.setSpreadingFactor(9);
//     LoRa.enableCrc();
// #elif LAUNCH_MODE
//     LoRa.setCodingRate4(6);
//     LoRa.setSignalBandwidth(125000);
//     LoRa.setSpreadingFactor(9);
// #endif
//     LoRa.setSyncWord(0x72);

//     Serial.println("LoRa set up!");

//     pinMode(LORA_RX_EN, OUTPUT);
//     pinMode(LORA_TX_EN, OUTPUT);
//     digitalWrite(LORA_RX_EN, HIGH);
//     digitalWrite(LORA_TX_EN, LOW);
    Serial.begin(115200);
    delay(1000);

    Serial.println("hi1");
    spi_bus.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);

    Serial.println("hi1");

    LoRa.setSPI(spi_bus);
    Serial.println("hi1");
    LoRa.setPins(LORA_CS, LORA_RST, LORA_D0);

    Serial.println("hi1");

    if (!LoRa.begin(915E6)) {
        Serial.println("Could not start LoRa!");
    }

    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(250E3);
    LoRa.setCodingRate4(5);
    LoRa.enableCrc();    // Better packet validation

    Serial.println("LoRa set up!");

    pinMode(LORA_RX_EN, OUTPUT);
    pinMode(LORA_TX_EN, OUTPUT);

    digitalWrite(LORA_RX_EN, HIGH);
    digitalWrite(LORA_TX_EN, LOW); // this can be maximally slow, since we want reliability here

    LoRa.setTxPower(17);
}

// void loop() {
//   //Serial.println("Initialized and [not] listening (april fools!)");
//     int packetSize = LoRa.parsePacket();
//     if (!packetSize)
//     {
//         delay(10);
//         return;
//     }

//     String msg = "";
//     while (LoRa.available())
//     {
//         msg += (char)LoRa.read();
//     }

//     Serial.println();
//     Serial.println("===== RECEIVED PACKET =====");
//     Serial.print("RSSI: ");
//     Serial.println(LoRa.packetRssi());
//     Serial.print("SNR: ");
//     Serial.println(LoRa.packetSnr());
//     Serial.print("DATA: ");
//     Serial.println(msg);
//     Serial.println("==========================");

  
// }

// void loop() {
//     int packetSize = LoRa.parsePacket();
//     if (!packetSize) {
//         delay(10);
//         return;
//     }

//     String telemetry = "";
//     while (LoRa.available()) {
//         telemetry += (char)LoRa.read();
//     }

//     Serial.println(telemetry);
// }

// void loop() {
//     int packetSize = LoRa.parsePacket();
//     if (!packetSize) {
//         delay(10);
//         return;
//     }

//     String msg = "";
//     while (LoRa.available()) {
//         msg += (char)LoRa.read();
//     }

//     long lat_i = 0, lon_i = 0, galt_i = 0;
//     float ax = NAN, ay = NAN, az = NAN;

//     int latPos = msg.indexOf("lat=");
//     int lonPos = msg.indexOf("lon=");
//     int galtPos = msg.indexOf("galt=");
//     int axPos = msg.indexOf("ax=");
//     int ayPos = msg.indexOf("ay=");
//     int azPos = msg.indexOf("az=");

//     if (latPos >= 0) lat_i = msg.substring(latPos + 4, msg.indexOf(",", latPos)).toInt();
//     if (lonPos >= 0) lon_i = msg.substring(lonPos + 4, msg.indexOf(",", lonPos)).toInt();
//     if (galtPos >= 0) galt_i = msg.substring(galtPos + 5, msg.indexOf(",", galtPos)).toInt();
//     if (axPos >= 0) ax = msg.substring(axPos + 3, msg.indexOf(",", axPos)).toFloat();
//     if (ayPos >= 0) ay = msg.substring(ayPos + 3, msg.indexOf(",", ayPos)).toFloat();
//     if (azPos >= 0) az = msg.substring(azPos + 3, msg.indexOf(",", azPos)).toFloat();

//     Serial.print("ax=");
//     Serial.print(ax, 2);
//     Serial.print(", ay=");
//     Serial.print(ay, 2);
//     Serial.print(", az=");
//     Serial.print(az, 2);
//     Serial.print(", lat=");
//     Serial.print(lat_i / 10000000.0, 4);
//     Serial.print(", lon=");
//     Serial.print(lon_i / 10000000.0, 4);
//     Serial.print(", alt=");
//     Serial.println(galt_i / 1000.0, 2);
// }

float getFloatField(const String& msg, const char* key, bool& found) {
    String token = String(key) + "=";
    int pos = msg.indexOf(token);
    if (pos < 0) {
        found = false;
        return NAN;
    }

    int start = pos + token.length();
    int end = msg.indexOf(",", start);
    if (end < 0) end = msg.length();

    found = true;
    return msg.substring(start, end).toFloat();
}

long getLongField(const String& msg, const char* key, bool& found) {
    String token = String(key) + "=";
    int pos = msg.indexOf(token);
    if (pos < 0) {
        found = false;
        return 0;
    }

    int start = pos + token.length();
    int end = msg.indexOf(",", start);
    if (end < 0) end = msg.length();

    found = true;
    return msg.substring(start, end).toInt();
}

void loop() {
    // Serial.println();
    // delay(1000);
    // Serial.println("===== RECEIVED PACKET =====");
    // Serial.print("RSSI: ");
    // Serial.println(LoRa.packetRssi());
    // Serial.print("SNR: ");
    // Serial.println(LoRa.packetSnr());

    // int packetSize = LoRa.parsePacket();
    // if (!packetSize) {
    //     delay(10);
    //     return;
    // }

    int packetSize = LoRa.parsePacket();

    if (!packetSize) {
        Serial.println("no packet");
        delay(500);
        return;
    }

    Serial.println();
    Serial.println("===== RECEIVED PACKET =====");
    Serial.print("RSSI: ");
    Serial.println(LoRa.packetRssi());
    Serial.print("SNR: ");
    Serial.println(LoRa.packetSnr());

    String msg = "";
    while (LoRa.available()) {
        msg += (char)LoRa.read();
    }
    msg.trim();

    bool hasLat, hasLon, hasGalt, hasGfix;
    bool hasAx, hasAy, hasAz, hasGx, hasGy, hasGz;
    bool hasA1v, hasA1a, hasA2v, hasA2a;

    long lat_i  = getLongField(msg, "lat", hasLat);
    long lon_i  = getLongField(msg, "lon", hasLon);
    long galt_i = getLongField(msg, "galt", hasGalt);
    int gfix    = (int)getLongField(msg, "gfix", hasGfix);

    float ax = getFloatField(msg, "ax", hasAx);
    float ay = getFloatField(msg, "ay", hasAy);
    float az = getFloatField(msg, "az", hasAz);

    float gx = getFloatField(msg, "gx", hasGx);
    float gy = getFloatField(msg, "gy", hasGy);
    float gz = getFloatField(msg, "gz", hasGz);

    int a1v = (int)getLongField(msg, "a1v", hasA1v);
    float a1a = getFloatField(msg, "a1a", hasA1a);

    int a2v = (int)getLongField(msg, "a2v", hasA2v);
    float a2a = getFloatField(msg, "a2a", hasA2a);

    float baroAlt = NAN;
    if (hasA1v && a1v && hasA1a && !isnan(a1a) && hasA2v && a2v && hasA2a && !isnan(a2a)) {
        baroAlt = (a1a + a2a) / 2.0f;
    } else if (hasA1v && a1v && hasA1a && !isnan(a1a)) {
        baroAlt = a1a;
    } else if (hasA2v && a2v && hasA2a && !isnan(a2a)) {
        baroAlt = a2a;
    }

    float lat = hasLat ? lat_i / 10000000.0f : NAN;
    float lon = hasLon ? lon_i / 10000000.0f : NAN;
    float gpsAlt = hasGalt ? galt_i / 1000.0f : NAN;

    Serial.print("baro_alt=");
    if (isnan(baroAlt)) Serial.print("NA"); else Serial.print(baroAlt, 2);

    Serial.print(", ax=");
    if (isnan(ax)) Serial.print("NA"); else Serial.print(ax, 3);
    Serial.print(", ay=");
    if (isnan(ay)) Serial.print("NA"); else Serial.print(ay, 3);
    Serial.print(", az=");
    if (isnan(az)) Serial.print("NA"); else Serial.print(az, 3);

    Serial.print(", gx=");
    if (isnan(gx)) Serial.print("NA"); else Serial.print(gx, 3);
    Serial.print(", gy=");
    if (isnan(gy)) Serial.print("NA"); else Serial.print(gy, 3);
    Serial.print(", gz=");
    if (isnan(gz)) Serial.print("NA"); else Serial.print(gz, 3);

    Serial.print(", lat=");
    if (isnan(lat)) Serial.print("NA"); else Serial.print(lat, 6);

    Serial.print(", lon=");
    if (isnan(lon)) Serial.print("NA"); else Serial.print(lon, 6);

    Serial.print(", gps_alt=");
    if (isnan(gpsAlt)) Serial.print("NA"); else Serial.print(gpsAlt, 2);

    Serial.print(", gps_fix=");
    if (!hasGfix) Serial.println("NA"); else Serial.println(gfix);
}

/**
 * TEST ROCKET 1 (NOSE CONE) PACKET VALIDATION CRITERIA
 *
 * Radio packets shall be received (Rx) in this format:
 *
 * "A <payload> Z"
 *
 * where the entire byte stream represents characters
 * encoded in UTF-8.
 */
// bool is_valid_nose_cone_packet(char *buf, uint16_t size) {
//     if (size < 3)
//         return false;
//     if (buf[0] != OPCODE_NC_HEAD_1)
//         return false;
//     if (buf[1] != OPCODE_NC_HEAD_2)
//         return false;
//     if (buf[size - 1] != OPCODE_NC_FOOTER)
//         return false;

//     return true;
// }

/**
 * TEST ROCKET 2 (BODY TUBE) PACKET VALIDATION CRITERIA
 *
 * Radio packets shall be received (Rx) in this format:
 *
 * OP_HEAD1 OP_HEAD2 <payload> OP_FOOT
 *
 * where <payload> is a UTF-8 encoded string of data that varies
 * by size based on the specific command sent.
 */
// bool is_valid_body_tube_packet(char *buf, uint16_t size) {
//     if (size < 3)
//         return false;
//     if (buf[0] != OPCODE_BT_HEAD_1)
//         return false;
//     if (buf[1] != OPCODE_BT_HEAD_2)
//         return false;
//     if (buf[size - 1] != OPCODE_BT_FOOTER)
//         return false;

//     return true;
// }
