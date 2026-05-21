#include <HardwareSerial.h>
#include <Arduino.h>

//Ethernet:
//BLACK WIRE GOES TO 7
//YELLOW WIRE GOES TO 8

#define RO_PIN 44
#define DI_PIN 43
#define DE_RE_PIN 41

#define fill 33
#define dump 34
#define vent 35
#define purge 36
#define qd 37
#define mpv 38
#define ignite 39

unsigned long long delay_time = 250;
unsigned long long last_time = 0;
HardwareSerial rs485Serial(2);

void setup() {
  Serial.begin(115200);
  rs485Serial.begin(115200, SERIAL_8N1, RO_PIN, DI_PIN);
  pinMode(ignite, OUTPUT);
  pinMode(fill, OUTPUT);
  pinMode(vent, OUTPUT);
  pinMode(dump, OUTPUT);
  pinMode(qd, OUTPUT);
  pinMode(mpv, OUTPUT);
  pinMode(purge, OUTPUT);
  digitalWrite(ignite, LOW);
  digitalWrite(fill, LOW);
  digitalWrite(vent, LOW);
  digitalWrite(dump, LOW);
  digitalWrite(qd, LOW);
  digitalWrite(mpv, LOW);
  digitalWrite(purge, LOW);
  /*
  while (!rs485Serial.available() || rs485Serial.read() != 'A'){
    Serial.println("Connection Failed");
  }
  Serial.println("Connection Established");*/
  Serial.println("Setup complete");
  last_time = millis();
}

void loop() {
  while (!rs485Serial.available() && millis() - last_time > 1000){
    Serial.println("Unavailable...");
    if (millis() - last_time > 20000){
      Serial.println("Aborted...");
      digitalWrite(fill, LOW);
      digitalWrite(vent, LOW);
      digitalWrite(dump, LOW);
      digitalWrite(qd, LOW);
      digitalWrite(mpv, LOW);
      digitalWrite(purge, LOW);
      digitalWrite(ignite, LOW);
    }
  }

  while (rs485Serial.available()) {
    char received = rs485Serial.read();
    //label:
    String message;
    if (received == 'A'){
      for (int i = 0; i < 7; i++){
        received = rs485Serial.read();
        if (received != '1' && received != '0'){
          Serial.println("Invalid Message: " + received);
          break;
          // goto label;
        }
        message += received;
      }
      Serial.println("Received");
      Serial.println(message);
    }

    const char ACTUATED = '1';
    //const short PURGE_SWITCH = 8;
    const short FILL_SWITCH = 4;
    //const short ABORT_SIREN_SWITCH = 9;
    const short DUMP_SWITCH = 5;
    const short VENT_SWITCH = 2;
    const short QD_SWITCH = 1;
    const short IGNITE_SWITCH = 3; //6; //3
    const short MPV_SWITCH = 6; //7;
    //const short OUTLET_SWITCH = 0; //7;
    const short ABORT_VALVE_SWITCH = 0; //0
    
    last_time = millis();
    if(message[ABORT_VALVE_SWITCH] == ACTUATED) {
      digitalWrite(ignite, LOW);//off
      digitalWrite(fill, LOW);//closed
      digitalWrite(vent, LOW);//open
      digitalWrite(dump, LOW);//open
      digitalWrite(qd, LOW);//open
      digitalWrite(mpv, LOW);
      digitalWrite(purge, LOW);//closed
      return;
    }    


    //message = ('A' + AbortValve0 + QD1 + Vent2 + Ignite3 + Fill4 + Dump5 + MPV6 + 'Z');

    if(message[FILL_SWITCH] == ACTUATED)
      digitalWrite(fill, HIGH);
    if(message[FILL_SWITCH] == '0')
      digitalWrite(fill, LOW);
    
    
    if(message[DUMP_SWITCH] == ACTUATED)
      digitalWrite(dump, HIGH);
    if(message[DUMP_SWITCH] == '0')
      digitalWrite(dump, LOW);
    
    if(message[VENT_SWITCH] == ACTUATED)
      digitalWrite(vent, HIGH);
    if(message[VENT_SWITCH] == '0')
      digitalWrite(vent, LOW);
    
    
    if(message[QD_SWITCH] == ACTUATED)
      digitalWrite(qd, HIGH);
    if(message[QD_SWITCH] == '0')
      digitalWrite(qd, LOW);
    
    if(message[IGNITE_SWITCH] == ACTUATED)
      digitalWrite(ignite, HIGH);
    if(message[IGNITE_SWITCH] == '0')
      digitalWrite(ignite, LOW);
    
    if(message[MPV_SWITCH] == ACTUATED)
      digitalWrite(mpv, HIGH);
    if(message[MPV_SWITCH] == '0')
      digitalWrite(mpv, LOW);
    
  }
}
