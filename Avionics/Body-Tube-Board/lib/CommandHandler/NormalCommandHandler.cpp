#include "CommandHandler.h"

NormalCommandHandler::NormalCommandHandler(TwaiCAN &can, StateMachine *state_machine)
    : CommandHandler(can, state_machine) {}

bool NormalCommandHandler::dispatch_command(
    radio_uplink_payload &uplink_payload, command_handler_response &response
) {
    CanFrame rxFrame = {};
    CanFrame txFrame = {};

    // turn off self-receipt and extra flags
    txFrame.flags = 0;

    response.response_buffer[0] = RadioCommands::COMMAND_TIMEOUT;
    response.response_size = 1;

    switch (uplink_payload.command_code) {
    case RadioCommands::ENTER_NORMAL_MODE_RADIO: {
        RP_LOG_TRACE(CMD_HANDLER, "ENTER_NORMAL_MODE_RADIO (0x%02x)", uplink_payload.command_code);

        txFrame.identifier = CanCommands::ENTER_NORMAL_MODE_CAN;
        txFrame.extd = 0;
        txFrame.data_length_code = 0;

        RP_LOG_TRACE(
            CMD_HANDLER, "Sending CAN frame: ENTER_NORMAL_MODE_CAN (id=0x%03x)", txFrame.identifier
        );
        _prepare_mode_transition();
        _state_machine->enter_normal_mode();
        _can_ctx->writeFrame(txFrame);

        RP_LOG_TRACE(
            CMD_HANDLER,
            "Expecting response: DIGITAL_BOARD_V1_MODE_TRANSITION_ACK with state=NORMAL"
        );
        if (_check_mode_transition_responses(StateMachine::States::NORMAL, response))
            return true;
        break;
    }
    case RadioCommands::ENTER_INSPECT_MODE_RADIO: {
        _state_machine->clear_enter_sd_mode();

        RP_LOG_TRACE(
            CMD_HANDLER, "Case: ENTER_INSPECT_MODE_RADIO (0x%02x)", uplink_payload.command_code
        );
        _prepare_mode_transition();

        _state_machine->enter_inspect_mode();

        txFrame.identifier = CanCommands::ENTER_INSPECT_MODE_CAN;
        txFrame.extd = 0;
        txFrame.data_length_code = 0;

        RP_LOG_TRACE(
            CMD_HANDLER, "Sending CAN frame: ENTER_INSPECT_MODE_CAN (id=0x%03x)", txFrame.identifier
        );
        _can_ctx->writeFrame(txFrame);

        RP_LOG_TRACE(
            CMD_HANDLER,
            "Expecting response: DIGITAL_BOARD_V1_MODE_TRANSITION_ACK with state=INSPECT"
        );
        if (_check_mode_transition_responses(StateMachine::States::INSPECT, response)) {
            return true;
        } else {
            RP_LOG_WARN(CMD_HANDLER, "Did not receive mode transition responses");
        }
        break;
    }
    default: {
        RP_LOG_TRACE(
            CMD_HANDLER,
            "Case: DEFAULT - Unrecognized command (0x%02x)",
            uplink_payload.command_code
        );
        response.response_buffer[0] = RadioCommands::RADIO_COMMAND_NOT_RECOGNIZED;
    }
    }

    return false;
}