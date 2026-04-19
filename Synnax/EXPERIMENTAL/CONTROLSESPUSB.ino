#include <Arduino.h>

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
const unsigned long COMMS_TIMEOUT = 10000;
const uint8_t PACKET_LENGTH = 11;
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

// Parse incoming bytes and push a CommandPacket to the queue when a valid frame arrives.
// Packet format: A [abort][qd][vent][ignite][fill][dump][mpv][purge][armed] Z  (11 bytes)
void parseAndQueue(uint8_t* buf) {
  if (buf[0] != 'A' || buf[PACKET_LENGTH - 1] != 'Z') {
    Serial.println("Bad packet markers");
    return;
  }
  CommandPacket cmd;
  cmd.abortValve = buf[1] == '1';
  cmd.qd         = buf[2] == '1';
  cmd.vent       = buf[3] == '1';
  cmd.ignite     = buf[4] == '1';
  cmd.fill       = buf[5] == '1';
  cmd.dump       = buf[6] == '1';
  cmd.mpv        = buf[7] == '1';
  cmd.purge      = buf[8] == '1';
  cmd.armState   = buf[9] == '1';
  cmd.timestamp  = millis();
  xQueueOverwrite(commandQueue, &cmd);
  last_msg_time = millis();
  comms_lost = false;
  Serial.println("Packet OK");
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
      if (cmd.armState) { //allow ignite and mpv
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

// Read bytes from Serial, scan for the 'A' start marker, then collect the
// full 11-byte packet and hand it off to parseAndQueue().
void serialRxTask(void* pvParameters) {
  uint8_t buf[PACKET_LENGTH];
  uint8_t idx = 0;
  while (true) {
    while (Serial.available()) {
      uint8_t b = (uint8_t)Serial.read();
      if (b == 'A') {
        idx = 0;
        buf[idx++] = b;
      } else if (idx > 0) {
        buf[idx++] = b;
        if (idx == PACKET_LENGTH) {
          parseAndQueue(buf);
          idx = 0;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
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

  xTaskCreatePinnedToCore(controlTask,  "Control Task",   4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(timeoutTask,  "Timeout Task",   4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(serialRxTask, "Serial Rx Task", 4096, NULL, 1, NULL, 0);

  last_msg_time = millis();

  Serial.println("Setup complete - waiting for serial packets from Pi");
}

void loop() {}