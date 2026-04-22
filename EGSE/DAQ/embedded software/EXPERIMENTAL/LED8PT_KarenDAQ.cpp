#include <Arduino.h>
#include <ADS1256.h>
#include "ADS8688.h"
#include <SPI.h>
#include <string>
#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "driver/gpio.h"
#include "Wire.h"
#include "Adafruit_MCP23X17.h"

// Load Cell (ADS1256) SPI Pins
#define ADS1256_MOSI 35
#define ADS1256_MISO 37
#define ADS1256_SCLK 36

#define ADS1256_CS   7
#define ADS1256_DRDY 4

#define ADS8688_CS   33

// LED indicator pin
#define LED 90
#define RO_PIN 40
#define DI_PIN 39
#define DE_RE_PIN 41
#define LED0 0 //PT4
#define LED1 1 //PT1
#define LED2 2 //PT0
#define LED3 3 //PT7
#define LED4 4 //PT2
#define LED5 5 //PT3
#define LED6 6 //PT5
#define LED7 7 //PT6
#define LED8 8 //LC0
#define LED9 9 //LC1
#define LED10 10 //LC2

#define SDA 38
#define SCL 39

// ─── DEBUG CONFIG ──────────────────────────────────────────────────────────────
#define DEBUG_WIFI        true   // WiFi connection events
#define DEBUG_MQTT        true   // MQTT connection + publish events
#define DEBUG_SPI         true   // SPI init success/failure
#define DEBUG_SAMPLING    true   // Per-sample values (can be noisy at 1kHz — set false once working)
#define DEBUG_DRDY        true   // DRDY timeout warnings
#define DEBUG_TASKS       true   // FreeRTOS task lifecycle
#define DEBUG_QUEUE       true   // Queue overwrite/peek events
#define DEBUG_CALIBRATION true   // Calibrated output values
#define DEBUG_HEARTBEAT   true   // Periodic alive print every N ms
#define HEARTBEAT_INTERVAL_MS 2000

// Throttle per-sample prints to avoid flooding serial (print every Nth sample)
#define SAMPLE_PRINT_EVERY 500  // print 1 out of every 500 samples (~2/sec at 1kHz)

#define DPRINT(tag, fmt, ...) Serial.printf("[DEBUG-%s] " fmt "\n", tag, ##__VA_ARGS__)
// ──────────────────────────────────────────────────────────────────────────────

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
const unsigned long PUBLISH_INTERVAL = 5; //increased published interval (might wanna go to 10+), u can put it back to 1 if u want, just testing the increased payload size

// WiFi + MQTT credentials
const char *ssid = "ILAY";
const char *password = "lebronpookie123";
const char *mqtt_server = "192.168.0.100";
const char *DAQ_TOPIC = "DAQ_transmitter/receiver";

WiFiClient espClient;
PubSubClient client(espClient);

SPIClass sharedSPI(FSPI);
ADS1256 loadCellADC(&sharedSPI, ADS1256_DRDY, ADS1256_CS, 2.5);
ADS8688 pressureADC;

Adafruit_MCP23X17 ledDriver;
bool ptCheck[8];

// Calibration coefficients
float calibrationA1 = -57938.14;
float calibrationB1 = 1.16948;
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

// ─── STATS COUNTERS ────────────────────────────────────────────────────────────
static uint32_t stat_sampleCount     = 0;
static uint32_t stat_drdyTimeouts    = 0;
static uint32_t stat_mutexFails      = 0;
static uint32_t stat_publishCount    = 0;
static uint32_t stat_publishFails    = 0;
static uint32_t stat_mqttDrops       = 0;
// ──────────────────────────────────────────────────────────────────────────────

void setup_wifi()
{
  delay(10);
  if (DEBUG_WIFI) DPRINT("WIFI", "Connecting to SSID: %s", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    attempts++;
    if (DEBUG_WIFI) DPRINT("WIFI", "Still connecting... attempt %d (status=%d)", attempts, WiFi.status());
    if (attempts > 40) {
      if (DEBUG_WIFI) DPRINT("WIFI", "ERROR: Could not connect after %d attempts. Check SSID/password.", attempts);
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setSleep(false);
    if (DEBUG_WIFI) DPRINT("WIFI", "Connected! IP=%s  RSSI=%d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    if (DEBUG_WIFI) DPRINT("WIFI", "FAILED to connect. WiFi status=%d", WiFi.status());
  }
}

void connect_client()
{
  if (client.connected()) return;

  if (WiFi.status() != WL_CONNECTED) {
    if (DEBUG_MQTT) DPRINT("MQTT", "Skipping connect — WiFi not up (status=%d)", WiFi.status());
    return;
  }

  if (millis() - last_mqtt_attempt < MQTT_RETRY_INTERVAL) return;
  last_mqtt_attempt = millis();

  String cid = "DAQ_TX-" + WiFi.macAddress();
  if (DEBUG_MQTT) DPRINT("MQTT", "Attempting connect to %s:1883 as clientId=%s", mqtt_server, cid.c_str());

  if (client.connect(cid.c_str()))
  {
    if (DEBUG_MQTT) DPRINT("MQTT", "Connected to broker OK");
  }
  else
  {
    int rc = client.state();
    if (DEBUG_MQTT) DPRINT("MQTT", "Connect FAILED. PubSubClient state=%d "
      "(−4=conn timeout, −3=conn lost, −2=connect failed, −1=disconnected, "
      "1=bad protocol, 2=bad clientId, 3=server unavailable, 4=bad creds, 5=unauthorized)", rc);
    stat_mqttDrops++;
  }
}

float getCalibratedValue(float m, float b, float raw)
{
  return (raw - b) / m;
}
float mValues[8] = {5.077578, 5.073776, 5.042717, 8.125911, 0, 0, 0, 0}; //needs the calibration values for the other 4
float bValues[8] = {1081.752319, 1063.822266, 1080.457153, 1139.25354, 0, 0, 0, 0};

bool waitForDRDY(uint32_t timeout_us = 2000)
{
  uint32_t start = micros();
  while (digitalRead(ADS1256_DRDY))
  {
    if (micros() - start > timeout_us)
    {
      stat_drdyTimeouts++;
      if (DEBUG_DRDY) DPRINT("DRDY", "TIMEOUT after %u us — total timeouts so far: %u", timeout_us, stat_drdyTimeouts);
      return false;
    }
  }
  return true;
}

bool isValidLCVoltage(float v) {
  return isfinite(v) && v > 0.1f && v < 7.5f;
}

bool isValidPTVoltage(float v) {
  return isfinite(v) && v > 0.1f && v < 7.5f;
}

void testBlink(uint8_t pin) {
  ledDriver.digitalWrite(pin, HIGH);
  delay(500);
  ledDriver.digitalWrite(pin, LOW);
  delay(90);
}

void samplingTask(void *pvParameters)
{
  if (DEBUG_TASKS) DPRINT("TASK", "samplingTask started on core %d", xPortGetCoreID());
  TickType_t lastWakeTime = xTaskGetTickCount();

  float ptVoltages[8];
  float ptCalibrated[8];
  uint32_t localSampleCount = 0;

  while (true)
  {
    float lcCalibrated1 = NAN;
    float lcCalibrated2 = NAN;
    bool lc1Valid = false;
    bool lc2Valid = false;
    bool anyPTValid = false;
    bool validSample = false;

    int32_t rawADC1 = 0;
    int32_t rawADC2 = 0;
    float lcVoltage1 = NAN;
    float lcVoltage2 = NAN;

    for (int i = 0; i < 8; i++) {
      ptVoltages[i] = NAN;
      ptCalibrated[i] = NAN;
      ptCheck[i] = false;
    }

    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE)
    {
      pressureADC.readAllChannels(ADS8688_CS, true, ptVoltages);
      delayMicroseconds(5);

      for (int i = 0; i < 8; i++) {
        ptCheck[i] = isValidPTVoltage(ptVoltages[i]);
        if (ptCheck[i]) {
          anyPTValid = true;
        }
      }

      if (waitForDRDY())
      {
        rawADC1 = loadCellADC.readDifferentialFaster(DIFF_0_1);
        lcVoltage1 = loadCellADC.convertToVoltage(rawADC1);

        lc1Valid = isValidLCVoltage(lcVoltage1);
        if (lc1Valid) {
          lcCalibrated1 = convertToWeightLC1(lcVoltage1);
        } else {
          if ((localSampleCount % SAMPLE_PRINT_EVERY) == 0) {
            DPRINT("SENSOR", "LC1 missing or invalid lcVoltage1=%.6f", lcVoltage1);
          }
        }
      }

      if (waitForDRDY())
      {
        rawADC2 = loadCellADC.readDifferentialFaster(DIFF_2_3);
        lcVoltage2 = loadCellADC.convertToVoltage(rawADC2);

        lc2Valid = isValidLCVoltage(lcVoltage2);
        if (lc2Valid) {
          lcCalibrated2 = convertToWeightLC2(lcVoltage2);
        } else {
          if (((localSampleCount % SAMPLE_PRINT_EVERY) == 0)) {
            DPRINT("SENSOR", "LC2 missing or invalid lcVoltage2=%.6f", lcVoltage2);
          }
        }
      }

      for (int i = 0; i < 8; i++) {
        if (!ptCheck[i]) {
          ptCalibrated[i] = NAN;
          if ((localSampleCount % SAMPLE_PRINT_EVERY) == 0) {
            DPRINT("SENSOR", "PT%d missing or invalid rawV=%.4f", i, ptVoltages[i]);
          }
        }
        else if (mValues[i] == 0.0f) {
          ptCalibrated[i] = NAN;
          if ((localSampleCount % SAMPLE_PRINT_EVERY) == 0) {
            DPRINT("SENSOR", "PT%d valid but no calibration loaded", i); //so it doesn't spam the serial monitor output
          }
        }
        else {
          ptCalibrated[i] = getCalibratedValue(mValues[i], bValues[i], ptVoltages[i]);
        }
      }

      localSampleCount++;
      stat_sampleCount++;

      if (DEBUG_SAMPLING && (localSampleCount % SAMPLE_PRINT_EVERY == 0))
      {
        DPRINT("SAMPLE",
          "#%lu | rawADC1=%ld lcVoltage1=%.6fV lcCal1=%.4f | rawADC2=%ld lcVoltage2=%.6fV lcCal2=%.4f",
          (unsigned long)localSampleCount,
          (long)rawADC1, lcVoltage1, lcCalibrated1,
          (long)rawADC2, lcVoltage2, lcCalibrated2);

        DPRINT("SAMPLE",
          "PT_raw[0..7]: %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f",
          ptVoltages[0], ptVoltages[1], ptVoltages[2], ptVoltages[3],
          ptVoltages[4], ptVoltages[5], ptVoltages[6], ptVoltages[7]);
      }

      xSemaphoreGive(spiMutex);
    }
    else
    {
      stat_mutexFails++;
      if (DEBUG_TASKS) DPRINT("MUTEX", "samplingTask failed to take spiMutex (total fails: %u)", stat_mutexFails);
    }

    validSample = anyPTValid || lc1Valid || lc2Valid;

    if (validSample)
    {
      SensorData newData;
      for (int i = 0; i < 8; i++) {
        newData.pt[i] = ptCalibrated[i];
      }

      newData.lc[0] = lcCalibrated1;
      newData.lc[1] = lcCalibrated2;
      newData.timestamp = millis();

      if (DEBUG_CALIBRATION && (localSampleCount % SAMPLE_PRINT_EVERY == 0))
      {
        DPRINT("CALIB", "PT_cal: %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f | LC1=%.4f LC2=%.4f | t=%lums",
          newData.pt[0], newData.pt[1], newData.pt[2], newData.pt[3],
          newData.pt[4], newData.pt[5], newData.pt[6], newData.pt[7],
          newData.lc[0], newData.lc[1], (unsigned long)newData.timestamp);
      }

      xQueueOverwrite(sensorQueue, &newData);

      if (DEBUG_QUEUE && (localSampleCount % SAMPLE_PRINT_EVERY == 0)) {
        DPRINT("QUEUE", "Overwrote queue at sample #%u", localSampleCount);
      }
    }

    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(SAMPLE_INTERVAL));
  }
}

void publishTask(void *pvParameters)
{
  if (DEBUG_TASKS) DPRINT("TASK", "publishTask started on core %d", xPortGetCoreID());
  char finalStr[300];
  SensorData localCopy;
  uint32_t localPublishCount = 0;

  while (true)
  {
    if (!client.connected())
    {
      if (DEBUG_MQTT && (localPublishCount % 100 == 0))
        DPRINT("MQTT", "publishTask: client not connected — skipping publish. State=%d", client.state());
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    if (xQueuePeek(sensorQueue, &localCopy, 0) == pdTRUE)
    {
      snprintf(
          finalStr,
          sizeof(finalStr),
          "rocket_data pt0=%4.10f,pt1=%4.10f,pt2=%4.10f,pt3=%4.10f,pt4=%4.10f,pt5=%4.10f,pt6=%4.10f,pt7=%4.10f,lc0=%4.10f,lc1=%4.10f,uptime_ms=%lu",
          localCopy.pt[0], localCopy.pt[1], localCopy.pt[2], localCopy.pt[3], localCopy.pt[4], localCopy.pt[5], localCopy.pt[6], localCopy.pt[7],
          localCopy.lc[0], localCopy.lc[1], localCopy.timestamp);

      bool ok = client.publish(DAQ_TOPIC, finalStr);
      localPublishCount++;
      stat_publishCount++;

      if (!ok) {
        stat_publishFails++;
        if (DEBUG_MQTT) DPRINT("MQTT", "publish() FAILED (total fails: %u). msg len=%d. State=%d",
          stat_publishFails, strlen(finalStr), client.state());
      } else if (DEBUG_MQTT && (localPublishCount % 500 == 0)) {
        DPRINT("MQTT", "publish OK #%u → topic=%s  payload_len=%d",
          localPublishCount, DAQ_TOPIC, strlen(finalStr));
      }
    }
    else
    {
      if (DEBUG_QUEUE) DPRINT("QUEUE", "publishTask: queue empty (peek returned false)");
    }

    vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL));
  }
}

void mqttTask(void *pvParameters)
{
  if (DEBUG_TASKS) DPRINT("TASK", "mqttTask started on core %d", xPortGetCoreID());
  uint32_t loopCount = 0;
  unsigned long lastHeartbeat = 0;

  while (true)
  {
    // WiFi watchdog
    if (WiFi.status() != WL_CONNECTED)
    {
      if (DEBUG_WIFI) DPRINT("WIFI", "mqttTask: WiFi dropped (status=%d) — reconnecting...", WiFi.status());
      WiFi.begin(ssid, password);
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    connect_client();
    client.loop();
    loopCount++;

    // Periodic heartbeat
    if (DEBUG_HEARTBEAT && (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL_MS))
    {
      lastHeartbeat = millis();
      DPRINT("HEARTBEAT", "uptime=%lus  WiFi=%s  RSSI=%d  MQTT=%s  "
        "samples=%u  drdyTO=%u  pubOK=%u  pubFail=%u  mqttDrops=%u",
        millis() / 1000,
        WiFi.status() == WL_CONNECTED ? "UP" : "DOWN",
        WiFi.RSSI(),
        client.connected() ? "CONN" : "DISC",
        stat_sampleCount, stat_drdyTimeouts,
        stat_publishCount, stat_publishFails,
        stat_mqttDrops);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup()
{
  // Force CS HIGH via low-level IDF driver ASAP — before Arduino framework
  // or flash controller can interfere with GPIO7
  gpio_config_t cs_conf = {};
  cs_conf.pin_bit_mask = (1ULL << ADS1256_CS);
  cs_conf.mode = GPIO_MODE_OUTPUT;
  cs_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  cs_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cs_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&cs_conf);
  gpio_set_level(GPIO_NUM_7, 1);
  // CS is now held HIGH before anything else runs

  Wire.begin(SDA, SCL); //hardcoded to pins 38, 39 --> SDA, SCL pins
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, HIGH);

  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(2000);

  DPRINT("BOOT", "=== DAQ firmware starting ===");
  DPRINT("BOOT", "CS GPIO%d forced HIGH via gpio_config before framework init", ADS1256_CS);
  DPRINT("BOOT", "ESP32 chip: %s  cores: %d  freq: %d MHz",
    ESP.getChipModel(), ESP.getChipCores(), ESP.getCpuFreqMHz());
  DPRINT("BOOT", "Free heap at boot: %u bytes", ESP.getFreeHeap());

  // pinMode(LED, OUTPUT);
  // digitalWrite(LED, HIGH);
  // if (DEBUG_TASKS) DPRINT("BOOT", "LED pin %d set HIGH", LED);

  // FreeRTOS primitives
  sensorQueue = xQueueCreate(1, sizeof(SensorData));
  spiMutex = xSemaphoreCreateMutex();
  if (sensorQueue == NULL) DPRINT("BOOT", "ERROR: sensorQueue creation FAILED");
  else DPRINT("BOOT", "sensorQueue created OK (depth=1, item=%d bytes)", sizeof(SensorData));
  if (spiMutex == NULL) DPRINT("BOOT", "ERROR: spiMutex creation FAILED");
  else DPRINT("BOOT", "spiMutex created OK");

  // SPI bus
  DPRINT("SPI", "Starting FSPI: SCLK=%d MISO=%d MOSI=%d", ADS1256_SCLK, ADS1256_MISO, ADS1256_MOSI);
  sharedSPI.begin(ADS1256_SCLK, ADS1256_MISO, ADS1256_MOSI, -1);
  DPRINT("SPI", "sharedSPI.begin() returned");

  if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE)
  {
    // ADS1256 init — DRDY pre-check before calling InitializeADC (which blocks on DRDY)
    DPRINT("SPI", "Initializing ADS1256 (CS=%d, DRDY=%d)...", ADS1256_CS, ADS1256_DRDY);
    pinMode(ADS1256_DRDY, INPUT);
    DPRINT("SPI", "DRDY pin %d state before init: %s",
      ADS1256_DRDY, digitalRead(ADS1256_DRDY) ? "HIGH (may hang — check wiring/power)" : "LOW (ok)");

    // Cap SPI clock to 1 MHz — ADS1256 max is 1.92 MHz; faster clocks cause silent misreads
    sharedSPI.setFrequency(1000000);
    sharedSPI.setDataMode(SPI_MODE1);
    sharedSPI.setBitOrder(MSBFIRST);
    DPRINT("SPI", "SPI frequency set to 1 MHz, Mode1, MSBFIRST");

    // Manually send RESET command (0xFE) before InitializeADC
    // This breaks the deadlock: library waits for DRDY, but chip needs a reset
    // command before it will toggle DRDY for the first time
    DPRINT("SPI", "Sending manual RESET command (0xFE) to kick ADS1256 out of idle...");
    pinMode(ADS1256_CS, OUTPUT);
    digitalWrite(ADS1256_CS, LOW);
    delayMicroseconds(10);
    sharedSPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
    sharedSPI.transfer(0xFE);
    sharedSPI.endTransaction();
    digitalWrite(ADS1256_CS, HIGH);
    DPRINT("SPI", "RESET command sent, waiting 100ms for chip to recover...");
    delay(100);

    // Now wait for DRDY — chip should be converting after reset
    {
      uint32_t t = millis();
      bool drdyOk = false;
      while (millis() - t < 3000) {
        if (digitalRead(ADS1256_DRDY) == LOW) { drdyOk = true; break; }
        delay(1);
      }
      if (!drdyOk) {
        DPRINT("SPI", "ERROR: DRDY still stuck HIGH after manual reset + 3s");
        DPRINT("SPI", "  SPI reset command not reaching chip — check MOSI/SCLK/CS traces");
      } else {
        DPRINT("SPI", "DRDY went LOW after %lu ms — ADS1256 responding, proceeding", millis() - t);
      }
    }

    loadCellADC.InitializeADC();
  
    DPRINT("SPI", "ADS1256 InitializeADC() done");
    loadCellADC.setPGA(PGA_64);
    DPRINT("SPI", "ADS1256 PGA set to 64");
    loadCellADC.setDRATE(DRATE_1000SPS);
    DPRINT("SPI", "ADS1256 DRATE set to 1000 SPS");

    // ADS8688 init
    DPRINT("SPI", "Initializing ADS8688 (CS=%d)...", ADS8688_CS);
    pressureADC.begin(ADS1256_MISO, ADS1256_SCLK, ADS1256_MOSI, ADS8688_CS, 4.1, 0x05);
    DPRINT("SPI", "ADS8688 begin() done");

    pressureADC.setInputRange(ADS8688_CS, 0x05);
    DPRINT("SPI", "ADS8688 setInputRange(0x05) done");

    xSemaphoreGive(spiMutex);
    DPRINT("SPI", "SPI mutex released after init");
  }
  else
  {
    DPRINT("SPI", "ERROR: Could not take spiMutex during setup — ADC init SKIPPED");
  }

  // WiFi + MQTT
  setup_wifi();

  DPRINT("MQTT", "Configuring MQTT server: %s:1883", mqtt_server);
  client.setServer(mqtt_server, 1883);
  client.setKeepAlive(60);
  DPRINT("MQTT", "MQTT keepAlive=60s");
  client.setBufferSize(512); //could go up to 1024
  DPRINT("MQTT", "buffer=%u", client.getBufferSize());

if (!ledDriver.begin_I2C(0x20, &Wire))
  DPRINT("LEDdriver", "FAILED");
else
  DPRINT("LEDdriver", "SUCCESS");

ledDriver.pinMode(LED0, OUTPUT);
ledDriver.pinMode(LED1, OUTPUT);
ledDriver.pinMode(LED2, OUTPUT);
ledDriver.pinMode(LED3, OUTPUT);
ledDriver.pinMode(LED4, OUTPUT);
ledDriver.pinMode(LED5, OUTPUT);
ledDriver.pinMode(LED6, OUTPUT);
ledDriver.pinMode(LED7, OUTPUT);
ledDriver.pinMode(LED8, OUTPUT);
ledDriver.pinMode(LED9, OUTPUT);
ledDriver.pinMode(LED10, OUTPUT);

testBlink(LED0);
testBlink(LED1);
testBlink(LED2);
testBlink(LED3);
testBlink(LED4);
testBlink(LED5);
testBlink(LED6);
testBlink(LED7);

  // Spawn tasks
  BaseType_t r;
  r = xTaskCreatePinnedToCore(samplingTask, "Sampling Task", 4096, NULL, 3, NULL, 1);
  DPRINT("TASKS", "samplingTask create: %s (priority=3, core=1, stack=4096)", r == pdPASS ? "OK" : "FAILED");

  r = xTaskCreatePinnedToCore(publishTask, "Publish Task", 4096, NULL, 2, NULL, 1);
  DPRINT("TASKS", "publishTask create: %s (priority=2, core=1, stack=4096)", r == pdPASS ? "OK" : "FAILED");

  r = xTaskCreatePinnedToCore(mqttTask, "MQTT Task", 4096, NULL, 1, NULL, 0);
  DPRINT("TASKS", "mqttTask create: %s (priority=1, core=0, stack=4096)", r == pdPASS ? "OK" : "FAILED");

  DPRINT("BOOT", "=== Setup complete. Free heap: %u bytes ===", ESP.getFreeHeap());


}

void loop() {}