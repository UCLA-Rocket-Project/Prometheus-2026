#include <WiFi.h>
#include <PubSubClient.h>

#define RO_PIN 16
#define DI_PIN 17
#define PIN_FILL 22
#define PIN_DUMP 21
#define PIN_VENT 19
#define PIN_QD 18
#define PIN_MPV 5
#define PIN_IGNITE 4
#define PIN_ABORT_VALVE 13
#define PIN_ABORT_SIREN 23
#define PIN_OUTLET 2
#define PIN_PURGE 15
volatile unsigned long last_msg_time = 0;
unsigned long last_mqtt_attempt = 0;
const unsigned long COMMS_TIMEOUT = 10000;
volatile bool comms_lost = false;


QueueHandle_t commandQueue;
struct CommandPacket {
  bool abortValve;
  bool qd;
  bool vent;
  bool ignite;
  bool fill;
  bool dump;
  bool mpv;
  bool purge;
  bool armState;
  uint32_t timestamp;
};

// Replace the next variables with your SSID/Password combination (router)
const char* ssid = "ILAY";
const char* password = "lebronpookie123";

const char* mqtt_server = "192.168.0.100";  //ipconfig in terminal
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
  Serial.println("\n=== MQTT MESSAGE RECEIVED ===");

  Serial.print("Topic: ");
  Serial.println(topic);

  Serial.print("Length: ");
  Serial.println(length);

  Serial.print("Raw payload: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Validation checks
  if (length != 11) {
    Serial.println("Invalid length");
    return;
  }

  if (payload[0] != 'A' || payload[length - 1] != 'Z') {
    Serial.println("Invalid start/end markers");
    return;
  }

  Serial.println("Packet format valid");

  CommandPacket cmd;

  cmd.abortValve = payload[1] == '1';
  cmd.qd         = payload[2] == '1';
  cmd.vent       = payload[3] == '1';
  cmd.ignite     = payload[4] == '1';
  cmd.fill       = payload[5] == '1';
  cmd.dump       = payload[6] == '1';
  cmd.mpv        = payload[7] == '1';
  cmd.purge      = payload[8] == '1';
  cmd.armState   = payload[9] == '1';

  cmd.timestamp = millis();

  if (commandQueue != NULL) {
    xQueueOverwrite(commandQueue, &cmd);
    Serial.println("Command pushed to queue");
  } else {
    Serial.println("Queue is NULL");
  }

  last_msg_time = millis();
  comms_lost = false;
}
void connect_client() {
  if (client.connected())
    return;

  String cid = "CONTROL_BOX-" + WiFi.macAddress();

  if (client.connect(cid.c_str())) {
    client.subscribe(SWITCHBOX_TOPIC);
    Serial.println("Connected to MQTT broker");
  } else {
    Serial.print("MQTT failed, rc=");
    Serial.println(client.state()); 
  }
}

void safeShutdown(const char* reason) {
  Serial.print("SAFE SHUTDOWN: ");
  Serial.println(reason);

  //LOW = on, HIGH = off (reason: switchbox wires mixed)
  //HARDWARE WILL SWITCH VENT AND DUMP, KEEP SAME AS REST
  digitalWrite(PIN_IGNITE, HIGH);     //off
  digitalWrite(PIN_FILL, HIGH);       //off
  digitalWrite(PIN_VENT, HIGH);       //off
  digitalWrite(PIN_DUMP, HIGH);       //off
  digitalWrite(PIN_QD, HIGH);         //off
  digitalWrite(PIN_MPV, HIGH);        // off
  digitalWrite(PIN_PURGE, HIGH);
  digitalWrite(PIN_ABORT_VALVE, LOW);  //on
}

void controlTask(void* pvParameters) {  //process callback info TASK
  CommandPacket cmd;

  while (true) {
    if (xQueueReceive(commandQueue, &cmd, portMAX_DELAY)) {
      if (cmd.abortValve) {
        safeShutdown("ABORT COMMAND");
        continue;
      }
      digitalWrite(PIN_FILL, cmd.fill ? LOW : HIGH);
      digitalWrite(PIN_DUMP, cmd.dump ? LOW : HIGH);
      digitalWrite(PIN_VENT, cmd.vent ? LOW : HIGH);
      digitalWrite(PIN_QD, cmd.qd ? LOW : HIGH);
      digitalWrite(PIN_PURGE, cmd.purge ? LOW : HIGH);
      if (!cmd.armState) { //allow ignite and mpv
        digitalWrite(PIN_MPV, cmd.mpv ? LOW : HIGH);
        digitalWrite(PIN_IGNITE, cmd.ignite ? LOW : HIGH);
      }
      else {
        digitalWrite(PIN_MPV, HIGH);
        digitalWrite(PIN_IGNITE, HIGH);
      }
    }
  }
}

void timeoutTask(void* pvParameters) {  //timeout (obviously) TASK
  while (true) {
    if (!comms_lost && millis() - last_msg_time > COMMS_TIMEOUT) {
      comms_lost = true;
      safeShutdown("COMMS TIMEOUT");
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void mqttTask(void* pvParameters) {  //wifi handling TASK
  while (true) {
  if (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));  // just wait
    continue;
  }
    if (!client.connected()) {
      if (millis() - last_mqtt_attempt > 2000) {
        connect_client();
        last_mqtt_attempt = millis();
      }
    }
    client.loop();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_ABORT_SIREN, OUTPUT);
  pinMode(PIN_IGNITE, OUTPUT);
  pinMode(PIN_FILL, OUTPUT);
  pinMode(PIN_VENT, OUTPUT);
  pinMode(PIN_DUMP, OUTPUT);
  pinMode(PIN_QD, OUTPUT);
  pinMode(PIN_MPV, OUTPUT);
  pinMode(PIN_PURGE, OUTPUT);
  pinMode(PIN_OUTLET, OUTPUT);
  pinMode(PIN_ABORT_VALVE, OUTPUT);

  safeShutdown("INITIAL SAFE");  // start in safe mode

  commandQueue = xQueueCreate(1, sizeof(CommandPacket));

  if (commandQueue == NULL) { //queue fail, BAD MEMORY ALLOCATION
    safeShutdown("QUEUE CREATION FAIL");
    while (true);
  }

  xTaskCreatePinnedToCore(controlTask, "Control Task", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(timeoutTask, "Timeout Task", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(mqttTask, "MQTT Task", 4096, NULL, 1, NULL, 0);

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

void loop() {}
