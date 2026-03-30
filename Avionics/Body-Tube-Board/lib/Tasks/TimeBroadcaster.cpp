#include "RadioBoardTasks.h"

#define TIME_BETWEEN_CAN_BROADCASTS (pdMS_TO_TICKS(3000UL))

// while this also uses the can bus, schedule it on the same core as the
// command handler so that they do not conflict on their use of the bus
// the command handler is also the one with the higher priority, so if it is
// blocking on waiting for a response, it will be the first one to run
void time_broadcaster(void *pv_params) {
    time_broadcaster_ctx *ctx = reinterpret_cast<time_broadcaster_ctx *>(pv_params);

    for (;;) {
        // only run on normal mode and after launch mode has been enabled
        ctx->state_machine->check_no_delay_and_normal_or_wait();

        CanFrame txFrame = {};

        txFrame.identifier = CanCommands::TIMESTAMP_SYNC;
        txFrame.extd = 0;
        txFrame.data_length_code = sizeof(unsigned long);

        unsigned long current_micros_time = micros();
        memcpy(txFrame.data, &current_micros_time, txFrame.data_length_code);

        ctx->can_bus->writeFrame(txFrame);
        RP_LOG_TRACE(TIME_SYNC, "Broadcasted radio time");

        vTaskDelay(TIME_BETWEEN_CAN_BROADCASTS);
    }
}