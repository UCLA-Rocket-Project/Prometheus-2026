/**
 * Wrapper that adds Radio logic for our use case
 */

#ifndef RADIO_CONTROLLER_H
#define RADIO_CONTROLLER_H

#include "GlobalStore.h"
#include "StateMachine.h"
#include "TransportController.h"
#include "esp32-hal.h"
#include "freertos/idf_additions.h"
#include "pins.h"
#include "radio_commands.h"
#include "util.h"

#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/task.h>

#define EXPECTED_RADIO_COMMAND_SIZE       4
#define RADIO_COMMAND_BUFFER_SIZE         16
#define RADIO_FREQUENCY                   908E6
#define RADIO_RINGBUF_DEFAULT_SIZE        2048
#define RADIO_ACK_WAIT_TIMEOUT            (pdMS_TO_TICKS(5000UL))
#define RADIO_RETRANSMIT_ATTEMPTS         8
#define RADIO_MAX_COMMAND_ACK_ERROR_COUNT 10

#ifndef __clang__
void IRAM_ATTR radio_receive_callback(int packet_size);
#endif

// TODO: change timeout so that nothing ever blocks?
// I think that might not be necessary since there will only ever be mutex contention during inspect
// mode, not flight
class RadioController : public TransportController {
  public:
    RadioController(
        SPIClass &spi_bus, StateMachine *sm, size_t buf_size = RADIO_RINGBUF_DEFAULT_SIZE
    );

    bool begin() override;

    /*
     * Extracts the received message from the queue and puts it in the dispatch queue
     * Since this is only used in commanding mode, this queue can be small
     */
    bool process_received_message(radio_uplink_payload &uplink_payload) override;

    size_t poll_and_receive_message();

    /**
     * Takes a payload and returns the length of the payload sent, not including the control
     * characters Note that this shares the LoRa instance with process_received_message
     */
    size_t send_command_message_response() override;
    size_t send_regular_message_response() override;

    void set_radio_in_launch_configuration() override;

  private:
    // only define things that will be used internally by the class, everything else should be
    // shared as much as possible
    SemaphoreHandle_t _radio_mutex = NULL;

    // Store SPI bus reference for deferred configuration in begin()
    // This avoids the static initialization order fiasco with the global LoRa object
    SPIClass *_spi_bus = nullptr;

    enum AckMapFlags : uint16_t { WAITING_ON_ACK = 0b0, ACK_RECEIVED = 0b1 };

    // event set used to check for acks. it will track the last command sent and an ack for that
    // command if the receiver sees that the command was sent, its ack would be the same command
    // again otherwise, it should restart the board

    // verified in freeRTOSConfig.h that the number of event bits is 24, and our enums fit it
    // perfectly
    // NOTE: this design is inconsistent with the uplinker's in that the uplinker uses a semaphore
    // I initially planned for this to have packet specific ACKs, but it would seem as if there is
    // no need for that now
    EventGroupHandle_t _ack_map;

    uint8_t _message_fail_count;

    /**
    Assumes that the mutex is alraedy held
    */
    size_t _send_command_message(uint8_t *payload, size_t payload_size);

    /**
     * Should be run whenever the radio receives a command code
     */
    void _clear_ack_received();

    /**
     * Run whenever the radio receives the ACK message, handled internally
     */
    void _set_ack_received();

    /**
     * Used to allow the radio to wait on a particular ack
     */
    bool _wait_on_ack();

    void _wait_for_ack_or_retransmit(uint8_t *payload, size_t payload_size);

    bool _process_received_message(
        uint8_t radio_buffer[], size_t buffer_size, radio_uplink_payload &uplink_payload
    );
};

#endif