#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "ADS8688.h"
#include "pins.h"

// PT channels on the ADS8688 are wired to indices 2, 3, 4
#define PT_CH_COUNT 3
static const int PT_CHANNELS[PT_CH_COUNT] = {2, 3, 4};

// Calibration: calibrated_psi = raw_mV * m + b
// Replace with real coefficients after running BodyTubeCalibration
static const float PT_M[PT_CH_COUNT] = {1.0f, 1.0f, 1.0f};
static const float PT_B[PT_CH_COUNT] = {0.0f, 0.0f, 0.0f};

ADS8688 pressureADC;
SPIClass sdSpi(HSPI);
static String logFile;

static String nextFilename() {
    for (int i = 0; i < 1000; i++) {
        String path = "/pt_log" + String(i) + ".csv";
        if (!SD.exists(path)) return path;
    }
    return "/pt_log_overflow.csv";
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== Body Tube PT Logger ===");

    // ADS8688
    pressureADC.begin(SPI4_MISO, SPI4_SCK, SPI4_MOSI, SPI4_CS2, 4.1f, 0x05);
    pressureADC.setInputRange(SPI4_CS2, 0x05);
    Serial.println("[ADC] OK");

    // SD card
    sdSpi.begin(SPI2_SCK, SPI2_MISO, SPI2_MOSI, SPI2_CS);
    if (!SD.begin(SPI2_CS, sdSpi)) {
        Serial.println("[SD] FAIL — continuing without logging");
    } else {
        logFile = nextFilename();
        File f = SD.open(logFile, FILE_WRITE);
        if (f) {
            f.println("timestamp_ms,pt2_mV,pt3_mV,pt4_mV,pt2_psi,pt3_psi,pt4_psi");
            f.close();
        }
        Serial.println("[SD] Logging to " + logFile);
    }
}

void loop() {
    float voltages[8];
    pressureADC.readAllChannels(SPI4_CS2, true, voltages);

    uint32_t t = millis();
    float raw[PT_CH_COUNT];
    float psi[PT_CH_COUNT];

    for (int i = 0; i < PT_CH_COUNT; i++) {
        raw[i] = voltages[PT_CHANNELS[i]];
        psi[i] = raw[i] * PT_M[i] + PT_B[i];
    }

    // Serial print
    Serial.print(t);
    Serial.print(" ms |");
    for (int i = 0; i < PT_CH_COUNT; i++) {
        Serial.print("  PT");
        Serial.print(PT_CHANNELS[i]);
        Serial.print("=");
        Serial.print(raw[i], 1);
        Serial.print(" mV (");
        Serial.print(psi[i], 2);
        Serial.print(" PSI)");
    }
    Serial.println();

    // SD log
    if (logFile.length() > 0) {
        File f = SD.open(logFile, FILE_APPEND);
        if (f) {
            f.print(t);
            for (int i = 0; i < PT_CH_COUNT; i++) {
                f.print(","); f.print(raw[i], 2);
            }
            for (int i = 0; i < PT_CH_COUNT; i++) {
                f.print(","); f.print(psi[i], 4);
            }
            f.println();
            f.close();
        }
    }

    delay(100);
}
