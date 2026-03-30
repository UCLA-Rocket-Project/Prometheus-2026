#include "RadioController.h"
#include "GlobalStore.h"
#include "LoRa.h"
#include "RP_Logger.h"
#include "StateMachine/StateMachine.h"
#include "TransportController.h"
#include "freertos/idf_additions.h"
#include "radio_commands.h"
#include <cstdint>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/portmacro.h>
#include <freertos/projdefs.h>

#define PACKET_IDENTIFIER_OVERHEAD 3

extern TaskHandle_t _ringbuf_receive_task_handle;
extern TaskHandle_t _reset_task_handler;

void radio_receive_callback(int packet_size) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xTaskNotifyFromISR(
        _ringbuf_receive_task_handle, packet_size, eSetValueWithOverwrite, &xHigherPriorityTaskWoken
    );
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

RadioController::RadioController(SPIClass &spi_bus, StateMachine *sm, size_t ringbuf_size)
    : TransportController(sm), _spi_bus(&spi_bus), _message_fail_count(0) {
    _radio_mutex = xSemaphoreCreateMutex();
    // command ringbuf can be smaller, you would not expect much from it
    _command_ringbuf = xRingbufferCreate(ringbuf_size, RINGBUF_TYPE_NOSPLIT);
    _regular_ringbuf = xRingbufferCreate(ringbuf_size, RINGBUF_TYPE_NOSPLIT);

    _ack_map = xEventGroupCreate();
}

bool RadioController::begin() {
    // Configure LoRa pins and SPI here (not in constructor) to avoid
    // static initialization order fiasco with the global LoRa object.
    // The global LoRa object in LoRa.cpp might be constructed after this class,
    // which would overwrite any configuration done in the constructor.
    LoRa.setSPI(*_spi_bus);
    LoRa.setPins(SPI4_CS, RADIO_RESET, DI0);

    if (!LoRa.begin(RADIO_FREQUENCY)) {
        return false;
    }

    LoRa.setTxPower(17);

    // Setup Antenna Switch Pins
    pinMode(RADIO_RXEN, OUTPUT);
    pinMode(RADIO_TXEN, OUTPUT);

    // Default to Receive Mode
    digitalWrite(RADIO_RXEN, HIGH);
    digitalWrite(RADIO_TXEN, LOW);

#ifdef USE_INTERRUPTS
    LoRa.onReceive(radio_receive_callback);
    LoRa.receive();
#endif

    // this can be maximally slow, since we want reliability here
    LoRa.setCodingRate4(8);
    LoRa.setSignalBandwidth(62500);
    LoRa.setSpreadingFactor(9);
    LoRa.enableCrc();
    LoRa.setSyncWord(0x72);

    return true;
}

#ifdef USE_INTERRUPTS
bool RadioController::process_received_message(radio_uplink_payload &uplink_payload) {
    // NOTE: This places some limitations on how the task using this can use it
    // but we are not going to think so much about that for now
    uint32_t received_packet_size{};
    BaseType_t result = xTaskNotifyWait(0x00, 0xFFFFFFFF, &received_packet_size, portMAX_DELAY);

    if (result == pdFALSE || received_packet_size <= 0) {
        return false;
    }

    xSemaphoreTake(_radio_mutex, portMAX_DELAY);
    uint8_t radio_buffer[RADIO_COMMAND_BUFFER_SIZE];
    size_t i = 0;

    // Read exactly 'receivedPacketSize' bytes
    while (i < received_packet_size && LoRa.available()) {
        radio_buffer[i] = LoRa.read();
        i++;
    }
    // NOTE: maybe removing print here might have changed something

    xSemaphoreGive(_radio_mutex);
    radio_buffer[i] = '\0';

    bool process_res = _process_received_message(radio_buffer, i, uplink_payload);
    xSemaphoreGive(_radio_mutex);

    return process_res;
}
#else
bool RadioController::process_received_message(radio_uplink_payload &uplink_payload) {
    xSemaphoreTake(_radio_mutex, portMAX_DELAY);

    // we dont use the packet size given by the outer loop here
    size_t payload_size = LoRa.parsePacket();
    if (payload_size == 0) {
        xSemaphoreGive(_radio_mutex);
        return false;
    } else if (!LoRa.available()) {
        xSemaphoreGive(_radio_mutex);
        return false;
    }

    uint8_t radio_buffer[RADIO_COMMAND_BUFFER_SIZE];
    size_t i = 0;

    while (LoRa.available() && i < RADIO_COMMAND_BUFFER_SIZE) {
        radio_buffer[i] = LoRa.read();
        i++;
    }

    radio_buffer[i] = '\0';

    RP_LOG_TRACE(RADIO_CMD_TRANSPORT, "Received a mesage of %u bytes", i);

    // just like in the uplinking board, I would prefer to hold on to this mutex the entire time I
    // am processing the data and when I am sending the ACK, so that I dont get interrupted by
    // another sending task that gets queued up
    bool process_res = _process_received_message(radio_buffer, i, uplink_payload);

    RP_LOG_TRACE(
        RADIO_CMD_TRANSPORT, "Processed message. CMD code is %x", uplink_payload.command_code
    );

    xSemaphoreGive(_radio_mutex);

    return process_res;
}
#endif

size_t RadioController::send_command_message_response() {
    size_t payload_size;
    uint8_t *payload = reinterpret_cast<uint8_t *>(
        xRingbufferReceive(_command_ringbuf, &payload_size, portMAX_DELAY)
    );

    if (payload_size == 0) {
        return 0;
    }

    xSemaphoreTake(_radio_mutex, portMAX_DELAY);

    size_t sent_size = _send_command_message(payload, payload_size);

    RP_LOG_TRACE(
        RADIO_CMD_TRANSPORT, "Sent a payload in command mode: sz: %lu, val: %d", sent_size, *payload
    );

    _wait_for_ack_or_retransmit(payload, payload_size);

    // the last command sent out by the radio in command mode is when
    // we get it to transition to launch mode, and ack that it is back in normal mode
    if (_state_machine->is_launch_mode() && payload_size == 1 &&
        payload[0] == RadioCommands::ENTER_NORMAL_MODE_RADIO) {
        vTaskDelete(_ringbuf_receive_task_handle);
        set_radio_in_launch_configuration();
    }

    xSemaphoreGive(_radio_mutex);
    vRingbufferReturnItem(_command_ringbuf, payload);

    return sent_size;
}

size_t RadioController::send_regular_message_response() {
    size_t payload_size;
    uint8_t *payload = reinterpret_cast<uint8_t *>(
        xRingbufferReceive(_regular_ringbuf, &payload_size, portMAX_DELAY)
    );

    if (payload_size == 0) {
        return 0;
    }

    if (!_state_machine->is_check_sd_state()) {
        RP_LOG_TRACE(RADIO_CMD_TRANSPORT, "Sending a message in normal mode");
        xSemaphoreTake(_radio_mutex, portMAX_DELAY);

        digitalWrite(RADIO_RXEN, LOW);
        digitalWrite(RADIO_TXEN, HIGH);

        LoRa.beginPacket();

        LoRa.write(RadioCommands::RADIO_REGULAR_MSG_START_BYTE_1);
        LoRa.write(RadioCommands::RADIO_REGULAR_MSG_START_BYTE_2);
        size_t sent_size = LoRa.write(payload, payload_size);
        LoRa.write(RadioCommands::RADIO_REGULAR_MSG_END_BYTE);

        LoRa.endPacket();

        digitalWrite(RADIO_TXEN, LOW);
        digitalWrite(RADIO_RXEN, HIGH);

#if USE_INTERRUPTS
        LoRa.receive();
#endif
        xSemaphoreGive(_radio_mutex);
    } else {
        RP_LOG_TRACE(RADIO_CMD_TRANSPORT, "Skipped sending message in normal mode");
    }

    vRingbufferReturnItem(_regular_ringbuf, payload);

    return 0;
}

void RadioController::set_radio_in_launch_configuration() {
    // TODO: test the data rates on this
    LoRa.setCodingRate4(6);
    LoRa.setSignalBandwidth(125000);
    LoRa.setSpreadingFactor(9);
}

void RadioController::_set_ack_received() {
    RP_LOG_INFO(RADIO_CMD_TRANSPORT, "Received an ACK!");
    xEventGroupSetBits(_ack_map, AckMapFlags::ACK_RECEIVED);
}

void RadioController::_clear_ack_received() {
    RP_LOG_INFO(RADIO_CMD_TRANSPORT, "Clear ack received...");
    xEventGroupClearBits(_ack_map, AckMapFlags::ACK_RECEIVED);
}

bool RadioController::_wait_on_ack() {
    if (_state_machine->is_launch_mode()) {
        return true;
    }

    // NOTE: This really caught me off guard, but we have to release the mutex here else we'll have
    // a deadlock
    xSemaphoreGive(_radio_mutex);

    EventBits_t state = xEventGroupWaitBits(
        _ack_map, AckMapFlags::ACK_RECEIVED, pdFALSE, pdTRUE, RADIO_ACK_WAIT_TIMEOUT
    );

    xSemaphoreTake(_radio_mutex, portMAX_DELAY);

    if (state == AckMapFlags::ACK_RECEIVED) {
        _clear_ack_received();
        RP_LOG_INFO(RADIO_CMD_TRANSPORT, "Ack Received!");
        return true;
    }

    return false;
}

void RadioController::_wait_for_ack_or_retransmit(uint8_t *payload, size_t payload_size) {
    bool ack_received = false;
    for (int i = 0; i < RADIO_RETRANSMIT_ATTEMPTS; i++) {
        if (!_wait_on_ack()) {
            RP_LOG_WARN(RADIO_CMD_TRANSPORT, "Did not receive ack, retransmitting...", millis());
            size_t sent_size = _send_command_message(payload, payload_size);
            RP_LOG_WARN(
                RADIO_CMD_TRANSPORT, "Resent paylod: sz: %lu, val: %d", sent_size, *payload
            );

            _message_fail_count++;
        } else {
            RP_LOG_INFO(RADIO_CMD_TRANSPORT, "Got ack!");
            ack_received = true;
            break;
        }
    }

    if (ack_received) {
        _message_fail_count = 0;
    } else if (_message_fail_count > 2) {
        xTaskNotify(_reset_task_handler, 0x33, eSetValueWithOverwrite);
    }
}

size_t RadioController::_send_command_message(uint8_t *payload, size_t payload_size) {
    digitalWrite(RADIO_RXEN, LOW);
    digitalWrite(RADIO_TXEN, HIGH);

    LoRa.beginPacket();

    LoRa.write(RadioCommands::RADIO_CMD_MSG_START_BYTE_1);
    LoRa.write(RadioCommands::RADIO_CMD_MSG_START_BYTE_2);
    size_t sent_size = LoRa.write(payload, payload_size);
    LoRa.write(RadioCommands::RADIO_CMD_MSG_END_BYTE);

    LoRa.endPacket();

    digitalWrite(RADIO_TXEN, LOW);
    digitalWrite(RADIO_RXEN, HIGH);

#ifdef USE_INTERRUPTS
    LoRa.receive();
#endif

    return sent_size;
}

bool RadioController::_process_received_message(
    uint8_t radio_buffer[], size_t buffer_size, radio_uplink_payload &uplink_payload
) {
    // expected message 0x08 0x09 CC <data?> 0x1F
    if (radio_buffer[0] != RadioCommands::RADIO_CMD_MSG_START_BYTE_1 ||
        radio_buffer[1] != RadioCommands::RADIO_CMD_MSG_START_BYTE_2 ||
        radio_buffer[buffer_size - 1] != RadioCommands::RADIO_CMD_MSG_END_BYTE) {
        RP_LOG_WARN(
            RADIO_CMD_TRANSPORT,
            "Incorrect task sequence detected! %x, %x, %x, %x",
            radio_buffer[0],
            radio_buffer[1],
            radio_buffer[2],
            radio_buffer[buffer_size - 1]
        );
        uplink_payload.command_code = RadioCommands::RADIO_START_OR_END_WRONG;
        return false;
    }

    RP_LOG_TRACE(RADIO_CMD_TRANSPORT, "Received valid packet of length: %lu", buffer_size);

    // if we received an ACK, we can just return early
    if (radio_buffer[2] == RadioCommands::RADIO_MESSAGE_ACK) {
        uplink_payload.command_code = RadioCommands::RADIO_MESSAGE_ACK;
        _set_ack_received();
        return true;
    }

    // if the command is good, send back an ACK
    uint8_t command_ack[] = {RadioCommands::RADIO_MESSAGE_ACK};

    // just like on the uplinking board, add a timeout here
    // I guess some food for thought is what is to happen if the message is received successfully,
    // but the ACK was not properly received? The command will be given again, but we do not
    // currently handle that
    delay(1000);
    _send_command_message(command_ack, 1);
    delay(500);

    RP_LOG_TRACE(RADIO_CMD_TRANSPORT, "Sent an ACK for %x", radio_buffer[2]);

    uplink_payload.command_code = radio_buffer[2];

    // if the packet length is more than 4, it means that there is extra metadata to add
    // example: 0x08 0x09 CC XX XX XX XX XX XX XX XX 0x1F
    if (buffer_size > PACKET_IDENTIFIER_OVERHEAD + 1) {
        RP_LOG_TRACE(RADIO_CMD_TRANSPORT, "Message is of size: %lu", buffer_size);
        memcpy(uplink_payload.payload, radio_buffer + 3, buffer_size - 4);
        uplink_payload.payload_size = buffer_size - 4;
    }

    return true;
}
