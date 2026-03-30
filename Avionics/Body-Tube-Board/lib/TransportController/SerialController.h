#ifndef SERIAL_CONTROLLER_H
#define SERIAL_CONTROLLER_H

#include "GlobalStore.h"
#include "TransportController.h"
#include "freertos/idf_additions.h"
#include <FreeRTOS.h>
#include <freertos/ringbuf.h>

#define SERIAL_BAUDRATE                       115200
#define SERIAL_COMMAND_BUFFER_SIZE            16
#define SERIAL_RINGBUFFER_SIZE                2048
#define SERIAL_BUF_SIZE                       1024
#define SERIAL_RESPONSE_START_SEQUENCE        "\x08\x09"
#define SERIAL_RESPONSE_END_SEQUENCE          "\x1F\n"
#define SERIAL_COMMANDER_PATTERN_REPEAT_COUNT 3
#define SERIAL_EXPECTED_COMMAND_SEQUENCE      "+++"

class SerialController : public TransportController {
  public:
    SerialController(StateMachine *state_machine);

    bool begin() override;

    /*
     * Extracts the received message from the queue and puts it in the dispatch queue
     * Since this is only used in commanding mode, this queue can be small
     */
    bool process_received_message(radio_uplink_payload &uplink_payload) override;

    /**
     * Takes a payload and returns the length of the payload sent, not including the control
     * characters Note that this shares the LoRa instance with process_received_message
     */
    size_t send_command_message_response() override;
    size_t send_regular_message_response() override;

    void set_radio_in_launch_configuration() override;

  private:
};
#endif