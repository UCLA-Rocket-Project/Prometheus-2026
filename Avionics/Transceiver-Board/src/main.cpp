#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>

// =====================================================
// Select exactly ONE hardware target in platformio.ini
// Example:
//   build_flags = -DDAQ_PERFBOARD
// or
//   build_flags = -DYAGI
// =====================================================
#if !defined(YAGI) && !defined(DAQ_PERFBOARD)
    #error "Define either YAGI or DAQ_PERFBOARD in platformio.ini build_flags"
#endif

// =====================================================
// Pin mapping
// =====================================================
#ifdef YAGI
    #define LORA_MISO  38
    #define LORA_MOSI  36
    #define LORA_SCK   37
    #define LORA_CS    13

    #define LORA_D0    10
    #define LORA_RST   35
    #define LORA_TX_EN 5
    #define LORA_RX_EN 6

SPIClass loraSpi(FSPI);
#endif

#ifdef DAQ_PERFBOARD
    #define LORA_MISO  35
    #define LORA_MOSI  25
    #define LORA_SCK   32
    #define LORA_CS    33

    #define LORA_D0    14
    #define LORA_RST   27
    #define LORA_TX_EN 17
    #define LORA_RX_EN 5

SPIClass loraSpi(VSPI);
#endif

// =====================================================
// Radio settings
// Match these to the transmitter
// =====================================================
static constexpr long LORA_FREQ = 915E6;
static constexpr int LORA_SPREADING_FACTOR = 7;
static constexpr long LORA_SIGNAL_BANDWIDTH = 250E3;
static constexpr int LORA_CODING_RATE_4 = 5; // 5 means 4/5
static constexpr bool LORA_ENABLE_CRC = true;
static constexpr uint8_t LORA_SYNC_WORD = 0x12;

// =====================================================
// Body tube packet framing
// =====================================================
static constexpr uint8_t OPCODE_BT_HEAD_1 = 0x1A;
static constexpr uint8_t OPCODE_BT_HEAD_2 = 0x1B;
static constexpr uint8_t OPCODE_BT_FOOTER = 0x1C;

// =====================================================
// Buffers
// =====================================================
static constexpr size_t RADIO_PACKET_BUF_SIZE = 256;

// =====================================================
// Helpers
// =====================================================
bool isValidBodyTubePacket(const uint8_t *buf, size_t size) {
    if (size < 3)
        return false;
    if (buf[0] != OPCODE_BT_HEAD_1)
        return false;
    if (buf[1] != OPCODE_BT_HEAD_2)
        return false;
    if (buf[size - 1] != OPCODE_BT_FOOTER)
        return false;
    return true;
}

void printHexPacket(const uint8_t *buf, size_t size) {
    Serial.print("HEX: ");
    for (size_t i = 0; i < size; i++) {
        if (buf[i] < 0x10)
            Serial.print('0');
        Serial.print(buf[i], HEX);
        if (i + 1 < size)
            Serial.print(' ');
    }
    Serial.println();
}

void printAsciiPayload(const uint8_t *buf, size_t size) {
    // payload is everything between header bytes and footer
    if (size < 3) {
        Serial.println("Payload: <none>");
        return;
    }

    const size_t payloadStart = 2;
    const size_t payloadLen = size - 3;

    Serial.print("Payload: ");
    for (size_t i = 0; i < payloadLen; i++) {
        char c = static_cast<char>(buf[payloadStart + i]);
        if (c >= 32 && c <= 126) {
            Serial.print(c);
        } else {
            Serial.print('.');
        }
    }
    Serial.println();
}

void configureRadioPins() {
    pinMode(LORA_RX_EN, OUTPUT);
    pinMode(LORA_TX_EN, OUTPUT);

    // Receiver mode
    digitalWrite(LORA_RX_EN, HIGH);
    digitalWrite(LORA_TX_EN, LOW);
}

bool initLoRa() {
    loraSpi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);

    LoRa.setSPI(loraSpi);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_D0);

    if (!LoRa.begin(LORA_FREQ)) {
        return false;
    }

    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING_RATE_4);

    if (LORA_ENABLE_CRC) {
        LoRa.enableCrc();
    } else {
        LoRa.disableCrc();
    }

    LoRa.setSyncWord(LORA_SYNC_WORD);

    return true;
}

void printRadioConfig() {
    Serial.println();
    Serial.println("====================================");
    Serial.println("Body Tube Receiver Test");
    Serial.println("====================================");
    Serial.print("Frequency: ");
    Serial.println(LORA_FREQ);
    Serial.print("SF: ");
    Serial.println(LORA_SPREADING_FACTOR);
    Serial.print("BW: ");
    Serial.println(LORA_SIGNAL_BANDWIDTH);
    Serial.print("CRC: ");
    Serial.println(LORA_ENABLE_CRC ? "ON" : "OFF");
    Serial.print("SyncWord: 0x");
    Serial.println(LORA_SYNC_WORD, HEX);
    Serial.println("Waiting for body tube packets...");
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(1200);

    Serial.println("Booting receiver test...");

    configureRadioPins();

    if (!initLoRa()) {
        Serial.println("ERROR: LoRa.begin() failed");
        Serial.println("Check power, antenna, pins, frequency, and SPI wiring.");
        while (true) {
            delay(1000);
        }
    }

    printRadioConfig();
}

void loop() {
    int packetSize = LoRa.parsePacket();
    if (!packetSize) {
        return;
    }

    uint8_t rxBuf[RADIO_PACKET_BUF_SIZE];
    size_t rxLen = 0;

    while (LoRa.available() && rxLen < RADIO_PACKET_BUF_SIZE) {
        rxBuf[rxLen++] = static_cast<uint8_t>(LoRa.read());
    }

    Serial.println("----- Packet received -----");
    Serial.print("Bytes: ");
    Serial.println(rxLen);
    Serial.print("RSSI: ");
    Serial.println(LoRa.packetRssi());
    Serial.print("SNR: ");
    Serial.println(LoRa.packetSnr());

    printHexPacket(rxBuf, rxLen);

    if (isValidBodyTubePacket(rxBuf, rxLen)) {
        Serial.println("Status: VALID BODY TUBE PACKET");
        printAsciiPayload(rxBuf, rxLen);
    } else {
        Serial.println("Status: INVALID / NOT BODY TUBE");
    }

    Serial.println();
}