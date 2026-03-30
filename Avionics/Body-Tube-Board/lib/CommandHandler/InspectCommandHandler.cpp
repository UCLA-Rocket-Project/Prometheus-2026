#include "CommandHandler.h"
#include "ESP32-TWAI-CAN.hpp"
#include "GlobalStore/GlobalStore.h"
#include "RP_Logger.h"
#include "buffered_sd.h"
#include "can_commands.h"
#include "esp_timer.h"
#include "radio_commands.h"
#include <cstdint>
#include <cstring>
#include <freertos/FreeRTOSConfig.h>
#include <stdint.h>

InspectCommandHandler::InspectCommandHandler(
    TwaiCAN &can, StateMachine *state_machine, BufferedSD *sd, GlobalClock *global_clock
)
    : CommandHandler(can, state_machine), _sd(sd), _global_clock(global_clock) {}

bool InspectCommandHandler::dispatch_command(
    radio_uplink_payload &uplink_payload, command_handler_response &response
) {
    CanFrame rx_frame = {};
    CanFrame tx_frame = {};

    RP_LOG_TRACE(
        CMD_HANDLER, "Received command in inspect mode: (%x)", uplink_payload.command_code
    );

    switch (uplink_payload.command_code) {
    case RadioCommands::ENTER_NORMAL_MODE_RADIO: {
        RP_LOG_TRACE(
            CMD_HANDLER, "Case: ENTER_NORMAL_MODE_RADIO (0x%02x)", uplink_payload.command_code
        );

        tx_frame.identifier = CanCommands::ENTER_NORMAL_MODE_CAN;
        tx_frame.extd = 0;
        tx_frame.data_length_code = 0;

        RP_LOG_TRACE(
            CMD_HANDLER, "Sending CAN frame: ENTER_NORMAL_MODE_CAN (id=0x%03x)", tx_frame.identifier
        );
        _prepare_mode_transition();
        _state_machine->enter_normal_mode();

        _can_ctx->writeFrame(tx_frame);

        if (_check_mode_transition_responses(StateMachine::States::NORMAL, response)) {
            return true;
        }
        break;
    }
    case RadioCommands::ENTER_INSPECT_MODE_RADIO: {
        RP_LOG_TRACE(
            CMD_HANDLER, "Case: ENTER_INSPECT_MODE_RADIO (0x%02x)", uplink_payload.command_code
        );

        _state_machine->clear_enter_sd_mode();

        tx_frame.identifier = CanCommands::ENTER_INSPECT_MODE_CAN;
        tx_frame.extd = 0;
        tx_frame.data_length_code = 0;

        RP_LOG_TRACE(
            CMD_HANDLER,
            "Sending CAN frame: ENTER_INSPECT_MODE_CAN (id=0x%03x)",
            tx_frame.identifier
        );
        _prepare_mode_transition();
        _state_machine->enter_inspect_mode();
        _can_ctx->writeFrame(tx_frame);

        if (_check_mode_transition_responses(StateMachine::States::INSPECT, response))
            return true;
        break;
    }
    case RadioCommands::ENTER_LAUNCH_MODE_RADIO: {
        RP_LOG_MUST(CMD_HANDLER, "Case: ENTER_LAUNCH_MODE (0x%02x)", uplink_payload.command_code);
        _state_machine->enter_launch_mode();

        tx_frame.identifier = CanCommands::ENTER_LAUNCH_MODE_CAN;
        tx_frame.extd = 0;
        tx_frame.data_length_code = 0;

        RP_LOG_MUST(
            CMD_HANDLER, "Sending CAN frame: REMOVE_DELAY (id=0x%03x)", tx_frame.identifier
        );
        response.response_buffer[0] = RadioCommands::ENTER_LAUNCH_MODE_RADIO;
        response.response_size = 1;

        RP_LOG_MUST(
            CMD_HANDLER,
            "Returning with response: ENTER_LAUNCH_MODE (0x%02x)",
            response.response_buffer[0]
        );
        return _can_ctx->writeFrame(tx_frame, CMD_LAUNCH_MODE_DELAY);
    }
    case RadioCommands::INSPECT_ANALOG_V1_SD: {
        RP_LOG_TRACE(
            CMD_HANDLER, "Case: INSPECT_ANALOG_V1_SD (0x%02x)", uplink_payload.command_code
        );

        // stop sending regular messages over radio in this mode
        _state_machine->enter_check_sd_mode();

        tx_frame.identifier = CanCommands::ANALOG_BOARD_V1_SD_CARD_DATA;
        tx_frame.data_length_code = 0;
        tx_frame.extd = 0;

        RP_LOG_TRACE(
            CMD_HANDLER,
            "Sending CAN frame: ANALOG_BOARD_V1_SD_CARD_DATA (id=0x%03x)",
            tx_frame.identifier
        );
        _can_ctx->writeFrame(tx_frame);

        RP_LOG_TRACE(CMD_HANDLER, "Expecting response: ANALOG_BOARD_V1_SD_CARD_DATA");
        if (_can_ctx->readFrame(rx_frame, CAN_RESPONSE_TIMEOUT)) {
            if (rx_frame.identifier == CanCommands::ANALOG_BOARD_V1_SD_CARD_DATA) {
                RP_LOG_TRACE(
                    CMD_HANDLER,
                    "SUCCESS: Received ANALOG_BOARD_V1_SD_CARD_DATA, dlc=%d",
                    rx_frame.data_length_code
                );

                char buf[256];
                sd_card_update update;
                memcpy(&update, rx_frame.data, sizeof(sd_card_update));
                snprintf(
                    buf,
                    256,
                    "file size: %u, last timestamp: %u",
                    update.file_size,
                    update.last_written_timestamp
                );
                RP_LOG_TRACE(SD_CARD, "%s", buf);

                memcpy(response.response_buffer, rx_frame.data, sizeof(rx_frame.data));
                response.response_size = rx_frame.data_length_code;
                return true;
            } else {
                RP_LOG_WARN(
                    CMD_HANDLER,
                    "FAILED: Expected ANALOG_BOARD_V1_SD_CARD_DATA, got id=0x%03x",
                    rx_frame.identifier
                );
            }
        } else {
            RP_LOG_WARN(CAN_BUS, "Can receive in inspect mode for analog SD data timed out");
        }
        response.response_buffer[0] = RadioCommands::CAN_RESPONSE_WRONG;
        break;
    }
    case RadioCommands::CLEAR_ANALOG_V1_SD: {
        RP_LOG_TRACE(CMD_HANDLER, "Case: CLEAR_ANALOG_V1_SD (0x%02x)", uplink_payload.command_code);
        tx_frame.identifier = CanCommands::ANALOG_BOARD_V1_CLEAR_SD_CARD;
        tx_frame.extd = 0;
        tx_frame.data_length_code = 0;

        RP_LOG_TRACE(
            CMD_HANDLER,
            "Sending CAN frame: ANALOG_BOARD_V1_CLEAR_SD_CARD (id=0x%03x)",
            tx_frame.identifier
        );
        _can_ctx->writeFrame(tx_frame);

        RP_LOG_TRACE(CMD_HANDLER, "Expecting response: ANALOG_BOARD_V1_CLEAR_SD_CARD");
        if (_can_ctx->readFrame(rx_frame, CAN_RESPONSE_TIMEOUT)) {
            if (rx_frame.identifier == CanCommands::ANALOG_BOARD_V1_CLEAR_SD_CARD) {
                RP_LOG_TRACE(
                    CMD_HANDLER,
                    "SUCCESS: Received ANALOG_BOARD_V1_CLEAR_SD_CARD, dlc=%d",
                    rx_frame.data_length_code
                );
                memcpy(response.response_buffer, rx_frame.data, sizeof(rx_frame.data));
                response.response_size = rx_frame.data_length_code;
                return true;
            } else {
                RP_LOG_WARN(
                    CMD_HANDLER,
                    "FAILED: Expected ANALOG_BOARD_V1_CLEAR_SD_CARD, got id=0x%03x",
                    rx_frame.identifier
                );
            }
        } else {
            RP_LOG_WARN(CAN_BUS, "Can receive in inspect mode for clear analog SD timed out");
        }
        response.response_buffer[0] = RadioCommands::CAN_RESPONSE_WRONG;
        break;
    }
    case RadioCommands::INSPECT_ANALOG_V1_LC: {
        RP_LOG_TRACE(
            CMD_HANDLER, "Case: INSPECT_ANALOG_V1_LC (0x%02x)", uplink_payload.command_code
        );
        tx_frame.identifier = CanCommands::ANALOG_BOARD_V1_LC_DATA;
        tx_frame.extd = 0;
        tx_frame.data_length_code = 0;

        RP_LOG_TRACE(
            CMD_HANDLER,
            "Sending CAN frame: ANALOG_BOARD_V1_LC_DATA (id=0x%03x)",
            tx_frame.identifier
        );
        _can_ctx->writeFrame(tx_frame);

        RP_LOG_TRACE(CMD_HANDLER, "Expecting response: ANALOG_BOARD_V1_LC_DATA");
        if (_can_ctx->readFrame(rx_frame, CAN_RESPONSE_TIMEOUT)) {
            if (rx_frame.identifier == CanCommands::ANALOG_BOARD_V1_LC_DATA) {
                RP_LOG_TRACE(
                    CMD_HANDLER,
                    "SUCCESS: Received ANALOG_BOARD_V1_LC_DATA, dlc=%d",
                    rx_frame.data_length_code
                );
                memcpy(response.response_buffer, rx_frame.data, sizeof(rx_frame.data));
                response.response_size = rx_frame.data_length_code;
                return true;
            } else {
                RP_LOG_WARN(
                    CMD_HANDLER,
                    "FAILED: Expected ANALOG_BOARD_V1_LC_DATA, got id=0x%03x",
                    rx_frame.identifier
                );
                RP_LOG_WARN(
                    CAN_BUS,
                    "Tried reading from LC in inspect mode, but got a bad "
                    "response: %d",
                    rx_frame.identifier
                );
            }
        } else {
            RP_LOG_WARN(CAN_BUS, "Can receive in inspect mode for analog LC data timed out");
        }
        response.response_buffer[0] = RadioCommands::CAN_RESPONSE_WRONG;
        break;
    }
    case RadioCommands::INSPECT_DIGITAL_V1_SD: {
        RP_LOG_TRACE(
            CMD_HANDLER, "Case: INSPECT_DIGITAL_V1_SD (0x%02x)", uplink_payload.command_code
        );

        // stop sending regular messages over radio in this mode
        _state_machine->enter_check_sd_mode();

        tx_frame.identifier = CanCommands::DIGITAL_BOARD_V1_SD_CARD_DATA;
        tx_frame.data_length_code = 0;
        tx_frame.extd = 0;

        RP_LOG_TRACE(
            CMD_HANDLER,
            "Sending CAN frame: DIGITAL_BOARD_V1_SD_CARD_DATA (id=0x%03x)",
            tx_frame.identifier
        );
        _can_ctx->writeFrame(tx_frame);

        RP_LOG_TRACE(CMD_HANDLER, "Expecting response: DIGITAL_BOARD_V1_SD_CARD_DATA");
        if (_can_ctx->readFrame(rx_frame, CAN_RESPONSE_TIMEOUT)) {
            if (rx_frame.identifier == CanCommands::DIGITAL_BOARD_V1_SD_CARD_DATA) {
                RP_LOG_TRACE(
                    CMD_HANDLER,
                    "SUCCESS: Received DIGITAL_BOARD_V1_SD_CARD_DATA, dlc=%d",
                    rx_frame.data_length_code
                );
                char buf[256];
                sd_card_update update;
                memcpy(&update, rx_frame.data, sizeof(sd_card_update));
                snprintf(
                    buf,
                    256,
                    "file size: %u, last timestamp: %u",
                    update.file_size,
                    update.last_written_timestamp
                );
                RP_LOG_TRACE(CMD_HANDLER, "%s", buf);

                memcpy(response.response_buffer, rx_frame.data, sizeof(rx_frame.data));
                response.response_size = rx_frame.data_length_code;
                return true;
            } else {
                RP_LOG_WARN(
                    CAN_BUS,
                    "FAILED: Expected DIGITAL_BOARD_V1_SD_CARD_DATA, got id=0x%03x",
                    rx_frame.identifier
                );
            }
        } else {
            RP_LOG_WARN(CAN_BUS, "Can receive in inspect mode for digital SD data timed out");
        }
        response.response_buffer[0] = RadioCommands::CAN_RESPONSE_WRONG;
        break;
    }
    case RadioCommands::CLEAR_DIGITAL_V1_SD: {
        RP_LOG_TRACE(
            CMD_HANDLER, "Case: CLEAR_DIGITAL_V1_SD (0x%02x)", uplink_payload.command_code
        );
        tx_frame.identifier = CanCommands::DIGITAL_BOARD_V1_CLEAR_SD_CARD;
        tx_frame.extd = 0;
        tx_frame.data_length_code = 0;

        RP_LOG_TRACE(
            CMD_HANDLER,
            "Sending CAN frame: DIGITAL_BOARD_V1_CLEAR_SD_CARD (id=0x%03x)",
            tx_frame.identifier
        );
        _can_ctx->writeFrame(tx_frame);

        RP_LOG_TRACE(CMD_HANDLER, "Expecting response: DIGITAL_BOARD_V1_CLEAR_SD_CARD");
        if (_can_ctx->readFrame(rx_frame, CAN_RESPONSE_TIMEOUT)) {
            if (rx_frame.identifier == CanCommands::DIGITAL_BOARD_V1_CLEAR_SD_CARD) {
                RP_LOG_TRACE(
                    CMD_HANDLER,
                    "SUCCESS: Received DIGITAL_BOARD_V1_CLEAR_SD_CARD, dlc=%d",
                    rx_frame.data_length_code
                );
                memcpy(response.response_buffer, rx_frame.data, sizeof(rx_frame.data));
                response.response_size = rx_frame.data_length_code;
                return true;
            } else {
                RP_LOG_WARN(
                    CAN_BUS,
                    "FAILED: Expected DIGITAL_BOARD_V1_CLEAR_SD_CARD, got id=0x%03x",
                    rx_frame.identifier
                );
            }
        } else {
            RP_LOG_WARN(CAN_BUS, "Can receive in inspect mode for clear digital SD timed out");
        }
        response.response_buffer[0] = RadioCommands::CAN_RESPONSE_WRONG;
        break;
    }
    case RadioCommands::INSPECT_DIGITAL_V1_SHOCK_1: {
        if (_inspect_shock_accel(RadioCommands::INSPECT_DIGITAL_V1_SHOCK_1, response)) {
            return true;
        }
        break;
    }
    case RadioCommands::INSPECT_DIGITAL_V1_SHOCK_2: {
        configASSERT(0 && "This should not exist yet");
        break;
    }
    case RadioCommands::INSPECT_DIGITAL_V1_IMU: {
        tx_frame.identifier = CanCommands::DIGITAL_BOARD_V1_IMU_DATA_ACC_XY;

        RP_LOG_TRACE(
            CMD_HANDLER, "Case: INSPECT_DIGITAL_V1_IMU (0x%02x)", uplink_payload.command_code
        );
        if (!_can_ctx->writeFrame(tx_frame, CAN_WRITE_TIMEOUT)) {
            RP_LOG_TRACE(CMD_HANDLER, "Could not successfully write CAN frame");
            response.response_buffer[0] = RadioCommands::COMMAND_TIMEOUT;
            break;
        }

        expected_packet_rx_info expected_info[] = {
            {CanCommands::DIGITAL_BOARD_V1_IMU_DATA_ACC_XY, 8},
            {CanCommands::DIGITAL_BOARD_V1_IMU_DATA_ACC_Z_GYR_X, 8},
            {CanCommands::DIGITAL_BOARD_V1_IMU_DATA_GYR_Y_GYR_Z, 8},
            {CanCommands::DIGITAL_BOARD_V1_IMU_TIMESTAMP, 4}
        };

        if (_inspect_multi_packet_response(expected_info, 4, response, "IMU")) {
            return true;
        }
        break;
    }
    case RadioCommands::INSPECT_DIGITAL_V1_ALTIMETER: {
        tx_frame.identifier = CanCommands::DIGITAL_BOARD_V1_ALTIMETER_DATA;

        RP_LOG_TRACE(
            CMD_HANDLER, "Case: INSPECT_DIGITAL_V1_ALTIMETER (0x%02x)", uplink_payload.command_code
        );
        if (!_can_ctx->writeFrame(tx_frame, CAN_WRITE_TIMEOUT)) {
            RP_LOG_TRACE(CMD_HANDLER, "Could not successfully write CAN frame");
            response.response_buffer[0] = RadioCommands::COMMAND_TIMEOUT;
            break;
        }

        expected_packet_rx_info expected_info[] = {
            {CanCommands::DIGITAL_BOARD_V1_ALTIMETER_DATA, 8},
            {CanCommands::DIGITAL_BOARD_V1_ALTIMETER_DATA_TIMESTAMP, 4}
        };

        if (_inspect_multi_packet_response(expected_info, 2, response, "Altimeter")) {
            return true;
        }

        break;
    }
    case RadioCommands::INSPECT_RADIO_BOARD_SD: {
        RP_LOG_TRACE(CMD_HANDLER, "Case: INSPECT_RADIO_SD (0x%02x)", uplink_payload.command_code);

        // stop sending regular messages over radio in this mode
        _state_machine->enter_check_sd_mode();

        sd_card_update radio_sd_data = _sd->get_file_update();
        memcpy(response.response_buffer, &radio_sd_data, 8);
        response.response_size = 8;

        char buf[256];
        snprintf(
            buf,
            256,
            "radio board file size: %u, last timestamp: %u",
            radio_sd_data.file_size,
            radio_sd_data.last_written_timestamp
        );
        RP_LOG_TRACE(CMD_HANDLER, "%s", buf);
        return true;
    }
    case RadioCommands::CLEAR_RADIO_SD: {
        RP_LOG_TRACE(CMD_HANDLER, "Case: CLEAR_RADIO_SD (0x%02x)", uplink_payload.command_code);

        int initial_file_count = _sd->count_top_level_files();
        if (_sd->delete_all_files() && _sd->count_top_level_files() <= initial_file_count) {
            // create a new file for the current program to use
            _sd->create_new_file();

            unsigned long new_free_space = _sd->get_free_space();
            memcpy(response.response_buffer, &new_free_space, sizeof(unsigned long));
            response.response_size = 4;
            return true;
        } else {
            response.response_buffer[0] = RadioCommands::RADIO_COMMAND_ERROR;
            response.response_size = 1;
        }
        break;
    }
    case RadioCommands::JUMP_CLOCK_RADIO: {
        RP_LOG_TRACE(CMD_HANDLER, "Case: JUMP_CLOCK_RADIO (0x%02x)", uplink_payload.command_code);

        if (uplink_payload.payload_size != sizeof(int64_t)) {
            RP_LOG_WARN(CMD_HANDLER, "clk payload wrong sz: %d", uplink_payload.payload_size);
            break;
        }

        // No one is going to
        // contend for this mutex in inspect mode anyways
        // Nonetheless, it seems like the update of the global_time may not be atomic, though I dont
        // think that this thread will ever be interrupted, since its priority is very high. Other
        // threads will only ever read from the value later
        memcpy(&_global_clock->global_time, uplink_payload.payload, sizeof(int64_t));
        _global_clock->last_update_recv_time = esp_timer_get_time();
        RP_LOG_TRACE(
            TIME_SYNC,
            "Glb: %lld, update: %lld",
            _global_clock->global_time,
            _global_clock->last_update_recv_time
        );

        // send it to the two other boards
        tx_frame.identifier = CanCommands::JUMP_CLOCK_CAN;
        memcpy(tx_frame.data, uplink_payload.payload, sizeof(int64_t));
        tx_frame.data_length_code = sizeof(int64_t);

        if (!_can_ctx->writeFrame(tx_frame, CAN_WRITE_TIMEOUT)) {
            RP_LOG_TRACE(CMD_HANDLER, "Failed to write CAN frame");
            response.response_buffer[0] = RadioCommands::COMMAND_TIMEOUT;
            break;
        }

        // return the updated time if true
        if (_check_clock_jump_response(response)) {
            int64_t curr_time = _global_clock->global_time + esp_timer_get_time() -
                                _global_clock->last_update_recv_time;
            memcpy(response.response_buffer, &curr_time, sizeof(int64_t));
            response.response_size = sizeof(int64_t);
            return true;
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

    // if we reached here, it means that the command itself was not successful
    // for now, the only error we would have is the timeout error
    // if there are any other errors, set them in the else of the individual switch statements
    response.response_buffer[0] = RadioCommands::COMMAND_TIMEOUT;
    response.response_size = 1;
    return false;
}

bool InspectCommandHandler::_inspect_multi_packet_response(
    expected_packet_rx_info packet_rx_info[], size_t num_responses_expected,
    command_handler_response &response, const char *radio_command_name
) {
    CanFrame rx_frame = {};

    for (int i = 0; i < num_responses_expected; i++) {
        if (_can_ctx->readFrame(rx_frame, CAN_RESPONSE_TIMEOUT)) {
            if (rx_frame.identifier == packet_rx_info[i].expected_command &&
                rx_frame.data_length_code == packet_rx_info[i].expected_data_length) {
                RP_LOG_TRACE(
                    CMD_HANDLER,
                    "SUCCESS: Received %s part %d, dlc=%d",
                    radio_command_name,
                    i + 1,
                    rx_frame.data_length_code
                );
                memcpy(
                    &response.response_buffer[response.response_size],
                    rx_frame.data,
                    rx_frame.data_length_code
                );

                response.response_size += rx_frame.data_length_code;
            } else {
                RP_LOG_TRACE(
                    CMD_HANDLER,
                    "FAILED: Expected %s, got id=0x%03x",
                    radio_command_name,
                    rx_frame.identifier
                );
                RP_LOG_TRACE(
                    CAN_BUS,
                    "Tried reading from %s in inspect mode, but got a bad "
                    "response: %d of length %d in part 1",
                    radio_command_name,
                    rx_frame.identifier,
                    rx_frame.data_length_code
                );
                response.response_buffer[0] = RadioCommands::CAN_RESPONSE_WRONG;

                return false;
            }
        } else {
            response.response_buffer[0] = RadioCommands::COMMAND_TIMEOUT;
            response.response_size = 1;
            return false;
        }
    }

    return true;
}

// TODO: clean up this unecessary function
bool InspectCommandHandler::_inspect_shock_accel(
    RadioCommands command_code, command_handler_response &response
) {
    CanFrame tx_frame = {};
    CanFrame rx_frame = {};

    RP_LOG_TRACE(CMD_HANDLER, "Case: DIGITAL_BOARD_V1_SHOCK (0x%02x)", command_code);

    // we will here use one command to trigger both responses
    if (command_code == RadioCommands::INSPECT_DIGITAL_V1_SHOCK_1) {
        tx_frame.identifier = CanCommands::DIGITAL_BOARD_V1_SHOCK_ACCEL_1_DATA_XY;
    } else if (command_code == RadioCommands::INSPECT_DIGITAL_V1_SHOCK_2) {
        tx_frame.identifier = CanCommands::DIGITAL_BOARD_V1_SHOCK_ACCEL_2_DATA_XY;
    }

    RP_LOG_TRACE(
        CMD_HANDLER, "Sending CAN frame: DIGITAL_BOARD_V1_SHOCK (id=0x%03x)", tx_frame.identifier
    );
    DELAY_BEFORE_CAN_RESPONSE();
    if (!_can_ctx->writeFrame(tx_frame, CAN_WRITE_TIMEOUT)) {
        RP_LOG_TRACE(CMD_HANDLER, "Could not successfully write CAN frame");
        response.response_buffer[0] = RadioCommands::COMMAND_TIMEOUT;
        return false;
    }

    expected_packet_rx_info packet_info[] = {
        {CanCommands::DIGITAL_BOARD_V1_SHOCK_ACCEL_1_DATA_XY, 8},
        {CanCommands::DIGITAL_BOARD_V1_SHOCK_ACCEL_1_DATA_Z, 8}
    };

    if (command_code == RadioCommands::INSPECT_DIGITAL_V1_SHOCK_2) {
        packet_info[0].expected_command = CanCommands::DIGITAL_BOARD_V1_SHOCK_ACCEL_2_DATA_XY;
        packet_info[1].expected_command = CanCommands::DIGITAL_BOARD_V1_SHOCK_ACCEL_2_DATA_Z;
    }

    return _inspect_multi_packet_response(packet_info, 2, response, "DIGITAL BOARD SHOCK");
}