/**
 * buttons.cpp - Button handling implementation
 *
 * Separated into three layers:
 *   1. Hardware read + debounce  (read_button_raw)
 *   2. Press detection           (buttons_poll → ButtonPress[BTN_COUNT])
 *   3. Mode / action dispatch    (resolve_mode, buttons_handle_interaction)
 */

#include "buttons.h"
#include "config.h"
#include "leds.h"
#include "sensor.h"
#include "watering.h"
#include "storage.h"

// =============================================================================
// CONSTANTS
// =============================================================================

static const uint8_t BTN_PINS[BTN_COUNT] = {
    PIN_BTN_MAIN,
    PIN_BTN_CAL_WET,
    PIN_BTN_CAL_DRY
};

// =============================================================================
// PER-BUTTON STATE (single struct, one debounce layer)
// =============================================================================

typedef struct {
    bool     is_down;          // currently held
    bool     long_reported;    // long-press already latched
    uint32_t press_start_ms;   // when the current press began
    uint32_t last_change_ms;   // last edge for debounce
    ButtonPress result;        // accumulated press type
} ButtonTracker;

static ButtonTracker s_btn[BTN_COUNT];

static void buttons_reset_all(void) {
    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
        s_btn[i].is_down        = false;
        s_btn[i].long_reported  = false;
        s_btn[i].press_start_ms = 0;
        s_btn[i].last_change_ms = 0;
        s_btn[i].result         = PRESS_NONE;
    }
}

static void buttons_reset_results(void) {
    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
        s_btn[i].result = PRESS_NONE;
    }
}

// =============================================================================
// RAW READ (active-low, pull-up)
// =============================================================================

static bool read_button_raw(uint8_t index) {
    return (digitalRead(BTN_PINS[index]) == LOW);
}

// =============================================================================
// PRESS DETECTION
// Call in a loop; returns true once ALL buttons are released.
// Results are accumulated in s_btn[].result between calls.
// =============================================================================

static bool buttons_poll(ButtonPress out[BTN_COUNT]) {
    const uint32_t now = millis();
    bool any_pressed = false;

    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
        const bool pressed = read_button_raw(i);

        // debounce gate
        if ((now - s_btn[i].last_change_ms) < BTN_DEBOUNCE_MS) {
            if (s_btn[i].is_down) any_pressed = true;
            continue;
        }

        if (pressed) {
            any_pressed = true;

            if (!s_btn[i].is_down) {                         // new press
                s_btn[i].is_down        = true;
                s_btn[i].press_start_ms = now;
                s_btn[i].long_reported  = false;
                s_btn[i].last_change_ms = now;
            } else if (!s_btn[i].long_reported &&
                       (now - s_btn[i].press_start_ms) >= BTN_LONG_PRESS_MS) {
                // mark that we've passed the long-press threshold; do not
                // set the final result yet — wait for release so visual
                // feedback only happens after the interaction completes.
                s_btn[i].long_reported = true;
            }
        } else if (s_btn[i].is_down) {                       // just released
            // On release decide whether it was a short or long press.
            if (s_btn[i].long_reported) {
                s_btn[i].result = PRESS_LONG;
            } else {
                s_btn[i].result = PRESS_SHORT;
            }
            s_btn[i].is_down        = false;
            s_btn[i].last_change_ms = now;
        }
    }

    if (any_pressed) return false;

    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
        out[i] = s_btn[i].result;
    }
    return true;
}

// =============================================================================
// PRESS-LIST HELPER
// =============================================================================

static bool presses_match(const ButtonPress p[BTN_COUNT],
                          ButtonPress p0, ButtonPress p1, ButtonPress p2) {
    return p[0] == p0 && p[1] == p1 && p[2] == p2;
}

// =============================================================================
// MODE RESOLUTION (pure logic, no side-effects)
// =============================================================================

static ButtonMode resolve_mode(ButtonMode mode,
                               uint32_t   last_mode_change_ms,
                               const ButtonPress presses[BTN_COUNT]) {
    // Timeout → fall back to general
    if (mode != MODE_GENERAL &&
        (millis() - last_mode_change_ms) > MODE_TIMEOUT_MS) {
        mode = MODE_GENERAL;
    }

    if (mode == MODE_GENERAL) {
        if      (presses_match(presses, PRESS_LONG,  PRESS_NONE, PRESS_NONE))
            mode = MODE_PLANT_WATERING;
        else if (presses_match(presses, PRESS_NONE,  PRESS_LONG,  PRESS_LONG))
            mode = MODE_CALIBRATION;              // Wet+Dry long → calibration
        else if (presses_match(presses, PRESS_LONG,  PRESS_LONG,  PRESS_LONG))
            mode = MODE_SET_OPTIMAL_HUMIDITY;      // All 3 long → set optimal humidity
        else if (presses_match(presses, PRESS_SHORT, PRESS_NONE,  PRESS_NONE))
            mode = MODE_DISPLAY_HUMIDITY;
        else if (presses_match(presses, PRESS_NONE,  PRESS_NONE,  PRESS_SHORT))
            mode = MODE_DISPLAY_OPTIMAL_HUMIDITY;

    } else if (mode == MODE_CALIBRATION) {
        if      (presses_match(presses, PRESS_NONE, PRESS_LONG, PRESS_NONE))
            mode = MODE_CALIBRATE_WET;   // Wet button → wet calibration
        else if (presses_match(presses, PRESS_NONE, PRESS_NONE, PRESS_LONG))
            mode = MODE_CALIBRATE_DRY;   // Dry button → dry calibration

    } else if (mode == MODE_SET_OPTIMAL_HUMIDITY) {
        if (presses_match(presses, PRESS_NONE, PRESS_SHORT, PRESS_NONE))
            mode = MODE_ADD_OPT_HUMIDITY;    // Wet button → increase (wetter)
        else if (presses_match(presses, PRESS_NONE, PRESS_NONE,  PRESS_SHORT))
            mode = MODE_LOWER_OPT_HUMIDITY;  // Dry button → decrease (drier)
    }

    return mode;
}

// =============================================================================
// INITIALIZATION
// =============================================================================

void buttons_init(void) {
    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
        pinMode(BTN_PINS[i], INPUT_PULLUP);
    }
    buttons_reset_all();
}

// =============================================================================
// ONE-SHOT ACTION HELPERS
// =============================================================================

static void perform_manual_watering(void) {
    led_green_blink(2, 100);
    // force_override = true: the user explicitly chose to water, so skip
    // the minimum-interval check.  Battery safety is still enforced.
    WateringResult result = watering_manual(true);

    switch (result) {
        case WATER_OK:          led_show_success();          break;
        case WATER_BATTERY_LOW: led_show_battery_critical();  break;
        default:                led_show_error();             break;
    }
}

static void perform_display_humidity(void) {
    led_display_humidity(sensor_read_humidity_percent());
}

static void perform_display_optimal_humidity(void) {
    led_display_number(storage_get_optimal_humidity());
}

static void perform_calibrate_wet(void) {
    led_red_on();                       // Red LED on during wet calibration
    (void)sensor_calibrate_wet();
    led_red_off();
    led_show_success();
}

static void perform_calibrate_dry(void) {
    led_green_on();                     // Green LED on during dry calibration
    (void)sensor_calibrate_dry();
    led_green_off();
    led_show_success();
}

static void adjust_optimal_humidity(int8_t direction) {
    uint8_t val = storage_get_optimal_humidity();

    if (direction > 0 && val <= (uint8_t)(100 - HUMIDITY_STEP)) {
        val += HUMIDITY_STEP;
    } else if (direction < 0 && val >= HUMIDITY_STEP) {
        val -= HUMIDITY_STEP;
    }

    storage_set_optimal_humidity(val);

    // Both LEDs are on in set-optimal-humidity mode.
    // Briefly turn off the relevant LED to confirm the adjustment.
    if (direction > 0) {
        led_green_off();
        delay(300);
        led_green_on();
    } else {
        led_red_off();
        delay(300);
        led_red_on();
    }
}

// =============================================================================
// CALIBRATION HEARTBEAT (non-blocking)
// =============================================================================

static bool     s_calib_heartbeat_on = false;
static uint32_t s_calib_heartbeat_ms = 0;

static void calibration_heartbeat_reset() {
    s_calib_heartbeat_on = false;
    s_calib_heartbeat_ms = 0;
    leds_all_off();
}

static void calibration_heartbeat_tick() {
    const uint32_t now = millis();
    const uint16_t on_ms  = 150;
    const uint16_t off_ms = 300;

    if (s_calib_heartbeat_ms == 0) {
        s_calib_heartbeat_ms = now;
        s_calib_heartbeat_on = true;
        led_green_on();
        led_red_on();
        return;
    }

    const uint16_t interval = s_calib_heartbeat_on ? on_ms : off_ms;
    if ((now - s_calib_heartbeat_ms) >= interval) {
        s_calib_heartbeat_ms = now;
        s_calib_heartbeat_on = !s_calib_heartbeat_on;
        if (s_calib_heartbeat_on) {
            led_green_on();
            led_red_on();
        } else {
            leds_all_off();
        }
    }
}

// =============================================================================
// MAIN INTERACTION LOOP
// =============================================================================

void buttons_handle_interaction(bool from_button_wake) {
    buttons_reset_all();

    // ── Seed buttons immediately ────────────────────────────────────
    // Back-date press_start_ms to 0 (≈ boot start) so the time the
    // button was physically held during boot counts towards the
    // long-press threshold.  This replaces the old BTN_WAKE_GRACE_MS
    // hack and the 960 ms wake-indicator blink that used to eat into
    // the press window.
    bool any_held = false;
    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
        if (read_button_raw(i)) {
            s_btn[i].is_down        = true;
            s_btn[i].press_start_ms = 0;
            s_btn[i].long_reported  = false;
            s_btn[i].last_change_ms = 0;
            any_held = true;
        }
    }

    // Brief wake acknowledgment (30 ms green pulse).
    led_green_on();
    delay(30);
    led_green_off();

    // If no button is currently held the press was released during
    // boot.  ESP32-C3 GPIO wake cannot identify which pin triggered,
    // so default to the most common short-press action.
    if (from_button_wake && !any_held) {
        perform_display_humidity();
        return;
    }

    ButtonMode mode                = MODE_GENERAL;
    uint32_t   last_mode_change_ms = millis();
    uint32_t   deadline_ms         = millis();

    bool    optimal_prompted     = false;

    while ((millis() - deadline_ms) < MODE_TIMEOUT_MS) {
        ButtonPress presses[BTN_COUNT] = {PRESS_NONE, PRESS_NONE, PRESS_NONE};

        if (!buttons_poll(presses)) {
            // Keep calibration heartbeat running while buttons are held
            if (mode == MODE_CALIBRATION) {
                calibration_heartbeat_tick();
            }
            yield();
            continue;
        }

        // Detect whether the user actually pressed something
        bool any_press = false;
        for (uint8_t i = 0; i < BTN_COUNT; ++i) {
            if (presses[i] != PRESS_NONE) any_press = true;
        }

        ButtonMode new_mode = resolve_mode(mode, last_mode_change_ms, presses);
        buttons_reset_results();

        // Unrecognized button combination → brief red flash as feedback
        if (any_press && new_mode == mode) {
            led_red_blink(1, 100);
            deadline_ms = millis();           // extend timeout so user can retry
            continue;
        }

        if (new_mode != mode) {
            if (mode == MODE_CALIBRATION) {
                calibration_heartbeat_reset();
            }
            if (mode == MODE_SET_OPTIMAL_HUMIDITY &&
                new_mode != MODE_ADD_OPT_HUMIDITY &&
                new_mode != MODE_LOWER_OPT_HUMIDITY) {
                leds_all_off();
            }
            if (new_mode == MODE_CALIBRATION) {
                calibration_heartbeat_reset();
            }
            mode                 = new_mode;
            last_mode_change_ms  = millis();
            deadline_ms          = millis();   // extend deadline on mode change
            optimal_prompted     = false;
        }

        switch (mode) {
            case MODE_GENERAL:
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

            case MODE_CALIBRATION:
                // Non-blocking heartbeat while waiting for calibration action.
                calibration_heartbeat_tick();
                break;

            case MODE_CALIBRATE_DRY:
                perform_calibrate_dry();
                return;

            case MODE_CALIBRATE_WET:
                perform_calibrate_wet();
                return;

            case MODE_SET_OPTIMAL_HUMIDITY:
                if (!optimal_prompted) {
                    // Both LEDs stay on while in this mode
                    led_green_on();
                    led_red_on();
                    optimal_prompted = true;
                }
                break;

            case MODE_LOWER_OPT_HUMIDITY:
                adjust_optimal_humidity(-1);
                mode                = MODE_SET_OPTIMAL_HUMIDITY;
                last_mode_change_ms = millis();
                deadline_ms         = millis();
                optimal_prompted    = false;
                break;

            case MODE_ADD_OPT_HUMIDITY:
                adjust_optimal_humidity(+1);
                mode                = MODE_SET_OPTIMAL_HUMIDITY;
                last_mode_change_ms = millis();
                deadline_ms         = millis();
                optimal_prompted    = false;
                break;

            default:
                break;
        }

        yield();
    }

    // Mode timed out — clean up LEDs
    calibration_heartbeat_reset();
}
