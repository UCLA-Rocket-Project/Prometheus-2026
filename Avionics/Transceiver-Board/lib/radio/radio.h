#ifndef RADIO_H_
#define RADIO_H_

#include <SPI.h>
#include <LoRa.h>

/* PIN DEFINITIONS */

// SPI Interface Pins
#define LORA_MISO    38
#define LORA_MOSI    36
#define LORA_SCK     37
#define LORA_CS      13

// Control Pins
#define LORA_D0      10
#define LORA_D1      11
#define LORA_RST     35
#define LORA_TX_EN   5
#define LORA_RX_EN   6

/* Other settings */
#define LORA_FREQ    915E6
// #define LORA_SPREADING_FACTOR 7
#define LORA_SIGNAL_BANDWIDTH 250E3

#endif // RADIO_H_