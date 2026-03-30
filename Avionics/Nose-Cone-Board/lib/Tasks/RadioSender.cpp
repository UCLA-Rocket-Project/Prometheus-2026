#include "RP_Logger.h"
#include "RadioBoardTasks.h"

void radio_regular_sender(void *pv_params) {
    radio_sender_ctx *sender_ctx = reinterpret_cast<radio_sender_ctx *>(pv_params);

    for (;;) {
        sender_ctx->state_machine->check_normal_or_wait();
        sender_ctx->radio_ctx->send_regular_message_response();

        if (!sender_ctx->state_machine->is_launch_mode()) {
            vTaskDelay(pdMS_TO_TICKS(3000UL));
        }
    }
}

// TODO: remove this task after entering launch mode?
// kind of like a 2 stage launcher
void radio_command_sender(void *pv_params) {
    radio_command_sender_ctx *sender_ctx = reinterpret_cast<radio_command_sender_ctx *>(pv_params);

    for (;;) {
        sender_ctx->radio_ctx->send_command_message_response();
    }
}