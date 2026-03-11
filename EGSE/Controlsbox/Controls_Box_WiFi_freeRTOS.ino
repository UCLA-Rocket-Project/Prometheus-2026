#include <WiFi.h>
#include <PubSubClient.h>

#define RO_PIN 16
#define DI_PIN 17
#define outlet 2
#define fill 22
#define dump 21
#define vent 19
#define qd 18
#define mpv 5
#define ignite 4
#define abortSiren 23
#define abortValve 13
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

void callback(char* topic, byte* payload, unsigned int length) {  //only reads info
  if (length != 12) return;
  if (payload[0] != 'A' || payload[length - 1] != 'Z') return;

  CommandPacket cmd;

  cmd.abortValve = payload[1] == '1';
  cmd.qd = payload[2] == '1';
  cmd.vent = payload[3] == '1';
  cmd.ignite = payload[4] == '1';
  cmd.fill = payload[5] == '1';
  cmd.dump = payload[6] == '1';
  cmd.mpv = payload[7] == '1';

  cmd.timestamp = millis();

  if (commandQueue != NULL)
    xQueueOverwrite(commandQueue, &cmd);

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

  } else
    Serial.println("Failed to reconnect to MQTT broker");
}

void safeShutdown(const char* reason) {
  Serial.print("SAFE SHUTDOWN: ");
  Serial.println(reason);

  //LOW = on, HIGH = off (reason: switchbox wires mixed)
  //HARDWARE WILL SWITCH VENT AND DUMP, KEEP SAME AS REST
  digitalWrite(ignite, HIGH);     //off
  digitalWrite(fill, HIGH);       //off
  digitalWrite(vent, HIGH);       //off
  digitalWrite(dump, HIGH);       //off
  digitalWrite(qd, HIGH);         //off
  digitalWrite(mpv, HIGH);        // off
  digitalWrite(abortValve, LOW);  //on
}

void controlTask(void* pvParameters) {  //process callback info TASK
  CommandPacket cmd;

  while (true) {
    if (xQueueReceive(commandQueue, &cmd, portMAX_DELAY)) {
      if (cmd.abortValve) {
        safeShutdown("ABORT COMMAND");
        continue;
      }

      digitalWrite(fill, cmd.fill ? LOW : HIGH);
      digitalWrite(dump, cmd.dump ? LOW : HIGH);
      digitalWrite(vent, cmd.vent ? LOW : HIGH);
      digitalWrite(qd, cmd.qd ? LOW : HIGH);
      digitalWrite(ignite, cmd.ignite ? LOW : HIGH);
      digitalWrite(mpv, cmd.mpv ? LOW : HIGH);
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
      WiFi.begin(ssid, password);
      vTaskDelay(pdMS_TO_TICKS(5));
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
  Serial.println("HII");  //hi

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
