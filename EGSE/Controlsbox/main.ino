#include <HardwareSerial.h>
#define RO_PIN 16
#define DI_PIN 17
#define outlet 2 //32
//#define SER_BUF_SIZE 1024
#define fill 22
#define dump 21
#define vent 19
#define qd 18
#define purge 5
#define mpv 5//4
#define ignite 4 //15
#define abortSiren 23
#define abortValve 13  //13
unsigned long long delay_time = 250;
unsigned long long last_time = 0;
HardwareSerial rs485Serial(2);

void setup() {
  Serial.begin(115200);
  rs485Serial.begin(115200, SERIAL_8N1, RO_PIN, DI_PIN);
  pinMode(abortSiren, OUTPUT);
  pinMode(ignite, OUTPUT);
  pinMode(fill, OUTPUT);
  pinMode(vent, OUTPUT);
  pinMode(dump, OUTPUT);
  pinMode(qd, OUTPUT);
  pinMode(mpv, OUTPUT);
  pinMode(purge, OUTPUT);
  pinMode(outlet, OUTPUT);
  pinMode(abortValve, OUTPUT);
  digitalWrite(abortSiren, LOW);//off
  digitalWrite(ignite, HIGH);//off
  digitalWrite(fill, HIGH);//closed
  digitalWrite(vent, HIGH);//open
  digitalWrite(dump, HIGH);//open
  digitalWrite(qd, HIGH);//open
  digitalWrite(mpv, HIGH);
  digitalWrite(purge, HIGH);//closed
  digitalWrite(outlet, HIGH);//closed
  digitalWrite(abortValve, LOW);//closed
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
     if (millis() - last_time > 10000){
       digitalWrite(abortSiren, HIGH);
    }
    if (millis() - last_time > 20000){
      Serial.println("Aborted...");
      digitalWrite(ignite, LOW);
      digitalWrite(fill, HIGH);
      digitalWrite(vent, HIGH);
      digitalWrite(dump, HIGH);
      digitalWrite(qd, HIGH);
      digitalWrite(mpv, HIGH);
      digitalWrite(purge, HIGH);
      digitalWrite(ignite, LOW);
      digitalWrite(abortValve, LOW);
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

    //message = ('A' + AbortValve0 + QD1 + Vent2 + Ignite3 + Fill4 + Dump5 + MPV6 + 'Z');

    if(message[FILL_SWITCH] == ACTUATED)
      digitalWrite(fill, LOW);
    if(message[FILL_SWITCH] == '0')
      digitalWrite(fill, HIGH);
    
/*   if(message[ABORT_SIREN_SWITCH] == ACTUATED)
      digitalWrite(abortSiren, HIGH);
    if(message[ABORT_SIREN_SWITCH] == '0')
      digitalWrite(abortSiren, LOW);*/
    
    if(message[DUMP_SWITCH] == ACTUATED)
      digitalWrite(dump,LOW);
    if(message[DUMP_SWITCH] == '0')
      digitalWrite(dump, HIGH);
    
    if(message[VENT_SWITCH] == ACTUATED)
      digitalWrite(vent,LOW);
    if(message[VENT_SWITCH] == '0')
      digitalWrite(vent, HIGH);
    
   /* if(message[PURGE_SWITCH] == ACTUATED)
      digitalWrite(purge, LOW);
    if(message[PURGE_SWITCH] == '0')
      digitalWrite(purge, HIGH);*/
    
    if(message[QD_SWITCH] == ACTUATED)
      digitalWrite(qd,LOW);
    if(message[QD_SWITCH] == '0')
      digitalWrite(qd, HIGH);
    
    if(message[IGNITE_SWITCH] == ACTUATED)
      digitalWrite(ignite, LOW);
    if(message[IGNITE_SWITCH] == '0')
      digitalWrite(ignite, HIGH);
    
    if(message[MPV_SWITCH] == ACTUATED)
      digitalWrite(mpv, LOW);
    if(message[MPV_SWITCH] == '0')
      digitalWrite(mpv, HIGH);
    
/*    if(message[OUTLET_SWITCH] == ACTUATED)
      digitalWrite(outlet, LOW);
    if(message[OUTLET_SWITCH] == '0')
    digitalWrite(outlet, HIGH); */
    
    if(message[ABORT_VALVE_SWITCH] == ACTUATED) {
      digitalWrite(ignite, LOW);//off
      digitalWrite(fill, HIGH);//closed
      digitalWrite(vent, HIGH);//open
      digitalWrite(dump, HIGH);//open
      digitalWrite(qd, HIGH);//open
      digitalWrite(mpv, HIGH);
      digitalWrite(purge, HIGH);//closed
      digitalWrite(abortValve, HIGH);
    }
    if(message[ABORT_VALVE_SWITCH] == '0')
      digitalWrite(abortValve, LOW);
    

    last_time = millis();
  
  }
}
