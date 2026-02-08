#ifndef ADS8688_H
#define ADS8688_H

#include <Arduino.h>
#include <SPI.h>

#define SPI_FREQ 17000000  

class ADS8688 {
public:
    ADS8688();
    void begin(int spiMiso, int spiSclk, int spiMosi, int cs1, float vRef, uint8_t rangeSetting);
    void setInputRange(uint8_t cs, uint8_t range);
    void setAutoScanMode(uint8_t cs);
    void enableChannelsForAutoScan(uint8_t cs, uint8_t channels);
    uint16_t readADCChannel(uint8_t cs, uint8_t channel);
    void readAllChannels(uint8_t cs, bool voltage, float voltages[8]);
    float convertToVoltage(uint16_t rawValue);

private:
    SPIClass spi = SPIClass(FSPI);
    int _cs1;
    float _vRef;
    uint8_t _rangeSetting;
    void selectADC(uint8_t cs);
    void deselectADC(uint8_t cs);
    void writeRegister(uint8_t cs, uint8_t reg, uint8_t data);
    uint16_t readRegister(uint8_t cs, uint8_t reg);
};

#endif
