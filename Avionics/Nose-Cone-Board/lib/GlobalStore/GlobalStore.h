#ifndef GLOBAL_STORE_H
#define GLOBAL_STORE_H

#include <freertos/FreeRTOS.h>

struct GlobalClock {
    // will be jumped once when we send the uplinking command
    // will all be in units of microseconds
    int64_t last_update_recv_time{0};
    int64_t global_time{0};

    // I would say for now tht there is no need for the mutex, since the only time the global clock
    // wil lbe modified is in inspect mode
    // SemaphoreHandle_t mu = NULL;
};

#define UPLINK_PAYLOAD_MAX_SIZE 8
struct radio_uplink_payload {
    uint8_t command_code{0xFF};
    uint8_t payload_size{0};
    uint8_t payload[UPLINK_PAYLOAD_MAX_SIZE]{};
};

#endif