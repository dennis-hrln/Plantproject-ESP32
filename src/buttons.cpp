/**
 * buttons.cpp - Button handling implementation
 */

#include "buttons.h"
#include "config.h"
#include "leds.h"
#include "sensor.h"
#include "watering.h"
#include "storage.h"

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

/**
 * Get GPIO pin for button ID.
 * Returns -1 for invalid button (not 0, since GPIO 0 is a valid pin).
 */
static int get_button_pin(ButtonId button) {
    switch (button) {
        case BTN_MAIN:      return (int)PIN_BTN_MAIN;
        case BTN_CAL_WET:   return (int)PIN_BTN_CAL_WET;
        case BTN_CAL_DRY:   return (int)PIN_BTN_CAL_DRY;
        default:            return -1;
    }
}

/**
 * Read button with non-blocking debouncing.
 */
static uint8_t get_button_index(ButtonId button) {
    switch (button) {
        case BTN_MAIN:      return 0;
        case BTN_CAL_WET:   return 1;
        case BTN_CAL_DRY:   return 2;
        default:            return 255;
    }
}

static bool read_button_debounced(ButtonId button) {
    const int pin = get_button_pin(button);
    const uint8_t index = get_button_index(button);

    if (pin < 0 || index > 2) {
        return false;
    }

    static bool last_raw[3] = {false, false, false};
    static bool stable_state[3] = {false, false, false};
    static uint32_t last_change_ms[3] = {0, 0, 0};

    const uint32_t now = millis();
    const bool raw = (digitalRead((uint8_t)pin) == LOW);  // Pressed = LOW (pull-up)

    if (raw != last_raw[index]) {
        last_raw[index] = raw;
        last_change_ms[index] = now;
    }

    if ((now - last_change_ms[index]) >= BTN_DEBOUNCE_MS) {
        stable_state[index] = raw;
    }

    return stable_state[index];
}

typedef enum {
    PRESS_NONE,
    PRESS_SHORT,
    PRESS_LONG
} ButtonPress;

typedef enum {
    MODE_GENERAL,
    MODE_PLANT_WATERING,
    MODE_DISPLAY_HUMIDITY,
    MODE_DISPLAY_OPTIMAL_HUMIDITY,
    MODE_CALIBRATION_MODE,
    MODE_CALIBRATE_DRY,
    MODE_CALIBRATE_WET,
    MODE_SET_OPTIMAL_HUMIDITY,
    MODE_LOWER_OPT_HUMIDITY,
    MODE_ADD_OPT_HUMIDITY
} ButtonMode;

static ButtonPress s_button_presses[3] = {PRESS_NONE, PRESS_NONE, PRESS_NONE};
static bool s_button_is_down[3] = {false, false, false};
static bool s_long_reported[3] = {false, false, false};
static uint32_t s_press_start_ms[3] = {0, 0, 0};
static uint32_t s_last_change_ms[3] = {0, 0, 0};

static void reset_buttons() {
    for (uint8_t i = 0; i < 3; ++i) {
        s_button_presses[i] = PRESS_NONE;
    }
}

static void reset_button_states() {
    for (uint8_t i = 0; i < 3; ++i) {
        s_button_is_down[i] = false;
        s_long_reported[i] = false;
        s_press_start_ms[i] = 0;
        s_last_change_ms[i] = 0;
    }
}

static bool press_list_matches(const ButtonPress presses[3],
                               ButtonPress p0,
                               ButtonPress p1,
                               ButtonPress p2) {
    return presses[0] == p0 && presses[1] == p1 && presses[2] == p2;
}

static bool buttons_check_presses(ButtonPress out_presses[3]) {
    const uint32_t now = millis();
    bool any_pressed = false;
    const ButtonId ids[3] = {BTN_MAIN, BTN_CAL_WET, BTN_CAL_DRY};

    for (uint8_t i = 0; i < 3; ++i) {
        const bool pressed = read_button_debounced(ids[i]);
        if (pressed) {
            any_pressed = true;
        }

        if (pressed) {
            if (!s_button_is_down[i]) {
                if ((now - s_last_change_ms[i]) >= BTN_DEBOUNCE_MS) {
                    s_button_is_down[i] = true;
                    s_press_start_ms[i] = now;
                    s_long_reported[i] = false;
                    s_last_change_ms[i] = now;
                }
            } else if (!s_long_reported[i] &&
                       (now - s_press_start_ms[i]) >= BTN_LONG_PRESS_MS) {
                s_button_presses[i] = PRESS_LONG;
                s_long_reported[i] = true;
            }
        } else if (s_button_is_down[i]) {
            if (!s_long_reported[i]) {
                s_button_presses[i] = PRESS_SHORT;
            }
            s_button_is_down[i] = false;
            s_last_change_ms[i] = now;
        }
    }

    if (any_pressed) {
        return false;
    }

    for (uint8_t i = 0; i < 3; ++i) {
        out_presses[i] = s_button_presses[i];
    }

    return true;
}

static ButtonMode check_mode_timeout(ButtonMode mode, uint32_t last_mode_change_ms) {
    if (mode != MODE_GENERAL && (millis() - last_mode_change_ms) > MODE_TIMEOUT_MS) {
        return MODE_GENERAL;
    }
    return mode;
}

static ButtonMode actioncheck(ButtonMode mode,
                              uint32_t last_mode_change_ms,
                              const ButtonPress presses[3]) {
    mode = check_mode_timeout(mode, last_mode_change_ms);

    if (mode == MODE_GENERAL) {
        if (press_list_matches(presses, PRESS_NONE, PRESS_NONE, PRESS_NONE)) {
            // Do nothing
        } else if (press_list_matches(presses, PRESS_LONG, PRESS_NONE, PRESS_NONE)) {
            mode = MODE_PLANT_WATERING;
        } else if (press_list_matches(presses, PRESS_NONE, PRESS_LONG, PRESS_LONG)) {
            mode = MODE_CALIBRATION_MODE;
        } else if (press_list_matches(presses, PRESS_LONG, PRESS_LONG, PRESS_LONG)) {
            mode = MODE_SET_OPTIMAL_HUMIDITY;
        } else if (press_list_matches(presses, PRESS_SHORT, PRESS_NONE, PRESS_NONE)) {
            mode = MODE_DISPLAY_HUMIDITY;
        } else if (press_list_matches(presses, PRESS_NONE, PRESS_NONE, PRESS_SHORT)) {
            mode = MODE_DISPLAY_OPTIMAL_HUMIDITY;
        }
    } else if (mode == MODE_CALIBRATION_MODE) {
        if (press_list_matches(presses, PRESS_NONE, PRESS_LONG, PRESS_NONE)) {
            mode = MODE_CALIBRATE_DRY;
        } else if (press_list_matches(presses, PRESS_NONE, PRESS_NONE, PRESS_LONG)) {
            mode = MODE_CALIBRATE_WET;
        }
    } else if (mode == MODE_SET_OPTIMAL_HUMIDITY) {
        if (press_list_matches(presses, PRESS_NONE, PRESS_SHORT, PRESS_NONE)) {
            mode = MODE_LOWER_OPT_HUMIDITY;
        } else if (press_list_matches(presses, PRESS_NONE, PRESS_NONE, PRESS_SHORT)) {
            mode = MODE_ADD_OPT_HUMIDITY;
        }
    }

    reset_buttons();
    return mode;
}

// =============================================================================
// INITIALIZATION
// =============================================================================

void buttons_init() {
    pinMode(PIN_BTN_MAIN, INPUT_PULLUP);
    pinMode(PIN_BTN_CAL_WET, INPUT_PULLUP);
    pinMode(PIN_BTN_CAL_DRY, INPUT_PULLUP);
}

// =============================================================================
// HIGH-LEVEL INTERACTION HANDLER
// =============================================================================

static void perform_manual_watering() {
    #ifdef DEBUG_SERIAL
    // Serial.println("Manual watering requested");
    #endif

    led_green_blink(2, 100);
    WateringResult result = watering_manual(false);

    #ifdef DEBUG_SERIAL
    // Serial.print("Watering result: ");
    // Serial.println((int)result);
    #endif

    if (result == WATER_OK) {
        led_show_success();
    } else if (result == WATER_TOO_SOON) {
        led_red_blink(3, 200);
    } else if (result == WATER_BATTERY_LOW) {
        led_show_battery_critical();
    } else {
        led_show_error();
    }
}

static void perform_display_humidity() {
    #ifdef DEBUG_SERIAL
    uint8_t h = sensor_read_humidity_percent();
    // Serial.print("Displaying humidity: ");
    // Serial.print(h);
    // Serial.println("%");
    #endif

    led_display_humidity();
}

static void perform_display_optimal_humidity() {
    uint8_t current = storage_get_optimal_humidity();
    led_display_number(current);
}

static void perform_calibrate_wet() {
    led_green_on();
    uint16_t raw = sensor_calibrate_wet();

    #ifdef DEBUG_SERIAL
    // Serial.print("Wet calibration complete. Raw value: ");
    // Serial.println(raw);
    #endif

    led_green_off();
    led_show_success();
}

static void perform_calibrate_dry() {
    led_red_on();
    uint16_t raw = sensor_calibrate_dry();

    #ifdef DEBUG_SERIAL
    // Serial.print("Dry calibration complete. Raw value: ");
    // Serial.println(raw);
    #endif

    led_red_off();
    led_show_success();
}

void buttons_handle_interaction_with_wake(uint64_t wake_mask) {
    #ifdef DEBUG_SERIAL
    // Serial.println("Button wake - handling interaction");
    #endif

    (void)wake_mask;

    delay(BTN_DEBOUNCE_MS);
    reset_buttons();
    reset_button_states();

    ButtonMode mode = MODE_GENERAL;
    uint32_t last_mode_change_ms = millis();
    uint32_t start = millis();
    bool calibration_prompted = false;
    bool optimal_prompted = false;
    uint8_t optimal_value = 0;
    bool optimal_loaded = false;

    while ((millis() - start) < MODE_TIMEOUT_MS) {
        ButtonPress presses[3] = {PRESS_NONE, PRESS_NONE, PRESS_NONE};

        if (!buttons_check_presses(presses)) {
            delay(10);
            continue;
        }

        ButtonMode new_mode = actioncheck(mode, last_mode_change_ms, presses);
        const bool mode_changed = (new_mode != mode);

        if (mode_changed) {
            mode = new_mode;
            last_mode_change_ms = millis();
            calibration_prompted = false;
            optimal_prompted = false;
            if (mode == MODE_SET_OPTIMAL_HUMIDITY) {
                optimal_loaded = false;
            }
        }

        switch (mode) {
            case MODE_GENERAL:
                // No action in general mode without a matching press sequence.
                break;

            case MODE_PLANT_WATERING:
                perform_manual_watering();
                return;

            case MODE_DISPLAY_HUMIDITY:
                perform_display_humidity();
                return;

            case MODE_DISPLAY_OPTIMAL_HUMIDITY:
                perform_display_optimal_humidity();
                return;

            case MODE_CALIBRATION_MODE:
                if (!calibration_prompted) {
                    led_show_calibration_confirm();
                    calibration_prompted = true;
                }
                break;

            case MODE_CALIBRATE_DRY:
                perform_calibrate_dry();
                return;

            case MODE_CALIBRATE_WET:
                perform_calibrate_wet();
                return;

            case MODE_SET_OPTIMAL_HUMIDITY:
                if (!optimal_loaded) {
                    optimal_value = storage_get_optimal_humidity();
                    optimal_loaded = true;
                }
                if (!optimal_prompted) {
                    led_display_number(optimal_value);
                    optimal_prompted = true;
                }
                break;

            case MODE_LOWER_OPT_HUMIDITY:
                if (!optimal_loaded) {
                    optimal_value = storage_get_optimal_humidity();
                    optimal_loaded = true;
                }
                if (optimal_value >= 5) {
                    optimal_value -= 5;
                }
                storage_set_optimal_humidity(optimal_value);
                led_red_blink(1, 150);
                mode = MODE_SET_OPTIMAL_HUMIDITY;
                last_mode_change_ms = millis();
                optimal_prompted = false;
                break;

            case MODE_ADD_OPT_HUMIDITY:
                if (!optimal_loaded) {
                    optimal_value = storage_get_optimal_humidity();
                    optimal_loaded = true;
                }
                if (optimal_value <= 95) {
                    optimal_value += 5;
                }
                storage_set_optimal_humidity(optimal_value);
                led_green_blink(1, 150);
                mode = MODE_SET_OPTIMAL_HUMIDITY;
                last_mode_change_ms = millis();
                optimal_prompted = false;
                break;

            default:
                break;
        }

        delay(10);
    }

    #ifdef DEBUG_SERIAL
    // Serial.println("No button event detected");
    #endif
}

void buttons_handle_interaction() {
    buttons_handle_interaction_with_wake(0);
}
