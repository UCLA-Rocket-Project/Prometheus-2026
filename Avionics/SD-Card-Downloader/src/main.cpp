#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <FS.h>
#include <SD.h>
#include <SPI.h>

// ========================================
// ACCESS POINT SETTINGS
// ========================================

const char *ap_ssid = "ESP32-SD";
const char *ap_password = "12345678";

// ========================================
// SD CARD PINS
// ========================================

#define SD_CS_XTSD 40
#define SD_HSCK 39
#define SD_HMISO 37
#define SD_HMOSI 38

// #define SD_SCLK  39
// #define SD_MISO  37
// #define SD_MOSI  38
// #define SD_CS    40

SPIClass spi(FSPI);

AsyncWebServer server(80);

// ========================================
// SETUP
// ========================================

void setup()
{

  Serial.begin(460800);

  delay(2000);

  Serial.println();
  Serial.println("BOOTING...");

  // ========================================
  // START ACCESS POINT
  // ========================================

  WiFi.mode(WIFI_AP);

  bool result = WiFi.softAP(ap_ssid, ap_password);

  if (!result)
  {
    Serial.println("AP START FAILED");
    return;
  }

  Serial.println("ACCESS POINT STARTED");

  Serial.print("IP ADDRESS: ");
  Serial.println(WiFi.softAPIP());

  // ========================================
  // INIT SPI
  // ========================================

  spi.begin(
      SD_HSCK,
      SD_HMISO,
      SD_HMOSI,
      SD_CS_XTSD);

  // ========================================
  // INIT SD
  // ========================================

  Serial.println("INITIALIZING SD...");

  if (!SD.begin(SD_CS_XTSD, spi))
  {

    Serial.println("SD INIT FAILED");

    return;
  }

  Serial.println("SD OK");

  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("NO SD CARD");
    return;
  }

  Serial.print("SD SIZE MB: ");

  Serial.println(
      SD.cardSize() / (1024 * 1024));

  // ========================================
  // LIST FILES
  // ========================================

  File root = SD.open("/");

  Serial.println("FILES:");

  while (true)
  {

    File file = root.openNextFile();

    if (!file)
      break;

    Serial.printf(
        "FILE: %s SIZE: %d\n",
        file.name(),
        file.size());

    file.close();
  }

  root.close();

  // ========================================
  // ROOT TEST
  // ========================================

  server.on("/", HTTP_GET,
            [](AsyncWebServerRequest *request)
            {
              Serial.println("ROOT REQUEST");

              request->send(
                  200,
                  "text/plain",
                  "ESP32 SERVER WORKING");
            });

  // ========================================
  // DOWNLOAD ROUTE
  // ========================================

  server.on("/download-chunked", HTTP_GET,
            [](AsyncWebServerRequest *request)
            {
              Serial.println("DOWNLOAD REQUEST");

              if (!request->hasArg("fileName"))
              {

                request->send(
                    400,
                    "text/plain",
                    "Missing fileName");

                return;
              }

              String filename =
                  "/" + request->arg("fileName");

              Serial.print("REQUESTED FILE: ");

              Serial.println(filename);

              if (!SD.exists(filename))
              {

                Serial.println("FILE NOT FOUND");

                request->send(
                    404,
                    "text/plain",
                    "File not found");

                return;
              }

              File testFile = SD.open(
                  filename,
                  FILE_READ);

              if (!testFile)
              {

                Serial.println("OPEN FAILED");

                request->send(
                    500,
                    "text/plain",
                    "Open failed");

                return;
              }

              size_t fileSize = testFile.size();

              Serial.print("FILE SIZE: ");

              Serial.println(fileSize);

              testFile.close();

              AsyncWebServerResponse *response =
                  request->beginResponse(
                      "text/csv",
                      fileSize,
                      [filename](
                          uint8_t *buffer,
                          size_t maxLen,
                          size_t index) -> size_t
                      {
                        static File file;

                        if (index == 0)
                        {

                          Serial.println("STREAM START");

                          if (file)
                            file.close();

                          file = SD.open(
                              filename,
                              FILE_READ);

                          if (!file)
                          {

                            Serial.println(
                                "STREAM OPEN FAILED");

                            return 0;
                          }
                        }

                        if (!file)
                          return 0;

                        size_t toRead =
                            min(maxLen, (size_t)512);

                        size_t bytesRead =
                            file.read(buffer, toRead);

                        if (bytesRead == 0)
                        {

                          Serial.println("STREAM DONE");

                          file.close();

                          return 0;
                        }

                        delay(2);

                        return bytesRead;
                      });

              response->addHeader(
                  "Content-Disposition",
                  "attachment; filename=" +
                      request->arg("fileName"));

              request->send(response);
            });

  // ========================================
  // START SERVER
  // ========================================

  server.begin();

  Serial.println("SERVER STARTED");
}

// ========================================
// LOOP
// ========================================

void loop()
{
}