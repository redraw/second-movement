/* MIT License, Copyright (c) 2026 */
#pragma once

#include "movement.h"

/*
 * Unit converter
 * --------------
 * Opens with a menu for resistors, capacitors, RC time, frequency / period,
 * antenna length, and temperature.
 * LIGHT advances the menu; ALARM or held LIGHT opens its selection.
 * Hold LIGHT in any tool to return to the menu. MODE advances to the next face.
 * LIGHT navigation does not turn on the backlight.
 *
 * Resistors use four color bands (two digits, multiplier, tolerance), and
 * capacitors use three printed digits (two digits and multiplier, in pF).
 *
 * For resistors, LIGHT cycles through bands r1-r4 and a result page (r=).
 * On a band page the lower line spells out the selected color. The upper
 * right digit gives its standard code: 0-9 for black through white, -1 for
 * gold and -2 for silver. On a resistor's tolerance band it gives 1, 2, 5
 * or 10 percent. ALARM advances the selected color.
 *
 * Hold ALARM to switch to value-to-code mode, marked by the signal icon for
 * resistors and the bell icon for capacitors.
 * For resistors, I1, I2, I3 and I4 edit the first significant digit, second
 * significant digit, power-of-ten multiplier and tolerance. LIGHT advances
 * between fields; ALARM increments the selected numeric field. The lower
 * line shows the resulting resistance throughout entry. Then LIGHT visits
 * b1-b4, each showing one resulting color. A four-band resistor can encode
 * only two significant digits, so values outside that precision are not
 * directly representable.
 *
 * For capacitors, LIGHT cycles through the three printed code digits C1-C3
 * and a result page (C=). ALARM advances the selected digit. In code-to-
 * value mode the lower line shows the three-digit code while editing and
 * the capacitance on C=. In value-to-code mode it shows the capacitance
 * while editing and the three-digit code on C=. For example, 104 is 100 nF.
 *
 * Numeric values occupy the four HH:MM digits; the two seconds digits show
 * the unit. The colon stands for a decimal comma: 4:70 with k is 4,7 kohms.
 * It is off for whole values. The resistor result page shows tolerance at
 * the upper right. Seconds stay blank below 1 kohm, show k for kilohms,
 * nn (a seven-segment M) for megaohms, and E9 for gigaohms.
 * Capacitor units are pF, nF, uF, mF and F.
 *
 * For temperature and antenna length, LIGHT advances through editable digits
 * and result pages; ALARM increments the selected digit. Temperature also has
 * an editable sign field; hold ALARM to select the input unit, C or F.
 *
 * The antenna tool estimates a center-fed half-wave wire dipole from its
 * frequency: total length = 143 / MHz metres, with two equal arms. LIGHT
 * visits four frequency digits, a multiplier field (Hz, kHz or MHz), total
 * length, and length per arm. ALARM changes the selected digit or multiplier.
 * Hold ALARM to switch between integer input and hundredths (colon shown).
 * The default is 7.10 MHz; zero frequency has no result. Lengths are shown
 * in metres, to the nearest centimetre when they fit on the display.
 *
 * In the frequency / period tool, LIGHT visits four input digits, a
 * multiplier field, and a result page. ALARM changes the selected digit or
 * multiplier; hold ALARM to switch between frequency and period input.
 * Frequency uses Hz through GHz; period uses ps through s. The two-character
 * input and result labels use H for Hz when a prefix is present (kH means
 * kHz). Both use four numeric digits and two unit digits. The result chooses
 * a suitable unit. Zero has no reciprocal.
 *
 * The RC tool uses the values currently selected in the resistor and
 * capacitor tools. LIGHT cycles through Fc (cutoff frequency 1 / (2*pi*R*C)),
 * 1t (the time constant R * C), and 5t (approximately complete charge or
 * discharge). Times use picoseconds through years; cutoff frequencies use
 * picohertz through terahertz. ALARM has no action here.
 *
 * For example, yellow-violet-red-gold gives 4:70 k with 5% tolerance for a
 * resistor. The face remains displayed through inactivity and low-energy mode.
 */
typedef struct {
    uint8_t direction;       // 0 code -> value, 1 value -> code
    uint8_t selected_band;
    uint8_t colors[4];
    uint8_t value[4];        // numeric-entry digits, multiplier, tolerance
} unit_converter_resistor_t;

typedef struct {
    uint8_t direction;
    uint8_t selected_band;
    uint8_t digits[4];       // three printed digits; fourth byte is unused
} unit_converter_capacitor_t;

typedef struct {
    uint8_t digits[4];
    uint8_t field;           // 0 sign, 1-4 digits, 5 result
    bool negative;
    bool fahrenheit;
} unit_converter_temperature_t;

typedef struct {
    uint8_t digits[4];
    uint8_t field;           // 0-3 digits, 4 multiplier, 5 total length, 6 each arm
    bool hundredths;
    uint8_t prefix;          // 0 Hz, 1 kHz, 2 MHz
} unit_converter_dipole_t;

typedef struct {
    uint8_t digits[4];
    uint8_t field;           // 0-3 digits, 4 multiplier, 5 result
    bool is_period;
    uint8_t prefix[2];       // 0 frequency, 1 period
} unit_converter_frequency_t;

typedef struct {
    uint8_t page;            // 0 cutoff frequency, 1 one tau, 2 five tau
} unit_converter_rc_t;

typedef enum {
    UNIT_CONVERTER_TOOL_RESISTOR,
    UNIT_CONVERTER_TOOL_CAPACITOR,
    UNIT_CONVERTER_TOOL_TEMPERATURE,
    UNIT_CONVERTER_TOOL_DIPOLE,
    UNIT_CONVERTER_TOOL_FREQUENCY,
    UNIT_CONVERTER_TOOL_RC,
    UNIT_CONVERTER_TOOL_COUNT,
} unit_converter_tool_t;

typedef struct {
    unit_converter_tool_t tool;
    uint8_t menu_index;      // menu order: resistor, capacitor, RC, frequency, antenna, temperature
    bool in_menu;
    unit_converter_resistor_t resistor;
    unit_converter_capacitor_t capacitor;
    unit_converter_temperature_t temperature;
    unit_converter_dipole_t dipole;
    unit_converter_frequency_t frequency;
    unit_converter_rc_t rc;
} unit_converter_state_t;

void unit_converter_face_setup(uint8_t watch_face_index, void **context_ptr);
void unit_converter_face_activate(void *context);
bool unit_converter_face_loop(movement_event_t event, void *context);
void unit_converter_face_resign(void *context);

#define unit_converter_face ((const watch_face_t){ \
    unit_converter_face_setup, \
    unit_converter_face_activate, \
    unit_converter_face_loop, \
    unit_converter_face_resign, \
    NULL, \
})
