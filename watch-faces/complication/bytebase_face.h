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

#ifndef BYTEBASE_FACE_H_
#define BYTEBASE_FACE_H_

/*
 * BYTEBASE face
 *
 * A face for displaying numbers in different bases: decimal, hexadecimal,
 * binary, and ASCII. Use the LIGHT button to cycle through display modes
 * (DEC, HEX, BIN, ASCII). Hold ALARM to enter edit mode, then use LIGHT
 * to advance to the next digit and ALARM to increment the current digit.
 */

#include "movement.h"

typedef enum {
    BYTEBASE_MODE_BINARY = 0,
    BYTEBASE_MODE_HEXADECIMAL,
    BYTEBASE_MODE_DECIMAL,
    BYTEBASE_MODE_ASCII,
    BYTEBASE_NUM_MODES
} bytebase_mode_t;

typedef struct {
    uint8_t current_mode;
    uint8_t current_digit;
    uint8_t value;
    bool editing;
    bool quick_ticks_running;
    bool binary_show_high_nibble;  // For binary mode: true = HI nibble, false = LO nibble
} bytebase_state_t;

void bytebase_face_setup(uint8_t watch_face_index, void ** context_ptr);
void bytebase_face_activate(void *context);
bool bytebase_face_loop(movement_event_t event, void *context);
void bytebase_face_resign(void *context);

#define bytebase_face ((const watch_face_t){ \
    bytebase_face_setup, \
    bytebase_face_activate, \
    bytebase_face_loop, \
    bytebase_face_resign, \
    NULL, \
})

#endif // BYTEBASE_FACE_H_