#include <Arduino.h>
// #include <SPI.h>

// This is untested

//change this!
const int numPins = 0;
const int pins[numPins] = {};

void initializePins(){
    for(int i = 0; i < numPins; i++){
        pinMode(pins[i], OUTPUT);
    }
}
void batchDigitalWrite(int value){
    for(int i = 0; i < numPins; i++){
        digitalWrite(pins[i], value);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    initializePins();
}

void loop() {
    batchDigitalWrite(LOW);
    delay(2000);
    batchDigitalWrite(HIGH);
    delay(2000);
}
