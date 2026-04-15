// THIS IS TRANSMITTER CODE + WEB APP (ESP32 HOSTED) + AP MODE + WEBSOCKET STREAMING

#include <Arduino.h>
#include <ADS1256.h>
#include "ADS8688.h"
#include <SPI.h>
#include <string>
#include <Adafruit_ADS1X15.h>
#include <HardwareSerial.h>

// ---- NEW: WiFi + Web server ----
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// ===================== Pins & HW =====================
// Load Cell (ADS1256) SPI Pins
#define ADS1256_MISO 35
#define ADS1256_SCLK 48
#define ADS1256_MOSI 34
#define ADS1256_CS   7
#define ADS1256_DRDY 4

// PT (ADS8688) SPI Pin
#define ADS8688_CS 36

// LED indicator pin
#define LED 38

#define RO_PIN 44 //16
#define DI_PIN 43 //17
#define DE_RE_PIN 41 //23

HardwareSerial rs485Serial(2);

// SPI bus shared between both ADCs
SPIClass sharedSPI(FSPI);

// ADS1256 instance
ADS1256 loadCellADC(&sharedSPI, ADS1256_DRDY, ADS1256_CS, 2.5);

bool printedAPInfo = false;

// ADS8688 instance
ADS8688 pressureADC;

// ===================== Calibration =====================
float calibrationA1 = -57938.14;
float calibrationB1 = 1.16948;
float convertToWeightLC1(float voltage) { return (calibrationA1 * voltage) + calibrationB1; }

float calibrationA2 = -401428.57;
float calibrationB2 = -3.46143;
float convertToWeightLC2(float voltage) { return (calibrationA2 * voltage) + calibrationB2; }

float getCalibratedValue(float m, float b, float raw) { return (raw - b) / m; }

float mValues[4] = {5.077578, 5.073776, 5.042717, 8.125911};
float bValues[4] = {1081.752319, 1063.822266, 1080.457153, 1139.25354};

// ===================== AP Mode Config =====================
const char* AP_SSID = "ESP32-ROCKET";
const char* AP_PASS = "12345678"; // >= 8 chars

// ===================== Web App / WebSocket =====================
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Simple live dashboard page served by ESP32
static const char INDEX_HTML[] PROGMEM =
"<!doctype html>"
"<html>"
"<head>"
"<meta charset='utf-8'/>"
"<meta name='viewport' content='width=device-width,initial-scale=1'/>"
"<title>Rocket Telemetry</title>"
"<style>"
"body{font-family:sans-serif;margin:18px;}"
".card{padding:14px;border:1px solid #ddd;border-radius:12px;margin-bottom:12px;}"
".row{display:flex;justify-content:space-between;padding:6px 0;}"
"</style>"
"</head>"
"<body>"
"<h2>Rocket Telemetry</h2>"

"<div class='card'>"
"<div class='row'><span>WebSocket</span><span id='ws'>connecting...</span></div>"
"<div class='row'><span>pt0</span><span id='pt0'>-</span></div>"
"<div class='row'><span>pt1</span><span id='pt1'>-</span></div>"
"<div class='row'><span>pt2</span><span id='pt2'>-</span></div>"
"<div class='row'><span>pt3</span><span id='pt3'>-</span></div>"
"<div class='row'><span>lc0</span><span id='lc0'>-</span></div>"
"<div class='row'><span>uptime</span><span id='up'>-</span></div>"
"</div>"

"<script>"
"const el=id=>document.getElementById(id);"
"const sock=new WebSocket('ws://'+location.host+'/ws');"
"sock.onopen=()=>el('ws').textContent='connected';"
"sock.onclose=()=>el('ws').textContent='disconnected';"
"sock.onerror=()=>el('ws').textContent='error';"
"sock.onmessage=(evt)=>{"
"const d=JSON.parse(evt.data);"
"if(d.pt0!==undefined)el('pt0').textContent=d.pt0.toFixed(4);"
"if(d.pt1!==undefined)el('pt1').textContent=d.pt1.toFixed(4);"
"if(d.pt2!==undefined)el('pt2').textContent=d.pt2.toFixed(4);"
"if(d.pt3!==undefined)el('pt3').textContent=d.pt3.toFixed(4);"
"if(d.lc0!==undefined)el('lc0').textContent=d.lc0.toFixed(4);"
"if(d.uptime_ms!==undefined)el('up').textContent=d.uptime_ms;"
"};"
"</script>"

"</body>"
"</html>";

struct Telemetry {
  float pt0 = NAN, pt1 = NAN, pt2 = NAN, pt3 = NAN;
  float lc0 = NAN;
  uint32_t uptime_ms = 0;
};

Telemetry telem;

void broadcastTelemetry() {
  StaticJsonDocument<256> doc;
  doc["pt0"] = telem.pt0;
  doc["pt1"] = telem.pt1;
  doc["pt2"] = telem.pt2;
  doc["pt3"] = telem.pt3;
  doc["lc0"] = telem.lc0;
  doc["uptime_ms"] = telem.uptime_ms;

  char out[256];
  size_t n = serializeJson(doc, out, sizeof(out));
  ws.textAll(out, n);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // push one packet immediately on connect
    broadcastTelemetry();
  }
}

void setupWeb() {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "ok");
  });

  server.begin();
}

// ===================== AP Mode WiFi =====================
void setupWiFiAP() {
  Serial.println("[WiFi] Starting AP...");

  WiFi.disconnect(true, true);
  delay(200);

  WiFi.mode(WIFI_AP);
  delay(200);

  bool ok = WiFi.softAP(AP_SSID, AP_PASS, 1 /*channel*/, 0 /*hidden*/, 8 /*max conn*/);
  delay(200);

  Serial.print("[WiFi] softAP ok = "); Serial.println(ok ? "true" : "false");
  Serial.print("[WiFi] AP IP = "); Serial.println(WiFi.softAPIP());
  Serial.println("[WiFi] Open: http://192.168.4.1/");
}

// ===================== Your original setup/loop =====================
void setup() {
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, HIGH);

  rs485Serial.begin(115200, SERIAL_8N1, RO_PIN, DI_PIN);
  Serial.begin(115200);
  delay(1500);                 // give serial monitor time
  Serial.println("\n\n*** SETUP STARTED ***");

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  // Start custom SPI bus
  sharedSPI.begin(ADS1256_SCLK, ADS1256_MISO, ADS1256_MOSI, -1);

  // Initialize ADS1256 (Load Cell)
  loadCellADC.InitializeADC();
  loadCellADC.setPGA(PGA_64);
  loadCellADC.setDRATE(DRATE_1000SPS);

  // Initialize ADS8688 (PTs)
  pressureADC.begin(ADS1256_MISO, ADS1256_SCLK, ADS1256_MOSI, ADS8688_CS, 4.1, 0x05);
  pressureADC.setInputRange(ADS8688_CS, 0x05);

  // ---- NEW: start AP + web app ----
  setupWiFiAP();
  setupWeb();

  Serial.println("Setup complete");
}

unsigned long lastPush = 0;
const unsigned long PUSH_PERIOD_MS = 500; // 500ms web updates

void loop() {

  if (!printedAPInfo) {
    printedAPInfo = true;
    Serial.println("\n=== AP MODE STARTED ===");
    Serial.print("AP SSID: "); Serial.println(AP_SSID);
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
    Serial.println("Open: http://192.168.4.1/");
}

  // --- PT Measurements (8 channels) ---
  float ptVoltages[8];
  float ptCalibrated[8];
  pressureADC.readAllChannels(ADS8688_CS, true, ptVoltages);

  for (int i = 0; i < 8; i++) {
    if (i >= 4) {
      ptCalibrated[i] = getCalibratedValue(mValues[i - 4], bValues[i - 4], ptVoltages[i]);
    }
  }

  float loadCell[2] = {-1, -1};
  loadCell[0] = -58439.4371 * loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_0_1)) + 1.19746;
  loadCell[1] = -395379.263  * loadCellADC.convertToVoltage(loadCellADC.readDifferentialFaster(DIFF_2_3)) + 27.13879;

  char finalStr[400];
  snprintf(
    finalStr,
    sizeof(finalStr),
    "rocket_data pt0=%.4f,pt1=%.4f,pt2=%.4f,pt3=%.4f,lc0=%.4f,uptime_ms=%lu",
    ptCalibrated[4],
    ptCalibrated[5],
    ptCalibrated[6],
    ptCalibrated[7],
    loadCell[0],
    millis()
  );

  Serial.println(finalStr);
  rs485Serial.println(finalStr);

  // ---- NEW: update telemetry struct for the web app ----
  telem.pt0 = ptCalibrated[4];
  telem.pt1 = ptCalibrated[5];
  telem.pt2 = ptCalibrated[6];
  telem.pt3 = ptCalibrated[7];
  telem.lc0 = loadCell[0];
  telem.uptime_ms = millis();

  // ---- NEW: push to all connected clients every 500ms ----
  unsigned long now = millis();
  if (now - lastPush >= PUSH_PERIOD_MS) {
    lastPush = now;
    broadcastTelemetry();
  }

  // Keep websocket client list clean
  ws.cleanupClients();

  delay(10);
}
