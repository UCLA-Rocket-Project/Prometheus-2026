#include "new_lcpt_calibrator.h"
#include <Arduino.h>
#include <string>

float getLCValue();
float getPTValue(int correctChannel);

float m1 = 100;
float b1 = 20;
float m2 = 100;
float b2 = 20;

std::vector<float> xVals;
std::vector<float> yVals;
std::vector<float> yVals2;
std::vector<float> yVals3;
std::vector<float> yVals4;

void computeBestFit(std::vector<float> yValsT) {
float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;

for (int i = 0; i < xVals.size(); i++)
{
sumX += xVals.at(i);
sumY += yValsT.at(i);
sumXY += xVals.at(i) * yValsT.at(i);
sumX2 += xVals.at(i) * xVals.at(i);
}

float n = xVals.size();

float m = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
float b = (sumY - m * sumX) / n;

Serial.println("\n--- Line of Best Fit ---");
Serial.print("m = ");
Serial.println(m, 6);
Serial.print("b = ");
Serial.println(b, 6);
}

void computeBestFit(float& m, float& b)
{
float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;

for (int i = 0; i < xVals.size(); i++)
{
sumX += xVals.at(i);
sumY += yVals.at(i);
sumXY += xVals.at(i) * yVals.at(i);
sumX2 += xVals.at(i) * xVals.at(i);
}

float n = xVals.size();

float new_m = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
float new_b = (sumY - m * sumX) / n;

m = new_m;
b = new_b;
}

void startCalibration()
{
Serial.begin(115200);
Serial.println("Starting Calibration:");
Serial.println("Enter Value(enter stop to end calibration): ");
while (!Serial.available())
{
delay(10);
}
String line = Serial.readStringUntil('\n');
while(line.length() == 0)
{
Serial.println("Invalid Value. Re-enter Value: ");
line = Serial.readStringUntil('\n');
line.trim();
}
while(true)
{
if(line == "compute")
{
computeBestFit(yVals);
// computeBestFit(yVals2);
// computeBestFit(yVals3);
// computeBestFit(yVals4);
}
else if(line == "print")
{
printTable(yVals);
// printTable(yVals2);
// printTable(yVals3);
// printTable(yVals4);
}
else if(line == "0" || line.toFloat() > 0)
{
float userInput = line.toFloat();
float dataInput1 = getPTValue(0);
float dataInput2 = getPTValue(1);
float dataInput3 = getPTValue(2);
float dataInput4 = getPTValue(3);
add(userInput, dataInput1, dataInput2, dataInput3, dataInput4);
}
else if (line == "clear") {
  clear();
}
else
{
Serial.println("Invalid command/input.");
}

Serial.println("Enter Next Value(enter stop to end calibration): ");
while (!Serial.available())
{
delay(10);
}
line = Serial.readStringUntil('\n');
line.trim();
}
Serial.print("Ended Calibration.");
}

void clear()
{
while(!xVals.empty())
{
xVals.pop_back();
yVals.pop_back();
yVals2.pop_back();
yVals3.pop_back();
yVals4.pop_back();
}
}

void removeLatest()
{
xVals.pop_back();
yVals.pop_back();
yVals2.pop_back();
yVals3.pop_back();
yVals4.pop_back();
}

bool remove(float xVal, std::vector<float> yValsT)
{
int removeIndex = -1;
int lastIndex = xVals.size() - 1;
for(int i = 0; i < xVals.size(); i++)
{
if(xVals.at(i) == xVal)
{
removeIndex = i;
break;
}
}
if(removeIndex == -1)
{
return false;
}
float tempX = xVals.at(removeIndex);
float tempY = yVals.at(removeIndex);
float tempY2 = yVals2.at(removeIndex);
float tempY3 = yVals3.at(removeIndex);
float tempY4 = yVals4.at(removeIndex);
xVals.at(removeIndex) = xVals.at(lastIndex);
yVals.at(removeIndex) = yVals.at(lastIndex);
yVals2.at(removeIndex) = yVals2.at(lastIndex);
yVals3.at(removeIndex) = yVals3.at(lastIndex);
yVals4.at(removeIndex) = yVals4.at(lastIndex);
xVals.at(lastIndex) = tempX;
yVals.at(lastIndex) = tempY;
yVals2.at(lastIndex) = tempY2;
yVals3.at(lastIndex) = tempY3;
yVals4.at(lastIndex) = tempY4;
xVals.pop_back();
yVals.pop_back();
yVals2.pop_back();
yVals3.pop_back();
yVals4.pop_back();return true;
}

void add(float xVal, float yVal, float yVal2, float yVal3, float yVal4)
{
xVals.push_back(xVal);
yVals.push_back(yVal);
yVals2.push_back(yVal2);
yVals3.push_back(yVal3);
yVals4.push_back(yVal4);
}

int size()
{
return xVals.size();
}

void sizePrint()
{
Serial.println(xVals.size());
}

void printTable(std::vector<float> yValsT)
{
Serial.println("X Value | Y Value");
for(int i = 0; i < xVals.size(); i++)
{
Serial.print(xVals.at(i));
Serial.print("|");
Serial.println(yValsT.at(i));
}
}

void sort()
{
int lowest;
for(int i = 0; i < xVals.size() - 1; i++)
{
lowest = i;
for(int j = i + 1; j < xVals.size(); j++)
{
if(xVals.at(j) < xVals.at(lowest))
{
lowest = j;
}
}
float tempX = xVals.at(lowest);
float tempY = yVals.at(lowest);
float tempY2 = yVals2.at(lowest);
float tempY3 = yVals3.at(lowest);
float tempY4 = yVals4.at(lowest);
xVals.at(lowest) = xVals.at(i);
yVals.at(lowest) = yVals.at(i);
yVals2.at(lowest) = yVals2.at(i);
yVals3.at(lowest) = yVals3.at(i);
yVals4.at(lowest) = yVals4.at(i);
xVals.at(i) = tempX;
yVals.at(i) = tempY;
yVals2.at(i) = tempY2;
yVals3.at(i) = tempY3;
yVals4.at(i) = tempY4;
}
}
