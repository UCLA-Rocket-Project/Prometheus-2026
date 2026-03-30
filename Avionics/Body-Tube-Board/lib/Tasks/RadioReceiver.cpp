#include "GlobalStore.h"
#include "RP_Logger.h"
#include "RadioBoardTasks.h"
#include "freertos/idf_additions.h"
#include "radio_commands.h"
#include "util.h"
#include <cstdio>
#include <freertos/portmacro.h>

#define UPLINK_PAYLOAD_MAX_SIZE 8
#define RADIO_POLL_TIMEOUT      (pdMS_TO_TICKS(200UL))

// all shared freeRTOS state should be handled here
#ifdef USE_INTERRUPTS
void radio_receiver(void *pv_params) {
    uint32_t received_packet_size;
    radio_receiver_ctx *task_ctx = reinterpret_cast<radio_receiver_ctx *>(pv_params);

    for (;;) {
        radio_uplink_payload uplink_payload{};

        if (!task_ctx->radio_ctx->process_received_message(uplink_payload)) {
            continue;
        }

        if (!task_ctx->radio_ctx->process_received_message(uplink_payload)) {
            continue;
        } else if (uplink_payload.command_code == RadioCommands::RADIO_MESSAGE_ACK) {
            continue;
        } else if (xQueueSend(task_ctx->command_queue, &uplink_payload, portMAX_DELAY) == pdFAIL) {
            RP_LOG_WARN(RADIO_CMD_TRANSPORT, "Could not forward command");
        }

        RP_LOG_TRACE(
            RADIO_CMD_TRANSPORT, "Received and forwarded command %x", uplink_payload.command_code
        );
    }
}
#else
void radio_receiver(void *pv_params) {
    uint32_t received_packet_size;
    radio_receiver_ctx *task_ctx = reinterpret_cast<radio_receiver_ctx *>(pv_params);

    for (;;) {
        radio_uplink_payload uplink_payload{};

        if (!task_ctx->radio_ctx->process_received_message(uplink_payload)) {
            vTaskDelay(RADIO_POLL_TIMEOUT);
            continue;
        } else if (uplink_payload.command_code == RadioCommands::RADIO_MESSAGE_ACK) {
            continue;
        } else if (xQueueSend(task_ctx->command_queue, &uplink_payload, portMAX_DELAY) == pdFAIL) {
            RP_LOG_WARN(RADIO_CMD_TRANSPORT, "Could not forward command");
            continue;
        }

        RP_LOG_TRACE(
            RADIO_CMD_TRANSPORT, "Received and forwarded command %x", uplink_payload.command_code
        );
        vTaskDelay(RADIO_POLL_TIMEOUT);
    }
}
#endif