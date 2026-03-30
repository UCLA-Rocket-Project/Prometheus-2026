#include "SerialController.h"
#include "GlobalStore.h"
#include "HardwareSerial.h"
#include "StateMachine.h"
#include "TransportController.h"
#include "freertos/ringbuf.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/unistd.h>

SerialController::SerialController(StateMachine *state_machine)
    : TransportController(state_machine) {
    _command_ringbuf = xRingbufferCreate(SERIAL_RINGBUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    _regular_ringbuf = xRingbufferCreate(SERIAL_RINGBUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
}

bool SerialController::begin() { return true; }

bool SerialController::process_received_message(radio_uplink_payload &uplink_payload) {
    uint8_t serial_buffer[SERIAL_COMMAND_BUFFER_SIZE]{};
    int i = 0;
    while (Serial.available() && i < SERIAL_COMMAND_BUFFER_SIZE - 1) {
        serial_buffer[i] = Serial.read();
        i++;
    }

    serial_buffer[i] = '\0';

    if (i < SERIAL_COMMANDER_PATTERN_REPEAT_COUNT + 1) {
        return false;
    }

    // Check that the last 3 bytes are "+++"
    if (memcmp(
            (serial_buffer + i - SERIAL_COMMANDER_PATTERN_REPEAT_COUNT),
            SERIAL_EXPECTED_COMMAND_SEQUENCE,
            SERIAL_COMMANDER_PATTERN_REPEAT_COUNT
        ) != 0) {
        return false;
    }

    uplink_payload.command_code = serial_buffer[0];

    // only memcpy if the payload has more than just a command
    if (i > SERIAL_COMMANDER_PATTERN_REPEAT_COUNT + 1) {
        memcpy(uplink_payload.payload, serial_buffer + 1, i - 4);
        uplink_payload.payload_size = i - 4;
    }

    return true;
}

size_t SerialController::send_command_message_response() {
    size_t payload_size;
    uint8_t *payload = reinterpret_cast<uint8_t *>(
        xRingbufferReceive(_command_ringbuf, &payload_size, portMAX_DELAY)
    );

    if (payload_size == 0) {
        return 0;
    }

    Serial.write(SERIAL_RESPONSE_START_SEQUENCE, 2);
    size_t sent_size = Serial.write(payload, payload_size);
    Serial.write(SERIAL_RESPONSE_END_SEQUENCE, 2);

    vRingbufferReturnItem(_command_ringbuf, payload);

    return sent_size;
}
size_t SerialController::send_regular_message_response() {
    size_t payload_size;
    uint8_t *payload = reinterpret_cast<uint8_t *>(
        xRingbufferReceive(_regular_ringbuf, &payload_size, portMAX_DELAY)
    );

    if (payload_size == 0) {
        return 0;
    }

    // TODO: send message here?

    return payload_size;
}

void SerialController::set_radio_in_launch_configuration() { return; }