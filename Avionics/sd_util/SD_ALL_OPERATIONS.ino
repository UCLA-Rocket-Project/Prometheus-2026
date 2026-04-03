#include <SPI.h>
#include <SD.h>
#include <FS.h>
#define REASSIGN_PINS
int sck = 39;
int miso = 37;
int mosi = 38;
int cs = 40;
SPIClass spi;
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
void createDir(fs::FS &fs, const char *path)
{
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path))
  {
    Serial.println("Dir created");
  }
  else
  {
    Serial.println("mkdir failed");
  }
}
void removeDir(fs::FS &fs, const char *path)
{
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path))
  {
    Serial.println("Dir removed");
  }
  else
  {
    Serial.println("rmdir failed");
  }
}
void readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\n", path);
  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.print("Read from file: ");
  while (file.available())
  {
    // Serial.print("Reading ...\n");
    Serial.write(file.read());
    delay(1);
  }
  file.close();
}
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
void renameFile(fs::FS &fs, const char *path1, const char *path2)
{
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2))
  {
    Serial.println("File renamed");
  }
  else
  {
    Serial.println("Rename failed");
  }
}
void deleteFile(fs::FS &fs, const char *path)
{
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path))
  {
    Serial.println("File deleted");
  }
  else
  {
    Serial.println("Delete failed");
  }
}
void deleteAllFiles(fs::FS &fs)
{
  Serial.printf("Deleting all files");
  File root = fs.open("/");
  File file = root.openNextFile();
  while (file)
  {
    String name = "/" + String(file.name());
    if (file.isDirectory())
    {
      Serial.print(name);
      Serial.print("is not a file");
      Serial.println();
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(name);
      Serial.println();
      deleteFile(fs, name.c_str());
    }
    file = root.openNextFile();
  }
}
void testFileIO(fs::FS &fs, const char *path)
{
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file)
  {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len)
    {
      size_t toRead = len;
      if (toRead > 512)
      {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %lu ms\n", flen, end);
    file.close();
  }
  else
  {
    Serial.println("Failed to open file for reading");
  }
  file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++)
  {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %lu ms\n", 2048 * 512, end);
  file.close();
}
void setup()
{
  Serial.begin(115200);
  delay(3000);
  tone(35, 1760);
  delay(1500);

  bool sd_initialized = false;
  while (!sd_initialized)
  {
#ifdef REASSIGN_PINS
    spi.begin(sck, miso, mosi, -1);
    sd_initialized = SD.begin(cs, spi);
#else
    sd_initialized = SD.begin();
#endif
    if (!sd_initialized)
    {
      Serial.println("Waiting for SD card... Insert card now.");
      delay(1000); // wait and retry
    }
  }
  noTone(35);
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
  writeFile(SD, "/foo.txt", "Hello ");
  appendFile(SD, "/foo.txt", "\nBye\n");
  readFile(SD, "/foo.txt");
  //  testFileIO(SD, "/test.txt");
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
}
void loop()
{
  Serial.println("i got here");
  Serial.println("Enter Instruction:");
  while (Serial.available() == 0)
  {
  } // wait for data available
  String inst = Serial.readString(); // read until timeout
  inst.trim();                       // remove any \r \n whitespace at the end of the String
  if (inst == "list")
  {
    Serial.println("Listing ...");
    listDir(SD, "/", 1);
  }
  else if (inst == "list-add")
  {
    Serial.println("Listing ...");
    listDir(SD, "/additional", 0);
  }
  else if (inst == "read")
  {
    Serial.println("Reading ...");
    Serial.println("Enter Filename to Read: ");
    while (Serial.available() == 0)
    {
    } // wait for data available
    String filen = "/" + Serial.readString();
    readFile(SD, filen.c_str());
  }
  else if (inst == "delete")
  {
    Serial.println("Deleting ...");
    Serial.println("Enter Filename to Delete: ");
    while (Serial.available() == 0)
    {
    } // wait for data available
    String filen = "/" + Serial.readString();
    deleteFile(SD, filen.c_str());
  }
  else if (inst == "delete-all")
  {
    Serial.println("Deleting All ...");
    deleteAllFiles(SD);
  }
  else if (inst == "create") {
    createDir(SD, "/additional");
  }
  else
  {
    Serial.println("Not a valid instruction");
  }
}
