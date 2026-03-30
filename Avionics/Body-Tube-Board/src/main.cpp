#include <Arduino.h>
#include <SPI.h>
#include <ADS1256.h>
#include "pins.h"

// Uncomment only if the library should NOT call begin() internally
// #define ADS1256_SPI_ALREADY_STARTED

// --------------------------------------------------
// ESP32-S3 custom SPI bus for body tube board
// --------------------------------------------------
SPIClass adcSpi(FSPI);

// ADS1256(DRDY, RESET, SYNC/PDWN, CS, VREF, SPI bus)
// From your body tube schematic:
//   DRDY = 14
//   RESET = not connected -> -1
//   SYNC  = not connected -> -1
//   CS    = 8
//   SPI   = adcSpi on SCK=17, MISO=16, MOSI=15
ADS1256 adc(DRDY, -1, -1, SPI4_CS, 2.500, &adcSpi);

// --------------------------------------------------
// Globals from original baseline
// --------------------------------------------------
long rawConversion = 0;
float voltageValue = 0;

int singleEndedChannels[8] = {
    SING_0, SING_1, SING_2, SING_3,
    SING_4, SING_5, SING_6, SING_7};

int differentialChannels[4] = {
    DIFF_0_1, DIFF_2_3, DIFF_4_5, DIFF_6_7};

int inputChannel = 0;
char inputMode = ' ';

int pgaValues[7] = {
    PGA_1, PGA_2, PGA_4, PGA_8,
    PGA_16, PGA_32, PGA_64};

int pgaSelection = 0;

int drateValues[16] = {
    DRATE_30000SPS,
    DRATE_15000SPS,
    DRATE_7500SPS,
    DRATE_3750SPS,
    DRATE_2000SPS,
    DRATE_1000SPS,
    DRATE_500SPS,
    DRATE_100SPS,
    DRATE_60SPS,
    DRATE_50SPS,
    DRATE_30SPS,
    DRATE_25SPS,
    DRATE_15SPS,
    DRATE_10SPS,
    DRATE_5SPS,
    DRATE_2SPS};

int drateSelection = 0;

String registers[11] = {
    "STATUS",
    "MUX",
    "ADCON",
    "DRATE",
    "IO",
    "OFC0",
    "OFC1",
    "OFC2",
    "FSC0",
    "FSC1",
    "FSC2"};

int registerToRead = 0;
int registerToWrite = 0;
int registerValueToWrite = 0;

// --------------------------------------------------
// Helpers
// --------------------------------------------------
static void printBanner()
{
    Serial.println();
    Serial.println("======================================");
    Serial.println("Body Tube Board ADS1256 PlatformIO Test");
    Serial.println("ESP32-S3 + SPI4 ADC bus");
    Serial.println("DRDY=14 CS=8 SCK=17 MISO=16 MOSI=15");
    Serial.println("======================================");
}

static void initBoardPins()
{
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(BUZZ, OUTPUT);

    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    digitalWrite(BUZZ, LOW);

    pinMode(SPI4_CS, OUTPUT);
    digitalWrite(SPI4_CS, HIGH);

    pinMode(SPI4_CS2, OUTPUT);
    digitalWrite(SPI4_CS2, HIGH);

    pinMode(DRDY, INPUT);
}

void setup()
{
    Serial.begin(115200);
    delay(1500);

    initBoardPins();
    printBanner();

    // Start custom SPI bus for ADS1256
    adcSpi.begin(SPI4_SCK, SPI4_MISO, SPI4_MOSI);

    Serial.println("Initializing ADS1256...");
    adc.InitializeADC();

    // Same initial config as your baseline
    adc.setPGA(PGA_1);
    adc.setMUX(DIFF_6_7);
    adc.setDRATE(DRATE_5SPS);

    Serial.print("PGA: ");
    Serial.println(adc.getPGA());
    delay(100);

    Serial.print("MUX: ");
    Serial.println(adc.readRegister(MUX_REG));
    delay(100);

    Serial.print("DRATE: ");
    Serial.println(adc.readRegister(DRATE_REG));
    delay(100);

    digitalWrite(LED1, HIGH);
    delay(250);
    digitalWrite(LED1, LOW);

    Serial.println("Ready. Send commands over serial.");
    delay(1000);
}

void loop()
{
    if (Serial.available() <= 0)
    {
        return;
    }

    char commandCharacter = Serial.read();

    switch (commandCharacter)
    {
    case 's': // Stop continuous conversion
        adc.stopConversion();
        break;

    case 'L': // Self-calibration
        adc.sendDirectCommand(SELFCAL);
        break;

    case 'G': // Read a single input continuously
        while (Serial.read() != 's')
        {
            Serial.println(adc.convertToVoltage(adc.readSingleContinuous()), 6);
        }
        adc.stopConversion();
        break;

    case 'C': // Cycle single-ended inputs
        while (Serial.read() != 's')
        {
            float channels[8];
            for (int j = 0; j < 8; j++)
            {
                channels[j] = adc.convertToVoltage(adc.cycleSingle());
            }

            for (int i = 0; i < 8; i++)
            {
                Serial.print(channels[i], 4);
                if (i < 7)
                {
                    Serial.print('\t');
                }
            }
            Serial.println();
        }
        adc.stopConversion();
        break;

    case 'D': // Cycle differential inputs
        while (Serial.read() != 's')
        {
            float channels[4];
            for (int j = 0; j < 4; j++)
            {
                channels[j] = adc.convertToVoltage(adc.cycleDifferential());
            }

            for (int i = 0; i < 4; i++)
            {
                Serial.print(channels[i], 4);
                if (i < 3)
                {
                    Serial.print('\t');
                }
            }
            Serial.println();
        }
        adc.stopConversion();
        break;

    case 'B':
    { // Speed test
        long numberOfSamples = 30000;
        long startTime = micros();

        for (long i = 0; i < numberOfSamples; i++)
        {
            adc.readSingleContinuous();
        }

        long finishTime = micros() - startTime;
        adc.stopConversion();

        Serial.print("Total conversion time for samples: ");
        Serial.print(finishTime);
        Serial.println(" us");

        Serial.print("Sampling rate: ");
        Serial.print(numberOfSamples * (1000000.0 / finishTime), 3);
        Serial.println(" SPS");
        break;
    }

    case 'T': // Serial test
        Serial.println("The serial connection is OK!");
        break;

    case 'a': // Single conversion
        rawConversion = adc.readSingle();
        voltageValue = adc.convertToVoltage(rawConversion);

        Serial.print("Single conversion result: ");
        Serial.println(voltageValue, 8);
        break;

    case 'M':
    { // Set MUX
        while (!Serial.available())
        {
        }
        inputMode = Serial.read();

        if (inputMode == 's')
        {
            while (!Serial.available())
            {
            }
            inputChannel = Serial.parseInt();
            adc.setMUX(singleEndedChannels[inputChannel]);
        }

        if (inputMode == 'd')
        {
            while (!Serial.available())
            {
            }
            inputChannel = Serial.parseInt();
            adc.setMUX(differentialChannels[inputChannel]);
        }
        break;
    }

    case 'P':
    { // Set PGA
        while (!Serial.available())
        {
        }
        pgaSelection = Serial.parseInt();
        adc.setPGA(pgaValues[pgaSelection]);

        Serial.print("PGA value: ");
        Serial.println(adc.getPGA());
        break;
    }

    case 'F':
    { // Set data rate
        while (!Serial.available())
        {
        }
        drateSelection = Serial.parseInt();

        Serial.print("DRATE selected: ");
        Serial.println(drateValues[drateSelection]);

        adc.setDRATE(drateValues[drateSelection]);

        Serial.print("DRATE register now: ");
        Serial.println(adc.readRegister(DRATE_REG));
        break;
    }

    case 'R':
    { // Read register
        while (!Serial.available())
        {
        }
        registerToRead = Serial.parseInt();

        Serial.print("Value of ");
        Serial.print(registers[registerToRead]);
        Serial.print(" register is: ");
        Serial.println(adc.readRegister(registerToRead));
        break;
    }

    case 'W':
    { // Write register
        while (!Serial.available())
        {
        }
        registerToWrite = Serial.parseInt();

        while (!Serial.available())
        {
        }
        registerValueToWrite = Serial.parseInt();

        adc.writeRegister(registerToWrite, registerValueToWrite);
        break;
    }

    default:
        Serial.print("Unknown command: ");
        Serial.println(commandCharacter);
        break;
    }
}