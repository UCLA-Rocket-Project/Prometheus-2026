#include <Arduino.h>
#include <ADS8688_Tester.h>
#include <PinOutputTester.h>

void setup(){
    // int testPins[] = {34, 35, 36, 4, 7};
    // runPinOutputTest(testPins, 5);

    runADS8688Test(35, 34, 48, 36);
}

void loop(){
    delay(1000);
}