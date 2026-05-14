#include <Arduino.h>

void initializePins(int pins[], int numPins){
    Serial.print("Initializing pins:");
    for(int i = 0; i < numPins; i++){
        pinMode(pins[i], OUTPUT);
        Serial.print(" ");
        Serial.print(pins[i]);
    }
    Serial.println();
}
void batchDigitalWrite(int pins[], int numPins, int value){
    for(int i = 0; i < numPins; i++){
        digitalWrite(pins[i], value);
    }
}
void switchPins(int pins[], int numPins, int onMs, int offMs){
    Serial.print("Writing high...");
    batchDigitalWrite(pins, numPins, HIGH);
    Serial.println(" Done.");
    delay(onMs);
    Serial.print("Writing low...");
    batchDigitalWrite(pins, numPins, LOW);
    Serial.println(" Done.");
    delay(offMs);
}

/// @brief Initializes and continuously flips all given pins on and off
void runPinOutputTest(int pins[], int numPins, int onMs = 2000, int offMs = 2000){
    while(!Serial)delay(100);
    delay(1000);
    Serial.println("Starting Pin Output Test.");

    initializePins(pins, numPins);

    while(true){
        switchPins(pins, numPins, onMs, offMs);
    }
}