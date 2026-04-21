#include "lcpt_calibration.h"
#include <Arduino.h>
#include <vector>
#include <stdlib.h>
#include <math.h>

float getLCValue(int channel);
float getPTValue(int channel);

enum SensorType {
  SENSOR_PT,
  SENSOR_LC,
};

static String readLineTrimmed() {
  while (!Serial.available()) {
    delay(10);
  }
  String line = Serial.readStringUntil('\n');
  line.trim();
  return line;
}

static bool parseFloatStrict(const String& s, float& out) {
  char* endPtr = nullptr;
  out = strtof(s.c_str(), &endPtr);
  return endPtr != s.c_str() && *endPtr == '\0';
}

static int readIntInRange(const char* prompt, int minV, int maxV) {
  while (true) {
    Serial.print(prompt);
    String line = readLineTrimmed();
    float maybeFloat = 0.0f;
    if (!parseFloatStrict(line, maybeFloat)) {
      Serial.println("Invalid number. Try again.");
      continue;
    }
    int value = (int)maybeFloat;
    if (value < minV || value > maxV || maybeFloat != (float)value) {
      Serial.print("Enter an integer from ");
      Serial.print(minV);
      Serial.print(" to ");
      Serial.println(maxV);
      continue;
    }
    return value;
  }
}

static float readFloatPrompt(const char* prompt) {
  while (true) {
    Serial.print(prompt);
    String line = readLineTrimmed();
    float value = 0.0f;
    if (parseFloatStrict(line, value)) {
      return value;
    }
    Serial.println("Invalid number. Try again.");
  }
}

static void waitForEnter(const char* prompt) {
  Serial.println(prompt);
  readLineTrimmed();
}

static float readRawAverage(SensorType type, int channel, int samples = 30) {
  float sum = 0.0f;
  for (int i = 0; i < samples; i++) {
    if (type == SENSOR_PT) sum += getPTValue(channel);
    else sum += getLCValue(channel);
    delay(10);
  }
  return sum / (float)samples;
}

static bool computeRawToEngineeringFit(const std::vector<float>& rawVals, const std::vector<float>& engVals, float& m, float& b) {
  if (rawVals.size() != engVals.size() || rawVals.size() < 2) return false;

  float sumRaw = 0.0f;
  float sumEng = 0.0f;
  float sumRawEng = 0.0f;
  float sumRaw2 = 0.0f;
  float n = (float)rawVals.size();

  for (size_t i = 0; i < rawVals.size(); i++) {
    float x = rawVals[i];
    float y = engVals[i];
    sumRaw += x;
    sumEng += y;
    sumRawEng += x * y;
    sumRaw2 += x * x;
  }

  float denom = (n * sumRaw2) - (sumRaw * sumRaw);
  if (fabs(denom) < 1e-9f) return false;

  m = (n * sumRawEng - sumRaw * sumEng) / denom;
  b = (sumEng - m * sumRaw) / n;
  return true;
}

static void liveReadChannel(SensorType type, int channel) {
  Serial.println();
  Serial.print("Live raw read for ");
  Serial.print(type == SENSOR_PT ? "PT" : "LC");
  Serial.print(channel);
  Serial.println(".");
  Serial.println("Press ENTER to read one sample average, or type q then ENTER to stop.");

  while (true) {
    Serial.print("> ");
    String cmd = readLineTrimmed();
    if (cmd == "q" || cmd == "Q") {
      Serial.println("Leaving live read mode.");
      return;
    }
    float raw = readRawAverage(type, channel);
    Serial.print("Current raw average = ");
    Serial.println(raw, 8);
  }
}

static void calibrateOneChannel(SensorType type, int channel) {
  Serial.println();
  Serial.print("=== Calibrating ");
  Serial.print(type == SENSOR_PT ? "PT" : "LC");
  Serial.print(channel);
  Serial.println(" ===");

  int points = readIntInRange("How many calibration points? (min 2): ", 2, 50);
  std::vector<float> engVals;
  std::vector<float> rawVals;
  engVals.reserve(points);
  rawVals.reserve(points);

  for (int i = 0; i < points; i++) {
    Serial.println();
    Serial.print("Point ");
    Serial.print(i + 1);
    Serial.print(" of ");
    Serial.println(points);

    float known = readFloatPrompt("Enter known value (engineering units): ");
    waitForEnter("Apply this known value physically, let it settle, then press Enter.");

    float raw = readRawAverage(type, channel);
    engVals.push_back(known);
    rawVals.push_back(raw);

    Serial.print("Captured raw average = ");
    Serial.println(raw, 8);
  }

  float m = 0.0f;
  float b = 0.0f;
  if (!computeRawToEngineeringFit(rawVals, engVals, m, b)) {
    Serial.println("Fit failed. Ensure you used at least 2 distinct raw points.");
    return;
  }

  Serial.println();
  Serial.println("Result (DIRECT FORM): calibrated = raw * m + b");
  Serial.print("m = ");
  Serial.println(m, 8);
  Serial.print("b = ");
  Serial.println(b, 8);

  Serial.println("Copy this into DAQESPUSB.ino:");
  if (type == SENSOR_PT) {
    Serial.print("PT_CALIBRATION[");
    Serial.print(channel);
    Serial.print("] = {");
    Serial.print(m, 8);
    Serial.print("f, ");
    Serial.print(b, 8);
    Serial.println("f};");
  } else {
    Serial.print("LC_CALIBRATION[");
    Serial.print(channel);
    Serial.print("] = {");
    Serial.print(m, 8);
    Serial.print("f, ");
    Serial.print(b, 8);
    Serial.println("f};");
  }
}

void startCalibration() {
  Serial.println();
  Serial.println("CALIBRATION PIPELINE");
  Serial.println("This computes DIRECT coefficients: calibrated = raw * m + b");

  while (true) {
    Serial.println();
    Serial.println("Choose mode:");
    Serial.println("  1 = Calibrate PT");
    Serial.println("  2 = Calibrate LC");
    Serial.println("  3 = Live raw read (no storage)");
    Serial.println("  q = quit");
    Serial.print("> ");

    String mode = readLineTrimmed();
    if (mode == "q" || mode == "Q") {
      Serial.println("Calibration ended.");
      return;
    }

    SensorType type;
    int maxChannels;
    if (mode == "1") {
      type = SENSOR_PT;
      maxChannels = 8;
    } else if (mode == "2") {
      type = SENSOR_LC;
      maxChannels = 2;
    } else if (mode == "3") {
      int typeChoice = readIntInRange("Read which sensor type? (1=PT, 2=LC): ", 1, 2);
      if (typeChoice == 1) {
        type = SENSOR_PT;
        maxChannels = 8;
      } else {
        type = SENSOR_LC;
        maxChannels = 2;
      }
      int channel = readIntInRange("Enter channel index: ", 0, maxChannels - 1);
      liveReadChannel(type, channel);
      continue;
    } else {
      Serial.println("Invalid selection.");
      continue;
    }

    int count = readIntInRange("How many channels do you want to calibrate now? ", 1, maxChannels);

    for (int i = 0; i < count; i++) {
      int channel = readIntInRange("Enter channel index: ", 0, maxChannels - 1);
      calibrateOneChannel(type, channel);
    }

    Serial.println();
    Serial.println("Batch complete.");
  }
}
