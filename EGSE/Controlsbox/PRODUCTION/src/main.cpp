#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define PIN_FILL 33
#define PIN_DUMP 34
#define PIN_VENT 35
#define PIN_PURGE 36
#define PIN_QD 37
#define PIN_MPV 38
#define PIN_IGNITE 39
#define PIN_OTHER 40
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

const char* mqtt_server = "192.168.0.100";  // ipconfig in terminal
const char* SWITCHBOX_TOPIC = "switchbox/commands";

WiFiClient espClient;
PubSubClient mosquittoClient(espClient);

// Helper Functions
void enterSafeState(const char* reason) {
    // This function sets all pins to OFF
    // If commands are still coming in, this will have little effect

    Serial.print("SAFE SHUTDOWN: ");
    Serial.println(reason);

    // LOW = on, HIGH = off (reason: switchbox wires mixed)
    digitalWrite(PIN_IGNITE, LOW);
    digitalWrite(PIN_FILL, LOW);
    digitalWrite(PIN_VENT, LOW);
    digitalWrite(PIN_DUMP, LOW);
    digitalWrite(PIN_QD, LOW);
    digitalWrite(PIN_MPV, LOW);
    digitalWrite(PIN_PURGE, LOW);
    digitalWrite(PIN_OTHER, LOW);
}
void initializeAllOutputPins() {
    pinMode(PIN_IGNITE, OUTPUT);
    pinMode(PIN_FILL, OUTPUT);
    pinMode(PIN_VENT, OUTPUT);
    pinMode(PIN_DUMP, OUTPUT);
    pinMode(PIN_QD, OUTPUT);
    pinMode(PIN_MPV, OUTPUT);
    pinMode(PIN_PURGE, OUTPUT);
    pinMode(PIN_OTHER, OUTPUT);
}


// WIFI + Mosquitto
void mqttMessageCallback(char* topic, byte* payload, unsigned int length) {
    // Called when a message is received over mosquitto
    // Validates the packet format
    // Adds to queue to be handled by freeRTOS

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
        Serial.print("Invalid packet length! Received length: ");
        Serial.println(length);
        return;
    }

    if (payload[0] != 'A' || payload[length - 1] != 'Z') {
        Serial.println("Invalid start/end markers");
        return;
    }

    // Serial.println("Packet format valid");

    CommandPacket cmd;

    cmd.abortValve = payload[1] == '1';
    cmd.qd = payload[2] == '1';
    cmd.vent = payload[3] == '1';
    cmd.ignite = payload[4] == '1';
    cmd.fill = payload[5] == '1';
    cmd.dump = payload[6] == '1';
    cmd.mpv = payload[7] == '1';
    cmd.purge = payload[8] == '1';
    cmd.armState = payload[9] == '1';

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
void setUpWifi() {
    // Connect to the wifi network running mosquitto
    // This function doesn't interact with mosquitto at all,
    // See connectToMosquittoTopic

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

    Serial.println();
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}
void connectToMosquittoTopic() {
    // Connects to the mosquitto topic / broker
    // @Ryder change this description if it is wrong

    //TODO: should this be moved to after next IF statement>?
    mosquittoClient.setServer(mqtt_server, 1883);
    mosquittoClient.setCallback(mqttMessageCallback);

    if (mosquittoClient.connected())
        return;

    String clientID = "CONTROL_BOX-" + WiFi.macAddress();

    if (mosquittoClient.connect(clientID.c_str())) {
        mosquittoClient.subscribe(SWITCHBOX_TOPIC);
        Serial.println("Connected to MQTT broker");
    } else {
        Serial.print("MQTT failed, rc=");
        Serial.println(mosquittoClient.state());
    }
}


// FreeRTOS Tasks
void taskPacketHandler(void* pvParameters) {
    // Takes the command packet off of commandQueue, added by mqttMessageCallback
    // Changes outputs accordingly

    CommandPacket cmd;

    while (true) {
        if (xQueueReceive(commandQueue, &cmd, portMAX_DELAY)) {
            if (cmd.abortValve) {
                enterSafeState("ABORT COMMAND");
                continue;
            }
            digitalWrite(PIN_FILL, cmd.fill ? HIGH : LOW);
            digitalWrite(PIN_DUMP, cmd.dump ? HIGH : LOW);
            digitalWrite(PIN_VENT, cmd.vent ? HIGH : LOW);
            digitalWrite(PIN_QD, cmd.qd ? HIGH : LOW);
            digitalWrite(PIN_PURGE, cmd.purge ? HIGH : LOW);
            if (cmd.armState) {  // allow ignite and mpv
                digitalWrite(PIN_MPV, cmd.mpv ? HIGH : LOW);
                digitalWrite(PIN_IGNITE, cmd.ignite ? HIGH : LOW);
            } else {
                digitalWrite(PIN_MPV, LOW);
                digitalWrite(PIN_IGNITE, LOW);
            }
        }
    }
}
void taskShutdownOnCommsLost(void* pvParameters) {
    // When communication is lost, set everything to safestate
    while (true) {
        if (!comms_lost && millis() - last_msg_time > COMMS_TIMEOUT) {
            comms_lost = true;
            enterSafeState("COMMS TIMEOUT");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void taskMosquittoTopicReconnection(void* pvParameters) {
    // Wait for wifi connection,
    // Attempt to reconnect to mosquitto topic while disconnected

    while (true) {
        if (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(500));  // just wait
            continue;
        }
        if (!mosquittoClient.connected()) {
            if (millis() - last_mqtt_attempt > 2000) {
                connectToMosquittoTopic();
                last_mqtt_attempt = millis();
            }
        }
        mosquittoClient.loop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void setup() {
    Serial.begin(115200);
    delay(1000);

    initializeAllOutputPins();
    enterSafeState("INITIAL SAFE");

    //Create FreeRTOS Queue
    commandQueue = xQueueCreate(1, sizeof(CommandPacket));
    if (commandQueue == NULL) {
        // Queue fail: bad memory allocation
        enterSafeState("QUEUE CREATION FAIL");
        while (true) {
        }
    }

    //Assign FreeRTOS Tasks
    //@Ryder: What are these parameters??
    xTaskCreatePinnedToCore(taskPacketHandler, "Control Task", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(taskShutdownOnCommsLost, "Timeout Task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskMosquittoTopicReconnection, "MQTT Task", 4096, NULL, 1, NULL, 0);


    // Initialize Wifi and connect to mosquitto:
    setUpWifi();

    // connect to MQTT server
    connectToMosquittoTopic();

    last_msg_time = millis();

    Serial.println("Setup complete.");
}

void loop(){}
