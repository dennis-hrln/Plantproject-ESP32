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
// HUMIDITY DISPLAY
// =============================================================================

/**
 * Display a two-digit number using LED flash pattern.
 * 
 * Pattern:
 * - Brief start indicator (both LEDs)
 * - Pause
 * - TENS digit: Long green flashes (600ms each)
 * - Longer pause (1s)
 * - ONES digit: Short green flashes (200ms each)
 * - End indicator
 * 
 * Special cases:
 * - 0 tens: Skip tens display
 * - 0 ones: One very brief flash
 * - 100%: Three long flashes (special pattern)
 */
void led_display_number(uint8_t value) {
    // Handle 100% special case
    if (value >= 100) {
        // Three long flashes for 100%
        led_green_blink(3, LED_FLASH_LONG_MS);
        return;
    }
    
    uint8_t tens = value / 10;
    uint8_t ones = value % 10;
    
    // Start indicator - brief flash of both LEDs
    led_green_on();
    led_red_on();
    delay(100);
    leds_all_off();
    delay(LED_DIGIT_PAUSE_MS);
    
    // Display tens digit with long flashes
    if (tens > 0) {
        for (uint8_t i = 0; i < tens; i++) {
            led_green_on();
            delay(LED_FLASH_LONG_MS);
            led_green_off();
            delay(LED_PAUSE_MS);
        }
    }
    // If tens is 0, we skip (no flashes for zero tens)
    
    // Pause between digits
    delay(LED_DIGIT_PAUSE_MS);
    
    // Display ones digit with short flashes
    if (ones > 0) {
        for (uint8_t i = 0; i < ones; i++) {
            led_green_on();
            delay(LED_FLASH_SHORT_MS);
            led_green_off();
            delay(LED_PAUSE_MS);
        }
    } else {
        // Zero ones: one very brief flash to indicate zero
        led_green_on();
        delay(80);
        led_green_off();
    }
    
    // End indicator - brief double flash
    delay(LED_PAUSE_MS);
    led_green_on();
    delay(50);
    led_green_off();
    delay(100);
    led_green_on();
    delay(50);
    led_green_off();
}

void led_display_humidity(uint8_t humidity) {
    led_display_number(humidity);
}

// =============================================================================
// STATUS INDICATORS
// =============================================================================

void led_show_battery_warning() {
    // Yellow effect: alternate red and green, or just red blinks
    led_red_blink(2, 300);
}

void led_show_battery_critical() {
    // Rapid red flashing
    led_red_blink(5, 100);
}

void led_show_calibration_confirm() {
    // Green-red alternating to confirm calibration
    led_green_on();
    delay(200);
    led_green_off();
    led_red_on();
    delay(200);
    led_red_off();
    led_green_on();
    delay(200);
    led_green_off();
}

void led_show_error() {
    // Rapid red blinks
    led_red_blink(3, 100);
}

void led_show_success() {
    // Two solid green blinks
    led_green_blink(2, 300);
}
