#include <WiFi.h>
#include <PubSubClient.h>

#define RO_PIN 16
#define DI_PIN 17
#define outlet 2   //32
//#define SER_BUF_SIZE 1024
#define fill 22
#define dump 21
#define vent 19
#define qd 18
#define mpv 5
#define ignite 4   //15
#define abortSiren 23
#define abortValve 13   //13
//unsigned long delay_time = 250;
unsigned long last_msg_time = 0;
unsigned long last_mqtt_attempt = 0;
const unsigned long COMMS_TIMEOUT = 10000;
bool comms_lost = false;


// Replace the next variables with your SSID/Password combination
const char* ssid = "ILAY";
const char* password = "lebronpookie123";
// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.0.100";
// run `ipconfig getifaddr en0` in your macbook terminal
const char* SWITCHBOX_TOPIC = "switchbox/commands";

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (length != 12) {
    Serial.println("Invalid message length");
    return;
  }

  if (payload[0] != 'A' || payload[length - 1] != 'Z') return;

  Serial.println("received!");

  char message[11];
  for (int i = 1; i < length - 1; i++) {
    if (payload[i] != '0' && payload[i] != '1') return;
    message[i - 1] = payload[i];
  }
  message[10] = '\0';

  Serial.print("Received: ");
  Serial.println(message);

  //char received = (char)payload[0];
  /*
    label:
        String message = "";
        if (received != 'A')
          return;
        else {
            for (int i = 1; i < length; i++) {
                received = (char)payload[i];
                if ((i != length - 1 && received != '1' && received != '0') || (i == length - 1 && received != 'Z')){
                    Serial.println("Invalid Message: " + String(received));
                    //goto label;
                    if (payload[0] != 'A' || payload[length-1] != 'Z') {
                      Serial.println("Invalid message end");
                      return;
                    }
                }
                message += received;
            }
            Serial.print("Received: ");
            Serial.println(message);
        }
  */

  const char ACTUATED = '1';

  const short ABORT_VALVE_SWITCH = 0;
  const short QD_SWITCH = 1;
  const short VENT_SWITCH = 2;
  const short IGNITE_SWITCH = 3;
  const short FILL_SWITCH = 4;
  const short DUMP_SWITCH = 5;
  const short MPV_SWITCH = 6;


  while (message[ABORT_VALVE_SWITCH] == ACTUATED) {
    safeShutdown("ABORT VALVE Command: ON -- EMERGENCY SHUTDOWN");
  }

  if (message[ABORT_VALVE_SWITCH] == '0') {
    Serial.println("ABORT VALVE Command: OFF");
    digitalWrite(abortValve, LOW);
  }
  if (message[FILL_SWITCH] == ACTUATED) {
    Serial.println("FILL Command: ON");
    digitalWrite(fill, LOW);
  }
  if (message[FILL_SWITCH] == '0') {
    Serial.println("FILL Command: OFF");
    digitalWrite(fill, HIGH);
  }

  if (message[DUMP_SWITCH] == ACTUATED) {
    Serial.println("DUMP Command: ON");
    digitalWrite(dump, LOW);
  }
  if (message[DUMP_SWITCH] == '0') {
    Serial.println("DUMP Command: OFF");
    digitalWrite(dump, HIGH);
  }

  if (message[VENT_SWITCH] == ACTUATED) {
    Serial.println("VENT Command: ON");
    digitalWrite(vent, LOW);
  }
  if (message[VENT_SWITCH] == '0') {
    Serial.println("VENT Command: OFF");
    digitalWrite(vent, HIGH);
  }

  if (message[QD_SWITCH] == ACTUATED) {
    Serial.println("QD Command: ON");
    digitalWrite(qd, LOW);
  }
  if (message[QD_SWITCH] == '0') {
    Serial.println("QD Command: OFF");
    digitalWrite(qd, HIGH);
  }

  if (message[IGNITE_SWITCH] == ACTUATED) {
    Serial.println("IGNITE Command: ON");
    digitalWrite(ignite, LOW);
  }
  if (message[IGNITE_SWITCH] == '0') {
    Serial.println("IGNITE Command: OFF");
    digitalWrite(ignite, HIGH);
  }

  if (message[MPV_SWITCH] == ACTUATED) {
    Serial.println("MPV Command: ON");
    digitalWrite(mpv, LOW);
  }
  if (message[MPV_SWITCH] == '0') {
    Serial.println("MPV Command: OFF");
    digitalWrite(mpv, HIGH);
  }


  // if(message[OUTLET_SWITCH] == ACTUATED){
  //     Serial.println("OUTLET Command: ON");
  //     digitalWrite(outlet, HIGH);
  // }
  // if(message[OUTLET_SWITCH] == '0'){
  //     Serial.println("OUTLET Command: OFF");
  //     digitalWrite(outlet, LOW);
  // }


  comms_lost = false;
  last_msg_time = millis();
}



void connect_client() {
  if (client.connected())
    return;
  String cid = "CONTROL_BOX-" + WiFi.macAddress();
  if (client.connect(cid.c_str())) {
    client.subscribe(SWITCHBOX_TOPIC);
    Serial.println("Connected to MQTT broker");

  } else {
    Serial.println("Failed to reconnect to MQTT broker");
  }
}

void safeShutdown(const char* reason) {
  Serial.print("SAFE SHUTDOWN: ");
  Serial.println(reason);

  //LOW = on, HIGH = off (reason: switchbox wires mixed)
  //VENT AND DUMP DIFFERENT FROM REST, START OPEN (HARDWARE WILL DO)
  digitalWrite(ignite, HIGH); //off
  digitalWrite(fill, HIGH); //off
  digitalWrite(vent, HIGH); //off
  digitalWrite(dump, HIGH); //off
  digitalWrite(qd, HIGH); //off
  digitalWrite(mpv, HIGH); // off
  digitalWrite(abortValve, LOW); //on
}

void setup() {
  Serial.begin(115200);

  delay(1000);

  Serial.println("HII");

  pinMode(abortSiren, OUTPUT);
  pinMode(ignite, OUTPUT);
  pinMode(fill, OUTPUT);
  pinMode(vent, OUTPUT);
  pinMode(dump, OUTPUT);
  pinMode(qd, OUTPUT);
  pinMode(mpv, OUTPUT);
  // pinMode(purge, OUTPUT);
  pinMode(outlet, OUTPUT);
  pinMode(abortValve, OUTPUT);



  safeShutdown("INITIAL SAFE");
  /*
    digitalWrite(abortSiren, LOW);  //off
    digitalWrite(ignite, LOW);      //off
    digitalWrite(fill, LOW);        //closed
    digitalWrite(vent, LOW);        //open
    digitalWrite(dump, LOW);        //open
    digitalWrite(qd, LOW);          //open
    digitalWrite(mpv, LOW);
    // digitalWrite(purge, LOW);//closed
    digitalWrite(outlet, LOW);      //closed
    digitalWrite(abortValve, LOW);  //closed
  */

  //wifi code
  WiFi.mode(WIFI_STA);
  setup_wifi();

  // connect to MQTT server
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  connect_client();

  last_msg_time = millis();

  Serial.println("done with setup");
}

void loop() {
  if (!comms_lost && millis() - last_msg_time > COMMS_TIMEOUT) {
    // Lost comms → safe state
    comms_lost = true;
    safeShutdown("COMMS TIMEOUT");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Network disconnection detected");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    delay(500);
    unsigned long long disconnect_start = millis();

    while (WiFi.status() != WL_CONNECTED) {
      if ((millis() - disconnect_start) > 20000) {
        safeShutdown("Disconnected for 20s EMERGENCY SHUTDOWN");

      }
      Serial.println("Reconnecting to Wifi...");
      delay(250);

    }

  }
  if (!client.connected()) {
    connect_client();

  }
  client.loop();
}
