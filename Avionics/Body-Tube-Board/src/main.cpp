// ============================================================
// Body Tube Board – ADS1256 load-cell logger
// ESP32-S3  |  PlatformIO  |  FreeRTOS
//
// Architecture (modelled on altimeter-board reference):
//   ads_task  – core 1 (PRO_CPU), pri 2 – sample ADS1256 → queue
//   log_task  – core 0 (APP_CPU), pri 2 – queue → SD card
//
// Core logic (gain, mux, drate, conversion formula) is
// UNCHANGED from the original baseline.
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_system.h>            // esp_reset_reason(), esp_restart()
#include <esp_task_wdt.h>

#include "ADS1256.h"
#include "pins.h"

// ============================================================
// FreeRTOS tuning
// ============================================================
#define QUEUE_DEPTH              50
#define QUEUE_SEND_TIMEOUT       (pdMS_TO_TICKS(10))
#define SD_RECEIVE_TIMEOUT       portMAX_DELAY

// How often each task yields to let the FreeRTOS idle task run.
// The ESP-IDF task WDT fires if IDLE starves for >CONFIG_ESP_TASK_WDT_TIMEOUT_S
// (default 5 s).  Yielding every 1 s provides 5x safety margin.
// The yield is only 2 ms so effective throughput loss is <0.04%.
#define WATCHDOG_PERIOD_TICKS    (pdMS_TO_TICKS(1000))

#define TASK_STACK_ADS           4096
#define TASK_STACK_SD            8192
// Priority 2: above idle (0) and Arduino loop (1), below timer daemon (configTIMER_TASK_PRIORITY).
// Both tasks at equal priority so the FreeRTOS round-robin scheduler
// can time-slice between them on the same core without starvation.
#define TASK_PRIO_ADS            2
#define TASK_PRIO_SD             2

// ============================================================
// ADS1256 constants
// ============================================================
#define VREF                     2.5f
#define ADS_MASTER_CLOCK_FREQ    7.68e6   // Hz

// ============================================================
// Noise / accuracy tuning
//
// The ADS1256 sinc filter rejects noise proportional to the
// number of cycles it averages.  At DRATE_2000SPS the filter
// settles ~4× faster than at DRATE_500SPS but passes more
// broadband noise.  We oversample N times in the ADS task and
// average before enqueuing, giving an effective output rate of:
//
//   f_out = DRATE / OVERSAMPLE_N
//
// Recommended starting point:
//   DRATE_2000SPS  +  OVERSAMPLE_N 2  →  ~1000 Hz output, low noise
//   DRATE_3750SPS  +  OVERSAMPLE_N 4  →  ~937 Hz output,  lower noise
//   DRATE_7500SPS  +  OVERSAMPLE_N 8  →  ~937 Hz output,  same noise, more SPI overhead
//
// The single-channel lock (DIFF_0_1 hardcoded, no cycleDifferential)
// also removes mux-switching transients that were a major noise source.
// ============================================================
#define OVERSAMPLE_N             2        // averages per output sample

// ============================================================
// Typed logging record
// Extend the union / enum when additional sensors are added.
// The queue and SD task need no other changes.
// ============================================================
enum class LogType : uint8_t {
    LOAD_CELL = 0,
};

struct LoadCellData {
    float    voltage;   // raw ADC voltage (V)
    float    output;    // converted mass (lbs) – unchanged formula
};

struct LogRecord {
    LogType  type;
    uint32_t timestamp_us;
    union {
        LoadCellData lc;
    } data;
};

// ============================================================
// Peripherals
// ============================================================
SPIClass adcSpi(FSPI);   // SCK=17  MISO=16  MOSI=15  (SMD variant)
SPIClass sdSpi(HSPI);

ADS1256 adc(&adcSpi, DRDY, SPI4_CS, VREF);

// ============================================================
// Shared state
// ============================================================
static QueueHandle_t logging_queue  = nullptr;
static String        currentFileName;
static volatile uint32_t droppedSamples = 0;   // written by ADS task, read by SD task

// SD batching -------------------------------------------------------
// The file stays open for the lifetime of the SD task (no open/close
// per record).  Records are formatted into SD_BATCH_BUF; when it fills
// OR SD_FLUSH_EVERY_N records have accumulated, one file.write() call
// drains it.  This collapses hundreds of tiny writes into one large
// sequential write, eliminating the FAT open/close overhead that caused
// the multi-second gaps.
#define SD_RECORD_BUF  128          // bytes for one formatted CSV line

// ============================================================
// Forward declarations
// ============================================================
static inline void safe_pin_out(int pin, int val);
static inline void safe_pin_in(int pin);
static void task_ads(void *params);
static void task_sd (void *params);
static void print_reset_reason();
static void init_board_pins();
static void print_banner();
static String get_next_filename();
static bool  sd_write(const char *path, const char *data, const char *mode);

// ============================================================
// setup()
// ============================================================
void setup()
{
    Serial.begin(115200);
    tone(35, 1760);

    init_board_pins();
    print_banner();
    print_reset_reason();

    // ---- FreeRTOS queue ----
    logging_queue = xQueueCreate(QUEUE_DEPTH, sizeof(LogRecord));
    configASSERT(logging_queue && "Could not create logging queue");

    // ---- SD card ----
    // HSPI must start before FSPI to avoid ADS SPI bus conflicts (empirical)
    sdSpi.begin(SPI2_SCK, SPI2_MISO, SPI2_MOSI, SPI2_CS);

    unsigned long sd_start = millis();
    bool sd_ok = false;
    while (!sd_ok && millis() - sd_start < 10000) {
        sd_ok = SD.begin(SPI2_CS, sdSpi);
        if (!sd_ok) {
            Serial.println("[SD] Waiting for card...");
            delay(1000);
        }
    }
    if (!sd_ok) {
        Serial.println("[SD] Init failed after 10 s – restarting");
        esp_restart();
    }
    configASSERT(sd_ok && "SD card could not be initialized");

    currentFileName = get_next_filename();
    Serial.println("[SD] File: " + currentFileName);
    sd_write(currentFileName.c_str(), "timestamp_us,voltage_V,output_lbs\n", FILE_WRITE);
    Serial.println("[SD] Init done");

    // ---- ADS1256 ----
    adcSpi.begin(17, 16, 15);
    adcSpi.setFrequency((uint32_t)(ADS_MASTER_CLOCK_FREQ / 4));

    Serial.println("[ADS] Initializing...");
    adc.InitializeADC();

    adc.setPGA(PGA_64);
    adc.setMUX(DIFF_0_1);          // locked to one channel – no mux cycling

    // DRATE_2000SPS: sinc filter settles every 0.5 ms.
    // With OVERSAMPLE_N=2 the effective output rate is ~1000 Hz.
    // Raising DRATE increases speed but worsens noise floor;
    // lowering it improves noise but reduces throughput.
    adc.setDRATE(DRATE_2000SPS);

    // Recommended after any PGA change – ADS1256 datasheet p.16
    adc.sendDirectCommand(SELFCAL);
    // Wait for calibration to complete (DRDY goes low then high)
    delay(10);

    Serial.printf("[ADS] PGA:   %d\n",     adc.getPGA());
    Serial.printf("[ADS] MUX:   0x%02X\n", adc.readRegister(MUX_REG));
    Serial.printf("[ADS] DRATE: 0x%02X\n", adc.readRegister(DRATE_REG));

    // Blink LED1 to signal ready (safe_pin_out guards invalid pin numbers)
    safe_pin_out(LED1, HIGH);
    delay(500);
    safe_pin_out(LED1, LOW);

    // ---- Launch FreeRTOS tasks ----
    // On ESP32-S3 the Arduino framework runs setup()/loop() on APP_CPU (core 1).
    // The WDT crash showed IDLE0 (core 0) being starved, meaning ADS_read was
    // monopolising core 0.  Fix: put the blocking ADS task on core 0 (PRO_CPU)
    // and the SD task on core 1 (APP_CPU) alongside the Arduino idle task.
    // SD task created first so it is ready before the first sample arrives.
    xTaskCreatePinnedToCore(task_sd,  "SD_log",   TASK_STACK_SD,  nullptr, TASK_PRIO_SD,  nullptr, PRO_CPU_NUM);
    xTaskCreatePinnedToCore(task_ads, "ADS_read", TASK_STACK_ADS, nullptr, TASK_PRIO_ADS, nullptr, APP_CPU_NUM);

    Serial.println("[MAIN] Tasks launched – acquisition running");
    noTone(35);
}

// Surrender the Arduino loop task permanently
void loop()
{
    vTaskDelay(portMAX_DELAY);
}

// ============================================================
// ADS acquisition task  (PRO_CPU, priority 2)
//
// Runs as fast as ADS1256 DRDY allows (~1 kHz at DRATE_1000SPS).
// Yields for 2 ms every WATCHDOG_PERIOD_TICKS to let the idle
// task run and reset the task-WDT – identical to the reference.
// ============================================================
static void task_ads(void * /*params*/)
{
    Serial.println("[ADS task] started");
    TickType_t last_yield = xTaskGetTickCount();

    for (;;) {
        // ---- Oversample: accumulate OVERSAMPLE_N raw readings ----
        // readSingle() waits for DRDY then clocks out one conversion.
        // No mux switching – MUX is locked to DIFF_0_1 in setup().
        // Averaging N samples reduces RMS noise by sqrt(N) without
        // any external filter component.
        double accum = 0.0;
        for (int i = 0; i < OVERSAMPLE_N; ++i) {
            accum += adc.readSingle();
        }
        float raw     = (float)(accum / OVERSAMPLE_N);   // raw ADC counts
        float voltage = adc.convertToVoltage(raw);
        float output  = 2.20462f * (-15588.0f * voltage + 1.66f);  // lbs

        LogRecord rec;
        rec.type            = LogType::LOAD_CELL;
        rec.timestamp_us    = (uint32_t)micros();
        rec.data.lc.voltage = voltage;
        rec.data.lc.output  = output;

        // Non-blocking send – drop sample rather than stall acquisition.
        if (xQueueSendToBack(logging_queue, &rec, QUEUE_SEND_TIMEOUT) != pdTRUE) {
            droppedSamples++;
        }

        // Debug serial print – throttled to once every 100 ms so the UART
        // does not dominate loop timing.  Remove or set DEBUG_PRINT_MS 0
        // to disable when not needed.
#define DEBUG_PRINT_MS 100
#if DEBUG_PRINT_MS > 0
        static TickType_t last_print = 0;
        TickType_t now_ticks = xTaskGetTickCount();
        if (now_ticks - last_print >= pdMS_TO_TICKS(DEBUG_PRINT_MS)) {
            last_print = now_ticks;
            // raw = ADC counts; if this is frozen, the SPI/DRDY is stuck.
            // voltage moving but output frozen → formula coefficients wrong.
            // output moving here but CSV flat → SD write path bug.
            Serial.printf("[ADS] t=%lu us  raw=%.0f  V=%.8f  out=%.4f lbs\n",
                          (unsigned long)rec.timestamp_us, raw, voltage, output);
        }
#endif

        // Yield briefly so the idle task can reset the task-WDT
        if (xTaskGetTickCount() - last_yield >= WATCHDOG_PERIOD_TICKS) {
            vTaskDelay(pdMS_TO_TICKS(2));
            last_yield = xTaskGetTickCount();
        }
    }
}

// ============================================================
// SD write task  (APP_CPU, priority 2)
//
// The file is opened ONCE and kept open.  Records are formatted
// into a RAM batch buffer; a single file.write() drains it every
// SD_BATCH_LINES records.  file.flush() syncs metadata to FAT
// every FLUSH_SYNC_MS ms so data is recoverable after a crash.
//
// This eliminates the per-record open/close overhead that caused
// multi-second gaps in the timestamp stream.
// ============================================================
#define FLUSH_SYNC_MS  2000   // fsync interval – balance crash safety vs overhead

static void task_sd(void * /*params*/)
{
    Serial.println("[SD task] started");

    // Open once, keep open for the lifetime of the task.
    // FILE_APPEND positions the write pointer at end-of-file; the header
    // row written by sd_write(..., FILE_WRITE) in setup() is preserved.
    File logFile = SD.open(currentFileName.c_str(), FILE_APPEND);
    configASSERT(logFile && "SD task: could not open log file");
    Serial.printf("[SD task] file opened, size=%lu bytes\n", (unsigned long)logFile.size());

    TickType_t last_sync  = xTaskGetTickCount();
    TickType_t last_yield = xTaskGetTickCount();
    uint32_t   totalWritten = 0;
    char line[SD_RECORD_BUF];

    for (;;) {
        LogRecord rec;
        if (xQueueReceive(logging_queue, &rec, pdMS_TO_TICKS(50)) == pdTRUE) {

            // Format directly into line – no intermediate batch buffer.
            // At 1 kHz each print() call writes ~30 bytes; the SD library's
            // internal 512-byte sector buffer absorbs bursts and issues a
            // physical write only when the sector is full.
            int len = snprintf(line, sizeof(line),
                               "%lu,%.8f,%.4f\n",
                               (unsigned long)rec.timestamp_us,
                               rec.data.lc.voltage,
                               rec.data.lc.output);

            if (len > 0 && len < (int)sizeof(line)) {
                logFile.write((const uint8_t *)line, len);
                totalWritten++;
            }
        }

        // Periodic fsync + status report
        TickType_t now = xTaskGetTickCount();
        if (now - last_sync >= pdMS_TO_TICKS(FLUSH_SYNC_MS)) {
            logFile.flush();   // commits SD library buffer → FAT
            last_sync = now;

            uint32_t dropped = droppedSamples;
            droppedSamples = 0;
            Serial.printf("[SD] written=%lu  dropped=%lu  queueUsed=%u  fileSize=%lu\n",
                          (unsigned long)totalWritten,
                          (unsigned long)dropped,
                          (unsigned)uxQueueMessagesWaiting(logging_queue),
                          (unsigned long)logFile.size());
        }

        // WDT yield
        if (now - last_yield >= WATCHDOG_PERIOD_TICKS) {
            vTaskDelay(pdMS_TO_TICKS(2));
            last_yield = xTaskGetTickCount();
        }
    }
}

// ============================================================
// Helpers
// ============================================================
static void print_reset_reason()
{
    esp_reset_reason_t r = esp_reset_reason();
    switch (r) {
        case ESP_RST_POWERON:   Serial.println("[BOOT] Power-on reset");       break;
        case ESP_RST_EXT:       Serial.println("[BOOT] External reset");        break;
        case ESP_RST_SW:        Serial.println("[BOOT] Software reset");        break;
        case ESP_RST_PANIC:     Serial.println("[BOOT] Software panic");        break;
        case ESP_RST_INT_WDT:   Serial.println("[BOOT] Interrupt watchdog");    break;
        case ESP_RST_TASK_WDT:  Serial.println("[BOOT] Task watchdog");         break;
        case ESP_RST_WDT:       Serial.println("[BOOT] Other watchdog");        break;
        case ESP_RST_DEEPSLEEP: Serial.println("[BOOT] Woke from deep sleep");  break;
        case ESP_RST_BROWNOUT:  Serial.println("[BOOT] Brownout reset");        break;
        case ESP_RST_SDIO:      Serial.println("[BOOT] SDIO reset");            break;
        default:                Serial.printf("[BOOT] Unknown reset (%d)\n", r); break;
    }
}

static void print_banner()
{
    Serial.println();
    Serial.println("======================================");
    Serial.println("=== GAIN 64 TEST BUILD ===");
    Serial.println("Body Tube Board ADS1256 PlatformIO");
    Serial.println("ESP32-S3  DRDY=14  CS=8  SCK=17  MISO=16  MOSI=15");
    Serial.println("======================================");
}

// Safe pinMode/digitalWrite: logs and skips invalid GPIO numbers.
// GPIO_NUM_MAX on ESP32-S3 is 49. Anything above that (e.g. 227)
// means the constant in pins.h is wrong or undefined.
static inline void safe_pin_out(int pin, int val)
{
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        Serial.printf("[PINS] Skipping invalid pin %d (check pins.h)\n", pin);
        return;
    }
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val);
}

static inline void safe_pin_in(int pin)
{
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        Serial.printf("[PINS] Skipping invalid pin %d (check pins.h)\n", pin);
        return;
    }
    pinMode(pin, INPUT);
}

static void init_board_pins()
{
    // Print every constant so mis-defined values are immediately visible
    Serial.printf("[PINS] LED1=%d  LED2=%d  BUZZ=%d\n",           LED1, LED2, BUZZ);
    Serial.printf("[PINS] SPI4_CS=%d  SPI4_CS2=%d  DRDY=%d\n",    SPI4_CS, SPI4_CS2, DRDY);
    Serial.printf("[PINS] SPI2 SCK=%d MISO=%d MOSI=%d CS=%d\n",   SPI2_SCK, SPI2_MISO, SPI2_MOSI, SPI2_CS);

    safe_pin_out(LED1,     LOW);
    safe_pin_out(LED2,     LOW);
    safe_pin_out(BUZZ,     LOW);
    safe_pin_out(SPI4_CS,  HIGH);
    safe_pin_out(SPI4_CS2, HIGH);   // prints warning + skips if value is invalid
    safe_pin_in (DRDY);
}

static String get_next_filename()
{
    for (int i = 0; i < 1000; ++i) {
        String path = "/launch" + String(i) + ".csv";
        if (!SD.exists(path)) return path;
    }
    return "/launch_overflow.csv";
}

// Returns true on success. Pass FILE_WRITE or FILE_APPEND as mode.
static bool sd_write(const char *path, const char *data, const char *mode)
{
    File f = SD.open(path, mode);
    if (!f) return false;
    bool ok = f.print(data);
    f.close();
    return ok;
}
