#include <Arduino.h>
#include <ADS1256.h>
#include "ADS8688.h"
#include <SPI.h>
#include <string>
#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "Adafruit_MCP23X17.h"

// SPI
#define MISO 35
#define MOSI 34
#define SCLK 48

// Load Cell SPI Pins
#define ADS1256_CS 7
#define ADS1256_DRDY 4

// PT SPI Pins
#define ADS8688_CS 36

// RS484 Pins -- Not Used Yet
#define RO_PIN 40
#define DI_PIN 39
#define DE_RE_PIN 41

// LED Drive I2C Pins
#define SDA 38
#define SCL 39

#define LED_RANGE_LOW 0
#define LED_RANGE_HIGH 10

int logicalPTIndexToDriverPin[8] = {
  4,
  5,
  0,
  6,
  7,
  3,
  2,
  1
};

int logicalToLibraryPtIndex[8] = {
  3,
  2,
  1,
  0,
  7,
  6,
  5,
  4
};



Adafruit_MCP23X17 ledDriver;
bool ptCheck[8];

struct SensorData
{
  float pt[8];
  float lc[2];
  uint32_t timestamp;
};

QueueHandle_t sensorQueue;
SemaphoreHandle_t spiMutex;

unsigned long last_mqtt_attempt = 0;
static unsigned long lastMqttLoop = 0;
const unsigned long MQTT_RETRY_INTERVAL = 5000;

const unsigned long SAMPLE_INTERVAL = 1;
const unsigned long PUBLISH_INTERVAL = 4;

const char *ssid = "ILAY";
const char *password = "lebronpookie123";
const char *mqtt_server = "192.168.0.100";
const char *DAQ_TOPIC = "DAQ_transmitter/receiver";

WiFiClient espClient;
PubSubClient client(espClient);

SPIClass sharedSPI(FSPI);
ADS1256 loadCellADC(&sharedSPI, ADS1256_DRDY, ADS1256_CS, 2.5);
ADS8688 pressureADC;


float calibrationA1 = -1.722651;
float calibrationB1 = 11.209444;
float calibrationA2 = -0.260906;
float calibrationB2 = -6.138861;
int hardCodeConstant1 = 20; //constant is hardcoded value to account for difference in load cell mounting (should know m is same)
float convertToWeightLC1(float voltage)
{
  return (voltage-calibrationB1)/calibrationA1 + hardCodeConstant1;
}

int hardCodeConstant2 = 52; //constant is hardcoded value to account for difference in load cell mounting (should know m is same)
float convertToWeightLC2(float voltage)
{
  return (voltage-calibrationB2)/calibrationA2 + hardCodeConstant2;
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
// float mValues[8] = {2.59713, 8.187604, 0.036137, 8.142941, 0, 8.218279, 8.126287, 5.134613};
// float bValues[8] = {1005.1352, 1002.913757, 1350.183472, 1035.41272, 0, 999.9568237, 968.913, 1067.228};

float mValues[8] = {1, 1, 1, 1, 1, 1, 1, 1};
float bValues[8] = {0, 0, 0, 0, 0, 0, 0, 0};

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
    float ptVoltages[8];
    float ptCalibrated[8];
    float logicalPtVoltageOrdering[8];
    float lcCalibrated0 = 0.0;
    float lcCalibrated1 = 0.0;
    bool validSample = false;

    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE)
    {
      pressureADC.readAllChannels(ADS8688_CS, true, ptVoltages);

      for(int i = 0; i < 8; i++){
        logicalPtVoltageOrdering[i] = ptVoltages[logicalToLibraryPtIndex[i]];
      }

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
        lcCalibrated0 = convertToWeightLC1(lcVoltage0);
        lcCalibrated1 = convertToWeightLC2(lcVoltage1);
      }

      xSemaphoreGive(spiMutex);
    }

    if (validSample)
    {
      for (int i = 0; i < 8; i++)
      {
        ptCalibrated[i] = getCalibratedValue(mValues[i], bValues[i], logicalPtVoltageOrdering[i]);
      }

      SensorData newData;
      for (int i = 0; i < 8; i++)
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

      client.publish(DAQ_TOPIC, finalStr);

      Serial.println(finalStr);
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
  Serial.begin(115200);
  while (!Serial)
    delay(10);
  delay(2000);

  //RS Pin Setup
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, HIGH);


  //LED Driver Init
  Wire.begin(SDA, SCL); //hardcoded to pins 38, 39 --> SDA, SCL pins

  if (!ledDriver.begin_I2C(0x20, &Wire)){
    Serial.println("LED FAILED");
  }else {
    Serial.println("LED SUCCESS");


    for(int i = LED_RANGE_LOW; i <= LED_RANGE_HIGH; i++){
      ledDriver.pinMode(i, OUTPUT);
      ledDriver.digitalWrite(i, LOW);
    }
  }

  sensorQueue = xQueueCreate(1, sizeof(SensorData));
  spiMutex = xSemaphoreCreateMutex();

  sharedSPI.begin(SCLK, MISO, MOSI, -1);
  if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE)
  {
    loadCellADC.InitializeADC();
    loadCellADC.setPGA(PGA_64);
    loadCellADC.setDRATE(DRATE_1000SPS);
    pressureADC.begin(MISO, SCLK, MOSI, ADS8688_CS, 4.1, 0x05);
    pressureADC.setInputRange(ADS8688_CS, 0x05);
    xSemaphoreGive(spiMutex);
  }

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setKeepAlive(60);

  xTaskCreatePinnedToCore(samplingTask, "Sampling Task", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(publishTask, "Publish Task", 6144, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(mqttTask, "MQTT Task", 4096, NULL, 1, NULL, 0);

  Serial.println("Setup complete");
}

void loop() {}
