/* MIT License, Copyright (c) 2026 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unit_converter_face.h"
#include "watch.h"

enum { BLACK, BROWN, RED, ORANGE, YELLOW, GREEN, BLUE, VIOLET, GREY, WHITE, GOLD, SILVER };

// The upper right digits of the classic LCD cannot render arbitrary letters.
// Spell the selected color on the six lower digits instead.
static const char *const color_names[] = {
    "BLACK", "BROWN", "RED", "ORANGE", "YELLOW", "GREEN",
    "BLUE", "VIOLET", "GREY", "WHITE", "GOLD", "SILVER"
};
static const int8_t multiplier_exponents[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -2
};
static const uint8_t tolerance_colors[] = { BROWN, RED, GOLD, SILVER };

static void tolerance_code(uint8_t color, char code[3]) {
    code[0] = color == SILVER ? '1' : ' ';
    code[1] = color == BROWN ? '1' : color == RED ? '2' :
              color == SILVER ? '0' : '5';
}

static uint8_t next_color(uint8_t current, uint8_t band, bool capacitor) {
    if (band == 0) return current == WHITE ? BROWN : current + 1;
    if (band == 1) return current == WHITE ? BLACK : current + 1;
    if (band == 2) return current == (capacitor ? WHITE : SILVER) ? BLACK : current + 1;
    for (uint8_t i = 0; i < 4; i++) {
        if (tolerance_colors[i] == current) return tolerance_colors[(i + 1) % 4];
    }
    return GOLD;
}

static uint64_t power_of_ten(uint8_t exponent) {
    uint64_t result = 1;
    while (exponent--) result *= 10;
    return result;
}

static const double frequency_factors[] = {
    1, 1e3, 1e6, 1e9
};
static const char *const frequency_input_units[] = {
    "Hz", "kH", "MH", "GH"
};
static const char *const frequency_output_units[] = {
    " Hz", "kHz", "MHz", "GHz"
};
static const double time_factors[] = {
    1e-12, 1e-9, 1e-6, 1e-3, 1, 60, 3600, 86400, 31557600
};
static const char *const time_units[] = {
    "pS", "nS", "uS", "mS", " S", "mn", "hr", "dy", "yr"
};

// Result in hundredths of an ohm or pF, so gold and silver multipliers
// remain exact without floating point arithmetic.
static uint64_t scaled_value(const uint8_t *colors) {
    int8_t exponent = multiplier_exponents[colors[2]];
    uint64_t significant = (uint64_t)colors[0] * 10 + colors[1];
    return significant * power_of_ten((uint8_t)(exponent + 2));
}

static void display_bottom_text(const char *text) {
    char bottom[7] = "      ";
    size_t length = strlen(text);
    memcpy(bottom, text, length < 6 ? length : 6);
    watch_clear_colon();
    watch_display_text(WATCH_POSITION_BOTTOM, bottom);
}

static void display_value(const uint8_t *fields, bool capacitor) {
    static const uint64_t scales[] = {
        100ULL, 100000ULL, 100000000ULL, 100000000000ULL, 100000000000000ULL
    };
    static const char *const capacitor_units[] = { "pF", "nF", "uF", "mF", " F" };
    // Two adjacent n glyphs suggest M on the seven-segment seconds digits.
    static const char *const resistor_units[] = { "  ", " k", "nn", "E9" };
    const char *const *units = capacitor ? capacitor_units : resistor_units;
    uint64_t value = scaled_value(fields);
    uint8_t group = 0;
    uint8_t max_group = capacitor ? 4 : 3;
    bool fractional = false;
    char number[5];

    // The number occupies HH:MM; the unit occupies the seconds. The colon
    // stands for a decimal comma, and is off for whole values.
    for (int8_t candidate = max_group; candidate >= 0; candidate--) {
        if (value >= scales[candidate]) {
            group = (uint8_t)candidate;
            break;
        }
    }
    uint32_t display_hundredths = (uint32_t)(value * 100 / scales[group]);
    if (display_hundredths % 100 != 0) {
        fractional = true;
        uint8_t whole = display_hundredths / 100;
        uint8_t fraction = display_hundredths % 100;
        number[0] = ' ';
        number[1] = (char)('0' + whole);
        number[2] = (char)('0' + fraction / 10);
        number[3] = (char)('0' + fraction % 10);
    } else {
        uint16_t whole = display_hundredths / 100;
        number[0] = whole >= 1000 ? (char)('0' + whole / 1000) : ' ';
        number[1] = whole >= 100 ? (char)('0' + (whole / 100) % 10) : ' ';
        number[2] = whole >= 10 ? (char)('0' + (whole / 10) % 10) : ' ';
        number[3] = (char)('0' + whole % 10);
    }
    number[4] = '\0';
    watch_display_text(WATCH_POSITION_HOURS, number);
    watch_display_text(WATCH_POSITION_MINUTES, number + 2);
    watch_display_text(WATCH_POSITION_SECONDS, units[group]);
    if (fractional) watch_set_colon();
    else watch_clear_colon();
}

static void band_code(uint8_t color, uint8_t band, char code[3]) {
    if (band == 3) {
        tolerance_code(color, code);
    } else if (band == 2 && color >= GOLD) {
        code[0] = '-';
        code[1] = color == GOLD ? '1' : '2';
    } else {
        code[1] = (char)('0' + color);
    }
}

static void render_component(const unit_converter_state_t *state) {
    char label[3];
    char value[8];
    char code[3] = "  ";
    const uint8_t *numeric_fields = NULL;
    bool result_page = state->selected_band == (state->component ? 3 : 4);
    uint8_t color = 0;
    label[0] = state->component ? 'C' : 'r';
    label[1] = result_page ? '=' : (char)('1' + state->selected_band);
    label[2] = '\0';

    if (state->component) {
        if (!result_page) color = state->selections[1][state->selected_band];
        if (result_page == (bool)state->direction) {
            value[0] = (char)('0' + state->selections[1][0]);
            value[1] = (char)('0' + state->selections[1][1]);
            value[2] = (char)('0' + state->selections[1][2]);
            value[3] = '\0';
        } else {
            numeric_fields = state->selections[1];
        }
        if (!result_page && state->direction) {
            code[1] = (char)('0' + color);
        }
    } else if (state->direction) {
        if (state->selected_band < 4) {
            label[0] = 'I';
            label[1] = (char)('1' + state->selected_band);
            numeric_fields = state->resistor_value;
            band_code(state->resistor_value[state->selected_band], state->selected_band, code);
        } else {
            uint8_t band = state->selected_band - 4;
            label[0] = 'b';
            label[1] = (char)('1' + band);
            color = state->resistor_value[band];
            snprintf(value, sizeof(value), "%s", color_names[color]);
            band_code(color, band, code);
        }
    } else if (result_page) {
        numeric_fields = state->selections[0];
        tolerance_code(state->selections[0][3], code);
    } else {
        color = state->selections[0][state->selected_band];
        snprintf(value, sizeof(value), "%s", color_names[color]);
        band_code(color, state->selected_band, code);
    }
    watch_display_text(WATCH_POSITION_TOP_LEFT, label);
    watch_display_text(WATCH_POSITION_TOP_RIGHT, code);
    if (numeric_fields) display_value(numeric_fields, state->component);
    else display_bottom_text(value);
    if (state->direction) watch_set_indicator(WATCH_INDICATOR_BELL);
    else watch_clear_indicator(WATCH_INDICATOR_BELL);
}

static uint16_t entered_number(const uint8_t digits[4]) {
    return (uint16_t)(digits[0] * 1000 + digits[1] * 100 +
                      digits[2] * 10 + digits[3]);
}

// Four numeric cells and two unit cells fit on both classic and custom LCDs.
// The colon separates the whole and fractional parts when one decimal fits.
static void display_ratio(int64_t numerator, uint32_t denominator, const char *unit) {
    char number[5];
    uint64_t magnitude = numerator < 0 ? (uint64_t)-numerator : (uint64_t)numerator;
    uint64_t rounded_tenths = (magnitude * 10 + denominator / 2) / denominator;
    bool negative = numerator < 0 && rounded_tenths != 0;
    bool decimal = rounded_tenths < 1000 && (!negative || rounded_tenths < 100);

    if (decimal) {
        uint64_t whole = rounded_tenths / 10;
        uint8_t tenth = rounded_tenths % 10;
        number[0] = negative ? '-' : whole >= 10 ? (char)('0' + whole / 10) : ' ';
        number[1] = (char)('0' + whole % 10);
        number[2] = (char)('0' + tenth);
        number[3] = ' ';
        number[4] = '\0';
        watch_set_colon();
    } else {
        uint64_t rounded = (magnitude + denominator / 2) / denominator;
        if (rounded > (negative ? 999 : 9999)) {
            display_bottom_text("Ovrflo");
            return;
        }
        snprintf(number, sizeof(number), "%4lld", negative ? -(long long)rounded : (long long)rounded);
        watch_clear_colon();
    }
    watch_display_text(WATCH_POSITION_HOURS, number);
    watch_display_text(WATCH_POSITION_MINUTES, number + 2);
    watch_display_text(WATCH_POSITION_SECONDS, unit);
}

static void render_numeric_input(const uint8_t digits[4], const char *unit) {
    char number[5];
    for (uint8_t i = 0; i < 4; i++) number[i] = (char)('0' + digits[i]);
    number[4] = '\0';
    watch_display_text(WATCH_POSITION_HOURS, number);
    watch_display_text(WATCH_POSITION_MINUTES, number + 2);
    watch_display_text(WATCH_POSITION_SECONDS, unit);
}

// A practical starting length for a center-fed half-wave wire dipole is
// 143 / f(MHz) metres (approximately the ARRL's 468 / f feet).
static void display_dipole_length(uint32_t frequency, uint8_t prefix, bool hundredths, bool one_arm) {
    uint64_t numerator_cm = 14300000000ULL * (hundredths ? 100 : 1) /
                            power_of_ten(prefix * 3);
    if (one_arm) numerator_cm /= 2;
    uint64_t centimetres = (numerator_cm + frequency / 2) / frequency;
    char number[5];
    if (centimetres < 10000) {
        number[0] = centimetres >= 1000 ? (char)('0' + centimetres / 1000) : ' ';
        number[1] = (char)('0' + centimetres / 100 % 10);
        number[2] = (char)('0' + centimetres / 10 % 10);
        number[3] = (char)('0' + centimetres % 10);
        number[4] = '\0';
        watch_set_colon();
    } else {
        uint64_t metres = (centimetres + 50) / 100;
        if (metres > 9999) {
            display_bottom_text("Ovrflo");
            return;
        }
        snprintf(number, sizeof(number), "%4lu", (unsigned long)metres);
        watch_clear_colon();
    }
    watch_display_text(WATCH_POSITION_HOURS, number);
    watch_display_text(WATCH_POSITION_MINUTES, number + 2);
    watch_display_text(WATCH_POSITION_SECONDS, " m");
}

static void display_frequency_hz(double hz) {
    if (hz == 0) {
        display_bottom_text("----  ");
        return;
    }
    for (int8_t i = 3; i >= 0; i--) {
        double value = hz / frequency_factors[i];
        if (value < 0.9995 && i > 0) continue;
        if (value < 0.05) break;
        if (value >= 999.5) {
            display_bottom_text("Ovrflo");
            return;
        }
        uint32_t tenths = (uint32_t)(value * 10 + 0.5);
        char bottom[7];
        if (tenths < 1000 && tenths % 10) {
            uint8_t whole = tenths / 10;
            bottom[0] = whole >= 10 ? (char)('0' + whole / 10) : ' ';
            bottom[1] = (char)('0' + whole % 10);
            bottom[2] = (char)('0' + tenths % 10);
            memcpy(bottom + 3, frequency_output_units[i], 4);
            display_bottom_text(bottom);
            watch_set_colon();
        } else {
            snprintf(bottom, sizeof(bottom), "%3lu%s",
                     (unsigned long)(uint32_t)(value + 0.5), frequency_output_units[i]);
            display_bottom_text(bottom);
        }
        return;
    }
    display_bottom_text("Undflo");
}

static void display_time_seconds(double seconds) {
    if (seconds == 0) {
        display_bottom_text("----  ");
        return;
    }
    for (int8_t i = 8; i >= 0; i--) {
        double value = seconds / time_factors[i];
        if (value < 1 && i > 0) continue;
        if (value < 0.05) break;
        if (value >= 9999.5) {
            display_bottom_text("Ovrflo");
            return;
        }
        uint32_t tenths = (uint32_t)(value * 10 + 0.5);
        char number[5];
        if (tenths < 1000 && tenths % 10) {
            uint8_t whole = tenths / 10;
            number[0] = whole >= 10 ? (char)('0' + whole / 10) : ' ';
            number[1] = (char)('0' + whole % 10);
            number[2] = (char)('0' + tenths % 10);
            number[3] = ' ';
            number[4] = '\0';
            watch_set_colon();
        } else {
            snprintf(number, sizeof(number), "%4lu", (unsigned long)(uint32_t)(value + 0.5));
            watch_clear_colon();
        }
        watch_display_text(WATCH_POSITION_HOURS, number);
        watch_display_text(WATCH_POSITION_MINUTES, number + 2);
        watch_display_text(WATCH_POSITION_SECONDS, time_units[i]);
        return;
    }
    display_bottom_text("Undflo");
}

static void display_time_ratio(uint64_t numerator, uint64_t denominator, const char *unit) {
    char number[5];
    uint64_t tenths = (numerator * 10 + denominator / 2) / denominator;
    if (tenths == 0 && numerator != 0) {
        display_bottom_text("Undflo");
        return;
    }
    if (tenths < 1000 && tenths % 10 != 0) {
        uint8_t whole = tenths / 10;
        number[0] = whole >= 10 ? (char)('0' + whole / 10) : ' ';
        number[1] = (char)('0' + whole % 10);
        number[2] = (char)('0' + tenths % 10);
        number[3] = ' ';
        number[4] = '\0';
        watch_set_colon();
    } else {
        uint64_t rounded = (numerator + denominator / 2) / denominator;
        if (rounded > 9999) {
            display_bottom_text("Ovrflo");
            return;
        }
        snprintf(number, sizeof(number), "%4lu", (unsigned long)rounded);
        watch_clear_colon();
    }
    watch_display_text(WATCH_POSITION_HOURS, number);
    watch_display_text(WATCH_POSITION_MINUTES, number + 2);
    watch_display_text(WATCH_POSITION_SECONDS, unit);
}

static void display_rc_cutoff(uint32_t significant, int8_t exponent) {
    static const char *const units[] = {
        "pHz", "nHz", "uHz", "mHz", " Hz", "kHz", "MHz", "GHz", "THz"
    };
    if (significant == 0) {
        display_bottom_text("----  ");
        return;
    }

    // 2*pi is approximated by 6283185 / 1000000. With time expressed in
    // picoseconds, the cutoff in Hz is 10^(18-exponent)/(6283185*significant).
    uint64_t denominator = 6283185ULL * significant;
    for (uint8_t i = 0; i < 9; i++) {
        int8_t unit_exponent = (int8_t)(i * 3) - 12;
        int8_t power = 18 - exponent - unit_exponent;
        if (power >= 14) continue; // at least 1000 of this unit
        uint64_t numerator = power_of_ten((uint8_t)power);
        uint64_t rounded = (numerator + denominator / 2) / denominator;
        if (rounded >= 1000 && i < 8) continue;

        uint64_t tenths = (numerator * 10 + denominator / 2) / denominator;
        char bottom[7];
        if (tenths < 1000 && tenths % 10 != 0) {
            uint8_t whole = tenths / 10;
            bottom[0] = whole >= 10 ? (char)('0' + whole / 10) : ' ';
            bottom[1] = (char)('0' + whole % 10);
            bottom[2] = (char)('0' + tenths % 10);
            memcpy(bottom + 3, units[i], 4);
            display_bottom_text(bottom);
            watch_set_colon();
        } else {
            snprintf(bottom, sizeof(bottom), "%3lu%s", (unsigned long)rounded, units[i]);
            display_bottom_text(bottom);
        }
        return;
    }
    display_bottom_text("Ovrflo");
}

static void render_rc(const unit_converter_state_t *state) {
    const uint8_t *resistor = state->component_directions[0] ? state->resistor_value : state->selections[0];
    const uint8_t *capacitor = state->selections[1];
    uint32_t significant = (uint32_t)(resistor[0] * 10 + resistor[1]) *
                           (capacitor[0] * 10 + capacitor[1]);
    int8_t exponent = multiplier_exponents[resistor[2]] + capacitor[2];

    watch_display_text(WATCH_POSITION_TOP_LEFT, "RC");
    watch_display_text(WATCH_POSITION_TOP_RIGHT,
                       state->rc_page == 0 ? "Fc" : state->rc_page == 1 ? "1t" : "5t");
    if (state->rc_page == 0) {
        display_rc_cutoff(significant, exponent);
        return;
    }
    if (state->rc_page == 2) significant *= 5;
    // R in ohms times C in picofarads gives time in picoseconds. Keep the
    // decimal exponent separate so even the largest component codes fit.
    if (exponent < 0) {
        display_time_ratio(significant, power_of_ten((uint8_t)-exponent), "pS");
    } else if (exponent < 12) {
        uint64_t ps = (uint64_t)significant * power_of_ten((uint8_t)exponent);
        if (ps < 1000ULL) display_time_ratio(ps, 1, "pS");
        else if (ps < 1000000ULL) display_time_ratio(ps, 1000, "nS");
        else if (ps < 1000000000ULL) display_time_ratio(ps, 1000000, "uS");
        else if (ps < 1000000000000ULL) display_time_ratio(ps, 1000000000, "mS");
        else if (ps < 60000000000000ULL) display_time_ratio(ps, 1000000000000ULL, " S");
        else if (ps < 3600000000000000ULL) display_time_ratio(ps, 60000000000000ULL, "mn");
        else display_time_ratio(ps, 3600000000000000ULL, "hr");
    } else {
        uint64_t seconds = (uint64_t)significant * power_of_ten((uint8_t)(exponent - 12));
        if (seconds < 60ULL) display_time_ratio(seconds, 1, " S");
        else if (seconds < 3600ULL) display_time_ratio(seconds, 60, "mn");
        else if (seconds < 86400ULL) display_time_ratio(seconds, 3600, "hr");
        else if (seconds < 31557600ULL) display_time_ratio(seconds, 86400, "dy");
        else display_time_ratio(seconds, 31557600, "yr");
    }
}

static void open_selected_tool(unit_converter_state_t *state) {
    static const uint8_t menu_tools[] = { 0, 1, 5, 4, 3, 2 };
    state->tool = menu_tools[state->menu_index];
    if (state->tool < 2) {
        if (state->component != state->tool) state->selected_band = 0;
        state->component = state->tool;
        state->direction = state->component_directions[state->tool];
    }
    state->in_menu = false;
}

static void render(const unit_converter_state_t *state) {
    static const char *const menu_names[] = { "rES   ", "CAP   ", "rC    ", "FrEq  ", "AntEnn", "tEMP  " };
    char label[3];
    watch_clear_display();

    if (state->in_menu) {
        watch_display_text(WATCH_POSITION_TOP_LEFT, "UC");
        watch_display_text(WATCH_POSITION_BOTTOM, menu_names[state->menu_index]);
        return;
    }
    if (state->tool < 2) {
        render_component(state);
        return;
    }
    if (state->tool == 5) {
        render_rc(state);
        return;
    }
    if (state->tool == 2) {
        bool result = state->temperature_field == 5;
        label[0] = !result && state->temperature_negative ? '-' : 't';
        label[1] = result ? '=' : state->temperature_field == 0 ? 'S' :
                   (char)('0' + state->temperature_field);
        label[2] = '\0';
        watch_display_text(WATCH_POSITION_TOP_LEFT, label);
        if (result) {
            int32_t input = entered_number(state->temperature_digits);
            if (state->temperature_negative) input = -input;
            if (state->temperature_fahrenheit) {
                display_ratio((int64_t)(input - 32) * 5, 9, "#C");
            } else {
                display_ratio((int64_t)input * 9 + 160, 5, "#F");
            }
        } else {
            render_numeric_input(state->temperature_digits,
                                 state->temperature_fahrenheit ? "#F" : "#C");
        }
        return;
    }

    if (state->tool == 3) {
        bool result = state->dipole_field >= 5;
        label[0] = 'A';
        label[1] = result ? (state->dipole_field == 5 ? '=' : 'A') :
                   state->dipole_field == 4 ? 'M' : (char)('1' + state->dipole_field);
        label[2] = '\0';
        watch_display_text(WATCH_POSITION_TOP_LEFT, label);
        if (!result) {
            render_numeric_input(state->dipole_digits,
                                 frequency_input_units[state->dipole_prefix]);
            if (state->dipole_hundredths) watch_set_colon();
            return;
        }
        uint16_t input = entered_number(state->dipole_digits);
        if (input == 0) display_bottom_text("----  ");
        else display_dipole_length(input, state->dipole_prefix,
                                   state->dipole_hundredths, state->dipole_field == 6);
        return;
    }

    bool result = state->frequency_field == 5;
    label[0] = state->frequency_is_period ? 'P' : 'F';
    label[1] = result ? '=' : state->frequency_field == 4 ? 'M' :
               (char)('1' + state->frequency_field);
    label[2] = '\0';
    watch_display_text(WATCH_POSITION_TOP_LEFT, label);
    if (!result) {
        render_numeric_input(state->frequency_digits,
                             state->frequency_is_period ?
                             time_units[state->frequency_prefix[1]] :
                             frequency_input_units[state->frequency_prefix[0]]);
        return;
    }
    uint16_t input = entered_number(state->frequency_digits);
    if (input == 0) {
        display_bottom_text("----  ");
    } else if (state->frequency_is_period) {
        display_frequency_hz(1.0 / (input * time_factors[state->frequency_prefix[1]]));
    } else {
        display_time_seconds(1.0 / (input * frequency_factors[state->frequency_prefix[0]]));
    }
}

void unit_converter_face_setup(uint8_t watch_face_index, void **context_ptr) {
    (void)watch_face_index;
    if (*context_ptr == NULL) {
        unit_converter_state_t *state = calloc(1, sizeof(*state));
        state->selections[0][0] = BROWN;
        state->selections[0][1] = BLACK;
        state->selections[0][2] = RED;
        state->selections[0][3] = GOLD;
        memcpy(state->resistor_value, state->selections[0], sizeof(state->resistor_value));
        state->selections[1][0] = 1;
        state->selections[1][1] = 0;
        state->selections[1][2] = 2;
        state->in_menu = true;
        state->temperature_digits[2] = 2;
        state->temperature_digits[3] = 5;
        state->dipole_digits[1] = 7;
        state->dipole_digits[2] = 1;
        state->dipole_hundredths = true;
        state->dipole_prefix = 2;
        state->frequency_digits[0] = 1;
        state->frequency_prefix[0] = 0;
        state->frequency_prefix[1] = 3;
        *context_ptr = state;
    }
}

void unit_converter_face_activate(void *context) {
    // Movement sends EVENT_ACTIVATE immediately after this callback. Render
    // there once, after it has cleared the display for this face.
    ((unit_converter_state_t *)context)->in_menu = true;
}

bool unit_converter_face_loop(movement_event_t event, void *context) {
    unit_converter_state_t *state = context;
    switch (event.event_type) {
        case EVENT_ACTIVATE:
            render(state);
            break;
        case EVENT_TICK:
        case EVENT_LOW_ENERGY_UPDATE:
        case EVENT_TIMEOUT:
            // The selection is static. Keep the LCD image and allow standby.
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
        case EVENT_LIGHT_LONG_UP:
            // LIGHT is used repeatedly for navigation; do not light the LED.
            break;
        case EVENT_LIGHT_BUTTON_UP:
            if (state->in_menu) state->menu_index = (state->menu_index + 1) % 6;
            else if (state->tool < 2) {
                state->selected_band = (state->selected_band + 1) %
                    (state->component ? 4 : state->direction ? 8 : 5);
            } else if (state->tool == 2) {
                state->temperature_field = (state->temperature_field + 1) % 6;
            } else if (state->tool == 3) {
                state->dipole_field = (state->dipole_field + 1) % 7;
            } else if (state->tool == 5) {
                state->rc_page = (state->rc_page + 1) % 3;
            } else {
                state->frequency_field = (state->frequency_field + 1) % 6;
            }
            render(state);
            break;
        case EVENT_LIGHT_LONG_PRESS:
            if (state->in_menu) open_selected_tool(state);
            else state->in_menu = true;
            render(state);
            break;
        case EVENT_ALARM_BUTTON_UP: {
            if (state->in_menu) {
                open_selected_tool(state);
                render(state);
                break;
            }
            if (state->tool == 5) break;
            if (state->tool == 2) {
                if (state->temperature_field == 0) state->temperature_negative = !state->temperature_negative;
                else if (state->temperature_field < 5) {
                    uint8_t *digit = &state->temperature_digits[state->temperature_field - 1];
                    *digit = (*digit + 1) % 10;
                }
                render(state);
                break;
            }
            if (state->tool == 3) {
                if (state->dipole_field < 4) {
                    uint8_t *digit = &state->dipole_digits[state->dipole_field];
                    *digit = (*digit + 1) % 10;
                } else if (state->dipole_field == 4) {
                    state->dipole_prefix = (state->dipole_prefix + 1) % 3;
                }
                render(state);
                break;
            }
            if (state->tool == 4) {
                if (state->frequency_field < 4) {
                    uint8_t *digit = &state->frequency_digits[state->frequency_field];
                    *digit = (*digit + 1) % 10;
                } else if (state->frequency_field == 4) {
                    uint8_t mode = state->frequency_is_period ? 1 : 0;
                    uint8_t count = state->frequency_is_period ? 5 : 4;
                    state->frequency_prefix[mode] = (state->frequency_prefix[mode] + 1) % count;
                }
                render(state);
                break;
            }
            uint8_t *field;
            if (!state->component && state->direction) {
                if (state->selected_band >= 4) break; // output pages are read-only
                field = &state->resistor_value[state->selected_band];
                if (state->selected_band == 2) {
                    // Numeric multiplier order: 10^-2, 10^-1, 10^0 ... 10^9.
                    *field = *field == SILVER ? GOLD : *field == GOLD ? BLACK :
                             *field == WHITE ? SILVER : *field + 1;
                } else {
                    *field = next_color(*field, state->selected_band, false);
                }
            } else {
                if (state->selected_band == (state->component ? 3 : 4)) break;
                field = &state->selections[state->component][state->selected_band];
                *field = next_color(*field, state->selected_band, state->component);
            }
            render(state);
            break;
        }
        case EVENT_ALARM_LONG_PRESS:
            if (state->in_menu) break;
            if (state->tool == 5) break;
            if (state->tool == 2) {
                state->temperature_fahrenheit = !state->temperature_fahrenheit;
                render(state);
                break;
            }
            if (state->tool == 3) {
                state->dipole_hundredths = !state->dipole_hundredths;
                render(state);
                break;
            }
            if (state->tool == 4) {
                state->frequency_is_period = !state->frequency_is_period;
                render(state);
                break;
            }
            if (state->tool == 0) {
                if (state->direction) {
                    memcpy(state->selections[0], state->resistor_value, sizeof(state->resistor_value));
                } else {
                    memcpy(state->resistor_value, state->selections[0], sizeof(state->resistor_value));
                }
            }
            state->direction = !state->direction;
            state->component_directions[state->component] = state->direction;
            state->selected_band = 0;
            render(state);
            break;
        default:
            return movement_default_loop_handler(event);
    }
    return true;
}

void unit_converter_face_resign(void *context) {
    (void)context;
    watch_clear_indicator(WATCH_INDICATOR_BELL);
}
