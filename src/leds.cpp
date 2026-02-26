/**
 * leds.cpp - LED control implementation
 */

#include "leds.h"
#include "config.h"

// =============================================================================
// INITIALIZATION
// =============================================================================

void leds_init() {
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    
    // Start with LEDs off
    leds_all_off();
}

// =============================================================================
// BASIC CONTROL
// =============================================================================

void led_green_on() {
    digitalWrite(PIN_LED_GREEN, HIGH);
}

void led_green_off() {
    digitalWrite(PIN_LED_GREEN, LOW);
}

void led_red_on() {
    digitalWrite(PIN_LED_RED, HIGH);
}

void led_red_off() {
    digitalWrite(PIN_LED_RED, LOW);
}

void leds_all_off() {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);
}

// =============================================================================
// FLASH PATTERNS
// =============================================================================

void led_green_blink(uint8_t count, uint16_t duration_ms) {
    for (uint8_t i = 0; i < count; i++) {
        led_green_on();
        delay(duration_ms);
        led_green_off();
        delay(LED_PAUSE_MS);
    }
}

void led_red_blink(uint8_t count, uint16_t duration_ms) {
    for (uint8_t i = 0; i < count; i++) {
        led_red_on();
        delay(duration_ms);
        led_red_off();
        delay(LED_PAUSE_MS);
    }
}

// =============================================================================
// PATTERN PLAYER
// =============================================================================

void leds_play_pattern(const LedStep steps[], uint8_t length,
                       uint16_t pause_ms, uint16_t gap_ms) {
    for (uint8_t i = 0; i < length; i++) {
        uint16_t g = steps[i].green_ms;
        uint16_t r = steps[i].red_ms;

        if (g > 0) led_green_on();
        if (r > 0) led_red_on();

        // Hold for the longer of the two durations
        delay((g > r) ? g : r);
        leds_all_off();

        // Pause between steps (skip after last step)
        if (pause_ms > 0 && i < length - 1) {
            delay(pause_ms);
        }
    }

    if (gap_ms > 0) delay(gap_ms);
}

// =============================================================================
// VALUE DISPLAY (humidity = green, battery = red)
// =============================================================================

static inline void led_on(bool use_red)  { use_red ? led_red_on()  : led_green_on();  }
static inline void led_off(bool use_red) { use_red ? led_red_off() : led_green_off(); }

static inline void led_blink(bool use_red, uint8_t count, uint16_t ms) {
    use_red ? led_red_blink(count, ms) : led_green_blink(count, ms);
}

void led_display_value(uint8_t value, bool use_red) {
    if (value > 100) value = 100;

    uint8_t tens = value / 10;
    uint8_t ones = value % 10;

    // Start indicator — both LEDs briefly
    led_green_on();
    led_red_on();
    delay(LED_NUMBER_START_MS);
    leds_all_off();
    delay(LED_DIGIT_PAUSE_MS);

    // Tens digit (long flashes)
    if (tens > 0) {
        led_blink(use_red, tens, LED_LONG);
    }

    delay(LED_DIGIT_PAUSE_MS);

    // Ones digit (short flashes) or zero indicator
    if (ones > 0) {
        led_blink(use_red, ones, LED_SHORT);
    } else {
        led_on(!use_red);
        delay(LED_RAPID);
        led_off(!use_red);
    }

    // End indicator — double flash (pattern from config.h)
    delay(LED_PAUSE_MS);
    PLAY_PATTERN(NUM_END);
}

// =============================================================================
// STATUS INDICATORS
// =============================================================================

void led_show_battery_warning() {
    PLAY_PATTERN(BATT_WARN);
}

void led_show_battery_critical() {
    PLAY_PATTERN(BATT_CRIT);
}

void led_show_calibration_confirm() {
    PLAY_PATTERN(CAL_CONFIRM);
}

void led_show_error() {
    PLAY_PATTERN(ERROR);
}

void led_show_success() {
    PLAY_PATTERN(SUCCESS);
}
