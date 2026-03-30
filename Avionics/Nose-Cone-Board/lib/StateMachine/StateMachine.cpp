#include "StateMachine.h"
#include "FreeRTOSConfig.h"
#include "RP_Logger.h"
#include "buffered_sd.h"
#include "freertos/idf_additions.h"

StateMachine::StateMachine(BufferedSD *sd, uint8_t initial_mode) : _sd(sd) {
    _sm_event_group = xEventGroupCreate();

    if (!_sm_event_group) {
        RP_LOG_WARN(STATE_MACHINE, "Could not initialize event group for state machine");
    }

    xEventGroupSetBits(_sm_event_group, initial_mode);
}

StateMachine::~StateMachine() {
    vEventGroupDelete(_sm_event_group);
    _sm_event_group = nullptr;
}

void StateMachine::enter_normal_mode() {
    // I'd reason you dont need a mutex here, just because there is ever only one thread modifying
    // the state flags
    RP_LOG_TRACE(STATE_MACHINE, "Entering normmal mode...");
    xEventGroupClearBits(_sm_event_group, MODE_INSPECT_FLAG);
    EventBits_t final_state = xEventGroupSetBits(_sm_event_group, MODE_REGULAR_FLAG);
    configASSERT(
        (final_state == MODE_REGULAR_FLAG ||
         final_state == (MODE_REGULAR_FLAG | MODE_LAUNCH_FLAG) ||
         final_state == (MODE_REGULAR_FLAG | MODE_SD_CHECK_FLAG)) &&
        "Board was left in an invalid state"
    );
}

void StateMachine::enter_inspect_mode() {
    RP_LOG_TRACE(STATE_MACHINE, "Entering inspect mode...");
    xEventGroupClearBits(_sm_event_group, MODE_REGULAR_FLAG);
    EventBits_t final_state = xEventGroupSetBits(_sm_event_group, MODE_INSPECT_FLAG);
    configASSERT(
        (final_state == MODE_INSPECT_FLAG ||
         final_state == (MODE_INSPECT_FLAG | MODE_LAUNCH_FLAG) ||
         final_state == (MODE_INSPECT_FLAG | MODE_SD_CHECK_FLAG)) &&
        "Board was left in an invalid state"
    );
}

void StateMachine::enter_no_op_mode() {
    RP_LOG_TRACE(STATE_MACHINE, "Entering no-op mode...");
    xEventGroupClearBits(_sm_event_group, (MODE_REGULAR_FLAG | MODE_INSPECT_FLAG));
}

void StateMachine::enter_check_sd_mode() {
    RP_LOG_TRACE(STATE_MACHINE, "Enter SD check mode...");
    EventBits_t final_state = xEventGroupSetBits(_sm_event_group, MODE_SD_CHECK_FLAG);
    // this should only ever be triggered in inspect mode
    configASSERT(
        final_state == (MODE_INSPECT_FLAG | MODE_SD_CHECK_FLAG) &&
        "Board was left in an invalid state"
    );
}

void StateMachine::clear_enter_sd_mode() {
    xEventGroupClearBits(_sm_event_group, MODE_SD_CHECK_FLAG);
}

static void play_launch_music() {
    constexpr int melody[] = {659, 659, 0, 659, 0, 523, 659, 0, 784};

    constexpr int durations[] = {150, 150, 150, 150, 150, 150, 150, 150, 300};

    int notes = sizeof(melody) / sizeof(melody[0]);

    for (int i = 0; i < notes; i++) {
        if (melody[i] > 0)
            RP_TONE(BUZZ, melody[i]);
        delay(durations[i]);
        noTone(BUZZ);
        delay(30);
    }
}

void StateMachine::enter_launch_mode() {
    RP_LOG_TRACE(STATE_MACHINE, "Removing send delay flag...");
    xEventGroupSetBits(_sm_event_group, MODE_LAUNCH_FLAG);
    _sd->clear_config_file();
    delay(10);
    _sd->update_config("111", 3);
    play_launch_music();
}