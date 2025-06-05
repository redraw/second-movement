/*
 * MIT License
 *
 * Copyright (c) 2025
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include "sleep_calculator_face.h"
#include "watch.h"
#include "watch_utility.h"

// Sleep cycle durations in minutes (including 15 min to fall asleep)
static const uint16_t SLEEP_DURATIONS[] = {
    1,    // test, 1 minute
    285,  // 4.5 hours (4h 45m including 15m to fall asleep)
    375,  // 6 hours (6h 15m including 15m to fall asleep)
    465,  // 7.5 hours (7h 45m including 15m to fall asleep)
    555   // 9 hours (9h 15m including 15m to fall asleep)
};

static const char* SLEEP_LABELS[] = {
    " 1",  // test, 1 minute
    ".4",  // 4.5 hours
    " 6",  // 6 hours
    ".7",  // 7.5 hours
    " 9"   // 9 hours
};

#define SLEEP_CALCULATOR_FACE_MAX_OPTIONS (int)(sizeof(SLEEP_DURATIONS) / sizeof(SLEEP_DURATIONS[0]))

//
// Private functions
//
static void _sleep_calculator_face_display_time(uint8_t hour, uint8_t minute) {
    uint8_t display_hour = hour;
    if (!movement_clock_mode_24h()) {
        if (hour >= 12) {
            watch_set_indicator(WATCH_INDICATOR_PM);
        } else {
            watch_clear_indicator(WATCH_INDICATOR_PM);
        }
        display_hour = hour % 12;
        if (display_hour == 0) display_hour = 12;
    } else {
        watch_set_indicator(WATCH_INDICATOR_24H);
    }
    
    // Display time and sleep duration label
    static char time_buf[7];
    sprintf(time_buf, "%2d%02d", display_hour, minute);
    watch_display_text(WATCH_POSITION_BOTTOM, time_buf);
    watch_set_colon();
}

static void _sleep_calculator_face_show_alarm(sleep_calculator_face_state_t *state) {
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "WAK", "WK");
    watch_set_indicator(WATCH_INDICATOR_BELL);
    _sleep_calculator_face_display_time(state->alarm_hour, state->alarm_minute);
    watch_display_text(WATCH_POSITION_TOP_RIGHT, "   ");
}                

static void _sleep_calculator_face_update_display(sleep_calculator_face_state_t *state) {
    uint8_t ref_hour = state->reference_hour;
    uint8_t ref_minute = state->reference_minute;
    uint16_t duration = SLEEP_DURATIONS[state->current_option];
    
    // Calculate when to wake up based on sleep time (current time)
    // Add sleep duration to sleep time
    int total_minutes = ref_hour * 60 + ref_minute + duration;
    uint8_t calc_hour = (total_minutes / 60) % 24;
    uint8_t calc_minute = total_minutes % 60;
    
    // Store calculated alarm time for later use
    state->alarm_hour = calc_hour;
    state->alarm_minute = calc_minute;
    
    // Display mode indicator
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "WAK", "WK");
    
    // Show bell icon if alarm is set
    if (state->alarm_is_set) {
        watch_set_indicator(WATCH_INDICATOR_BELL);
    } else {
        watch_clear_indicator(WATCH_INDICATOR_BELL);
    }
    
    // Display the calculated time
    _sleep_calculator_face_display_time(calc_hour, calc_minute);

    // Show sleep duration in top right
    watch_display_text(WATCH_POSITION_TOP_RIGHT, SLEEP_LABELS[state->current_option]);
}

static inline void button_beep() {
    if (movement_button_should_sound()) {
        watch_buzzer_play_note_with_volume(BUZZER_NOTE_C7, 50, movement_button_volume());
    }
}

//
// Exported functions
//

void sleep_calculator_face_setup(uint8_t watch_face_index, void **context_ptr) {
    (void) watch_face_index;
    
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(sleep_calculator_face_state_t));
        sleep_calculator_face_state_t *state = (sleep_calculator_face_state_t *)*context_ptr;
        memset(*context_ptr, 0, sizeof(sleep_calculator_face_state_t));
        
        // Default to 7.5 hours of sleep
        state->current_option = 2; // 7.5 hours
        state->reference_hour = 0;
        state->reference_minute = 0;
    }
}

void sleep_calculator_face_activate(void *context) {
    sleep_calculator_face_state_t *state = (sleep_calculator_face_state_t *)context;

    // Use current time as reference when activating
    watch_date_time_t now = movement_get_local_date_time();
    state->reference_hour = now.unit.hour;
    state->reference_minute = now.unit.minute;
    
    if (state->alarm_is_set) {
        // If alarm is set, show it immediately
        _sleep_calculator_face_show_alarm(state);
    } else {
        // Otherwise, update display with current time and selected sleep duration
        _sleep_calculator_face_update_display(state);
    }
}

void sleep_calculator_face_resign(void *context) {
    (void) context;
}

bool sleep_calculator_face_loop(movement_event_t event, void *context) {
    sleep_calculator_face_state_t *state = (sleep_calculator_face_state_t *)context;
    
    switch (event.event_type) {
        case EVENT_ACTIVATE:
            break;
            
        case EVENT_TICK:
            {
                if (!state->alarm_is_set) {
                    // Update reference time every minute to keep calculations current
                    watch_date_time_t now = movement_get_local_date_time();
                    if (now.unit.minute != state->reference_minute) {
                        state->reference_hour = now.unit.hour;
                        state->reference_minute = now.unit.minute;
                        _sleep_calculator_face_update_display(state);
                    }
                }
            }
            break;
            
        case EVENT_LIGHT_BUTTON_DOWN:
            // Normal LED behavior
            movement_illuminate_led();
            break;
            
        case EVENT_ALARM_BUTTON_UP:
            // Set alarm off
            state->alarm_is_set = false;
            movement_set_alarm_enabled(false);
            // Cycle through sleep duration options
            state->current_option = (state->current_option + 1) % SLEEP_CALCULATOR_FACE_MAX_OPTIONS;
            _sleep_calculator_face_update_display(state);
            button_beep();
            break;
            
        case EVENT_ALARM_LONG_PRESS:
            {
                // Toggle alarm on
                state->alarm_is_set = true;
                movement_set_alarm_enabled(true);
                _sleep_calculator_face_update_display(state);
                button_beep();
            }
            break;
            
        case EVENT_BACKGROUND_TASK:
            // Play alarm when wake time is reached
            movement_play_alarm();
            // Reset alarm after playing
            state->alarm_is_set = false;
            movement_set_alarm_enabled(false);
            break;
            
        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;
            
        case EVENT_LOW_ENERGY_UPDATE:
            break;
            
        default:
            movement_default_loop_handler(event);
            break;
    }
    
    return true;
}

movement_watch_face_advisory_t sleep_calculator_face_advise(void *context) {
    sleep_calculator_face_state_t *state = (sleep_calculator_face_state_t *)context;
    movement_watch_face_advisory_t retval = { 0 };

    if (state->alarm_is_set) {
        watch_date_time_t now = movement_get_local_date_time();
        
        // Use stored alarm time instead of recalculating
        retval.wants_background_task = (state->alarm_hour == now.unit.hour && state->alarm_minute == now.unit.minute);
    }

    return retval;
}
