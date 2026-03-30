#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <stdint.h>

#include "buffered_sd.h"
#include "freertos/idf_additions.h"
#include "pins.h"
#include "util.h"
#include <RP_Logger.h>

// flag to tell you if the function is in inspect mode or not
// bottom 2 bits have influence on the states
#define MODE_NO_OP_FLAG    0b0
#define MODE_REGULAR_FLAG  0b0001
#define MODE_INSPECT_FLAG  0b0010

// next 2 bits are uplinking flags that should not influence the state behavior
#define MODE_LAUNCH_FLAG   0b0100
#define MODE_SD_CHECK_FLAG 0b1000

class StateMachine {
  public:
    StateMachine(BufferedSD *sd, uint8_t initial_mode = MODE_REGULAR_FLAG);
    ~StateMachine();

    enum States { NORMAL, INSPECT, NOOP, ERROR };

    // these functions should probably have the highest priority

    // there is some concern also about how to ensure that all the other tasks
    // reach a known state if this mode transition occurs during a task itself

    // perhaps a better solution would be to just reset the entire board when transitioning back
    // into normal mode
    void enter_normal_mode();
    void enter_inspect_mode();
    void enter_no_op_mode();

    void enter_launch_mode();

    void enter_check_sd_mode();

    // we need this just in case the SD check mode fails and the board is left in a state where the
    // normal broadcast does not work by clearing this everytime we enter inspect, we would always
    // allow the board to broadcast again properly
    // I guess it requires some extra knowledge of how the system works, but I think that is alright
    void clear_enter_sd_mode();

    inline States get_current_state();
    inline bool is_launch_mode();
    inline bool is_check_sd_state();

    // if not in desired state, block until it is
    inline void check_normal_or_wait();
    inline void check_inspect_or_wait();
    inline void check_no_delay_and_normal_or_wait();

  private:
    EventGroupHandle_t _sm_event_group = nullptr;
    BufferedSD *_sd;
};

inline void StateMachine::check_normal_or_wait() {
    xEventGroupWaitBits(_sm_event_group, MODE_REGULAR_FLAG, pdFALSE, pdTRUE, portMAX_DELAY);
}

inline void StateMachine::check_inspect_or_wait() {
    xEventGroupWaitBits(_sm_event_group, MODE_INSPECT_FLAG, pdFALSE, pdTRUE, portMAX_DELAY);
}

inline void StateMachine::check_no_delay_and_normal_or_wait() {
    xEventGroupWaitBits(
        _sm_event_group, (MODE_REGULAR_FLAG | MODE_LAUNCH_FLAG), pdFALSE, pdTRUE, portMAX_DELAY
    );
}

inline StateMachine::States StateMachine::get_current_state() {
    EventBits_t state = xEventGroupGetBits(_sm_event_group);

    if ((state & 0b11) == MODE_REGULAR_FLAG) {
        return NORMAL;
    } else if ((state & 0b11) == MODE_INSPECT_FLAG) {
        return INSPECT;
    } else if ((state & 0b11) == MODE_NO_OP_FLAG) {
        return NOOP;
    }

    RP_LOG_WARN(STATE_MACHINE, "Accidentally reached this buggy state! %X", state);
    return ERROR;
}

inline bool StateMachine::is_launch_mode() {
    EventBits_t state = xEventGroupGetBits(_sm_event_group);
    return ((state & MODE_LAUNCH_FLAG) != 0);
}

inline bool StateMachine::is_check_sd_state() {
    EventBits_t state = xEventGroupGetBits(_sm_event_group);
    return ((state & MODE_SD_CHECK_FLAG) != 0);
}

#endif