/**
 * Interface for what the command handler should look like
 *
 * Will also define the inspect and command mode class interfaces
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "GlobalStore/GlobalStore.h"
#include "RadioController.h"
#include "StateMachine.h"
#include "buffered_sd.h"
#include "can_commands.h"
#include "radio_commands.h"
#include "util.h"

#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>
#include <RP_Logger.h>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/task.h>
#include <stdint.h>

#define TX_RX_DELAY                 (pdMS_TO_TICKS(750UL))
#define CAN_RESPONSE_TIMEOUT        (pdMS_TO_TICKS(7500UL))
#define CMD_LAUNCH_MODE_DELAY       (pdMS_TO_TICKS(10000UL))
#define CAN_TX_Q_SIZE               10
#define CAN_RX_Q_SIZE               10
#define CAN_WRITE_TIMEOUT           (pdMS_TO_TICKS(3000UL))

#define MAX_STRUCT_SIZE             28
#define CAN_RESPONSE_DELAY          (pdMS_TO_TICKS(30UL))
#define DELAY_BEFORE_CAN_RESPONSE() (vTaskDelay(CAN_RESPONSE_DELAY))

struct command_handler_response {
    uint8_t response_buffer[MAX_STRUCT_SIZE];
    size_t response_size;
};

class CommandHandler {
  public:
    CommandHandler(TwaiCAN &can, StateMachine *state_machine);

    virtual bool
    dispatch_command(radio_uplink_payload &uplink_payload, command_handler_response &response) = 0;

  protected:
    TwaiCAN *_can_ctx;
    StateMachine *_state_machine;

    /**
     * We faced an issue where the CAN bus was populated with low priority messages on a mode
     * switch, and we want to prevent that from happening.
     * 1. Have the radio board enter a no-op mode so that it stops all operations
     * 2. Issue a no-op instruction to tell the boards all to stop whatever they are doing, and send
     * whatever they have to the CAN bus
     * 3. Wait for a small period of time (1500ms for now) and clear the tx and rx queues
     */
    bool _prepare_mode_transition();

    /**
     * This requires that we read twice from the CAN bus to check that we got everything that we
     * want
     */
    bool
    _check_mode_transition_responses(uint8_t expected_state, command_handler_response &response);

    // kind of manual hard-coded right now, but I will deal with it
    bool _check_clock_jump_response(command_handler_response &response);
};

class InspectCommandHandler : public CommandHandler {
  public:
    InspectCommandHandler(
        TwaiCAN &can, StateMachine *state_machine, BufferedSD *sd, GlobalClock *global_clock
    );

    bool dispatch_command(
        radio_uplink_payload &uplink_payload, command_handler_response &response
    ) override;

  private:
    struct expected_packet_rx_info {
        CanCommands expected_command;
        int expected_data_length;
    };

    bool _inspect_multi_packet_response(
        expected_packet_rx_info packet_rx_info[], size_t num_responses_expected,
        command_handler_response &response, const char *radio_command_name
    );
    bool _inspect_shock_accel(RadioCommands radio_command, command_handler_response &response);

    BufferedSD *_sd;
    GlobalClock *_global_clock;
};

class NormalCommandHandler : public CommandHandler {
  public:
    NormalCommandHandler(TwaiCAN &can, StateMachine *state_machine);

    bool dispatch_command(
        radio_uplink_payload &uplink_payload, command_handler_response &response
    ) override;
};

#ifndef BUFFERED_SD_H_
struct sd_card_update {
    uint32_t file_size;
    uint32_t last_written_timestamp;
};
#endif

#endif