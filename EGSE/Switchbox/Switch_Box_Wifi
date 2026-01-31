////////// ARDUINO UNO CODE //////////
/*
#include "SoftwareSerial.h"

#define TX 12
#define RX 13

EspSoftwareSerial::UART rs485Serial;
rs485Serial.enableIntTx(false)

rs485Serial.begin(115200, EspSoftwareSerial::SWSERIAL_8N1, RX, TX)
*/
//#include <HardwareSerial.h>
#include <WiFi.h>
#include <PubSubClient.h>





#define RO_PIN 19  //32 boRD
#define DI_PIN 18  //35
//jumper wire to HIGH

#define RE 26  //13
#define DE 4
//RE to 26
//DE to 27

#define fill 33        //23
#define abortValve 22  //21
#define dump 27     //19
#define vent 13        //18
#define qd 23           //4 // always on
#define ignite 32       //2 or maybe 32?
#define mpv 35         //15
#define heatpad 5      //17
#define siren 13       //22

unsigned long long delay_time = 250;
unsigned long long last_time = 0;

const char* ssid = "ILAY";
const char* password = "lebronpookie123";
const char* mqtt_server = "192.168.0.100";
const char* SWITCHBOX_TOPIC = "switchbox/commands";

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");
}

//HardwareSerial rs485Serial(2);
//SoftwareSerial rs485Serial(RO_PIN, DI_PIN);

void reconnect() {
  while (!client.connected()) {
    if (client.connect("SWITCH_BOX")) {
      Serial.println("MQTT connected");

    } else {
      delay(1000);
    }
  }
}

void setup() {
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);
  digitalWrite(RE, HIGH);
  digitalWrite(DE, HIGH);

  pinMode(fill, INPUT_PULLDOWN);
  pinMode(abortValve, INPUT_PULLDOWN);
  pinMode(dump, INPUT_PULLDOWN);
  pinMode(vent, INPUT_PULLDOWN);
  pinMode(qd, INPUT_PULLDOWN);
  pinMode(ignite, INPUT_PULLDOWN);
  pinMode(mpv, INPUT_PULLDOWN);
  pinMode(siren, INPUT_PULLDOWN);

  Serial.begin(115200);
      //rs485Serial.begin(115200, SERIAL_8N1, RO_PIN, DI_PIN);
    setup_wifi();
  client.setServer(mqtt_server, 1883);

  Serial.println("Setup Complete");

  last_time = millis();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if ((millis() - last_time) > delay_time) {
    String message = "A";
    message += String(digitalRead(abortValve));
    message += String(digitalRead(qd));
    message += String(digitalRead(vent));
    message += String(digitalRead(ignite));
    message += String(digitalRead(fill));
    message += String(digitalRead(dump));
    message += String(digitalRead(mpv));
    message += '0';
    message += '0';
    message += '0';
    message += 'Z';

        /*
    String Purge = String(digitalRead(purge));
    String Fill = String(digitalRead(fill));
    String AbortValve = String(digitalRead(abortValve));
    String Dump = String(digitalRead(dump));
    String Vent = String(digitalRead(vent));
    String QD = String (digitalRead(qd));
    String Ignite = String(digitalRead(ignite));
    String MPV = String(digitalRead(mpv));
    String Heatpad = String(digitalRead(heatpad));
    String Siren = String(digitalRead(siren));

    message = ('A' + AbortValve + QD + Vent + Ignite + Purge + Fill + Dump + Heatpad + MPV + Siren + 'Z');
    //rs485Serial.println(message);
    */

      client.publish(SWITCHBOX_TOPIC, message.c_str(), true);
    Serial.println("Sent:");
    Serial.println(message);

    last_time = millis();
  }
}
