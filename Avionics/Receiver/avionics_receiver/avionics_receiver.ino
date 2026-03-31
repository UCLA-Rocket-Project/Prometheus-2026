#include <SPI.h>
#include <RadioLib.h>

// ==========================
// RADIO PINS (RECEIVER BOARD)
// ==========================
#define RADIO_SCK   37
#define RADIO_MISO  38
#define RADIO_MOSI  36
#define RADIO_CS    13
#define RADIO_RST   35
#define RADIO_DIO1  14
#define RADIO_BUSY  39

#define RXEN 6
#define TXEN 5

// ==========================
// RADIO OBJECT
// ==========================
SPIClass radioSPI(FSPI);
SX1262 radio = new Module(RADIO_CS, RADIO_DIO1, RADIO_RST, RADIO_BUSY, radioSPI);

// ==========================
// SETTINGS (MUST MATCH TX)
// ==========================
#define FREQ        915.0
#define BANDWIDTH   250.0
#define SPREADING   7
#define CODINGRATE  5
#define SYNCWORD    0x12
#define POWER       22
#define PREAMBLE    8

// ==========================
// RX MODE
// ==========================
void setRadioRX() {
  digitalWrite(TXEN, LOW);
  digitalWrite(RXEN, HIGH);
  delayMicroseconds(100);
}

// ==========================
// SETUP
// ==========================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TXEN, OUTPUT);
  pinMode(RXEN, OUTPUT);
  pinMode(RADIO_RST, OUTPUT);

  digitalWrite(RADIO_RST, HIGH);
  setRadioRX();

  radioSPI.begin(RADIO_SCK, RADIO_MISO, RADIO_MOSI, -1);

  int state = radio.begin(
    FREQ,
    BANDWIDTH,
    SPREADING,
    CODINGRATE,
    SYNCWORD,
    POWER,
    PREAMBLE,
    0.0
  );

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Radio init failed: ");
    Serial.println(state);
    while (true);
  }

  radio.setCRC(true);

  Serial.println("Receiver Ready 🚀");
}

// ==========================
// LOOP
// ==========================
void loop() {
  String data;

  int state = radio.receive(data);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("----- PACKET -----");
    Serial.println(data);

    Serial.print("RSSI: ");
    Serial.println(radio.getRSSI());

    Serial.print("SNR: ");
    Serial.println(radio.getSNR());

    // Basic framing check
    if (data.startsWith("A,") && data.endsWith(",Z")) {
      Serial.println("VALID PACKET ✅");
    } else {
      Serial.println("INVALID FORMAT ❌");
    }

    Serial.println("------------------\n");
  }
}