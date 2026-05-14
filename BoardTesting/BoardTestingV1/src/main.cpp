#include <Arduino.h>
// #include <ADS8688_Tester.cpp>
#include <PinOutputTester.h>

void setup(){
    int testPins[] = {34, 35, 36, 4, 7};
    runPinOutputTest(testPins, 5);
}

void loop(){
    delay(1000);
}