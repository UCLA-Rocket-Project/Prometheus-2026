#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// Replace with your network credentials
const char *ssid = "iPhone";
const char *password = "Okkkkkkk";

#define FILE_NAME_MAX_LENGTH 100
#define CSV_ENTRY_MAX_LENGTH 1024

char newFileName[FILE_NAME_MAX_LENGTH];

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// File downloadFile; //fix 1

// #define SD_HSCK   12  // Replace with your HSCK pin
// #define SD_HMISO  13  // Replace with your HMISO pin
// #define SD_HMOSI  11  // Replace with your HMOSI pin
// #define SD_CS_XTSD    6  // Replace with your CS_XTSD pin

// #define SD_HSCK 14
// #define SD_CS_XTSD 15
// #define SD_HMOSI 13
// #define SD_HMISO 12

// SD card analog
// #define SD_CS_XTSD 40
// #define SD_HSCK 39
// #define SD_HMISO 37
// #define SD_HMOSI 38

// sd card digital
// #define SD_CS_XTSD 6
// #define SD_HSCK 17
// #define SD_HMISO 16
// #define SD_HMOSI 15

// Nose cone
// #define SD_CS_XTSD 9
// #define SD_HSCK 10
// #define SD_HMISO 11
// #define SD_HMOSI 12

// int sck = 39;
// int miso = 37;
// int mosi = 38;
// int cs = 2;

// digital v2
#define SD_CS_XTSD 8
#define SD_HSCK 9
#define SD_HMISO 10
#define SD_HMOSI 11

// #define SD_CS_XTSD 9
// #define SD_HSCK 10
// #define SD_HMISO 11
// #define SD_HMOSI 12

SPIClass spi;

// Write to the SD card
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

// Append data to the SD card
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

// Delete file
void deleteFile(fs::FS &fs, const char *path)
{
  Serial.printf("Deleting file: %s\r\n", path);
  if (fs.remove(path))
  {
    Serial.println("- file deleted");
  }
  else
  {
    Serial.println("- delete failed");
  }
}

void clearAllFiles(fs::FS &fs)
{
  File root = fs.open("/", FILE_READ);

  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    Serial.printf("Deleting: %s\n", file.name());
    char fileName[30];
    snprintf(fileName, 30, "/%s", file.name());
    deleteFile(fs, fileName);
    file = root.openNextFile();
  }

  root = fs.open("/additional", FILE_READ);

  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }

  file = root.openNextFile();
  while (file)
  {
    Serial.printf("Deleting: %s\n", file.name());
    char fileName[64];
    snprintf(fileName, 64, "/additional/%s", file.name());
    deleteFile(fs, fileName);
    file = root.openNextFile();
  }
}

// Function that initializes wi-fi
void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.path(), levels - 1);
      }
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void listSDCardFiles(fs::FS &fs, char *inputBuffer)
{
  sprintf(inputBuffer, "<html><body><ul>");

  File root = fs.open("/");

  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }

  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    char fileName[30];
    memset(fileName, 0, 30);
    sprintf(fileName, "<li>%s | %d</li>", file.name(), file.size());
    strcat(inputBuffer, fileName);
    Serial.println(fileName);
    file = root.openNextFile();
  }

  strcat(inputBuffer, "</ul></body></html>");
  Serial.println(inputBuffer);
}

// Init microSD card
void initSDCard(SPIClass &spi)
{

  if (!SD.begin(SD_CS_XTSD, spi))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  listDir(SD, "/", 1);
}

void setup()
{
  Serial.begin(460800);
  delay(1000);
  initWiFi();

  spi.begin(SD_HSCK, SD_HMISO, SD_HMOSI, -1);
  initSDCard(spi);

  // Handle the root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "OK"); });

  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("sheesh")) {
      char sheeshParam[100];
      snprintf(sheeshParam, 100, request->getParam("sheesh")->value().c_str());
      Serial.printf("Received: %s\n", sheeshParam);
    }

    request->send(200, "text/plain", "WOOHOO"); });

  server.on("/list-files", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    char list[2048];
    listSDCardFiles(SD, list);
    char listToSend[2048];
    // sprintf(listToSend, "R\"rawliteral(%s)rawliteral\"", list);
    request->send(200, "text/html", list); });
  /*
    server.on("/download-chunked", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (!request->hasParam("fileName")) {
        request->send(200, "text/plain", "welp");
        return;
      }

      String filename;
      String filenameWithTxt =
          "/" + request->getParam("fileName")->value() ;

      if (SD.exists(filenameWithTxt)) {
        filename = filenameWithTxt;
      } else {
        request->send(404, "text/plain", "Provided file not found");
        return;
      }

      downloadFile = SD.open(filename, FILE_READ);
      size_t fileSize = downloadFile.size();
      String fileSizeString = String(fileSize);

      AsyncWebServerResponse *response = request->beginChunkedResponse(
          "text/plain",
          [fileSize](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            Serial.printf("%u / %u\n", index, fileSize);

            if (fileSize <= index) {
              Serial.println("finished");
              downloadFile.close();
              return 0;
            }

            const int chunkSize = min(maxLen, fileSize - index);

            downloadFile.readBytes((char *)buffer, chunkSize);

            Serial.printf("Sending %u\n", chunkSize);

            // give the idle task time to do cleanup
            // delay(1);

            return chunkSize;
          });

      response->addHeader(asyncsrv::T_Cache_Control, "public,max-age=60");
      response->addHeader(asyncsrv::T_ETag, fileSizeString);

      request->send(response);
    });
  */
  // server.on("/download-chunked", HTTP_GET, [](AsyncWebServerRequest *request) {
  //     if (!request->hasParam("fileName")) {
  //         request->send(400, "text/plain", "Missing fileName");
  //         return;
  //     }

  //     String filename = "/" + request->getParam("fileName")->value() ;

  //     if (!SD.exists(filename)) {
  //         request->send(404, "text/plain", "File not found");
  //         return;
  //     }

  //     File file = SD.open(filename, FILE_READ);
  //     size_t fileSize = file.size();

  //     AsyncWebServerResponse *response = request->beginChunkedResponse(
  //         "application/octet-stream",
  //         [file, fileSize](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {

  //             if (index >= fileSize) {
  //                 file.close();
  //                 return 0;
  //             }

  //             size_t chunkSize = min(maxLen, fileSize - index);
  //             size_t bytesRead = file.read(buffer, chunkSize);

  //             delay(1);
  //             return bytesRead;
  //         });

  //     request->send(response);
  // });
  server.on("/download-chunked", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      if (!request->hasParam("fileName")) {
          request->send(400, "text/plain", "Missing fileName");
          return;
      }
  
      String filename = "/" + request->getParam("fileName")->value() ;
  
      if (!SD.exists(filename)) {
          request->send(404, "text/plain", "File not found");
          return;
      }
  
      File *file = new File(SD.open(filename, FILE_READ));
      size_t fileSize = file->size();
  
      AsyncWebServerResponse *response =
          request->beginChunkedResponse(
              "application/octet-stream",
              [file, fileSize](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
  
                  if (index >= fileSize) {
                      file->close();
                      delete file;
                      return 0;
                  }
  
                  size_t chunkSize = min(maxLen, fileSize - index);
                  size_t bytesRead = file->read(buffer, chunkSize);
  
                  return bytesRead;
              });
  
      request->send(response); });

  server.on("/delete-all-files", HTTP_GET, [](AsyncWebServerRequest *req)
            {
    if (!req->hasParam("password") ||
        req->getParam("password")->value() != "juanmygoat") {
      req->send(400, "You dont have the permissions to do this");
      return;
    }

    clearAllFiles(SD);
    req->send(200, "All files deleted"); });

  // fix
  /*
  server.on(
      "/resumable-download-chunked", HTTP_GET,
      [](AsyncWebServerRequest *request) {
        if (!request->hasParam("fileName")) {
          request->send(200, "text/plain", "welp");
          return;
        }

        String filename;
        String filenameWithTxt =
            "/additional/" + request->getParam("fileName")->value() ;

        if (SD.exists(filenameWithTxt)) {
          filename = filenameWithTxt;
        } else {
          request->send(404, "text/plain", "Provided file not found");
          return;
        }

        size_t start = 0;
        if (request->hasParam("startPoint")) {
          start = request->getParam("startPoint")->value().toInt();
        }

        downloadFile = SD.open(filename, FILE_READ);
        size_t fileSize = downloadFile.size();
        size_t endIndex = fileSize - 1;
        String fileSizeString = String(fileSize);
        downloadFile.seek(start);

        if (start >= endIndex) {
          request->send(404, "text/plain", "The file is already at the end");
          return;
        }

        AsyncWebServerResponse *response = request->beginChunkedResponse(
            "application/octet-stream",
            [fileSize, start](uint8_t *buffer, size_t maxLen,
                              size_t index) -> size_t {
              Serial.printf("%u / %u\n", index, fileSize);

              if (fileSize <= index || start + index >= fileSize - 1) {
                Serial.println("finished");
                downloadFile.close();
                return 0;
              }

              const int chunkSize = min(maxLen, fileSize - index);
              downloadFile.readBytes((char *)buffer, chunkSize);
              Serial.printf("Sending %u\n", chunkSize);

              return chunkSize;
            });

        response->addHeader(asyncsrv::T_Cache_Control, "public,max-age=60");
        response->addHeader(asyncsrv::T_ETag, fileSizeString);

        request->send(response);
      });
    */
  server.on("/resumable-download-chunked", HTTP_GET, [](AsyncWebServerRequest *request)
            {
       if (!request->hasParam("fileName")) {
           request->send(400, "text/plain", "Missing fileName");
           return;
       }
   
       String filename = "/additional/" + request->getParam("fileName")->value() ;
   
       if (!SD.exists(filename)) {
           request->send(404, "text/plain", "File not found");
           return;
       }
   
       size_t start = 0;
       if (request->hasParam("startPoint")) {
           start = request->getParam("startPoint")->value().toInt();
       }
   
       File file = SD.open(filename, FILE_READ);
       size_t fileSize = file.size();
   
       if (start >= fileSize) {
           request->send(416, "text/plain", "Start point beyond file size");
           file.close();
           return;
       }
   
       file.seek(start);
   
       AsyncWebServerResponse *response = request->beginChunkedResponse(
           "application/octet-stream",
           [file, fileSize, start](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
   
               size_t absoluteIndex = start + index;
   
               if (absoluteIndex >= fileSize) {
                   file.close();
                   return 0;
               }
   
               size_t chunkSize = min(maxLen, fileSize - absoluteIndex);
               size_t bytesRead = file.read(buffer, chunkSize);
   
               delay(1);
               return bytesRead;
           });
   
       request->send(response); });

  // fix
  /*
 server.on("/get-file-size", HTTP_GET, [](AsyncWebServerRequest *request) {
   if (!request->hasParam("fileName")) {
     request->send(200, "text/plain", "welp");
     return;
   }

   String filename;
   String filenameWithTxt =
       "/additional/" + request->getParam("fileName")->value() ;

   if (SD.exists(filenameWithTxt)) {
     filename = filenameWithTxt;
   }

   downloadFile = SD.open(filename, FILE_READ);
   size_t fileSize = downloadFile.size();

   request->send(200, "text/plain", String(fileSize));
   return;
 });

 server.begin();
}
 */
  server.on("/get-file-size", HTTP_GET, [](AsyncWebServerRequest *request)
            {
     if (!request->hasParam("fileName")) {
         request->send(400, "text/plain", "Missing fileName");
         return;
     }
 
     String filename = "/additional/" +
                       request->getParam("fileName")->value() +
                       ".txt";
 
     if (!SD.exists(filename)) {
         request->send(404, "text/plain", "File not found");
         return;
     }
 
     File file = SD.open(filename, FILE_READ);
     size_t fileSize = file.size();
     file.close();
 
     request->send(200, "text/plain", String(fileSize)); });

  server.begin();
}

void loop() {}

void getLatestCSV(char *latestFileName)
{
  int counter = 1;
  char candidateFileName[FILE_NAME_MAX_LENGTH + 1];
  char lastFoundFile[FILE_NAME_MAX_LENGTH + 1] = "ERROR"; // set as ERROR
  // just in case

  while (true)
  {
    sprintf(candidateFileName, "/launch%d.txt", counter);
    // sprintf(candidateFileName, "/data%d.csv", counter);
    Serial.print("Checking file: ");
    Serial.println(candidateFileName);

    if (!SD.exists(candidateFileName))
    {
      Serial.print("The latest file name was found to be: ");
      Serial.println(lastFoundFile);
      sprintf(latestFileName, lastFoundFile);
      return;
    }

    sprintf(lastFoundFile, candidateFileName);
    ++counter;
  }
}