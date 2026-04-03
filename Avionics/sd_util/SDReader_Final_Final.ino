// #include <SPI.h>
// #include <SD.h>
// #include <FS.h>

// #define SD_HSCK 9
// #define SD_HMISO 10
// #define SD_HMOSI 11
// #define SD_CS_XTSD 8

// // =======================
// // READ FILE
// // =======================
// void readFile(fs::FS &fs, const char *path) {
//   Serial.printf("Reading file: %s\n", path);

//   File file = fs.open(path);
//   if (!file) {
//     Serial.println("Failed to open file for reading");
//     return;
//   }

//   Serial.println("----- FILE START -----");
//   while (file.available()) {
//     Serial.write(file.read());
//   }
//   Serial.println();
//   Serial.println("----- FILE END -----");

//   file.close();
// }

// // =======================
// // LIST CSV FILES
// // =======================
// void listCsvFiles(fs::FS &fs) {
//   Serial.println("Listing CSV files:");

//   File root = fs.open("/");
//   File file = root.openNextFile();

//   int count = 0;

//   while (file) {
//     if (!file.isDirectory()) {
//       String name = String(file.name());

//       if (name.endsWith(".csv")) {
//         Serial.print(count);
//         Serial.print(": /");
//         Serial.println(name);
//         count++;
//       }
//     }

//     file.close();
//     file = root.openNextFile();
//   }

//   root.close();

//   if (count == 0) {
//     Serial.println("No CSV files found.");
//   }
// }

// // =======================
// // GET LAST CSV FILE
// // =======================
// String getLastCsvFile(fs::FS &fs) {
//   File root = fs.open("/");
//   File file = root.openNextFile();

//   String lastFile = "";

//   while (file) {
//     if (!file.isDirectory()) {
//       String name = String(file.name());

//       if (name.endsWith(".csv")) {
//         lastFile = "/" + name;  // overwrite → last one wins
//       }
//     }

//     file.close();
//     file = root.openNextFile();
//   }

//   root.close();

//   return lastFile;
// }

// // =======================
// // DELETE ALL FILES
// // =======================
// void deleteAllFiles(fs::FS &fs) {
//   Serial.println("Deleting all files...");

//   File root = fs.open("/");
//   File file = root.openNextFile();
//   int count = 0;

//   while (file) {
//     if (!file.isDirectory()) {
//       String path = "/" + String(file.name());
//       file.close();
//       if (fs.remove(path.c_str())) {
//         Serial.print("Deleted: ");
//         Serial.println(path);
//         count++;
//       } else {
//         Serial.print("Failed to delete: ");
//         Serial.println(path);
//       }
//     } else {
//       file.close();
//     }
//     file = root.openNextFile();
//   }

//   root.close();
//   Serial.print("Done. Deleted ");
//   Serial.print(count);
//   Serial.println(" file(s).");
// }

// // =======================
// // SETUP
// // =======================
// void setup() {
//   Serial.begin(115200);
//   while (!Serial);

//   SPI.begin(SD_HSCK, SD_HMISO, SD_HMOSI, SD_CS_XTSD);

//   if (!SD.begin(SD_CS_XTSD)) {
//     Serial.println("SD card initialization failed!");
//     return;
//   }

//   Serial.println("SD card initialized.");

//   // =========================
//   // USE WHAT YOU WANT BELOW
//   // =========================

//   // 1. LIST ALL CSV FILES
//   //listCsvFiles(SD);

//   // // 2. READ LAST CSV FILE
//   // String lastFile = getLastCsvFile(SD);
//   // if (lastFile != "") {
//   //   readFile(SD, lastFile.c_str());
//   // } else {
//   //   Serial.println("No CSV file found to read.");
//   // }

//   // 3. READ SPECIFIC FILE
//    readFile(SD, "/log28.csv");

//   // 4. DELETE ALL FILES
//   // deleteAllFiles(SD);
// }

// // =======================
// void loop() {
//   // Do nothing
// }


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
  readFile(SD, "/log26.csv");

  // Delete all files:
  // deleteAllFiles(SD);
}

void loop() {
  // Do nothing
}
