#include "ESP32-TWAI-CAN.hpp"
#include "RadioBoardTasks.h"
#include "can_commands.h"
#include "esp_system.h"
#include "freertos/idf_additions.h"
#include "util.h"
#include <freertos/portmacro.h>

#define RESET_CAN_RESPONSE_TIMEOUT (pdMS_TO_TICKS(1000UL))

static bool _check_mode_transition_responses(reset_task_ctx *ctx);

void reset_task(void *pv_params) {
    reset_task_ctx *ctx = reinterpret_cast<reset_task_ctx *>(pv_params);

    for (;;) {
        uint32_t notif = 0;
        xTaskNotifyWait(0x0, 0xFFFFFFFF, &notif, portMAX_DELAY);

        CanFrame tx_frame = {};
        tx_frame.identifier = CanCommands::RESET_BOARDS_CAN;
        ctx->can_bus->writeFrame(tx_frame);

        if (!_check_mode_transition_responses(ctx)) {
            RP_LOG_MUST(RESET_HANDLER, "Could not get the other boards to reset!");
        }

        // once everyone else resets, you should reset too
        esp_restart();
    }
}

static bool _check_mode_transition_responses(reset_task_ctx *ctx) {
    bool received_analog_ack, received_digital_ack = false;

    for (int i = 0; i < 2; i++) {
        CanFrame rx_frame = {};

        if (ctx->can_bus->readFrame(rx_frame, RESET_CAN_RESPONSE_TIMEOUT)) {
            if ((rx_frame.identifier == CanCommands::DIGITAL_BOARD_V1_MODE_TRANSITION_ACK ||
                 rx_frame.identifier == CanCommands::ANALOG_BOARD_V1_MODE_TRANSITION_ACK) &&
                rx_frame.data_length_code == 1 && rx_frame.data[0] == CAN_COMMAND_RESET_CODE) {
                RP_LOG_MUST(
                    RESET_HANDLER,
                    "Received MODE TRANSITION (0x%x), state=%d",
                    rx_frame.identifier,
                    CAN_COMMAND_RESET_CODE
                );

                if (rx_frame.identifier == CanCommands::DIGITAL_BOARD_V1_MODE_TRANSITION_ACK) {
                    received_digital_ack = true;
                } else if (rx_frame.identifier ==
                           CanCommands::ANALOG_BOARD_V1_MODE_TRANSITION_ACK) {
                    received_analog_ack = true;
                }

            } else {
                RP_LOG_MUST(
                    RESET_HANDLER,
                    "FAILED: Expected either DIGITAL / ANALOG MODE TRANSITION ACK, got "
                    "id=0x%03x, "
                    "dlc=%d",
                    rx_frame.identifier,
                    rx_frame.data_length_code
                );
            }
        } else {
            RP_LOG_MUST(RESET_HANDLER, "Receive for CAN acknowledgements timed out");
            return false;
        }
    }

    if (received_analog_ack && received_digital_ack) {
        return true;
    }
    return false;
}