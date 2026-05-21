#include "ADS8688.h"

#include <Arduino.h>
#include <SPI.h>


ADS8688::ADS8688() {}

void ADS8688::begin(int spiMiso, int spiSclk, int spiMosi, int cs1, float vRef, uint8_t rangeSetting) {
    _cs1 = cs1;
    _vRef = vRef;
    _rangeSetting = rangeSetting;
    pinMode(spiMiso, INPUT_PULLUP);
    pinMode(spiSclk, OUTPUT);

    spi.begin(spiSclk, spiMiso, spiMosi, _cs1);
    pinMode(_cs1, OUTPUT);
    digitalWrite(_cs1, HIGH);
}

void ADS8688::selectADC(uint8_t cs) {
    digitalWrite(cs, LOW);
}

void ADS8688::deselectADC(uint8_t cs) {
    digitalWrite(cs, HIGH);
}

void ADS8688::writeRegister(uint8_t cs, uint8_t reg, uint8_t data) {
    selectADC(cs);
    uint16_t command = ((reg & 0x7F) << 9) | (1 << 8) | (data & 0xFF);
    spi.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE1));
    spi.transfer16(command);
    spi.endTransaction();
    deselectADC(cs);
}

uint16_t ADS8688::readRegister(uint8_t cs, uint8_t reg) {
    selectADC(cs);
    uint16_t command = ((reg & 0x7F) << 9) | (0 << 8) | (0x00 & 0xFF);
    spi.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE1));
    spi.transfer16(command);
    uint16_t result = spi.transfer16(0x0000);
    spi.endTransaction();
    deselectADC(cs);
    return result;
}

void ADS8688::setInputRange(uint8_t cs, uint8_t range) {
    for (uint8_t channel = 0; channel < 8; channel++) {
        writeRegister(cs, 0x05 + channel, range);
    }
}

void ADS8688::setAutoScanMode(uint8_t cs) {
    selectADC(cs);
    spi.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE1));
    spi.transfer16(0xA000);
    spi.endTransaction();
    deselectADC(cs);
}

void ADS8688::enableChannelsForAutoScan(uint8_t cs, uint8_t channels) {
    writeRegister(cs, 0x01, channels);
}

uint16_t ADS8688::readADCChannel(uint8_t cs, uint8_t channel) {
    if (channel > 7) return 0;
    selectADC(cs);
    uint16_t command = 0xC000 + (channel * 0x0400);
    spi.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE1));
    spi.transfer16(command);
    uint16_t result = spi.transfer16(0x0000);
    spi.endTransaction();
    deselectADC(cs);
    return result;
}

void ADS8688::readAllChannels(uint8_t cs, bool voltage, float voltages[8]) {
    for (uint8_t i = 0; i < 8; i++) {
        uint16_t result = readADCChannel(cs, (i + 7) % 8);
        voltages[i] = convertToVoltage(result)*1000; // Return voltage in mV
        // voltages[i] = result;
    }
}


float ADS8688::convertToVoltage(uint16_t rawValue) {
    float maxVoltage = 2.5 * _vRef;
    bool bipolar = true;
    switch (_rangeSetting) {
        case 0x0: maxVoltage = 2.5 * _vRef; break;
        case 0x01: maxVoltage = 1.25 * _vRef; break;
        case 0x02: maxVoltage = 0.625 * _vRef; break;
        case 0x05: maxVoltage = 2.5 * _vRef; bipolar = false; break;
        case 0x06: maxVoltage = 1.25 * _vRef; bipolar = false; break;
    }
    if (bipolar) {
        return ((int16_t)rawValue) * (maxVoltage / 32768.0);
    } else {
        return (rawValue / 65535.0) * maxVoltage;
    }
}
