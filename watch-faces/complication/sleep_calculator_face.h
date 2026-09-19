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

#ifndef SLEEP_CALCULATOR_FACE_H_
#define SLEEP_CALCULATOR_FACE_H_

/*
 * SLEEP CALCULATOR face
 *
 * A Sensor Watch face that calculates optimal wake times based on current time
 * (when you plan to sleep) using 90-minute sleep cycles for better sleep quality.
 *
 * FEATURES:
 *
 * Four Sleep Duration Options:
 *   - 4.5 hours (3 sleep cycles)
 *   - 6 hours (4 sleep cycles) 
 *   - 7.5 hours (5 sleep cycles)
 *   - 9 hours (6 sleep cycles)
 *
 * Smart Calculations:
 *   - Includes 15 minutes to fall asleep
 *   - Based on 90-minute REM sleep cycles
 *   - Updates automatically with current time
 *
 * HOW TO USE:
 *
 * Controls:
 *   º ALARM Button: Cycle through sleep duration options (4H → 6H → 7H → 9H)
 *   º ALARM Long Press: Set/unset alarm at calculated wake time
 *   º LIGHT Button: Illuminate LED (normal behavior)
 *
 * Display:
 *   º Top Left: "WAK" (Wake time indicator)
 *   º Top Right: Sleep duration (4H, 6H, 7H, or 9H)
 *   º Bottom: Calculated wake time (HH:MM format)
 *   º Colon: Always displayed
 *   º Bell Icon: Shows when alarm is set for calculated wake time
 *   º PM Indicator: Shows when in 12-hour mode and time is PM
 *
 * EXAMPLE USAGE:
 *
 *   - Current time: 10:30 PM (when you're going to sleep)
 *   - Selected duration: 7H (7.5 hours)
 *   - Display shows: 06:15 AM (optimal wake time)
 *
 * SLEEP SCIENCE:
 *
 * The calculator is based on sleep research showing that:
 *   1. Sleep Cycles: Sleep occurs in ~90-minute cycles
 *   2. REM Sleep: Waking at the end of a cycle feels more natural
 *   3. Fall Asleep Time: Most people take ~15 minutes to fall asleep
 *   4. Optimal Durations: 4.5, 6, 7.5, or 9 hours align with complete sleep cycles
 *
 * TECHNICAL DETAILS:
 *
 *   - Memory Usage: Minimal state storage (current option, reference time)
 *   - Update Frequency: Updates every minute to stay current
 *   - Power Usage: Standard watch face power consumption
 *
 * TIPS:
 *
 *   - Use when planning your bedtime to find optimal wake times
 *   - The 7.5-hour option (5 cycles) is often optimal for most adults
 *   - Consider your personal sleep needs - some people need more or fewer cycles
 */

#include "movement.h"

typedef struct {
    uint8_t current_option;          // 0-3 for different sleep duration options
    uint8_t reference_hour;          // Reference time (current time or set time)
    uint8_t reference_minute;
    uint8_t alarm_is_set;            // 1 if alarm is set, 0 if not
    uint8_t alarm_hour;              // Calculated alarm hour
    uint8_t alarm_minute;            // Calculated alarm minute
} sleep_calculator_face_state_t;

void sleep_calculator_face_setup(uint8_t watch_face_index, void **context_ptr);
void sleep_calculator_face_activate(void *context);
bool sleep_calculator_face_loop(movement_event_t event, void *context);
void sleep_calculator_face_resign(void *context);
movement_watch_face_advisory_t sleep_calculator_face_advise(void *context);

#define sleep_calculator_face ((const watch_face_t){ \
    sleep_calculator_face_setup, \
    sleep_calculator_face_activate, \
    sleep_calculator_face_loop, \
    sleep_calculator_face_resign, \
    sleep_calculator_face_advise \
})

#endif // SLEEP_CALCULATOR_FACE_H_