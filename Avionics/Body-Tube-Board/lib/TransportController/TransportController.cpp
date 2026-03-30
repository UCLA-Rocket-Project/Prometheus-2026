#include "TransportController.h"

void TransportController::queue_item_to_send(
    uint8_t *payload, size_t payload_size, TransportMessageType message_type
) {
    if (message_type == TransportController::TransportMessageType::COMMAND) {
        uint8_t *msg_buf;
        if (xRingbufferSendAcquire(
                _command_ringbuf, (void **)&msg_buf, payload_size, portMAX_DELAY
            ) == pdFALSE) {
#ifdef RADIO_SERIAL_DEBUG
            RP_LOG_WARN(SERIAL_CMD_TRANSPORT, "Could not acquire space in message ringbuf");
#else
            RP_LOG_WARN(RADIO_CMD_TRANSPORT, "Could not acquire space in message ringbuf");
#endif

            return;
        }

        memcpy(msg_buf, payload, payload_size);
        xRingbufferSendComplete(_command_ringbuf, msg_buf);
    } else if (message_type == TransportController::TransportMessageType::REGULAR) {
        uint8_t *msg_buf;
        if (xRingbufferSendAcquire(
                _regular_ringbuf, (void **)&msg_buf, payload_size, portMAX_DELAY
            ) == pdFALSE) {
#ifdef RADIO_SERIAL_DEBUG

            RP_LOG_WARN(SERIAL_CMD_TRANSPORT, "Could not acquire space in regular ringbuf");
#else
            RP_LOG_WARN(RADIO_CMD_TRANSPORT, "Could not acquire space in regular ringbuf");
#endif

            return;
        }

        memcpy(msg_buf, payload, payload_size);
        xRingbufferSendComplete(_regular_ringbuf, msg_buf);
    }
}

void TransportController::queue_command_failed(uint8_t failure_code) {
    uint8_t *msg_buf;
    if (xRingbufferSendAcquire(_command_ringbuf, (void **)&msg_buf, 1, portMAX_DELAY) == pdFALSE) {
#ifdef RADIO_SERIAL_DEBUG

        RP_LOG_WARN(SERIAL_CMD_TRANSPORT, "Could not acquire space in RB");
#else
        RP_LOG_WARN(RADIO_CMD_TRANSPORT, "Could not acquire space in RB");
#endif

        return;
    }

    msg_buf[0] = failure_code;

    xRingbufferSendComplete(_command_ringbuf, msg_buf);
}