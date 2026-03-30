#include <Arduino.h>
#include <Wire.h> //Needed for I2C to GNSS

#include "pins.h"

#include <SparkFun_u-blox_GNSS_v3.h>

#include <SPI.h>
#include <LoRa.h>

#include <Adafruit_LSM6DSOX.h>
#include <Adafruit_Sensor.h>
#include <MS5611_SPI.h>
#include "SD.h"
#include "FS.h"

SFE_UBLOX_GNSS myGNSS;
TwoWire wire = TwoWire(0);

// TODO : SOLVE THESE ERRORS :
//  [E][esp32-hal-gpio.c:102] __pinMode(): Invalid pin selected - A pin number being used is invalid for the ESP32-S3.
// This could be due to wrong pin numbers.

// [E][esp32-hal-cpu.c:110] addApbChangeCallback(): duplicate func=0x4200ff58 arg=0x3fc924d8 -
// This is a duplicate SPI bus registration issue, likely from initializing multiple SPIClass
// instances or calling begin() multiple times.

// [E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (3) The physical
// drive cannot work - SD card mount failure.

struct GpsData
{
    int32_t latitude;
    int32_t longitude;
    int32_t altitude; // gets height in mm above sea level
    int32_t heading;
};

Adafruit_LSM6DSOX imu;

struct IMUData
{
    float accelX;
    float accelY;
    float accelZ;

    float gyroX;
    float gyroY;
    float gyroZ;

    float imuTemp;
};

#define SEALEVELPRESSURE_HPA (1013.25)

struct AltData
{
    float temperature;
    float pressure;
    float altitude;
};

#define FILE_NAME_MAX_LENGTH 100
#define CSV_ENTRY_MAX_LENGTH 1024

SPIClass custom_spi_bus;
SPIClass alt_spi_bus;

MS5611_SPI ms5611(ALT_CS1, &alt_spi_bus);

const char *newFileName = "/datahehe.csv";

bool getNewFilename(char *new_file_name);
void writeFile(fs::FS &fs, const char *path, const char *message);
void getGPSData(GpsData &gpsData);
void getIMUData(IMUData &imuData);
void getAltData(AltData &altData);
void writeSensorData(GpsData &gpsData, IMUData &imuData, AltData &altData);
void writeToLora(GpsData &gpsData, IMUData &imuData, AltData &altData, unsigned long timeSinceStart);

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // move SPI initialization here to not overwrite the initialization of other sensors
    custom_spi_bus.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
    alt_spi_bus.begin(SCLK_B, MISO_B, MOSI_B, -1);

    // setup and write to the SD Card
    if (!SD.begin(SD_CS, custom_spi_bus))
    {
        Serial.println("Failed to initialize SD card");
        while (1)
            ;
    }

    // getNewFilename(newFileName);
    Serial.println(newFileName);
    writeFile(SD, newFileName, "gps_latitude,gps_longitude,gps_altitude,gps_heading,imu_accel_x,imu_accel_y,imu_accel_z,imu_gyro_x,imu_gyro_y,imu_gyro_z,imu_temp,alt_temperature,alt_pressure,alt_altitude\n");

    // GPS setup
    wire.begin(GPS_SDA, GPS_SCL);
    // myGNSS.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial

    while (myGNSS.begin(wire, gnssAddress) == false) // Connect to the u-blox module using our custom port and address
    {
        Serial.println(F("Unable to initialize GPS"));
        delay(1000);
    }
    myGNSS.setI2COutput(COM_TYPE_UBX); // Set the I2C port to output UBX only (turn off NMEA noise)

    // IMU (ISM330 / LSM6DSOX) setup
    if (!imu.begin_SPI(IMU_CS, &custom_spi_bus))
    {
        Serial.println("Failed to find IMU");
        while (1)
        {
            delay(10);
        }
    }
    Serial.println("IMU found!");

    // Altimeter (MS5611) setup
    if (!ms5611.begin())
    {
        Serial.println("Failed to find MS5611");
        while (1)
            ;
    }
    Serial.println("MS5611 found!");

    pinMode(RXEN, OUTPUT);
    pinMode(TXEN, OUTPUT);

    digitalWrite(TXEN, LOW);
    digitalWrite(RXEN, HIGH);

    Serial.println("LoRa Sender");

    // Initialize SPI with custom pins (SCK, MISO, MOSI, CS)
    SPI.begin(SCLK_A_PIN, MISO_A_PIN, MOSI_A_PIN, RADIO_CS);

    // Tell LoRa library to use custom SPI
    LoRa.setSPI(SPI);

    // Set custom LoRa pins (CS, RESET, DIO0)
    LoRa.setPins(RADIO_CS, RST_RADIO, BOOT);

    if (!LoRa.begin(915E6))
    {
        Serial.println("Starting LoRa failed!");
        while (1)
            ;
    }

    LoRa.setTxPower(20);
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(250E3);

    Serial.println("LoRa started successfully");
}

void loop()
{
    struct GpsData gpsData = {-1, -1, -1, -1};
    getGPSData(gpsData);

    struct IMUData imuData;
    getIMUData(imuData);

    struct AltData altData = {-1, -1, -1};
    getAltData(altData);
    unsigned long timeSinceStart = millis();
    writeToLora(gpsData, imuData, altData, timeSinceStart);

    writeSensorData(gpsData, imuData, altData);
    delay(2000);
}

void getGPSData(GpsData &gpsData)
{
    if (myGNSS.getPVT())
    {
        gpsData.latitude = myGNSS.getLatitude();
        gpsData.longitude = myGNSS.getLongitude();
        gpsData.altitude = myGNSS.getAltitudeMSL(); // Altitude above Mean Sea Level
        gpsData.heading = myGNSS.getHeading();

        Serial.print(F("Lat: "));
        Serial.print(gpsData.latitude);
        Serial.print(F(" Long: "));
        Serial.print(gpsData.longitude);
        Serial.print(F(" (degrees * 10^-7)"));
        Serial.print(F(" Alt: "));
        Serial.print(gpsData.altitude);
        Serial.print(F(" (mm)"));
        Serial.print(F(" Heading: "));
        Serial.print(gpsData.heading);
        Serial.print(F(" (degrees * 10^-5)"));
        Serial.println();
    }
}

void getIMUData(IMUData &imuData)
{
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    imu.getEvent(&accel, &gyro, &temp);

    imuData.imuTemp = temp.temperature;
    imuData.accelX = accel.acceleration.x;
    imuData.accelY = accel.acceleration.y;
    imuData.accelZ = accel.acceleration.z;
    imuData.gyroX = gyro.gyro.x;
    imuData.gyroY = gyro.gyro.y;
    imuData.gyroZ = gyro.gyro.z;

    Serial.print("\t\tTemp: ");
    Serial.print(imuData.imuTemp);
    Serial.println(" C");
    Serial.print("\t\tAccel X: ");
    Serial.print(imuData.accelX);
    Serial.print(" Y: ");
    Serial.print(imuData.accelY);
    Serial.print(" Z: ");
    Serial.print(imuData.accelZ);
    Serial.println(" m/s^2");
    Serial.print("\t\tGyro X: ");
    Serial.print(imuData.gyroX);
    Serial.print(" Y: ");
    Serial.print(imuData.gyroY);
    Serial.print(" Z: ");
    Serial.print(imuData.gyroZ);
    Serial.println(" rad/s");
    Serial.println();
}

void getAltData(AltData &altData)
{
    if (ms5611.read() == MS5611_READ_OK)
    {
        altData.temperature = ms5611.getTemperature();
        altData.pressure = ms5611.getPressure();
        altData.altitude = 44330.0f * (1.0f - pow(altData.pressure / SEALEVELPRESSURE_HPA, 0.1903f));

        Serial.print("Temperature = ");
        Serial.print(altData.temperature);
        Serial.println(" C");
        Serial.print("Pressure = ");
        Serial.print(altData.pressure);
        Serial.println(" hPa");
        Serial.print("Altitude = ");
        Serial.print(altData.altitude);
        Serial.println(" m");
        Serial.println();
    }
}

bool getNewFilename(char *new_file_name)
{
    int counter = 0;
    char candidate_file_name[FILE_NAME_MAX_LENGTH + 1];
    while (true)
    {
        sprintf(candidate_file_name, "/data%d.csv", counter);
        Serial.print("Checking file: ");
        Serial.println(candidate_file_name);
        if (!SD.exists(candidate_file_name))
        {
            sprintf(new_file_name, candidate_file_name);
            return true;
        }
        ++counter;
    }
    return false;
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        Serial.println("File written");
    }
    else
    {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (file.print(message))
    {
        Serial.println("Message appended");
    }
    else
    {
        Serial.println("Append failed");
    }
    file.close();
}

void writeSensorData(GpsData &gpsData, IMUData &imuData, AltData &altData)
{
    char csvEntry[CSV_ENTRY_MAX_LENGTH];
    sprintf(csvEntry, "%d,%d,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
            gpsData.latitude,
            gpsData.longitude,
            gpsData.altitude,
            gpsData.heading,
            imuData.accelX,
            imuData.accelY,
            imuData.accelZ,
            imuData.gyroX,
            imuData.gyroY,
            imuData.gyroZ,
            imuData.imuTemp,
            altData.temperature,
            altData.pressure,
            altData.altitude);

    appendFile(SD, newFileName, csvEntry);

    Serial.print("\n\nWrote: ");
    Serial.println(csvEntry);
}

void writeToLora(GpsData &gpsData, IMUData &imuData, AltData &altData, unsigned long timeSinceStart)
{
    uint8_t gpsDataByteArray[sizeof(GpsData)];
    memcpy(gpsDataByteArray, &gpsData, sizeof(GpsData));

    uint8_t imuDataArray[sizeof(IMUData)];
    memcpy(imuDataArray, &imuData, sizeof(IMUData));

    uint8_t altDataArray[sizeof(AltData)];
    memcpy(altDataArray, &altData, sizeof(AltData));

    uint8_t timestampArray[sizeof(unsigned long)];
    memcpy(timestampArray, &timeSinceStart, sizeof(unsigned long));

    Serial.println("Sending packet: ");

    digitalWrite(TXEN, HIGH);
    digitalWrite(RXEN, LOW);

    LoRa.beginPacket();
    LoRa.print("A ");
    LoRa.write(gpsDataByteArray, sizeof(GpsData));
    LoRa.write(imuDataArray, sizeof(IMUData));
    LoRa.write(altDataArray, sizeof(AltData));
    LoRa.write(timestampArray, sizeof(unsigned long));
    LoRa.print(" Z");
    LoRa.endPacket();

    digitalWrite(TXEN, LOW);
    digitalWrite(RXEN, HIGH);

    Serial.println("Finished Sending");
}