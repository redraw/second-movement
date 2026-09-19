/*
 * MIT License
 *
 * Copyright (c) 2025 Joey Castillo
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
#include "bytebase_face.h"
#include "watch.h"

static const char* mode_names[] = {"BIn", "HEX", "dEC", "ASCII"};
static const char* mode_names_fallback[] = {"BI", "HE", "dE", "AS"};

static void _abort_quick_ticks(bytebase_state_t *state) {
    if (state->quick_ticks_running) {
        state->quick_ticks_running = false;
        movement_request_tick_frequency(4);
    }
}

static void _handle_digit_increment(bytebase_state_t *state) {
    switch (state->current_mode) {
        case BYTEBASE_MODE_DECIMAL:
            if (state->current_digit == 0) {
                // Edit ones digit (least significant)
                uint8_t ones = state->value % 10;
                ones = (ones + 1) % 10;
                state->value = (state->value / 10) * 10 + ones;
                if (state->value > 255) state->value = (state->value / 10) * 10;
            } else if (state->current_digit == 1) {
                // Edit tens digit
                uint8_t tens = (state->value / 10) % 10;
                tens = (tens + 1) % 10;
                state->value = (state->value % 10) + (state->value / 100) * 100 + (tens * 10);
                if (state->value > 255) state->value = (state->value / 100) * 100 + (state->value % 10);
            } else if (state->current_digit == 2) {
                // Edit hundreds digit (most significant)
                uint8_t hundreds = (state->value / 100) % 10;
                hundreds = (hundreds + 1) % 10;
                state->value = (state->value % 100) + (hundreds * 100);
                if (state->value > 255) state->value = state->value % 100;
            }
            break;
        case BYTEBASE_MODE_HEXADECIMAL:
            if (state->current_digit == 0) {
                // Edit low nibble (least significant)
                uint8_t nibble = state->value & 0xF;
                nibble = (nibble + 1) & 0xF;
                state->value = (state->value & 0xF0) | nibble;
            } else if (state->current_digit == 1) {
                // Edit high nibble (most significant)
                uint8_t nibble = (state->value >> 4) & 0xF;
                nibble = (nibble + 1) & 0xF;
                state->value = (state->value & 0x0F) | (nibble << 4);
            }
            break;
        case BYTEBASE_MODE_BINARY:
            if (state->current_digit < 8) {
                // Toggle bit from LSB to MSB (0-7)
                state->value ^= (1 << state->current_digit);
                // Ensure value stays within 8-bit range
                state->value &= 0xFF;
            }
            break;
        case BYTEBASE_MODE_ASCII:
            // Increment ASCII value directly
            state->value = (state->value + 1) % 128;
            break;
    }
}

static void _display_value(bytebase_state_t *state, uint8_t subsecond) {
    char buf[16];
    
    // Display mode name or ASCII character in top position
    if (state->current_mode == BYTEBASE_MODE_ASCII) {
        // Clear other positions first to avoid interference
        watch_display_text(WATCH_POSITION_TOP_RIGHT, "  ");
        watch_display_text(WATCH_POSITION_HOURS, "  ");
        watch_display_text(WATCH_POSITION_MINUTES, "  ");
        watch_display_text(WATCH_POSITION_SECONDS, "  ");
        
        // Display ASCII character in top center position
        if (state->value >= 32 && state->value <= 126) {
            buf[0] = (char)state->value;
            buf[1] = '\0';
        } else if (state->value == 0) {
            strcpy(buf, "NUL");
        } else if (state->value < 32) {
            sprintf(buf, "^%c", (char)(state->value + 64));
        } else if (state->value == 127) {
            strcpy(buf, "DEL");
        } else {
            strcpy(buf, "?");
        }
        
        // Handle blinking in ASCII mode - hide character during blink
        if (state->editing && (subsecond % 2 == 0)) {
            watch_display_text(WATCH_POSITION_TOP, "   ");
        } else {
            watch_display_text(WATCH_POSITION_TOP, buf);
        }
    } else {
        // For all other modes, show mode name
        watch_display_text_with_fallback(WATCH_POSITION_TOP, 
            (char*)mode_names[state->current_mode], 
            (char*)mode_names_fallback[state->current_mode]);
    }
    
    // Display value based on current mode
    switch (state->current_mode) {
        case BYTEBASE_MODE_DECIMAL:
            sprintf(buf, " %03d", state->value & 0xFF);
            // Handle blinking during editing
            if (state->editing && (subsecond % 2 == 0)) {
                switch(state->current_digit) {
                    case 0: // ones digit
                        buf[3] = ' ';
                        break;
                    case 1: // tens digit  
                        buf[2] = ' ';
                        break;
                    case 2: // hundreds digit
                        buf[1] = ' ';
                        break;
                }
            }
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
            break;
            
        case BYTEBASE_MODE_HEXADECIMAL:
            sprintf(buf, "%02x", state->value & 0xFF);
            // Handle blinking during editing
            if (state->editing && (subsecond % 2 == 0)) {
                switch(state->current_digit) {
                    case 0: // low nibble (second hex digit)
                        buf[1] = ' ';
                        break;
                    case 1: // high nibble (first hex digit)
                        buf[0] = ' ';
                        break;
                }
            }
            watch_display_text(WATCH_POSITION_MINUTES, buf);
            break;
            
        case BYTEBASE_MODE_BINARY: {
            // Show current nibble across HH:MM positions
            char hours_buf[3], minutes_buf[3];
            
            if (state->binary_show_high_nibble) {
                // Show high nibble (bits 7-4)
                uint8_t high_nibble = (state->value >> 4) & 0x0F;
                sprintf(hours_buf, "%c%c", 
                    (high_nibble & 0x08) ? '1' : '0',
                    (high_nibble & 0x04) ? '1' : '0');
                sprintf(minutes_buf, "%c%c",
                    (high_nibble & 0x02) ? '1' : '0',
                    (high_nibble & 0x01) ? '1' : '0');
                    
                // Handle blinking during editing
                if (state->editing && (subsecond % 2 == 0)) {
                    if (state->current_digit == 7) hours_buf[0] = ' ';      // bit 7
                    if (state->current_digit == 6) hours_buf[1] = ' ';      // bit 6  
                    if (state->current_digit == 5) minutes_buf[0] = ' ';    // bit 5
                    if (state->current_digit == 4) minutes_buf[1] = ' ';    // bit 4
                }
                
                watch_display_text(WATCH_POSITION_TOP_RIGHT, " H");
            } else {
                // Show low nibble (bits 3-0)
                uint8_t low_nibble = state->value & 0x0F;
                sprintf(hours_buf, "%c%c",
                    (low_nibble & 0x08) ? '1' : '0',
                    (low_nibble & 0x04) ? '1' : '0');
                sprintf(minutes_buf, "%c%c",
                    (low_nibble & 0x02) ? '1' : '0',
                    (low_nibble & 0x01) ? '1' : '0');
                    
                // Handle blinking during editing
                if (state->editing && (subsecond % 2 == 0)) {
                    if (state->current_digit == 3) hours_buf[0] = ' ';      // bit 3
                    if (state->current_digit == 2) hours_buf[1] = ' ';      // bit 2
                    if (state->current_digit == 1) minutes_buf[0] = ' ';    // bit 1  
                    if (state->current_digit == 0) minutes_buf[1] = ' ';    // bit 0
                }
                
                watch_display_text(WATCH_POSITION_TOP_RIGHT, " L");
            }
            
            watch_display_text(WATCH_POSITION_HOURS, hours_buf);
            watch_display_text(WATCH_POSITION_MINUTES, minutes_buf);
            watch_display_text(WATCH_POSITION_SECONDS, "  ");
            break;
        }
        case BYTEBASE_MODE_ASCII:
            // Display "ASCII" in bottom position
            strcpy(buf, "ASCII ");
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
            break;
    }
}

void bytebase_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(bytebase_state_t));
        bytebase_state_t *state = (bytebase_state_t *)*context_ptr;
        state->current_mode = BYTEBASE_MODE_BINARY;
        state->current_digit = 0;
        state->value = 42;
        state->editing = false;
        state->quick_ticks_running = false;
        state->binary_show_high_nibble = false;  // Start with low nibble
    }
}

void bytebase_face_activate(void *context) {
    bytebase_state_t *state = (bytebase_state_t *)context;
    state->editing = false;
    state->current_digit = 0;
    _abort_quick_ticks(state);
    movement_request_tick_frequency(4);
}

bool bytebase_face_loop(movement_event_t event, void *context) {
    bytebase_state_t *state = (bytebase_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            _display_value(state, event.subsecond);
            break;
        case EVENT_TICK:
            if (state->quick_ticks_running) {
                if (HAL_GPIO_BTN_ALARM_read()) {
                    _handle_digit_increment(state);
                } else {
                    _abort_quick_ticks(state);
                }
            }
            _display_value(state, event.subsecond);
            break;
        case EVENT_MODE_BUTTON_UP:
            _abort_quick_ticks(state);
            movement_move_to_next_face();
            return false;
        case EVENT_LIGHT_BUTTON_DOWN:
            if (state->editing) {
                if (state->current_mode == BYTEBASE_MODE_ASCII) break;
                // LIGHT advances to next digit
                uint8_t max_digits = 3;
                if (state->current_mode == BYTEBASE_MODE_HEXADECIMAL) max_digits = 2;
                else if (state->current_mode == BYTEBASE_MODE_BINARY) max_digits = 8;
                
                state->current_digit = (state->current_digit + 1) % max_digits;
                
                // For binary mode, switch nibble display when crossing nibble boundary
                if (state->current_mode == BYTEBASE_MODE_BINARY) {
                    if (state->current_digit == 4) {
                        // Moving from bit 3 to bit 4, switch from LO to HI nibble
                        state->binary_show_high_nibble = true;
                    } else if (state->current_digit == 0) {
                        // Wrapping back to bit 0, switch to LO nibble
                        state->binary_show_high_nibble = false;
                    }
                }
            } else {
                // Switching modes - clear display first
                watch_clear_display();
                state->current_mode = (state->current_mode + 1) % BYTEBASE_NUM_MODES;
                state->current_digit = 0;
            }
            break;
        case EVENT_ALARM_LONG_PRESS:
            if (state->current_mode == BYTEBASE_MODE_ASCII) break;
            if (!state->editing) {
                // Enter edit mode
                state->editing = true;
                state->current_digit = 0;
                state->binary_show_high_nibble = false;
                movement_request_tick_frequency(4);
            } else {
                // Exit edit mode
                state->editing = false;
                state->current_digit = 0;
                movement_request_tick_frequency(1);
                _abort_quick_ticks(state);
            }
            break;
        case EVENT_ALARM_LONG_UP:
            if (state->editing) {
                _abort_quick_ticks(state);
            }
            break;
        case EVENT_ALARM_BUTTON_UP:
            if (state->current_mode == BYTEBASE_MODE_ASCII) break;
            if (state->editing) {
                _handle_digit_increment(state);
            } else if (state->current_mode == BYTEBASE_MODE_BINARY) {
                // In binary mode (not editing), short alarm press switches HI/LO nibble
                state->binary_show_high_nibble = !state->binary_show_high_nibble;
            }
            break;
        case EVENT_TIMEOUT:
            _abort_quick_ticks(state);
            state->editing = false;
            movement_move_to_face(0);
            break;
        default:
            return movement_default_loop_handler(event);
    }

    _display_value(state, event.subsecond);
    return true;
}

void bytebase_face_resign(void *context) {
    bytebase_state_t *state = (bytebase_state_t *)context;
    _abort_quick_ticks(state);
    state->editing = false;
}