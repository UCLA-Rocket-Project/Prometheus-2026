#ifndef RADIO_BOARD_TASKS_H
#define RADIO_BOARD_TASKS_H

#include "CommandHandler.h"
#include "ESP32-TWAI-CAN.hpp"
#include "GlobalStore.h"
#include "RadioController.h"
#include "StateMachine.h"
#include "TransportController.h"
#include "buffered_sd.h"

#include "freertos/idf_additions.h"
#include "util.h"

#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

struct radio_receiver_ctx {
    TransportController *radio_ctx;
    QueueHandle_t command_queue;
};

void radio_receiver(void *pv_params);

struct serial_command_receiver_ctx {
    TransportController *serial_ctx;
    QueueHandle_t command_queue;
    QueueHandle_t uart_events_queue;
};

void serial_command_receiver(void *pv_params);

struct radio_sender_ctx {
    TransportController *radio_ctx;
    StateMachine *state_machine;
};

void radio_regular_sender(void *pv_params);

struct radio_command_sender_ctx {
    TransportController *radio_ctx;
    StateMachine *state_machine;
};

void radio_command_sender(void *pv_params);

struct radio_mock_ctx {
    TransportController *radio_ctx;
};

void radio_mock(void *pv_params);

struct time_broadcaster_ctx {
    StateMachine *state_machine;
    TwaiCAN *can_bus;
};

void time_broadcaster(void *pv_params);

struct background_can_receiver_ctx {
    StateMachine *state_machine;
    TwaiCAN *can_bus;
    BufferedSD *sd;
    TransportController *radio_ctx;
};

// run this on the other core
// this will handle
// 1. receiving of background can messages
// 2. using that to update global state
// 3. broadcasting this new global state over radio
// since the scheduling queue is shared, it is guaranteed that even though there are 2 objects
// wating for a message on the same can bus, that only the one with the higher priority would read
// from the CAN bus
void can_background_receiver(void *pv_params);

// not sure if this is the cleanest way to implement a reset function
// but I will make a slightly scuffed implementation first
struct reset_task_ctx {
    TwaiCAN *can_bus;
    StateMachine *state_machine;
};

/**
Waits indefinitely for the reset handler to be called. Will not run otherwise
*/
void reset_task(void *pv_params);

struct can_dispatcher_ctx {
    QueueHandle_t command_queue;
    CommandHandler **command_handler_ctxs;
    TransportController *radio_ctx;
    StateMachine *state_machine_ctx;
};

void can_dispatcher(void *pv_params);

#endif