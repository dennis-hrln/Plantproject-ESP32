/**
 * leds.h - LED control interface
 * 
 * Controls status LEDs for user feedback.
 * Provides flash patterns for humidity display.
 */

#ifndef LEDS_H
#define LEDS_H

#include <Arduino.h>

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * Initialize LED GPIO pins.
 */
void leds_init();

// =============================================================================
// BASIC CONTROL
// =============================================================================

/**
 * Turn green LED on or off.
 */
void led_green_on();
void led_green_off();

/**
 * Turn red LED on or off.
 */
void led_red_on();
void led_red_off();

/**
 * Turn all LEDs off.
 */
void leds_all_off();

// =============================================================================
// FLASH PATTERNS
// =============================================================================

/**
 * Blink green LED a specified number of times.
 * 
 * @param count Number of blinks
 * @param duration_ms Duration of each blink
 */
void led_green_blink(uint8_t count, uint16_t duration_ms);

/**
 * Blink red LED a specified number of times.
 * 
 * @param count Number of blinks
 * @param duration_ms Duration of each blink
 */
void led_red_blink(uint8_t count, uint16_t duration_ms);

// =============================================================================
// HUMIDITY DISPLAY
// =============================================================================

/**
 * Display a two-digit number using LED flash pattern.
 * Tens digit: long flashes
 * Ones digit: short flashes
 * Pause between digits.
 * 
 * Example: 47% → 4 long flashes, pause, 7 short flashes
 * 
 * @param value Value to display (0-99)
 */
void led_display_number(uint8_t value);

/**
 * Display a humidity value using LED flash pattern.
 * Caller reads the sensor and passes the value.
 *
 * @param humidity Humidity percentage (0-100)
 */
void led_display_humidity(uint8_t humidity);

/**
 * Display battery percentage using RED LED flash pattern.
 * Tens digit: long red flashes, ones digit: short red flashes.
 *
 * @param percent Battery percentage (0-100)
 */
void led_display_battery_percent(uint8_t percent);

// =============================================================================
// STATUS INDICATORS
// =============================================================================

/**
 * Show battery warning pattern (red LED).
 */
void led_show_battery_warning();

/**
 * Show battery critical pattern (red LED).
 */
void led_show_battery_critical();

/**
 * Show calibration mode entered (confirmation).
 */
void led_show_calibration_confirm();

/**
 * Show error pattern.
 */
void led_show_error();

/**
 * Show success pattern.
 */
void led_show_success();

#endif // LEDS_H
