#include "CommandHandler.h"
#include "StateMachine.h"
#include "can_commands.h"
#include "radio_commands.h"
#include <cstdint>

CommandHandler::CommandHandler(TwaiCAN &can, StateMachine *state_machine)
    : _can_ctx(&can), _state_machine(state_machine) {}

bool CommandHandler::_prepare_mode_transition() {
    RP_LOG_TRACE(CMD_HANDLER, "Entering NO OP mode to clear CAN bus first\n");
    _state_machine->enter_no_op_mode();

    vTaskDelay(TX_RX_DELAY);
    _can_ctx->clearTransmitQueue();

    CanFrame no_op_frame = {};
    no_op_frame.identifier = CanCommands::FINISH_ALL_OPERATIONS;
    _can_ctx->writeFrame(no_op_frame);
    // wait for all the messages to have been loaded into the can bus
    vTaskDelay(TX_RX_DELAY);
    // it is also alright to clear the queue here since we do not expect a response from no-op mode
    bool res = _can_ctx->clearReceiveQueue();

    RP_LOG_TRACE(CMD_HANDLER, "Finished no-op timeout resuming original operation\n");
    return res;
}

bool CommandHandler::_check_mode_transition_responses(
    uint8_t expected_state, command_handler_response &response
) {
    bool received_analog_ack, received_digital_ack = false;

    CanFrame rx_frame = {};
    for (int i = 0; i < 2; i++) {
        if (_can_ctx->readFrame(rx_frame, CAN_RESPONSE_TIMEOUT)) {
            if ((rx_frame.identifier == CanCommands::DIGITAL_BOARD_V1_MODE_TRANSITION_ACK ||
                 rx_frame.identifier == CanCommands::ANALOG_BOARD_V1_MODE_TRANSITION_ACK) &&
                rx_frame.data_length_code == 1 && rx_frame.data[0] == expected_state) {
                RP_LOG_TRACE(
                    CMD_HANDLER,
                    "[INSPECT] SUCCESS: Received MODE TRANSITION (0x%x), state=%d\n",
                    rx_frame.identifier,
                    expected_state
                );

                if (rx_frame.identifier == CanCommands::DIGITAL_BOARD_V1_MODE_TRANSITION_ACK) {
                    received_digital_ack = true;
                } else if (rx_frame.identifier ==
                           CanCommands::ANALOG_BOARD_V1_MODE_TRANSITION_ACK) {
                    received_analog_ack = true;
                }

            } else {
                RP_LOG_WARN(
                    CMD_HANDLER,
                    "[INSPECT] FAILED: Expected either DIGITAL / ANALOG MODE TRANSITION ACK, got "
                    "id=0x%03x, "
                    "dlc=%d\n",
                    rx_frame.identifier,
                    rx_frame.data_length_code
                );
                RP_LOG_WARN(
                    CAN_BUS,
                    "in rx queue: %d, in tx queue %d, err counter: %d, tx error: %d, tx failed: "
                    "%d\n",
                    _can_ctx->inRxQueue(),
                    _can_ctx->inTxQueue(),
                    _can_ctx->busErrCounter(),
                    _can_ctx->txErrorCounter(),
                    _can_ctx->txFailedCounter()
                );
            }
        } else {
            response.response_buffer[0] = RadioCommands::COMMAND_TIMEOUT;
            RP_LOG_WARN(CAN_BUS, "CAN receive for mode transition timed out\n");
            return false;
        }
    }

    if (received_analog_ack && received_digital_ack) {
        response.response_buffer[0] = expected_state;
        response.response_size = 1;
        return true;
    }
    response.response_buffer[0] = RadioCommands::CAN_RESPONSE_WRONG;
    return false;
}

bool CommandHandler::_check_clock_jump_response(command_handler_response &response) {
    bool received_analog_ack, received_digital_ack = false;

    CanFrame rx_frame = {};
    for (int i = 0; i < 2; i++) {
        if (_can_ctx->readFrame(rx_frame, CAN_RESPONSE_TIMEOUT)) {
            if ((rx_frame.identifier == CanCommands::DIGITAL_BOARD_V1_JUMP_CLOCK ||
                 rx_frame.identifier == CanCommands::ANALOG_BOARD_V1_JUMP_CLOCK) &&
                rx_frame.data_length_code == 0) {
                RP_LOG_TRACE(
                    CMD_HANDLER,
                    "[INSPECT] SUCCESS: Received ACK_CLK_JMP (0x%x)",
                    rx_frame.identifier
                );

                if (rx_frame.identifier == CanCommands::DIGITAL_BOARD_V1_JUMP_CLOCK) {
                    received_digital_ack = true;
                } else if (rx_frame.identifier == CanCommands::ANALOG_BOARD_V1_JUMP_CLOCK) {
                    received_analog_ack = true;
                }

            } else {
                RP_LOG_WARN(
                    CMD_HANDLER,
                    "[INSPECT] FAILED: Expected either DIGITAL / ANALOG clk jmp ack, got "
                    "id=0x%03x, "
                    "dlc=%d\n",
                    rx_frame.identifier,
                    rx_frame.data_length_code
                );
                RP_LOG_WARN(
                    CAN_BUS,
                    "in rx queue: %d, in tx queue %d, err counter: %d, tx error: %d, tx failed: "
                    "%d\n",
                    _can_ctx->inRxQueue(),
                    _can_ctx->inTxQueue(),
                    _can_ctx->busErrCounter(),
                    _can_ctx->txErrorCounter(),
                    _can_ctx->txFailedCounter()
                );
            }
        } else {
            response.response_buffer[0] = RadioCommands::COMMAND_TIMEOUT;
            RP_LOG_WARN(CAN_BUS, "CAN receive for clk jmp timed out");
            return false;
        }
    }

    if (received_analog_ack && received_digital_ack) {
        return true;
    } else if (!received_analog_ack) {
        RP_LOG_WARN(CMD_HANDLER, "No ANALOG ACK");
    } else if (!received_digital_ack) {
        RP_LOG_WARN(CMD_HANDLER, "No DIGITAL ACK");
    }

    response.response_buffer[0] = RadioCommands::CAN_RESPONSE_WRONG;
    return false;
}
