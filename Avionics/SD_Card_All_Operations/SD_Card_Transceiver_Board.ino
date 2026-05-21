#include <SPI.h>
#include <SD.h>
#include <FS.h>

// ============================================================
// PINS - TRANSCEIVER BOARD
// ============================================================
#define SD_SCLK  48
#define SD_MISO  35
#define SD_MOSI  36
#define SD_CS    47

// =======================
// READ FILE
// =======================
void readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("----- FILE START -----");
  while (file.available())
    Serial.write(file.read());
  Serial.println();
  Serial.println("----- FILE END -----");

  file.close();
}

// =======================
// DOWNLOAD CSV (RAW)
// =======================
void downloadCsv(fs::FS &fs, const char *path)
{
  File file = fs.open(path);
  if (!file)
  {
    Serial.println("ERROR: File not found");
    return;
  }

  while (file.available())
    Serial.write(file.read());

  file.close();
}

// =======================
// LIST CSV FILES
// =======================
void listCsvFiles(fs::FS &fs)
{
  Serial.println("Listing CSV files:");

  File root = fs.open("/");
  File file = root.openNextFile();
  int count = 0;

  while (file)
  {
    if (!file.isDirectory())
    {
      String name = String(file.name());
      if (name.endsWith(".csv"))
      {
        Serial.print(count);
        Serial.print(": /");
        Serial.println(name);
        count++;
      }
    }
    file.close();
    file = root.openNextFile();
  }

  root.close();

  if (count == 0)
    Serial.println("No CSV files found.");
}

// =======================
// GET LAST CSV FILE
// =======================
String getLastCsvFile(fs::FS &fs)
{
  File root = fs.open("/");
  File file = root.openNextFile();
  String lastFile = "";

  while (file)
  {
    if (!file.isDirectory())
    {
      String name = String(file.name());
      if (name.endsWith(".csv"))
        lastFile = "/" + name;
    }
    file.close();
    file = root.openNextFile();
  }

  root.close();
  return lastFile;
}

// =======================
// DOWNLOAD MENU
// =======================
void downloadMenu(fs::FS &fs)
{
  Serial.println("Enter CSV filename to download (e.g. /log4.csv):");

  String filename = "";
  while (filename == "")
  {
    if (Serial.available())
    {
      filename = Serial.readStringUntil('\n');
      filename.trim();
    }
  }

  if (!filename.startsWith("/"))
    filename = "/" + filename;

  Serial.println("----- DOWNLOAD START -----");
  downloadCsv(fs, filename.c_str());
  Serial.println("\n----- DOWNLOAD END -----");
}

// =======================
// DELETE ALL FILES
// =======================
void deleteAllFiles(fs::FS &fs)
{
  Serial.println("Deleting all files...");

  File root = fs.open("/");
  File file = root.openNextFile();
  int count = 0;

  while (file)
  {
    if (!file.isDirectory())
    {
      String path = "/" + String(file.name());
      file.close();
      if (fs.remove(path.c_str()))
      {
        Serial.print("Deleted: ");
        Serial.println(path);
        count++;
      }
      else
      {
        Serial.print("Failed to delete: ");
        Serial.println(path);
      }
    }
    else
    {
      file.close();
    }
    file = root.openNextFile();
  }

  root.close();
  Serial.print("Done. Deleted ");
  Serial.print(count);
  Serial.println(" file(s).");
}

// =======================
// SETUP
// =======================
SPIClass spiA(FSPI);

void setup()
{
  Serial.begin(115200);
  while (!Serial);

  spiA.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, spiA, 4000000))
  {
    Serial.println("SD card initialization failed!");
    return;
  }

  Serial.println("SD card initialized.");

  // =========================
  // USE WHAT YOU WANT BELOW
  // =========================

  // 1. LIST ALL CSV FILES
  //listCsvFiles(SD);

  // 2. READ LAST CSV FILE
  // String lastFile = getLastCsvFile(SD);
  // if (lastFile != "") {
  //   readFile(SD, lastFile.c_str());
  // } else {
  //   Serial.println("No CSV file found to read.");
  // }

  // 3. READ SPECIFIC FILE
  readFile(SD, "/log1.csv");

  // 4. DOWNLOAD SPECIFIC FILE (MENU)
  // downloadMenu(SD);

  // 5. DELETE ALL FILES
  // deleteAllFiles(SD);
}

void loop()
{
  // Do nothing
}
