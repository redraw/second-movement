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
static const char *const frequency_units[] = {
    "Hz", "kH", "MH", "GH"
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

static void render_resistor(const unit_converter_resistor_t *resistor) {
    char label[3];
    char value[8];
    char code[3] = "  ";
    const uint8_t *numeric_fields = NULL;
    bool result_page = resistor->selected_band == 4;
    uint8_t color = 0;
    label[0] = 'r';
    label[1] = result_page ? '=' : (char)('1' + resistor->selected_band);
    label[2] = '\0';

    if (resistor->direction) {
        if (resistor->selected_band < 4) {
            label[0] = 'I';
            label[1] = (char)('1' + resistor->selected_band);
            numeric_fields = resistor->value;
            band_code(resistor->value[resistor->selected_band], resistor->selected_band, code);
        } else {
            uint8_t band = resistor->selected_band - 4;
            label[0] = 'b';
            label[1] = (char)('1' + band);
            color = resistor->value[band];
            snprintf(value, sizeof(value), "%s", color_names[color]);
            band_code(color, band, code);
        }
    } else if (result_page) {
        numeric_fields = resistor->colors;
        tolerance_code(resistor->colors[3], code);
    } else {
        color = resistor->colors[resistor->selected_band];
        snprintf(value, sizeof(value), "%s", color_names[color]);
        band_code(color, resistor->selected_band, code);
    }
    watch_display_text(WATCH_POSITION_TOP_LEFT, label);
    watch_display_text(WATCH_POSITION_TOP_RIGHT, code);
    if (numeric_fields) display_value(numeric_fields, false);
    else display_bottom_text(value);
    if (resistor->direction) watch_set_indicator(WATCH_INDICATOR_SIGNAL);
    else watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
}

static void render_capacitor(const unit_converter_capacitor_t *capacitor) {
    char label[3] = { 'C', capacitor->selected_band == 3 ? '=' : (char)('1' + capacitor->selected_band), '\0' };
    char code[3] = "  ";
    bool result_page = capacitor->selected_band == 3;

    watch_display_text(WATCH_POSITION_TOP_LEFT, label);
    if (!result_page && capacitor->direction) code[1] = (char)('0' + capacitor->digits[capacitor->selected_band]);
    watch_display_text(WATCH_POSITION_TOP_RIGHT, code);
    if (result_page == (bool)capacitor->direction) {
        char value[4] = {
            (char)('0' + capacitor->digits[0]),
            (char)('0' + capacitor->digits[1]),
            (char)('0' + capacitor->digits[2]), '\0'
        };
        display_bottom_text(value);
    } else {
        display_value(capacitor->digits, true);
    }
    if (capacitor->direction) watch_set_indicator(WATCH_INDICATOR_BELL);
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
        watch_display_text(WATCH_POSITION_SECONDS, frequency_units[i]);
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

static void render_rc(const unit_converter_rc_t *rc,
                      const unit_converter_resistor_t *resistor_state,
                      const unit_converter_capacitor_t *capacitor_state) {
    const uint8_t *resistor = resistor_state->direction ? resistor_state->value : resistor_state->colors;
    const uint8_t *capacitor = capacitor_state->digits;
    uint32_t significant = (uint32_t)(resistor[0] * 10 + resistor[1]) *
                           (capacitor[0] * 10 + capacitor[1]);
    int8_t exponent = multiplier_exponents[resistor[2]] + capacitor[2];

    watch_display_text(WATCH_POSITION_TOP_LEFT, "RC");
    watch_display_text(WATCH_POSITION_TOP_RIGHT,
                       rc->page == 0 ? "Fc" : rc->page == 1 ? "1t" : "5t");
    if (rc->page == 0) {
        display_rc_cutoff(significant, exponent);
        return;
    }
    if (rc->page == 2) significant *= 5;
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
    static const unit_converter_tool_t menu_tools[] = {
        UNIT_CONVERTER_TOOL_RESISTOR,
        UNIT_CONVERTER_TOOL_CAPACITOR,
        UNIT_CONVERTER_TOOL_RC,
        UNIT_CONVERTER_TOOL_FREQUENCY,
        UNIT_CONVERTER_TOOL_DIPOLE,
        UNIT_CONVERTER_TOOL_TEMPERATURE,
    };
    state->tool = menu_tools[state->menu_index];
    state->in_menu = false;
}

static void render_temperature(const unit_converter_temperature_t *temperature) {
    char label[3];
    bool result = temperature->field == 5;
    label[0] = !result && temperature->negative ? '-' : 't';
    label[1] = result ? '=' : temperature->field == 0 ? 'S' :
               (char)('0' + temperature->field);
    label[2] = '\0';
    watch_display_text(WATCH_POSITION_TOP_LEFT, label);
    if (result) {
        int32_t input = entered_number(temperature->digits);
        if (temperature->negative) input = -input;
        if (temperature->fahrenheit) {
            display_ratio((int64_t)(input - 32) * 5, 9, "#C");
        } else {
            display_ratio((int64_t)input * 9 + 160, 5, "#F");
        }
    } else {
        render_numeric_input(temperature->digits,
                             temperature->fahrenheit ? "#F" : "#C");
    }
}

static void render_dipole(const unit_converter_dipole_t *dipole) {
    char label[3];
    bool result = dipole->field >= 5;
    label[0] = 'A';
    label[1] = result ? (dipole->field == 5 ? '=' : 'A') :
               dipole->field == 4 ? 'M' : (char)('1' + dipole->field);
    label[2] = '\0';
    watch_display_text(WATCH_POSITION_TOP_LEFT, label);
    if (!result) {
        render_numeric_input(dipole->digits,
                             frequency_units[dipole->prefix]);
        if (dipole->hundredths) watch_set_colon();
        return;
    }
    uint16_t input = entered_number(dipole->digits);
    if (input == 0) display_bottom_text("----  ");
    else display_dipole_length(input, dipole->prefix,
                               dipole->hundredths, dipole->field == 6);
}

static void render_frequency(const unit_converter_frequency_t *frequency) {
    char label[3];
    bool result = frequency->field == 5;
    label[0] = frequency->is_period ? 'P' : 'F';
    label[1] = result ? '=' : frequency->field == 4 ? 'M' :
               (char)('1' + frequency->field);
    label[2] = '\0';
    watch_display_text(WATCH_POSITION_TOP_LEFT, label);
    if (!result) {
        render_numeric_input(frequency->digits,
                             frequency->is_period ?
                             time_units[frequency->prefix[1]] :
                             frequency_units[frequency->prefix[0]]);
        return;
    }
    uint16_t input = entered_number(frequency->digits);
    if (input == 0) {
        display_bottom_text("----  ");
    } else if (frequency->is_period) {
        display_frequency_hz(1.0 / (input * time_factors[frequency->prefix[1]]));
    } else {
        display_time_seconds(1.0 / (input * frequency_factors[frequency->prefix[0]]));
    }
}

static void render(const unit_converter_state_t *state) {
    static const char *const menu_names[] = { "rES   ", "CAP   ", "rC    ", "FrEq  ", "AntEnn", "tEMP  " };
    watch_clear_display();
    if (state->in_menu) {
        watch_display_text(WATCH_POSITION_TOP_LEFT, "UC");
        watch_display_text(WATCH_POSITION_BOTTOM, menu_names[state->menu_index]);
        return;
    }
    switch (state->tool) {
        case UNIT_CONVERTER_TOOL_RESISTOR: render_resistor(&state->resistor); break;
        case UNIT_CONVERTER_TOOL_CAPACITOR: render_capacitor(&state->capacitor); break;
        case UNIT_CONVERTER_TOOL_TEMPERATURE: render_temperature(&state->temperature); break;
        case UNIT_CONVERTER_TOOL_DIPOLE: render_dipole(&state->dipole); break;
        case UNIT_CONVERTER_TOOL_FREQUENCY: render_frequency(&state->frequency); break;
        case UNIT_CONVERTER_TOOL_RC: render_rc(&state->rc, &state->resistor, &state->capacitor); break;
        case UNIT_CONVERTER_TOOL_COUNT: break;
    }
}

void unit_converter_face_setup(uint8_t watch_face_index, void **context_ptr) {
    (void)watch_face_index;
    if (*context_ptr == NULL) {
        unit_converter_state_t *state = calloc(1, sizeof(*state));
        state->resistor.colors[0] = BROWN;
        state->resistor.colors[1] = BLACK;
        state->resistor.colors[2] = RED;
        state->resistor.colors[3] = GOLD;
        memcpy(state->resistor.value, state->resistor.colors, sizeof(state->resistor.value));
        state->capacitor.digits[0] = 1;
        state->capacitor.digits[1] = 0;
        state->capacitor.digits[2] = 2;
        state->in_menu = true;
        state->temperature.digits[2] = 2;
        state->temperature.digits[3] = 5;
        state->dipole.digits[1] = 7;
        state->dipole.digits[2] = 1;
        state->dipole.hundredths = true;
        state->dipole.prefix = 2;
        state->frequency.digits[0] = 1;
        state->frequency.prefix[0] = 0;
        state->frequency.prefix[1] = 3;
        *context_ptr = state;
    }
}

void unit_converter_face_activate(void *context) {
    // EVENT_ACTIVATE renders the saved menu or tool state after Movement
    // clears the display for this face.
    (void)context;
}

static bool resistor_loop(movement_event_t event, unit_converter_resistor_t *resistor) {
    uint8_t *field;
    switch (event.event_type) {
        case EVENT_LIGHT_BUTTON_UP:
            resistor->selected_band = (resistor->selected_band + 1) %
                (resistor->direction ? 8 : 5);
            return true;
        case EVENT_ALARM_BUTTON_UP:
            if (resistor->direction) {
                if (resistor->selected_band >= 4) return false;
                field = &resistor->value[resistor->selected_band];
                if (resistor->selected_band == 2) {
                    // Numeric multiplier order: 10^-2, 10^-1, 10^0 ... 10^9.
                    *field = *field == SILVER ? GOLD : *field == GOLD ? BLACK :
                             *field == WHITE ? SILVER : *field + 1;
                } else {
                    *field = next_color(*field, resistor->selected_band, false);
                }
            } else {
                if (resistor->selected_band == 4) return false;
                field = &resistor->colors[resistor->selected_band];
                *field = next_color(*field, resistor->selected_band, false);
            }
            return true;
        case EVENT_ALARM_LONG_PRESS:
            if (resistor->direction) {
                memcpy(resistor->colors, resistor->value, sizeof(resistor->value));
            } else {
                memcpy(resistor->value, resistor->colors, sizeof(resistor->value));
            }
            resistor->direction = !resistor->direction;
            resistor->selected_band = 0;
            return true;
        default:
            return false;
    }
}

static bool capacitor_loop(movement_event_t event, unit_converter_capacitor_t *capacitor) {
    switch (event.event_type) {
        case EVENT_LIGHT_BUTTON_UP:
            capacitor->selected_band = (capacitor->selected_band + 1) % 4;
            return true;
        case EVENT_ALARM_BUTTON_UP:
            if (capacitor->selected_band == 3) return false;
            capacitor->digits[capacitor->selected_band] =
                next_color(capacitor->digits[capacitor->selected_band],
                           capacitor->selected_band, true);
            return true;
        case EVENT_ALARM_LONG_PRESS:
            capacitor->direction = !capacitor->direction;
            capacitor->selected_band = 0;
            return true;
        default:
            return false;
    }
}

static bool temperature_loop(movement_event_t event, unit_converter_temperature_t *temperature) {
    switch (event.event_type) {
        case EVENT_LIGHT_BUTTON_UP:
            temperature->field = (temperature->field + 1) % 6;
            return true;
        case EVENT_ALARM_BUTTON_UP:
            if (temperature->field == 0) temperature->negative = !temperature->negative;
            else if (temperature->field < 5) {
                uint8_t *digit = &temperature->digits[temperature->field - 1];
                *digit = (*digit + 1) % 10;
            }
            return true;
        case EVENT_ALARM_LONG_PRESS:
            temperature->fahrenheit = !temperature->fahrenheit;
            return true;
        default:
            return false;
    }
}

static bool dipole_loop(movement_event_t event, unit_converter_dipole_t *dipole) {
    switch (event.event_type) {
        case EVENT_LIGHT_BUTTON_UP:
            dipole->field = (dipole->field + 1) % 7;
            return true;
        case EVENT_ALARM_BUTTON_UP:
            if (dipole->field < 4) {
                uint8_t *digit = &dipole->digits[dipole->field];
                *digit = (*digit + 1) % 10;
            } else if (dipole->field == 4) {
                dipole->prefix = (dipole->prefix + 1) % 3;
            }
            return true;
        case EVENT_ALARM_LONG_PRESS:
            dipole->hundredths = !dipole->hundredths;
            return true;
        default:
            return false;
    }
}

static bool frequency_loop(movement_event_t event, unit_converter_frequency_t *frequency) {
    switch (event.event_type) {
        case EVENT_LIGHT_BUTTON_UP:
            frequency->field = (frequency->field + 1) % 6;
            return true;
        case EVENT_ALARM_BUTTON_UP:
            if (frequency->field < 4) {
                uint8_t *digit = &frequency->digits[frequency->field];
                *digit = (*digit + 1) % 10;
            } else if (frequency->field == 4) {
                uint8_t mode = frequency->is_period ? 1 : 0;
                uint8_t count = frequency->is_period ? 5 : 4;
                frequency->prefix[mode] = (frequency->prefix[mode] + 1) % count;
            }
            return true;
        case EVENT_ALARM_LONG_PRESS:
            frequency->is_period = !frequency->is_period;
            return true;
        default:
            return false;
    }
}

static bool rc_loop(movement_event_t event, unit_converter_rc_t *rc) {
    if (event.event_type != EVENT_LIGHT_BUTTON_UP) return false;
    rc->page = (rc->page + 1) % 3;
    return true;
}

bool unit_converter_face_loop(movement_event_t event, void *context) {
    unit_converter_state_t *state = context;
    switch (event.event_type) {
        case EVENT_ACTIVATE:
            render(state);
            return true;
        case EVENT_TICK:
        case EVENT_LOW_ENERGY_UPDATE:
        case EVENT_TIMEOUT:
            // The selection is static. Keep the LCD image and allow standby.
            return true;
        case EVENT_LIGHT_BUTTON_DOWN:
        case EVENT_LIGHT_LONG_UP:
            // LIGHT is used repeatedly for navigation; do not light the LED.
            return true;
        case EVENT_LIGHT_LONG_PRESS:
            if (state->in_menu) open_selected_tool(state);
            else state->in_menu = true;
            render(state);
            return true;
        case EVENT_LIGHT_BUTTON_UP:
            if (state->in_menu) {
                state->menu_index = (state->menu_index + 1) % UNIT_CONVERTER_TOOL_COUNT;
                render(state);
                return true;
            }
            break;
        case EVENT_ALARM_BUTTON_UP:
            if (state->in_menu) {
                open_selected_tool(state);
                render(state);
                return true;
            }
            break;
        case EVENT_ALARM_LONG_PRESS:
            if (state->in_menu) return true;
            break;
        default:
            return movement_default_loop_handler(event);
    }

    bool changed = false;
    switch (state->tool) {
        case UNIT_CONVERTER_TOOL_RESISTOR: changed = resistor_loop(event, &state->resistor); break;
        case UNIT_CONVERTER_TOOL_CAPACITOR: changed = capacitor_loop(event, &state->capacitor); break;
        case UNIT_CONVERTER_TOOL_TEMPERATURE: changed = temperature_loop(event, &state->temperature); break;
        case UNIT_CONVERTER_TOOL_DIPOLE: changed = dipole_loop(event, &state->dipole); break;
        case UNIT_CONVERTER_TOOL_FREQUENCY: changed = frequency_loop(event, &state->frequency); break;
        case UNIT_CONVERTER_TOOL_RC: changed = rc_loop(event, &state->rc); break;
        case UNIT_CONVERTER_TOOL_COUNT: break;
    }
    if (changed) render(state);
    return true;
}

void unit_converter_face_resign(void *context) {
    (void)context;
    watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
    watch_clear_indicator(WATCH_INDICATOR_BELL);
}
