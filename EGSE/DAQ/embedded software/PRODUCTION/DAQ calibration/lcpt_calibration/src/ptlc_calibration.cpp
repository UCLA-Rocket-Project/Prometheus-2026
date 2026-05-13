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

enum InputAction {
  INPUT_VALUE,
  INPUT_FINISH,
  INPUT_DELETE_LAST,
  INPUT_VIEW_CURRENT,
  INPUT_VIEW_POINTS,
};

static InputAction readFloatOrControl(const char* prompt, float& value) {
  while (true) {
    Serial.print(prompt);
    String line = readLineTrimmed();
    if (line == "q" || line == "Q") {
      return INPUT_FINISH;
    }
    if (line == "d" || line == "D") {
      return INPUT_DELETE_LAST;
    }
    if (line == "v" || line == "V") {
      return INPUT_VIEW_CURRENT;
    }
    if (line == "p" || line == "P") {
      return INPUT_VIEW_POINTS;
    }
    if (parseFloatStrict(line, value)) {
      return INPUT_VALUE;
    }
    Serial.println("Invalid input. Enter a numeric value, v to view current voltage, p to view captured points, d to delete last, or q to finish.");
  }
}

static std::vector<int> readUniqueChannels(int count, int maxChannels) {
  std::vector<int> channels;
  channels.reserve(count);

  while ((int)channels.size() < count) {
    int channel = readIntInRange("Enter channel index: ", 0, maxChannels - 1);
    bool alreadySelected = false;
    for (size_t i = 0; i < channels.size(); i++) {
      if (channels[i] == channel) {
        alreadySelected = true;
        break;
      }
    }

    if (alreadySelected) {
      Serial.println("Channel already selected. Choose a different channel.");
      continue;
    }

    channels.push_back(channel);
  }

  return channels;
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
  if (type == SENSOR_LC && (channel < 0 || channel > 1)) {
    Serial.println("Invalid LC channel. Use 0 or 1.");
    return;
  }

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
    int samples = (type == SENSOR_LC) ? 3 : 10;
    float raw = readRawAverage(type, channel, samples);
    if (!isfinite(raw)) {
      Serial.println("Read failed (non-finite value). Try again.");
      continue;
    }
    Serial.print("Current raw average = ");
    Serial.println(raw * 100000.0f, 8);
  }
}

static void liveReadAllChannels() {
  Serial.println();
  Serial.println("Live raw read for all PT and LC channels.");
  Serial.println("Press ENTER to read one sample average set, or type q then ENTER to stop.");

  while (true) {
    Serial.print("> ");
    String cmd = readLineTrimmed();
    if (cmd == "q" || cmd == "Q") {
      Serial.println("Leaving live read mode.");
      return;
    }

    for (int channel = 0; channel < 8; channel++) {
      if (channel > 0) {
        Serial.print(" ");
      }
      Serial.print("PT");
      Serial.print(channel);
      Serial.print("=");
      Serial.print(readRawAverage(SENSOR_PT, channel, 3) * 100000.0f, 1);
    }

    for (int channel = 0; channel < 2; channel++) {
      Serial.print(" ");
      Serial.print("LC");
      Serial.print(channel);
      Serial.print("=");
      Serial.print(readRawAverage(SENSOR_LC, channel, 3) * 100000.0f, 1);
    }

    Serial.println();
  }
}

static void printCalibrationResult(SensorType type, int channel, float m, float b) {
  Serial.println();
  Serial.print("Result for ");
  Serial.print(type == SENSOR_PT ? "PT" : "LC");
  Serial.print(channel);
  Serial.println(":");
  Serial.println("DIRECT FORM: calibrated = raw * m + b");
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

static void printCompactCalibrationLine(SensorType type, const std::vector<int>& channels, const std::vector<float>& mVals, const std::vector<float>& bVals, const std::vector<bool>& fitOk) {
  Serial.println();
  Serial.print("Compact coefficients: ");
  bool printedAny = false;
  for (size_t i = 0; i < channels.size(); i++) {
    if (!fitOk[i]) {
      continue;
    }

    if (printedAny) {
      Serial.print(" ");
    }

    if (type == SENSOR_PT) {
      Serial.print("PT_CALIBRATION[");
    } else {
      Serial.print("LC_CALIBRATION[");
    }
    Serial.print(channels[i]);
    Serial.print("]={");
    Serial.print(mVals[i], 8);
    Serial.print("f,");
    Serial.print(bVals[i], 8);
    Serial.print("f};");
    printedAny = true;
  }

  if (!printedAny) {
    Serial.print("(no valid fits)");
  }
  Serial.println();
}

static void calibrateChannelBatch(SensorType type, const std::vector<int>& channels, bool compactOutput) {
  Serial.println();
  Serial.print("=== Calibrating ");
  Serial.print(type == SENSOR_PT ? "PT" : "LC");
  Serial.println(" channel batch ===");
  Serial.print("Selected channels: ");
  for (size_t i = 0; i < channels.size(); i++) {
    if (i > 0) {
      Serial.print(", ");
    }
    Serial.print(channels[i]);
  }
  Serial.println();

  Serial.println("Enter calibration points one at a time.");
  Serial.println("Type v to view current sensor voltage.");
  Serial.println("Type p to view all captured points.");
  Serial.println("Type d to delete the last captured point.");
  Serial.println("Type q instead of a value when you want to finish and compute the fit.");

  std::vector<float> engVals;
  std::vector<std::vector<float>> rawValsByChannel(channels.size());

  while (true) {
    Serial.println();
    Serial.print("Point ");
    Serial.print((int)engVals.size() + 1);
    Serial.println(":");

    float known = 0.0f;
    InputAction action = readFloatOrControl("Enter known value (engineering units, v to view, d to delete last, p for all caputred points, or q to finish): ", known);
    if (action == INPUT_FINISH) {
      break;
    }
    if (action == INPUT_DELETE_LAST) {
      if (engVals.empty()) {
        Serial.println("No captured points to delete.");
        continue;
      }

      engVals.pop_back();
      for (size_t channelIndex = 0; channelIndex < rawValsByChannel.size(); channelIndex++) {
        if (!rawValsByChannel[channelIndex].empty()) {
          rawValsByChannel[channelIndex].pop_back();
        }
      }
      Serial.println("Deleted last captured calibration point.");
      continue;
    }
    if (action == INPUT_VIEW_CURRENT) {
      Serial.println();
      Serial.println("=== Current Sensor Readings ===");
      for (size_t channelIndex = 0; channelIndex < channels.size(); channelIndex++) {
        int channel = channels[channelIndex];
        float raw = readRawAverage(type, channel);
        Serial.print(type == SENSOR_PT ? "PT" : "LC");
        Serial.print(channel);
        Serial.print(" = ");
        Serial.println(raw * 100000.0f, 8);
      }
      Serial.println("===============================");
      Serial.println();
      continue;
    }
    if (action == INPUT_VIEW_POINTS) {
      if (engVals.empty()) {
        Serial.println("No captured points yet.");
      } else {
        Serial.println();
        Serial.println("=== Captured Calibration Points ===");
        for (size_t i = 0; i < engVals.size(); i++) {
          Serial.print("Point ");
          Serial.print((int)i + 1);
          Serial.print(": Engineering = ");
          Serial.print(engVals[i], 8);
          for (size_t channelIndex = 0; channelIndex < channels.size(); channelIndex++) {
            Serial.print(" | ");
            Serial.print(type == SENSOR_PT ? "PT" : "LC");
            Serial.print(channels[channelIndex]);
            Serial.print(" raw = ");
            Serial.print(rawValsByChannel[channelIndex][i] * 100000.0f, 8);
          }
          Serial.println();
        }
        Serial.println("===================================");
        Serial.println();
      }
      continue;
    }

    waitForEnter("Apply this known value physically to all selected channels, let it settle, then press Enter.");
    engVals.push_back(known);

    Serial.println();
    Serial.print("Engineering value: ");
    Serial.println(known, 8);
    for (size_t channelIndex = 0; channelIndex < channels.size(); channelIndex++) {
      int channel = channels[channelIndex];
      float raw = readRawAverage(type, channel);
      rawValsByChannel[channelIndex].push_back(raw);

      Serial.print("Captured raw average for ");
      Serial.print(type == SENSOR_PT ? "PT" : "LC");
      Serial.print(channel);
      Serial.print(" = ");
      Serial.println(raw * 100000.0f, 8);
    }

  }

  if (engVals.size() < 2) {
    Serial.println();
    Serial.println("Need at least 2 calibration points before computing a fit. Batch canceled.");
    return;
  }

  Serial.println();
  std::vector<float> mVals(channels.size(), 0.0f);
  std::vector<float> bVals(channels.size(), 0.0f);
  std::vector<bool> fitOk(channels.size(), false);

  for (size_t channelIndex = 0; channelIndex < channels.size(); channelIndex++) {
    float m = 0.0f;
    float b = 0.0f;
    if (!computeRawToEngineeringFit(rawValsByChannel[channelIndex], engVals, m, b)) {
      Serial.print("Fit failed for ");
      Serial.print(type == SENSOR_PT ? "PT" : "LC");
      Serial.print(channels[channelIndex]);
      Serial.println(". Ensure you used at least 2 distinct raw points.");
      continue;
    }

    mVals[channelIndex] = m;
    bVals[channelIndex] = b;
    fitOk[channelIndex] = true;

    if (!compactOutput) {
      printCalibrationResult(type, channels[channelIndex], m, b);
    }
  }

  if (compactOutput) {
    printCompactCalibrationLine(type, channels, mVals, bVals, fitOk);
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
      while (true) {
        Serial.print("Read which sensor type? (1=PT, 2=LC, a=all): ");
        String liveSelection = readLineTrimmed();

        if (liveSelection == "a" || liveSelection == "A") {
          liveReadAllChannels();
          break;
        }

        if (liveSelection == "1") {
          type = SENSOR_PT;
          maxChannels = 8;
          int channel = readIntInRange("Enter channel index: ", 0, maxChannels - 1);
          liveReadChannel(type, channel);
          break;
        }

        if (liveSelection == "2") {
          type = SENSOR_LC;
          maxChannels = 2;
          int channel = readIntInRange("Enter channel index: ", 0, maxChannels - 1);
          liveReadChannel(type, channel);
          break;
        }

        Serial.println("Invalid selection.");
      }
      continue;
    } else {
      Serial.println("Invalid selection.");
      continue;
    }

    Serial.println("Channel selection:");
    Serial.print("  a = all ");
    Serial.print(type == SENSOR_PT ? "PT" : "LC");
    Serial.println(" channels");
    Serial.println("  number = choose how many channels manually");

    std::vector<int> channels;
    bool compactOutput = false;
    while (true) {
      Serial.print("Enter channel selection (a or count): ");
      String selection = readLineTrimmed();

      if (selection == "a" || selection == "A") {
        channels.reserve(maxChannels);
        for (int ch = 0; ch < maxChannels; ch++) {
          channels.push_back(ch);
        }
        compactOutput = true;
        break;
      }

      float maybeCount = 0.0f;
      if (!parseFloatStrict(selection, maybeCount)) {
        Serial.println("Invalid input. Enter a or an integer channel count.");
        continue;
      }

      int count = (int)maybeCount;
      if (maybeCount != (float)count || count < 1 || count > maxChannels) {
        Serial.print("Enter an integer from 1 to ");
        Serial.print(maxChannels);
        Serial.print(", or a for all ");
        Serial.print(type == SENSOR_PT ? "PT" : "LC");
        Serial.println(" channels.");
        continue;
      }

      channels = readUniqueChannels(count, maxChannels);
      compactOutput = false;
      break;
    }

    calibrateChannelBatch(type, channels, compactOutput);

    Serial.println();
    Serial.println("Batch complete.");
  }
}
