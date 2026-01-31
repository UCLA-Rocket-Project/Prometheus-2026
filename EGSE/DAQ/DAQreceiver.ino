// RS485 Receiver (ESP32)
//0: nothing.     1: tank.       2: fill.      3: pneumatics
#include <HardwareSerial.h>

//#define RO_PIN 35  // RX
//#define DI_PIN 32   // TX

#include <string>
#define RO_PIN 35
#define DI_PIN 32
#define RE_PIN 19// Receiver Enable (LOW = enabled)
#define DE_PIN 19// Driver Enable (LOW = disabled)

unsigned long long delay_time = 250;
unsigned long long last_time = 0;
HardwareSerial rs485Serial(2);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  rs485Serial.begin(115200, SERIAL_8N1, RO_PIN, DI_PIN);
  pinMode(RE_PIN, OUTPUT);
  pinMode(DE_PIN, OUTPUT);
  digitalWrite(RE_PIN, LOW);//on
  digitalWrite(DE_PIN, LOW);//off
}
void loop() {
  // put your main code here, to run repeatedly:
  while (!rs485Serial.available() && millis() - last_time > 1000){
    Serial.println("Unavailable...");
  }
  while (rs485Serial.available()){
    String message;
    for (int i = 0; i < 500; i++){
      char received = rs485Serial.read();
      if (received == '\n'){
        break;
      }
      message += String(received);
    }
    //Serial.println("Recieved:");
    String printMsg = String(message);
    Serial.println(printMsg);
    last_time = millis();
  }
}
