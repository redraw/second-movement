/*
 * MIT License
 *
 * Copyright (c) 2022 Joey Castillo
 * Copyright (c) 2025 Agustin Bacigalup <redraw@sdf.org>
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
#include "activity_logging_face.h"
#include "filesystem.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_common_display.h"

static char _activity_logging_get_mood_face(uint16_t active_minutes) {
    if (active_minutes >= 150) {
        return ')';  // happy: 150+ minutes
    } else if (active_minutes >= 30) {
        return '1';  // neutral: 30-149 minutes
    } else {
        return '(';  // sad: 0-29 minutes
    }
}

static void _activity_logging_face_update_display(activity_logging_state_t *state) {
    char buf[8];
    watch_date_time_t timestamp = movement_get_local_date_time();

    // Set title for day mode
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "ACT", "AC");

    if (state->display_index == 0) {
        // if we are at today, just show the count so far
        sprintf(buf, "%2d", timestamp.unit.day);
        watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);

        if (state->show_emoticon) {
            char mood_face = _activity_logging_get_mood_face(state->active_minutes_today);
            sprintf(buf, "  %c  ", mood_face);
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
            watch_set_colon();
        } else {
            sprintf(buf, "%4d  ", state->active_minutes_today);
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
        }

        // also indicate that this is the active day — we are still sensing active minutes!
        watch_set_indicator(WATCH_INDICATOR_SIGNAL);
    } else {
        // otherwise we need to go into the log.
        watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
        int32_t pos = ((int16_t)state->data_points - (int32_t)state->display_index) % ACTIVITY_LOGGING_NUM_DAYS;
        
        // display date using helper function
        snprintf(buf, 8, "%2d", _get_day_of_month_for_days_back(state->display_index));
        watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);

        if (pos < 0) {
            // no data at this index
            watch_display_text(WATCH_POSITION_BOTTOM, "no dat");
        } else {
            // we are displaying the number active minutes
            snprintf(buf, 8, "%4d  ", state->activity_log[pos]);
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
        }
    }
}

static void _activity_logging_handle_histogram_page(activity_logging_state_t *state) {
    char buf[8];
    
    // Prepare data array for statistics calculation
    uint16_t data[12];
    uint8_t count = 12;
    
    if (state->timeframe_mode == ACTIVITY_LOGGING_TIMEFRAME_12H) {
        // 12-hour mode: collect last 12 hours of data
        watch_date_time_t datetime = movement_get_local_date_time();
        int current_hour = datetime.unit.hour;
        
        for (int i = 0; i < 12; i++) {
            int hour_index = (current_hour - 11 + i + 24) % 24;
            data[i] = state->hourly_data[hour_index];
        }
    } else {
        // 12-day mode: collect last 12 days of data
        for (int i = 0; i < 12; i++) {
            int day_index = (state->data_points - 12 + i) % ACTIVITY_LOGGING_NUM_DAYS;
            if (day_index < 0 || day_index >= state->data_points) {
                data[i] = 0;  // No data
            } else {
                data[i] = state->activity_log[day_index];
            }
        }
    }
    
    // Handle different histogram views
    switch (state->histogram_view) {
        case HISTOGRAM_VIEW_CHART:
            // Chart view: show histogram bars with HR/DA + "12"
            if (state->timeframe_mode == ACTIVITY_LOGGING_TIMEFRAME_12H) {
                watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "HR", "HR");
            } else {
                watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "DA", "DA");
            }
            watch_display_text(WATCH_POSITION_TOP_RIGHT, "12");
            watch_display_text(WATCH_POSITION_BOTTOM, "      ");
            _draw_histogram(state);
            break;
            
        case HISTOGRAM_VIEW_MEDIAN:
            // Median view: "MED"/"ME" + empty top right + value in bottom
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "MED", "ME");
            watch_display_text(WATCH_POSITION_TOP_RIGHT, "  ");
            snprintf(buf, 8, "%4d  ", _calculate_median(data, count));
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
            break;
            
        case HISTOGRAM_VIEW_MIN:
            // Min view: "MIN"/"MI" + hour/day in top right + value in bottom
            {
                uint8_t min_index;
                uint16_t min_value = _find_min_with_index(data, count, &min_index);
                
                watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "MIN", "MI");
                
                if (state->timeframe_mode == ACTIVITY_LOGGING_TIMEFRAME_12H) {
                    // Show hour (0-23) for minimum
                    watch_date_time_t datetime = movement_get_local_date_time();
                    int current_hour = datetime.unit.hour;
                    int min_hour = (current_hour - 11 + min_index + 24) % 24;
                    snprintf(buf, 8, "%2d", min_hour);
                } else {
                    // Show day number for minimum using proper day calculation
                    // min_index 0 = 12 days back, min_index 11 = 1 day back (yesterday)
                    uint8_t days_back = 12 - min_index;
                    snprintf(buf, 8, "%2d", _get_day_of_month_for_days_back(days_back));
                }
                watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
                
                snprintf(buf, 8, "%4d  ", min_value);
                watch_display_text(WATCH_POSITION_BOTTOM, buf);
            }
            break;
            
        case HISTOGRAM_VIEW_MAX:
            // Max view: "MAX"/"MA" + hour/day in top right + value in bottom
            {
                uint8_t max_index;
                uint16_t max_value = _find_max_with_index(data, count, &max_index);
                
                watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "MAX", "MA");
                
                if (state->timeframe_mode == ACTIVITY_LOGGING_TIMEFRAME_12H) {
                    // Show hour (0-23) for maximum
                    watch_date_time_t datetime = movement_get_local_date_time();
                    int current_hour = datetime.unit.hour;
                    int max_hour = (current_hour - 11 + max_index + 24) % 24;
                    snprintf(buf, 8, "%2d", max_hour);
                } else {
                    // Show day number for maximum using proper day calculation
                    // max_index 0 = 11 days back, max_index 11 = today
                    uint8_t days_back = 11 - max_index;
                    snprintf(buf, 8, "%2d", _get_day_of_month_for_days_back(days_back));
                }
                watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
                
                snprintf(buf, 8, "%4d  ", max_value);
                watch_display_text(WATCH_POSITION_BOTTOM, buf);
            }
            break;
    }

    // Show signal indicator if we're in 12-hour mode (tracking today)
    if (state->timeframe_mode == ACTIVITY_LOGGING_TIMEFRAME_12H) {
        watch_set_indicator(WATCH_INDICATOR_SIGNAL);
    } else {
        watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
    }
}

// Event handlers for each mode
static void _activity_logging_handle_day_events(movement_event_t event, activity_logging_state_t *state) {
    switch (event.event_type) {
        case EVENT_LIGHT_BUTTON_DOWN:
            break;
        case EVENT_LIGHT_BUTTON_UP:
            // In MODE_DAY, LIGHT button goes forward in history (towards today)
            if (state->display_index > 0) {
                state->display_index--;
                _activity_logging_handle_today_page(state);
            }
            break;
        case EVENT_LIGHT_LONG_PRESS:
            // Long LIGHT press changes to histogram mode
            state->mode = MODE_HISTOGRAM;
            state->display_index = 0;  // Reset to today's view when entering histogram
            _activity_logging_handle_histogram_page(state);
            break;
        case EVENT_ALARM_BUTTON_DOWN:
            // In MODE_DAY, ALARM button goes backwards in history
            state->display_index = (state->display_index + 1) % ACTIVITY_LOGGING_NUM_DAYS;
            _activity_logging_handle_today_page(state);
            break;
        case EVENT_ACTIVATE:
            if (watch_sleep_animation_is_running()) {
                watch_stop_sleep_animation();
            }
            _activity_logging_handle_today_page(state);
            break;
        case EVENT_TICK:
            if (movement_get_local_date_time().unit.second == 0 && state->display_index == 0) {
                _activity_logging_handle_today_page(state);
            }
            break;
        case EVENT_LOW_ENERGY_UPDATE:
            // start tick animation if necessary
            if (!watch_sleep_animation_is_running()) watch_start_sleep_animation(1000);
            _activity_logging_handle_today_page(state);
            break;
        case EVENT_TIMEOUT:
            // snap back to today on timeout
            state->display_index = 0;
            _activity_logging_handle_today_page(state);
            break;
        default:
            break;
    }
}

static void _activity_logging_handle_histogram_events(movement_event_t event, activity_logging_state_t *state) {
    switch (event.event_type) {
        case EVENT_LIGHT_BUTTON_DOWN:
            break;
        case EVENT_LIGHT_BUTTON_UP:
            // In histogram mode, LIGHT button cycles through views: chart → median → min → max → chart
            state->histogram_view = (state->histogram_view + 1) % 4;
            _activity_logging_handle_histogram_page(state);
            break;
        case EVENT_LIGHT_LONG_PRESS:
            // Long LIGHT press changes to day mode
            state->mode = MODE_DAY;
            _activity_logging_handle_today_page(state);
            break;
        case EVENT_ALARM_BUTTON_DOWN:
            // In histogram mode, ALARM button cycles through timeframes
            if (state->timeframe_mode == ACTIVITY_LOGGING_TIMEFRAME_12H) {
                state->timeframe_mode = ACTIVITY_LOGGING_TIMEFRAME_12D;
            } else {
                state->timeframe_mode = ACTIVITY_LOGGING_TIMEFRAME_12H;
            }
            _activity_logging_handle_histogram_page(state);
            break;
        case EVENT_ACTIVATE:
            if (watch_sleep_animation_is_running()) {
                watch_stop_sleep_animation();
            }
            _activity_logging_handle_histogram_page(state);
            break;
        case EVENT_TICK:
            if (movement_get_local_date_time().unit.second == 0 && state->display_index == 0) {
                _activity_logging_handle_histogram_page(state);
            }
            break;
        case EVENT_LOW_ENERGY_UPDATE:
            // start tick animation if necessary
            if (!watch_sleep_animation_is_running()) watch_start_sleep_animation(1000);
            _activity_logging_handle_histogram_page(state);
            break;
        case EVENT_TIMEOUT:
            // snap back to today on timeout
            state->display_index = 0;
            _activity_logging_handle_histogram_page(state);
            break;
        default:
            break;
    }
}

void activity_logging_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(activity_logging_state_t));
        memset(*context_ptr, 0, sizeof(activity_logging_state_t));

        activity_logging_state_t *state = (activity_logging_state_t *)*context_ptr;

        // At first run, tell Movement to run the accelerometer in the background. It will now run at this rate forever.
        movement_set_accelerometer_background_rate(LIS2DW_DATA_RATE_LOWEST);
    }
}

void activity_logging_face_activate(void *context) {
    activity_logging_state_t *state = (activity_logging_state_t *)context;
    state->display_index = 0;
    state->show_emoticon = true;
    state->emoticon_tick_count = 0;
}


bool activity_logging_face_loop(movement_event_t event, void *context) {
    activity_logging_state_t *state = (activity_logging_state_t *)context;
    switch (event.event_type) {
        case EVENT_LIGHT_LONG_PRESS:
            movement_illuminate_led();
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            state->display_index = (state->display_index + ACTIVITY_LOGGING_NUM_DAYS - 1) % ACTIVITY_LOGGING_NUM_DAYS;
            _activity_logging_face_update_display(state);
            break;
        case EVENT_ALARM_BUTTON_DOWN:
            state->display_index = (state->display_index + 1) % ACTIVITY_LOGGING_NUM_DAYS;
            // fall through
        case EVENT_ACTIVATE:
            if (watch_sleep_animation_is_running()) {
                watch_stop_sleep_animation();
            }
            _activity_logging_face_update_display(state);
            break;
        case EVENT_TICK:
            {
                if (movement_get_local_date_time().unit.second == 0 && state->display_index == 0) {
                    _activity_logging_face_update_display(state);
                }

                // Handle emoticon display timing using tick counter
                if (state->show_emoticon) {
                    state->emoticon_tick_count++;
                    if (state->emoticon_tick_count > 1) {
                        state->show_emoticon = false;
                        watch_clear_colon();
                        _activity_logging_face_update_display(state);
                    }
                }
            }
            break;
        case EVENT_BACKGROUND_TASK:
            {
                size_t pos = state->data_points % ACTIVITY_LOGGING_NUM_DAYS;
                state->activity_log[pos] = state->active_minutes_today;
                state->data_points++;
                state->active_minutes_today = 0;
            }
            break;
        default:
            movement_default_loop_handler(event);
            break;
    }

    return true;
}

void activity_logging_face_resign(void *context) {
    (void) context;
}

movement_watch_face_advisory_t activity_logging_face_advise(void *context) {
    activity_logging_state_t *state = (activity_logging_state_t *)context;
    movement_watch_face_advisory_t retval = { 0 };

    watch_date_time_t datetime = movement_get_local_date_time();
    
    // Reset current hour's data at the beginning of each hour (minute 0)
    if (datetime.unit.minute == 0) {
        state->hourly_data[datetime.unit.hour] = 0;
    }

    if (!HAL_GPIO_A4_read()) {
        // only count this as an active minute if the previous minute was also active.
        // otherwise, set the flag and we'll count the next minute if the wearer is still active.
        if (state->previous_minute_was_active) {
            state->active_minutes_today++;
            state->hourly_data[datetime.unit.hour]++;

        } else {
            state->previous_minute_was_active = true;
        }
    } else {
        state->previous_minute_was_active = false;
    }

    // request a background task at midnight to shuffle the data into the log
    if (datetime.unit.hour == 0 && datetime.unit.minute == 0) {
        retval.wants_background_task = true;
    }

    return retval;
}
