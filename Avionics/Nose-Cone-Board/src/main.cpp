#include <Arduino.h>
#include <Wire.h> //Needed for I2C to GNSS

#include <SparkFun_u-blox_GNSS_v3.h>

#include <SPI.h>
#include <LoRa.h>

#include <Adafruit_ICM20X.h>
#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>

#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"

#include "SD.h"
#include "FS.h"

// GPS
#define GPS_SDA 22
#define GPS_SCL 21
#define gnssAddress 0x42

SFE_UBLOX_GNSS myGNSS;
TwoWire wire = TwoWire(0);

struct GpsData
{
    int32_t latitude;
    int32_t longitude;
    int32_t altitude; // gets height in mm above sea level
    int32_t heading;
};

// ICM 20948
#define ICM_CS 17
#define ICM_SCK 18
#define ICM_MISO 13
#define ICM_MOSI 23

Adafruit_ICM20948 icm;
uint16_t measurement_delay_us = 65535; // Delay between measurements for testing

struct ICMData
{
    float accelX;
    float accelY;
    float accelZ;

    float gyroX;
    float gyroY;
    float gyroZ;

    float magX;
    float magY;
    float magZ;

    float icmTemp;
};

// BMP390
#define BMP_CS 16
#define BMP_SCK 18
#define BMP_MISO 13
#define BMP_MOSI 23

#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BMP3XX bmp;

struct BMPData
{
    double bmpTemp;
    double pressure;
    double altitude;
};

// SD card
#define SD_CS 4
#define SD_SCK 18
#define SD_MISO 13
#define SD_MOSI 23

#define FILE_NAME_MAX_LENGTH 100
#define CSV_ENTRY_MAX_LENGTH 1024

SPIClass custom_spi_bus;
const char *newFileName = "/datahehe.csv";

bool getNewFilename(char *new_file_name);
void writeFile(fs::FS &fs, const char *path, const char *message);
void getGPSData(GpsData &gpsData);
void getICMData(ICMData &icmData);
void getBMPData(BMPData &bmpData);
void writeSensorData(GpsData &gpsData, ICMData &icmData, BMPData &bmpData);

void writeToLora(GpsData &gpsData, ICMData &icmData, BMPData &bmpData, unsigned long timeSinceStart);

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // move SPI initialization here to not overwrite the initialization of other sensors
    custom_spi_bus.begin(SD_SCK, SD_MISO, SD_MOSI, -1);

    // setup and write to the SD Card
    if (!SD.begin(SD_CS, custom_spi_bus))
    {
        Serial.println("Failed to initialize SD card");
        while (1)
            ;
    }

    // getNewFilename(newFileName);
    Serial.println(newFileName);
    writeFile(SD, newFileName, "gps_latitude,gps_logitude,gps_altitude,gps_heading,icm_accel_x,icm_accel_y,icm_accel_z,icm_gyro_x,icm_gyro_y,icm_gyro_z,icm_mag_x,icm_mag_y,icm_mag_z,icm_temp,bmp_temperature,bmp_pressure,bmp_altitude");

    // GPS setup
    wire.begin(GPS_SDA, GPS_SCL);
    // myGNSS.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial

    while (myGNSS.begin(wire, gnssAddress) == false) // Connect to the u-blox module using our custom port and address
    {
        Serial.println(F("Unable to initialize GPS"));
        delay(1000);
    }
    myGNSS.setI2COutput(COM_TYPE_UBX); // Set the I2C port to output UBX only (turn off NMEA noise)

    // ICM20648 setup
    if (!icm.begin_SPI(ICM_CS, &custom_spi_bus))
    {

        Serial.println("Failed to find ICM20948 chip");
        while (1)
        {
            delay(10);
        }
    }

    Serial.println("ICM20948 Found!");
    // icm.setAccelRange(ICM20948_ACCEL_RANGE_16_G);
    Serial.print("Accelerometer range set to: ");
    switch (icm.getAccelRange())
    {
    case ICM20948_ACCEL_RANGE_2_G:
        Serial.println("+-2G");
        break;
    case ICM20948_ACCEL_RANGE_4_G:
        Serial.println("+-4G");
        break;
    case ICM20948_ACCEL_RANGE_8_G:
        Serial.println("+-8G");
        break;
    case ICM20948_ACCEL_RANGE_16_G:
        Serial.println("+-16G");
        break;
    }
    Serial.println("OK");

    // icm.setGyroRange(ICM20948_GYRO_RANGE_2000_DPS);
    Serial.print("Gyro range set to: ");
    switch (icm.getGyroRange())
    {
    case ICM20948_GYRO_RANGE_250_DPS:
        Serial.println("250 degrees/s");
        break;
    case ICM20948_GYRO_RANGE_500_DPS:
        Serial.println("500 degrees/s");
        break;
    case ICM20948_GYRO_RANGE_1000_DPS:
        Serial.println("1000 degrees/s");
        break;
    case ICM20948_GYRO_RANGE_2000_DPS:
        Serial.println("2000 degrees/s");
        break;
    }

    //  icm.setAccelRateDivisor(4095);
    uint16_t accel_divisor = icm.getAccelRateDivisor();
    float accel_rate = 1125 / (1.0 + accel_divisor);

    Serial.print("Accelerometer data rate divisor set to: ");
    Serial.println(accel_divisor);
    Serial.print("Accelerometer data rate (Hz) is approximately: ");
    Serial.println(accel_rate);

    //  icm.setGyroRateDivisor(255);
    uint8_t gyro_divisor = icm.getGyroRateDivisor();
    float gyro_rate = 1100 / (1.0 + gyro_divisor);

    Serial.print("Gyro data rate divisor set to: ");
    Serial.println(gyro_divisor);
    Serial.print("Gyro data rate (Hz) is approximately: ");
    Serial.println(gyro_rate);

    // icm.setMagDataRate(AK09916_MAG_DATARATE_10_HZ);
    Serial.print("Magnetometer data rate set to: ");
    switch (icm.getMagDataRate())
    {
    case AK09916_MAG_DATARATE_SHUTDOWN:
        Serial.println("Shutdown");
        break;
    case AK09916_MAG_DATARATE_SINGLE:
        Serial.println("Single/One shot");
        break;
    case AK09916_MAG_DATARATE_10_HZ:
        Serial.println("10 Hz");
        break;
    case AK09916_MAG_DATARATE_20_HZ:
        Serial.println("20 Hz");
        break;
    case AK09916_MAG_DATARATE_50_HZ:
        Serial.println("50 Hz");
        break;
    case AK09916_MAG_DATARATE_100_HZ:
        Serial.println("100 Hz");
        break;
    }
    Serial.println();

    if (!bmp.begin_SPI(BMP_CS, &custom_spi_bus))
    {
        Serial.println("Could not find a valid BMP3 sensor, check wiring!");
        while (1)
            ;
    }

    // Set up oversampling and filter initialization
    bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP3_ODR_50_HZ);

    pinMode(RX_ENABLE, OUTPUT);
    pinMode(TX_ENABLE, OUTPUT);

    digitalWrite(TX_ENABLE, LOW);
    digitalWrite(RX_ENABLE, HIGH);

    Serial.println("LoRa Sender");

    // Initialize SPI with custom pins (SCK, MISO, MOSI, CS)
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

    // Tell LoRa library to use custom SPI
    LoRa.setSPI(SPI);

    // Set custom LoRa pins (CS, RESET, DIO0)
    LoRa.setPins(CS_PIN, RESET_PIN, G0_PIN);

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

    struct ICMData icmData;
    getICMData(icmData);

    struct BMPData bmpData = {-1, -1, -1};
    getBMPData(bmpData);
    unsigned long timeSinceStart = millis();
    writeToLora(gpsData, icmData, bmpData, timeSinceStart);

    writeSensorData(gpsData, icmData, bmpData);
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

void getICMData(ICMData &icmData)
{
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t mag;
    sensors_event_t temp;
    icm.getEvent(&accel, &gyro, &temp, &mag);

    icmData.icmTemp = temp.temperature;
    Serial.print("\t\tTemperature *C");
    Serial.print(temp.temperature);
    Serial.println();

    icmData.accelX = accel.acceleration.x;
    icmData.accelY = accel.acceleration.y;
    icmData.accelZ = accel.acceleration.z;
    Serial.print("\t\tAccel X: ");
    Serial.print(accel.acceleration.x);
    Serial.print(" \tY: ");
    Serial.print(accel.acceleration.y);
    Serial.print(" \tZ: ");
    Serial.print(accel.acceleration.z);
    Serial.println(" m/s^2 ");

    icmData.magX = mag.magnetic.x;
    icmData.magY = mag.magnetic.y;
    icmData.magZ = mag.magnetic.z;
    Serial.print("\t\tMag X: ");
    Serial.print(mag.magnetic.x);
    Serial.print(" \tY: ");
    Serial.print(mag.magnetic.y);
    Serial.print(" \tZ: ");
    Serial.print(mag.magnetic.z);
    Serial.println(" uT");

    icmData.gyroX = gyro.gyro.x;
    icmData.gyroY = gyro.gyro.y;
    icmData.gyroZ = gyro.gyro.z;
    Serial.print("\t\tGyro X: ");
    Serial.print(gyro.gyro.x);
    Serial.print(" \tY: ");
    Serial.print(gyro.gyro.y);
    Serial.print(" \tZ: ");
    Serial.print(gyro.gyro.z);
    Serial.println(" radians/s ");
    Serial.println();
}

void getBMPData(BMPData &bmpData)
{
    if (bmp.performReading())
    {
        bmpData.bmpTemp = bmp.temperature;
        bmpData.pressure = bmp.pressure / 100.0;
        bmpData.altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);

        Serial.print("Temperature = ");
        Serial.print(bmpData.bmpTemp);
        Serial.println(" *C");

        Serial.print("Pressure = ");
        Serial.print(bmpData.pressure);
        Serial.println(" hPa");

        Serial.print("Approx. Altitude = ");
        Serial.print(bmpData.altitude);
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

void writeSensorData(GpsData &gpsData, ICMData &icmData, BMPData &bmpData)
{
    char csvEntry[CSV_ENTRY_MAX_LENGTH];
    sprintf(csvEntry, "%3.8d,%3.8d,%5.8d,%5.8d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
            gpsData.latitude,
            gpsData.longitude,
            gpsData.altitude,
            gpsData.heading,
            icmData.accelX,
            icmData.accelY,
            icmData.accelZ,
            icmData.gyroX,
            icmData.gyroY,
            icmData.gyroZ,
            icmData.magX,
            icmData.magY,
            icmData.magZ,
            icmData.icmTemp,
            bmpData.bmpTemp,
            bmpData.pressure,
            bmpData.altitude);

    appendFile(SD, newFileName, csvEntry);

    Serial.print("\n\nWrote:");
    Serial.println(csvEntry);
}

void writeToLora(GpsData &gpsData, ICMData &icmData, BMPData &bmpData, unsigned long timeSinceStart)
{
    uint8_t gpsDataByteArray[sizeof(GpsData)];
    memcpy(gpsDataByteArray, &gpsData, sizeof(GpsData));

    uint8_t icmDataArray[sizeof(ICMData)];
    memcpy(icmDataArray, &icmData, sizeof(ICMData));

    uint8_t bmpDataArray[sizeof(BMPData)];
    memcpy(bmpDataArray, &bmpData, sizeof(BMPData));

    uint8_t timestampArray[sizeof(unsigned long)];
    memcpy(timestampArray, &timeSinceStart, sizeof(unsigned long));

    Serial.println("Sending packet: ");

    digitalWrite(TX_ENABLE, HIGH);
    digitalWrite(RX_ENABLE, LOW);

    LoRa.beginPacket();
    LoRa.print("A ");
    LoRa.write(gpsDataByteArray, sizeof(GpsData));
    LoRa.write(icmDataArray, sizeof(ICMData));
    LoRa.write(bmpDataArray, sizeof(BMPData));
    LoRa.write(timestampArray, sizeof(unsigned long));
    LoRa.print(" Z");
    LoRa.endPacket();

    digitalWrite(TX_ENABLE, LOW);
    digitalWrite(RX_ENABLE, HIGH);

    Serial.println("Finished Sending");
}