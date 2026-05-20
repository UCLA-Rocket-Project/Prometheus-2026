#include <Arduino.h>

#define PIN_FILL 33
#define PIN_DUMP 34
#define PIN_VENT 35
#define PIN_PURGE 36
#define PIN_QD 37
#define PIN_MPV 38
#define PIN_IGNITE 39
#define PIN_OTHER 40

#define PIN_LED1 1
#define PIN_LED2 2

volatile unsigned long last_msg_time = 0;
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

    bool inSafeState = true;
    digitalWrite(PIN_LED1, inSafeState ? HIGH : LOW);
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

    pinMode(PIN_LED1, OUTPUT);
    pinMode(PIN_LED2, OUTPUT);
}
void flashLEDs(int numFlashes = 10, int onMs = 50, int offMs = 200){
    for(int i = 0; i < numFlashes; i++){
        digitalWrite(PIN_LED1, HIGH);
        digitalWrite(PIN_LED2, HIGH);

        delay(onMs);

        digitalWrite(PIN_LED1, LOW);
        digitalWrite(PIN_LED2, LOW);

        delay(offMs);
    }
}

bool parseAndQueueCommand(const char* payload, size_t length) {
    // Validates packet format and enqueues latest command.
    // Validation checks
    if (length != 11) {
        Serial.print("Invalid packet length. Received: ");
        Serial.println(length);
        return false;
    }

    if (payload[0] != 'A' || payload[length - 1] != 'Z') {
        Serial.println("Invalid start/end markers");
        return false;
    }

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
        return false;
    }

    last_msg_time = millis();
    comms_lost = false;
    return true;
}
void taskSerialCommandInput(void* pvParameters) {
    // Reads one command per line from serial monitor.
    // Expected packet format: AxxxxxxxxxZ (11 chars total).
    String line;

    while (true) {
        while (Serial.available() > 0) {
            char c = (char)Serial.read();
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                if (line.length() > 0) {
                    Serial.println("\n=== SERIAL MESSAGE RECEIVED ===");
                    Serial.print("Raw payload: ");
                    Serial.println(line);
                    parseAndQueueCommand(line.c_str(), line.length());
                    line = "";
                }
            } else {
                line += c;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


// FreeRTOS Tasks
void taskPacketHandler(void* pvParameters) {
    // Takes the command packet off of commandQueue, added by taskSerialCommandInput
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

            bool inSafeState = 
            !cmd.fill && 
            !cmd.dump && 
            !cmd.vent && 
            !cmd.qd && 
            !cmd.purge && 
            (
                !cmd.armState ||
                (!cmd.mpv && !cmd.ignite)
            );
            digitalWrite(PIN_LED1, inSafeState ? HIGH : LOW);
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


void setup() {
    Serial.begin(115200);
    delay(1000);

    initializeAllOutputPins();
    enterSafeState("INITIAL SAFE");

    flashLEDs();

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
    xTaskCreatePinnedToCore(taskSerialCommandInput, "Serial Input Task", 4096, NULL, 1, NULL, 0);

    last_msg_time = millis();

    Serial.println("Setup complete.");
    Serial.println("Serial command format: AxxxxxxxxxZ (11 chars), send one packet per line.");
}

void loop(){}
