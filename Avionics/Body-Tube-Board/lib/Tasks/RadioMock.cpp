#include "RadioBoardTasks.h"
#include "TransportController.h"

void radio_mock(void *pv_params) {
    radio_mock_ctx *ctx = reinterpret_cast<radio_mock_ctx *>(pv_params);

    for (;;) {
        char val[] = "Hi there!";
        ctx->radio_ctx->queue_item_to_send(
            (uint8_t *)val, strlen(val), TransportController::TransportMessageType::REGULAR
        );
        delay(1000);
    }
}