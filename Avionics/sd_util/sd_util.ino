#include <SPI.h>
#include <SD.h>
#include <FS.h>

#define SD_HSCK 9
#define SD_HMISO 10
#define SD_HMOSI 11
#define SD_CS_XTSD 8

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void deleteAllFiles(fs::FS &fs) {
  Serial.println("Deleting all files...");

  File root = fs.open("/");
  File file = root.openNextFile();
  int count = 0;

  while (file) {
    if (!file.isDirectory()) {
      String path = "/" + String(file.name());
      file.close();
      if (fs.remove(path.c_str())) {
        Serial.print("Deleted: ");
        Serial.println(path);
        count++;
      } else {
        Serial.print("Failed to delete: ");
        Serial.println(path);
      }
    } else {
      file.close();
    }
    file = root.openNextFile();
  }

  root.close();
  Serial.print("Done. Deleted ");
  Serial.print(count);
  Serial.println(" file(s).");
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SPI.begin(SD_HSCK, SD_HMISO, SD_HMOSI, SD_CS_XTSD);

  if (!SD.begin(SD_CS_XTSD)) {
    Serial.println("SD card initialization failed!");
    return;
  }

  Serial.println("SD card initialized.");

  // --- Choose one action below, comment out the other ---

  // Read a specific file:
  readFile(SD, "/log0.csv");

  // Delete all files:
  // deleteAllFiles(SD);
}

void loop() {
  // Do nothing
}
