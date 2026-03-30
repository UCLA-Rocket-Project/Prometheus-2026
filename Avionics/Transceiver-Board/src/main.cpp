#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>

#ifdef YAGI
    // SPI Interface Pins
    #define LORA_MISO  38
    #define LORA_MISO  38
    #define LORA_MOSI  36
    #define LORA_SCK   37
    #define LORA_CS    13

    // Control Pins
    #define LORA_D0    10
    #define LORA_RST   35
    #define LORA_TX_EN 5
    #define LORA_RX_EN 6
#endif

//(using TR1 nose cone as a receiver)
// #define LORA_MISO    11
// #define LORA_MOSI    12
// #define LORA_SCK     10
// #define LORA_CS      13

// // Control Pins
// #define LORA_D0      18
// #define LORA_RST     17
// #define LORA_TX_EN   14
// #define LORA_RX_EN   21

#ifdef DAQ_PERFBOARD
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
#endif

/* Other settings */
#define LORA_FREQ             908E6
#define LORA_SPREADING_FACTOR 7
#define LORA_SIGNAL_BANDWIDTH 250E3

#ifdef YAGI
SPIClass spi_bus(FSPI);
SPIClass spi_bus_2(HSPI);
#elif DAQ_PERFBOARD
SPIClass spi_bus(VSPI);
#endif

/* Buffer sizes */
#define RADIO_PACKET_BUF_SIZE 256
#define RSSI_BUF_SIZE         32
#define PRINT_BUF_SIZE        RADIO_PACKET_BUF_SIZE + RSSI_BUF_SIZE

//=============================================
//========== PACKET OP CODES ==================

#define OPCODE_BT_HEAD_1      0x1A
#define OPCODE_BT_HEAD_2      0x1B
#define OPCODE_BT_FOOTER      0x1C

#define OPCODE_NC_HEAD_1      0x17
#define OPCODE_NC_HEAD_2      0x18
#define OPCODE_NC_FOOTER      0x19

//=============================================
//=============================================

bool is_valid_body_tube_packet(char *buf, uint16_t size);
bool is_valid_nose_cone_packet(char *buf, uint16_t size);

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

    Serial.println("LoRa set up!");

    pinMode(LORA_RX_EN, OUTPUT);
    pinMode(LORA_TX_EN, OUTPUT);

    digitalWrite(LORA_RX_EN, HIGH);
    digitalWrite(LORA_TX_EN, LOW); // this can be maximally slow, since we want reliability here

    LoRa.setTxPower(17);
}

void loop() {
    int packet_size = LoRa.parsePacket();

    if (packet_size) {
        //=========== TASK 1: parse bytes from LoRa, validate ===========
        char rx_buf[RADIO_PACKET_BUF_SIZE];
        int rx_idx = 0;

        while (LoRa.available() && rx_idx < RADIO_PACKET_BUF_SIZE) {
            rx_buf[rx_idx++] = (char)LoRa.read();
        }

        rx_buf[rx_idx] = '\0';

        bool bt_valid = is_valid_body_tube_packet(rx_buf, rx_idx);
        bool nc_valid = is_valid_nose_cone_packet(rx_buf, rx_idx);

        //===========//===========//===========//===========//===========
        //===========//========== BODY TUBE RX  ============//===========
        //===========//===========//===========//===========//===========
        if (bt_valid) {
            //=========== TASK 2: decode + print string packet ===========

            // find the payload
            int payload_start = 2;        // after OPCODE_HEAD_1 OPCODE_HEAD_2
            int payload_len = rx_idx - 3; // remove all 3 OPCODEs

            if (payload_len < 1)
                return;

            char print_buf[PRINT_BUF_SIZE];

            print_buf[0] = 'B';
            print_buf[1] = ',';

            // copy payload, remove OPCODEs
            memcpy(print_buf + 2, &rx_buf[payload_start], payload_len);

            int print_idx = payload_len + 2;

            //=========== TASK 3: append RSSI ===========

            print_idx += snprintf(
                print_buf + print_idx, PRINT_BUF_SIZE - print_idx, ",%d", LoRa.packetRssi()
            );

            //=========== TASK 4: print buffer ===========

            print_buf[min(print_idx, PRINT_BUF_SIZE - 1)] = '\0';

            // print to laptop serial
            Serial.println(print_buf);
        }

        //===========//===========//===========//===========//===========
        //===========//========== NOSE CONE RX  ============//===========
        //===========//===========//===========//===========//===========
        if (nc_valid) {
            // find the payload
            int payload_start = 2;        // after OPCODE_HEAD_1 OPCODE_HEAD_2
            int payload_len = rx_idx - 3; // remove all 3 OPCODEs

            if (payload_len < 1)
                return;

            char print_buf[PRINT_BUF_SIZE];

            print_buf[0] = 'N';
            print_buf[1] = ',';

            // copy payload, remove OPCODEs
            memcpy(print_buf + 2, &rx_buf[payload_start], payload_len);

            int print_idx = payload_len + 2;

            //=========== TASK 3: append RSSI ===========

            print_idx += snprintf(
                print_buf + print_idx, PRINT_BUF_SIZE - print_idx, ",%d", LoRa.packetRssi()
            );

            //=========== TASK 4: print buffer ===========

            print_buf[min(print_idx, PRINT_BUF_SIZE - 1)] = '\0';

            // print to laptop serial
            Serial.println(print_buf);
        }
    }
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
bool is_valid_nose_cone_packet(char *buf, uint16_t size) {
    if (size < 3)
        return false;
    if (buf[0] != OPCODE_NC_HEAD_1)
        return false;
    if (buf[1] != OPCODE_NC_HEAD_2)
        return false;
    if (buf[size - 1] != OPCODE_NC_FOOTER)
        return false;

    return true;
}

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
bool is_valid_body_tube_packet(char *buf, uint16_t size) {
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