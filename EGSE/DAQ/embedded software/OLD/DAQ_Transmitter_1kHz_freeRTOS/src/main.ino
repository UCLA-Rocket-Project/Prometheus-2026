//first attempt at implementing freeRTOS, should be decently optimized
//potential improvements: add buffer to catch any hiccups, implement watchdog timer for any faults, send bytes instead of string (expensive)
#include <Arduino.h>
#include <ADS1256.h>
#include "ADS8688.h"
#include <SPI.h>
#include <string>
#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
#include <PubSubClient.h>

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
  float pt[4];
  float lc[1];
  uint32_t timestamp;
};

QueueHandle_t sensorQueue;
SemaphoreHandle_t spiMutex;

unsigned long last_mqtt_attempt = 0;
static unsigned long lastMqttLoop = 0;
const unsigned long MQTT_RETRY_INTERVAL = 5000;

const unsigned long SAMPLE_INTERVAL = 1;   // 1 kHz
const unsigned long PUBLISH_INTERVAL = 1; // 1 kHz

// WiFi + MQTT credentials
const char *ssid = "ILAY";
const char *password = "lebronpookie123";
const char *mqtt_server = "192.168.0.100";
const char *DAQ_TOPIC = "DAQ_transmitter/receiver";

WiFiClient espClient;
PubSubClient client(espClient);

// SPI bus shared between both ADCs
SPIClass sharedSPI(FSPI);
// ADS1256 instance
ADS1256 loadCellADC(&sharedSPI, ADS1256_DRDY, ADS1256_CS, 2.5);
// ADS8688 instance
ADS8688 pressureADC;
// Calibration coefficients for load cell
float calibrationA1 = -57938.14;
float calibrationB1 = 1.16948;
// Convert voltage to weight
float convertToWeightLC1(float voltage)
{
  return (calibrationA1 * voltage) + calibrationB1;
}
float calibrationA2 = -401428.57;
float calibrationB2 = -3.46143;
float convertToWeightLC2(float voltage)
{
  return (calibrationA2 * voltage) + calibrationB2;
}
void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("still connecting...");
  }

  WiFi.setSleep(false);

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void connect_client()
{
  if (client.connected())
    return;
  if (WiFi.status() != WL_CONNECTED)
    return;

  if (millis() - last_mqtt_attempt < MQTT_RETRY_INTERVAL)
    return;
  last_mqtt_attempt = millis();

  String cid = "DAQ_TX-" + WiFi.macAddress();
  if (client.connect(cid.c_str()))
  {
    Serial.println("Connected to MQTT broker");
  }
  else
  {
    Serial.println("MQTT connect failed");
  }
}

float getCalibratedValue(float m, float b, float raw)
{
  return (raw - b) / m;
}
float mValues[4] = {5.077578, 5.073776, 5.042717, 8.125911};
float bValues[4] = {1081.752319, 1063.822266, 1080.457153, 1139.25354};

bool waitForDRDY(uint32_t timeout_us = 2000)
{
  uint32_t start = micros();
  while (digitalRead(ADS1256_DRDY))
  {
    if (micros() - start > timeout_us)
    {
      return false; // timed out, skip this sample
    }
  }
  return true;
}

void samplingTask(void *pvParameters)
{
  Serial.println("SAMPLING TASK");
  TickType_t lastWakeTime = xTaskGetTickCount();

  float ptVoltages[8];
  float ptCalibrated[8];

  while (true)
  {
    float lcCalibrated = 0.0;
    bool validSample = false;

    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE)
    {
      pressureADC.readAllChannels(ADS8688_CS, true, ptVoltages);
      delayMicroseconds(5);

      if (waitForDRDY())
      {
        float lcVoltage = loadCellADC.convertToVoltage(
            loadCellADC.readDifferentialFaster(DIFF_0_1));

        lcCalibrated = convertToWeightLC1(lcVoltage);
        validSample = true;
      }

      xSemaphoreGive(spiMutex);
    }

    if (validSample)
    {
      for (int i = 4; i < 8; i++)
      {
        ptCalibrated[i] = getCalibratedValue(
            mValues[i - 4],
            bValues[i - 4],
            ptVoltages[i]);
      }

      SensorData newData;

      for (int i = 0; i < 4; i++)
      {
        newData.pt[i] = ptCalibrated[i + 4];
      }

      newData.lc[0] = lcCalibrated;
      newData.timestamp = millis();

      xQueueOverwrite(sensorQueue, &newData);
    }

    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(SAMPLE_INTERVAL));
  }
}

void publishTask(void *pvParameters)
{
  Serial.println("PUBLISH TASK");
  char finalStr[300];
  SensorData localCopy;

  while (true)
  {
    if (!client.connected())
    {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (xQueuePeek(sensorQueue, &localCopy, 0) == pdTRUE)
    {
      snprintf(
          finalStr,
          sizeof(finalStr),
          "rocket_data pt0=%4.10f,pt1=%4.10f,pt2=%4.10f,pt3=%4.10f,lc0=%4.10f,uptime_ms=%lu",
          localCopy.pt[0],
          localCopy.pt[1],
          localCopy.pt[2],
          localCopy.pt[3],
          localCopy.lc[0],
          localCopy.timestamp);

      client.publish(DAQ_TOPIC, finalStr);
    }

    vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL));
  }
}

void mqttTask(void *pvParameters)
{
  while (true)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      WiFi.begin(ssid, password);
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    connect_client();
    client.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup()
{
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, HIGH);
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial)
    delay(10);
  delay(2000);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  // Create mutex for shared data
  sensorQueue = xQueueCreate(1, sizeof(SensorData));
  spiMutex = xSemaphoreCreateMutex();

  // Start custom SPI bus
  sharedSPI.begin(ADS1256_SCLK, ADS1256_MISO, ADS1256_MOSI, -1);
  // Initialize ADS1256 (Load Cell)
  if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE)
  {
    loadCellADC.InitializeADC();
    loadCellADC.setPGA(PGA_64);
    // loadCellADC.setMUX(DIFF_0_1);
    loadCellADC.setDRATE(DRATE_1000SPS);
    // Initialize ADS8688 (PTs)
    pressureADC.begin(ADS1256_MISO, ADS1256_SCLK, ADS1256_MOSI, ADS8688_CS, 4.1, 0x05);
    pressureADC.setInputRange(ADS8688_CS, 0x05);
    xSemaphoreGive(spiMutex);
  }
  // // WiFi + MQTT setup
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setKeepAlive(60);

  xTaskCreatePinnedToCore(samplingTask, "Sampling Task", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(publishTask, "Publish Task", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(mqttTask, "MQTT Task", 4096, NULL, 1, NULL, 0);

  Serial.println("Setup complete");
}

void loop() {}
