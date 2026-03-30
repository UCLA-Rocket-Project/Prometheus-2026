#include "Arduino.h"
#include "CommandHandler.h"
#include "GlobalStore.h"
#include "HardwareSerial.h"
#include "RadioBoardTasks.h"
#include "driver/uart.h"
#include "freertos/idf_additions.h"
#include "hal/uart_types.h"
#include "util.h"
#include <cstdint>
#include <cstdio>
#include <freertos/portmacro.h>
#include <freertos/projdefs.h>

void serial_command_receiver(void *pv_params) {
    serial_command_receiver_ctx *task_ctx =
        reinterpret_cast<serial_command_receiver_ctx *>(pv_params);

    for (;;) {
        radio_uplink_payload radio_payload{};
        uart_event_t event{};

        if (!Serial.available()) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        if (!task_ctx->serial_ctx->process_received_message(radio_payload)) {
            continue;
        }

        RP_LOG_TRACE(SERIAL_CMD_TRANSPORT, "Received Command: %x", radio_payload.command_code);

        // process an ACK message
        if (radio_payload.command_code == RadioCommands::RADIO_MESSAGE_ACK) {
            continue;
        } else if (xQueueSend(task_ctx->command_queue, &radio_payload, portMAX_DELAY) == pdPASS) {
            RP_LOG_TRACE(SERIAL_CMD_TRANSPORT, "Received command %x", radio_payload.command_code);
        } else {
            RP_LOG_WARN(
                SERIAL_CMD_TRANSPORT, "Could not queue command %x", radio_payload.command_code
            );
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}