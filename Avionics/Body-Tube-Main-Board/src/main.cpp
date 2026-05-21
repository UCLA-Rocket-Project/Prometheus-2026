// ============================================================
// Body Tube Board – ADS8688 pressure-transducer logger
// ESP32-S3  |  PlatformIO  |  FreeRTOS
//
// Architecture:
//   ads_task – sample ADS8688 channels 2,3,4 → queue + UART TX
//   log_task – queue → SD card
//
// ADS8688 library behavior:
//   adc.readAllChannels(CS, true, rawVoltages);
//   rawVoltages[] are returned in mV because ADS8688.cpp does:
//      voltages[i] = convertToVoltage(result) * 1000;
//
// Logs only final calibrated PT values:
//   CH2 = PT0
//   CH3 = PT1
//   CH4 = PT2
//
// Calibration:
//   calibrated_PT = m * raw + b
//
// UART (rs485Serial / Serial2):
//   TX=11, RX=10, 115200 baud
//   Packet: "rocket_data pt0=X,pt1=X,pt2=X,uptime_ms=X\n"
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "ADS8688.h"
#include "pins.h"

// ============================================================
// UART to Transceiver Board
// ============================================================
#define UART_RX 10
#define UART_TX 11
HardwareSerial rs485Serial(2);

// ============================================================
// FreeRTOS tuning
// ============================================================
#define QUEUE_DEPTH 50
#define QUEUE_SEND_TIMEOUT (pdMS_TO_TICKS(10))
#define SD_RECEIVE_TIMEOUT portMAX_DELAY

#define WATCHDOG_PERIOD_TICKS (pdMS_TO_TICKS(1000))

#define TASK_STACK_ADS 4096
#define TASK_STACK_SD 8192

#define TASK_PRIO_ADS 2
#define TASK_PRIO_SD 2

// ============================================================
// ADS8688 constants
// ============================================================
#define ADC_MOSI 15
#define ADC_MISO 16
#define ADC_CLK 17

#define ADS8688_CS 6

#define ADS8688_VREF 4.1f
#define ADS8688_RANGE_CODE 0x05

#define PRESSURE_CHANNEL_COUNT 3
#define ADS8688_TOTAL_CHANNELS 8

// Only log ADS8688 channels 2, 3, and 4.
//   CH2 = PT0
//   CH3 = PT1
//   CH4 = PT2
const uint8_t PT_CHANNELS[PRESSURE_CHANNEL_COUNT] = {
    2, 3, 4};

// ============================================================
// PT calibration
// calibrated = m * raw + b
//
// raw is in mV (ADS8688.cpp multiplies by 1000).
// Indexed by PT number, not raw ADS8688 channel:
//   [0] → ADS8688 CH2 = PT0
//   [1] → ADS8688 CH3 = PT1
//   [2] → ADS8688 CH4 = PT2
// ============================================================
const float m[PRESSURE_CHANNEL_COUNT] = {
    0.19996646f, // PT0
    0.12304097f, // PT1
    0.12338010f  // PT2
};

const float b[PRESSURE_CHANNEL_COUNT] = {
    -212.79606628f, // PT0
    -136.33337402f, // PT1
    -137.97013855f  // PT2
};

// ============================================================
// Typed logging record
// ============================================================
enum class LogType : uint8_t
{
  PRESSURE = 0,
};

struct PressureData
{
  float calibrated[PRESSURE_CHANNEL_COUNT]; // final calibrated PT values only
};

struct LogRecord
{
  LogType type;
  uint32_t timestamp_us;

  union
  {
    PressureData pt;
  } data;
};

// ============================================================
// Peripherals
// ============================================================

// Your ADS8688.cpp uses its own internal SPIClass object named `spi`.
// So this must use the default constructor.
ADS8688 adc;

// SD card can still use HSPI.
SPIClass sdSpi(HSPI);

// ============================================================
// Shared state
// ============================================================
static QueueHandle_t logging_queue = nullptr;
static String currentFileName;
static volatile uint32_t droppedSamples = 0;

#define SD_RECORD_BUF 128

// ============================================================
// Forward declarations
// ============================================================
static inline void safe_pin_out(int pin, int val);
static inline void safe_pin_in(int pin);

static void task_ads(void *params);
static void task_sd(void *params);

static void print_reset_reason();
static void init_board_pins();
static void print_banner();
static String get_next_filename();
static bool sd_write(const char *path, const char *data, const char *mode);

static void ads8688_init();
static float calibrate_pt(uint8_t pt_index, float rawPT);

// ============================================================
// setup()
// ============================================================
void setup()
{
  Serial.begin(115200);
  rs485Serial.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  delay(100);

  tone(BUZZ, 3000);

  init_board_pins();
  print_banner();
  print_reset_reason();

  // ---- FreeRTOS queue ----
  logging_queue = xQueueCreate(QUEUE_DEPTH, sizeof(LogRecord));
  configASSERT(logging_queue && "Could not create logging queue");

  // ---- SD card ----
  sdSpi.begin(SPI2_SCK, SPI2_MISO, SPI2_MOSI, SPI2_CS);

  unsigned long sd_start = millis();
  bool sd_ok = false;

  while (!sd_ok && millis() - sd_start < 10000)
  {
    sd_ok = SD.begin(SPI2_CS, sdSpi);

    if (!sd_ok)
    {
      Serial.println("[SD] Waiting for card...");
      delay(1000);
    }
  }

  if (!sd_ok)
  {
    Serial.println("[SD] Init failed after 10 s - restarting");
    esp_restart();
  }

  currentFileName = get_next_filename();
  Serial.println("[SD] File: " + currentFileName);

  sd_write(
      currentFileName.c_str(),
      "timestamp_us,"
      "PT0_calibrated,"
      "PT1_calibrated,"
      "PT2_calibrated\n",
      FILE_WRITE);

  Serial.println("[SD] Init done");

  // ---- ADS8688 ----
  Serial.println("[ADS8688] Initializing...");
  ads8688_init();
  Serial.println("[ADS8688] Init done; logging calibrated PT0, PT1, PT2");

  safe_pin_out(LED1, HIGH);
  delay(500);
  safe_pin_out(LED1, LOW);

  // ---- Launch FreeRTOS tasks ----
  xTaskCreatePinnedToCore(
      task_sd,
      "SD_log",
      TASK_STACK_SD,
      nullptr,
      TASK_PRIO_SD,
      nullptr,
      PRO_CPU_NUM);

  xTaskCreatePinnedToCore(
      task_ads,
      "ADS_read",
      TASK_STACK_ADS,
      nullptr,
      TASK_PRIO_ADS,
      nullptr,
      APP_CPU_NUM);

  Serial.println("[MAIN] Tasks launched - acquisition running");

  noTone(BUZZ);
}

void loop()
{
  vTaskDelay(portMAX_DELAY);
}

// ============================================================
// ADS8688 acquisition task
// ============================================================
static void task_ads(void * /*params*/)
{
  Serial.println("[ADS8688 task] started");

  TickType_t last_yield = xTaskGetTickCount();

  float rawVoltages[ADS8688_TOTAL_CHANNELS];

  for (;;)
  {
    LogRecord rec;
    rec.type = LogType::PRESSURE;
    rec.timestamp_us = (uint32_t)micros();

    // Your ADS8688.cpp reads all channels and returns mV.
    adc.readAllChannels(ADS8688_CS, true, rawVoltages);

    for (uint8_t i = 0; i < PRESSURE_CHANNEL_COUNT; ++i)
    {
      uint8_t adc_ch = PT_CHANNELS[i];

      // Direct raw PT value from ADS8688.
      // NOTE: this is in mV because ADS8688.cpp multiplies by 1000.
      float rawPT = rawVoltages[adc_ch];

      // Final calibrated PT output.
      float calibrated = calibrate_pt(i, rawPT);

      rec.data.pt.calibrated[i] = calibrated;
    }

    if (xQueueSendToBack(logging_queue, &rec, QUEUE_SEND_TIMEOUT) != pdTRUE)
    {
      droppedSamples++;
    }

    // Send to transceiver board over UART
    char uartBuf[128];
    snprintf(uartBuf, sizeof(uartBuf),
             "rocket_data pt0=%.4f,pt1=%.4f,pt2=%.4f,uptime_ms=%lu",
             rec.data.pt.calibrated[0],
             rec.data.pt.calibrated[1],
             rec.data.pt.calibrated[2],
             (unsigned long)millis());
    rs485Serial.println(uartBuf);

#define DEBUG_PRINT_MS 100
#if DEBUG_PRINT_MS > 0
    static TickType_t last_print = 0;
    TickType_t now_ticks = xTaskGetTickCount();

    if (now_ticks - last_print >= pdMS_TO_TICKS(DEBUG_PRINT_MS))
    {
      last_print = now_ticks;

      Serial.printf("[PT] t=%lu us", (unsigned long)rec.timestamp_us);

      for (uint8_t i = 0; i < PRESSURE_CHANNEL_COUNT; ++i)
      {
        Serial.printf(
            "  PT%u=%.4f",
            i,
            rec.data.pt.calibrated[i]);
      }

      Serial.println();
    }
#endif

    // Slow acquisition slightly so SD can keep up during testing.
    vTaskDelay(pdMS_TO_TICKS(5));

    if (xTaskGetTickCount() - last_yield >= WATCHDOG_PERIOD_TICKS)
    {
      vTaskDelay(pdMS_TO_TICKS(2));
      last_yield = xTaskGetTickCount();
    }
  }
}

// ============================================================
// SD write task
// ============================================================
#define FLUSH_SYNC_MS 2000

static void task_sd(void * /*params*/)
{
  Serial.println("[SD task] started");

  File logFile = SD.open(currentFileName.c_str(), FILE_APPEND);
  configASSERT(logFile && "SD task: could not open log file");

  Serial.printf(
      "[SD task] file opened, size=%lu bytes\n",
      (unsigned long)logFile.size());

  TickType_t last_sync = xTaskGetTickCount();
  TickType_t last_yield = xTaskGetTickCount();

  uint32_t totalWritten = 0;
  char line[SD_RECORD_BUF];

  for (;;)
  {
    LogRecord rec;

    if (xQueueReceive(logging_queue, &rec, pdMS_TO_TICKS(50)) == pdTRUE)
    {
      if (rec.type == LogType::PRESSURE)
      {
        int len = snprintf(
            line,
            sizeof(line),
            "%lu",
            (unsigned long)rec.timestamp_us);

        for (
            uint8_t i = 0;
            i < PRESSURE_CHANNEL_COUNT && len > 0 && len < (int)sizeof(line);
            ++i)
        {
          int written = snprintf(
              line + len,
              sizeof(line) - len,
              ",%.4f",
              rec.data.pt.calibrated[i]);

          if (written < 0)
          {
            len = -1;
            break;
          }

          len += written;
        }

        if (len > 0 && len < (int)sizeof(line) - 1)
        {
          line[len++] = '\n';
          line[len] = '\0';

          logFile.write((const uint8_t *)line, len);
          totalWritten++;
        }
      }
    }

    TickType_t now = xTaskGetTickCount();

    if (now - last_sync >= pdMS_TO_TICKS(FLUSH_SYNC_MS))
    {
      logFile.flush();
      last_sync = now;

      uint32_t dropped = droppedSamples;
      droppedSamples = 0;

      Serial.printf(
          "[SD] written=%lu  dropped=%lu  queueUsed=%u  fileSize=%lu\n",
          (unsigned long)totalWritten,
          (unsigned long)dropped,
          (unsigned)uxQueueMessagesWaiting(logging_queue),
          (unsigned long)logFile.size());
    }

    if (now - last_yield >= WATCHDOG_PERIOD_TICKS)
    {
      vTaskDelay(pdMS_TO_TICKS(2));
      last_yield = xTaskGetTickCount();
    }
  }
}

// ============================================================
// ADS8688 init
// Compatible with your ADS8688.cpp
// ============================================================
static void ads8688_init()
{
  delay(10);

  adc.begin(
      ADC_MISO,
      ADC_CLK,
      ADC_MOSI,
      ADS8688_CS,
      ADS8688_VREF,
      ADS8688_RANGE_CODE);

  adc.setInputRange(ADS8688_CS, ADS8688_RANGE_CODE);

  delay(10);

  float warmup[ADS8688_TOTAL_CHANNELS];
  adc.readAllChannels(ADS8688_CS, true, warmup);
}

// ============================================================
// PT calibration
// ============================================================
static float calibrate_pt(uint8_t pt_index, float rawPT)
{
  if (pt_index >= PRESSURE_CHANNEL_COUNT)
  {
    return rawPT;
  }

  return m[pt_index] * rawPT + b[pt_index];
}

// ============================================================
// Helpers
// ============================================================
static void print_reset_reason()
{
  esp_reset_reason_t r = esp_reset_reason();

  switch (r)
  {
  case ESP_RST_POWERON:
    Serial.println("[BOOT] Power-on reset");
    break;

  case ESP_RST_EXT:
    Serial.println("[BOOT] External reset");
    break;

  case ESP_RST_SW:
    Serial.println("[BOOT] Software reset");
    break;

  case ESP_RST_PANIC:
    Serial.println("[BOOT] Software panic");
    break;

  case ESP_RST_INT_WDT:
    Serial.println("[BOOT] Interrupt watchdog");
    break;

  case ESP_RST_TASK_WDT:
    Serial.println("[BOOT] Task watchdog");
    break;

  case ESP_RST_WDT:
    Serial.println("[BOOT] Other watchdog");
    break;

  case ESP_RST_DEEPSLEEP:
    Serial.println("[BOOT] Woke from deep sleep");
    break;

  case ESP_RST_BROWNOUT:
    Serial.println("[BOOT] Brownout reset");
    break;

  case ESP_RST_SDIO:
    Serial.println("[BOOT] SDIO reset");
    break;

  default:
    Serial.printf("[BOOT] Unknown reset (%d)\n", r);
    break;
  }
}

static void print_banner()
{
  Serial.println();
  Serial.println("======================================");
  Serial.println("=== ADS8688 PRESSURE LOGGER BUILD ===");
  Serial.println("Body Tube Board ADS8688 PlatformIO");
  Serial.printf(
      "ESP32-S3  ADS8688_CS=%d  SCK=%d  MISO=%d  MOSI=%d\n",
      ADS8688_CS,
      ADC_CLK,
      ADC_MISO,
      ADC_MOSI);
  Serial.println("Logging calibrated PT0, PT1, PT2 only");
  Serial.println("ADS8688 library returns rawPT in mV");
  Serial.println("Calibration: calibrated_PT = m * rawPT + b");
  Serial.println("======================================");
}

static inline void safe_pin_out(int pin, int val)
{
  if (pin < 0 || pin >= GPIO_NUM_MAX)
  {
    Serial.printf("[PINS] Skipping invalid pin %d, check pins.h\n", pin);
    return;
  }

  pinMode(pin, OUTPUT);
  digitalWrite(pin, val);
}

static inline void safe_pin_in(int pin)
{
  if (pin < 0 || pin >= GPIO_NUM_MAX)
  {
    Serial.printf("[PINS] Skipping invalid pin %d, check pins.h\n", pin);
    return;
  }

  pinMode(pin, INPUT);
}

static void init_board_pins()
{
  Serial.printf("[PINS] LED1=%d  LED2=%d  BUZZ=%d\n", LED1, LED2, BUZZ);
  Serial.printf(
      "[PINS] ADS8688_CS=%d  SPI4_CS=%d  SPI4_CS2=%d\n",
      ADS8688_CS,
      SPI4_CS,
      SPI4_CS2);
  Serial.printf(
      "[PINS] SPI2 SCK=%d MISO=%d MOSI=%d CS=%d\n",
      SPI2_SCK,
      SPI2_MISO,
      SPI2_MOSI,
      SPI2_CS);

  safe_pin_out(LED1, LOW);
  safe_pin_out(LED2, LOW);
  safe_pin_out(BUZZ, LOW);

  safe_pin_out(ADS8688_CS, HIGH);
  safe_pin_out(SPI4_CS2, HIGH);
}

static String get_next_filename()
{
  for (int i = 0; i < 1000; ++i)
  {
    String path = "/launch" + String(i) + ".csv";

    if (!SD.exists(path))
    {
      return path;
    }
  }

  return "/launch_overflow.csv";
}

static bool sd_write(const char *path, const char *data, const char *mode)
{
  File f = SD.open(path, mode);

  if (!f)
  {
    return false;
  }

  bool ok = f.print(data);
  f.close();

  return ok;
}
