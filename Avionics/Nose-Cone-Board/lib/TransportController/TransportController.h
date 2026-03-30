#ifndef TRANSPORT_CONTROLLER_H
#define TRANSPORT_CONTROLLER_H

#include "GlobalStore.h"
#include "StateMachine.h"
#include "freertos/ringbuf.h"
#include <cstdint>

class TransportController {
  public:
    enum TransportMessageType { REGULAR, COMMAND };

    TransportController(StateMachine *state_machine) : _state_machine(state_machine) {};

    virtual bool begin() = 0;
    virtual bool process_received_message(radio_uplink_payload &uplink_payload) = 0;

    /**
     * Takes a payload and returns the length of the payload sent, not including the control
     * characters Note that this shares the LoRa instance with process_received_message
     */
    virtual size_t send_command_message_response() = 0;
    virtual size_t send_regular_message_response() = 0;

    // NOTE: there are optimizations that can be done here, but we are skipping that for now
    void queue_item_to_send(uint8_t *payload, size_t payload_size, TransportMessageType msg_type);
    void queue_command_failed(uint8_t failure_code);

    virtual void set_radio_in_launch_configuration() = 0;

  protected:
    StateMachine *_state_machine = nullptr;
    RingbufHandle_t _command_ringbuf = nullptr;
    RingbufHandle_t _regular_ringbuf = nullptr;
};

#endif