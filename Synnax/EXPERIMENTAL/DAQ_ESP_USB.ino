#include <Arduino.h>
#include <ADS1256.h>
#include "ADS8688.h"
#include <SPI.h>
#include <string>
#include <Adafruit_ADS1X15.h>

// Load Cell (ADS1256) SPI Pins
#define ADS1256_MISO 35
#define ADS1256_SCLK 48
#define ADS1256_MOSI 34
#define ADS1256_CS 7
#define ADS1256_DRDY 4

// PT (ADS8688) SPI
#define ADS8688_CS 36

// LED indicator pin
#define LED 38
#define RO_PIN 44    // 16
#define DI_PIN 43    // 17
#define DE_RE_PIN 41 // 23

struct SensorData
{
  float pt[8];
  float lc[2];
  uint32_t timestamp;
};

QueueHandle_t sensorQueue;
SemaphoreHandle_t spiMutex;

const unsigned long SAMPLE_INTERVAL = 1;
const unsigned long PUBLISH_INTERVAL = 4;

const int PT_CHANNEL_COUNT = 8;
const int LC_CHANNEL_COUNT = 2;

SPIClass sharedSPI(FSPI);
ADS1256 loadCellADC(&sharedSPI, ADS1256_DRDY, ADS1256_CS, 2.5);
ADS8688 pressureADC;



// HOW TO UPDATE (for bums):
//Find the sensor you want (pt0..pt7 or lc0..lc1).
//Replace only the numbers for m and b (and mountOffset for load cells).
//If you want to disable one PT channel, set m = 0.0 (output becomes 0.0).
struct LinearCalibration {
  float m;
  float b;
};

struct LoadCellCalibration {
  float m;
  float b;
  float mountOffset; //when tighten bolt, sometimes b value change, this fixes it.
};

LinearCalibration PT_CALIBRATION[PT_CHANNEL_COUNT] = {
//{m, b}
  {2.59713f, 1005.1352f},    // pt0
  {8.187604f, 1002.913757f}, // pt1
  {0.036137f, 1350.183472f}, // pt2
  {8.142941f, 1035.41272f},  // pt3
  {0.0f, 0.0f},              // pt4 (disabled if m=0)
  {8.218279f, 999.9568237f}, // pt5
  {8.126287f, 968.913f},     // pt6
  {5.134613f, 1067.228f}     // pt7
};

LoadCellCalibration LC_CALIBRATION[LC_CHANNEL_COUNT] = {
//{m, b, mountOffset}
  {-1.722651f, 11.209444f, 20.0f},  // lc0
  {-0.260906f, -6.138861f, 52.0f}   // lc1
};

float applyLinearCalibration(float raw, const LinearCalibration& cal)
{
  if (cal.m == 0.0f)
    return 0.0f;
  return (raw - cal.b) / cal.m;
}

float applyLoadCellCalibration(float voltage, const LoadCellCalibration& cal)
{
  if (cal.m == 0.0f)
    return 0.0f;
  return (voltage - cal.b) / cal.m + cal.mountOffset;
}

bool waitForDRDY(uint32_t timeout_us = 2000)
{
  uint32_t start = micros();
  while (digitalRead(ADS1256_DRDY))
  {
    if (micros() - start > timeout_us)
    {
      return false;
    }
  }
  return true;
}

void samplingTask(void *pvParameters)
{
  Serial.println("SAMPLING TASK");
  TickType_t lastWakeTime = xTaskGetTickCount();

  while (true)
  {
    float ptVoltages[PT_CHANNEL_COUNT];
    float ptCalibrated[PT_CHANNEL_COUNT];
    float lcCalibrated0 = 0.0;
    float lcCalibrated1 = 0.0;
    bool validSample = false;

    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE)
    {
      pressureADC.readAllChannels(ADS8688_CS, true, ptVoltages);
      delayMicroseconds(5);

      bool drdy0 = waitForDRDY();
      float lcVoltage0 = drdy0
        ? loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_0_1))*100000.0f
        : 0.0f;

      bool drdy1 = waitForDRDY();
      float lcVoltage1 = drdy1
        ? loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_2_3))*100000.0f
        : 0.0f;

      validSample = drdy0 && drdy1;

      if (validSample)
      {
        lcCalibrated0 = applyLoadCellCalibration(lcVoltage0, LC_CALIBRATION[0]);
        lcCalibrated1 = applyLoadCellCalibration(lcVoltage1, LC_CALIBRATION[1]);
      }

      xSemaphoreGive(spiMutex);
    }

    if (validSample)
    {
      for (int i = 0; i < PT_CHANNEL_COUNT; i++)
      {
        ptCalibrated[i] = applyLinearCalibration(ptVoltages[i], PT_CALIBRATION[i]);
      }

      SensorData newData;
      for (int i = 0; i < PT_CHANNEL_COUNT; i++)
        newData.pt[i] = ptCalibrated[i];

      newData.lc[0] = lcCalibrated0;
      newData.lc[1] = lcCalibrated1;
      newData.timestamp = millis();

      xQueueOverwrite(sensorQueue, &newData);
    }

    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(SAMPLE_INTERVAL));
  }
}

void publishTask(void *pvParameters)
{
  Serial.println("SERIAL TX TASK");
  char finalStr[300];
  SensorData localCopy;

  while (true)
  {
    if (xQueuePeek(sensorQueue, &localCopy, 0) == pdTRUE)
    {
      snprintf(
          finalStr,
          sizeof(finalStr),
          "rocket_data pt0=%4.2f,pt1=%4.2f,pt2=%4.2f,pt3=%4.2f,pt4=%4.2f,pt5=%4.2f,pt6=%4.2f,pt7=%4.2f,lc0=%4.10f,lc1=%4.10f,uptime_ms=%lu",
          localCopy.pt[0],
          localCopy.pt[1],
          localCopy.pt[2],
          localCopy.pt[3],
          localCopy.pt[4],
          localCopy.pt[5],
          localCopy.pt[6],
          localCopy.pt[7],
          localCopy.lc[0],
          localCopy.lc[1],
          localCopy.timestamp);
      Serial.println(finalStr);
    }

    vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL));
  }
}

void pinModeInit() {
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, HIGH);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
}

void loadCellInit() {
  loadCellADC.InitializeADC();
  loadCellADC.setPGA(PGA_64);
  loadCellADC.setDRATE(DRATE_1000SPS);
  pressureADC.begin(ADS1256_MISO, ADS1256_SCLK, ADS1256_MOSI, ADS8688_CS, 4.1, 0x05);
  pressureADC.setInputRange(ADS8688_CS, 0x05);
  xSemaphoreGive(spiMutex); 
}

void setup()
{
  pinModeInit();

  Serial.begin(115200);
  while (!Serial)
    delay(10);
  delay(2000);

  sensorQueue = xQueueCreate(1, sizeof(SensorData));
  spiMutex = xSemaphoreCreateMutex();

  sharedSPI.begin(ADS1256_SCLK, ADS1256_MISO, ADS1256_MOSI, -1);
  if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE)
  {
    loadCellInit();
  }

  xTaskCreatePinnedToCore(samplingTask, "Sampling Task", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(publishTask, "Serial Tx Task", 6144, NULL, 2, NULL, 1);

  Serial.println("Setup complete");
}

void loop() {}
