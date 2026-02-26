/**
 * leds.h - LED control interface
 * 
 * Controls status LEDs for user feedback.
 * Provides shared value/status display patterns.
 */

#ifndef LEDS_H
#define LEDS_H

#include <Arduino.h>
#include "config.h"

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

/**
 * Play a pattern defined as an array of {green_ms, red_ms} steps.
 *
 * @param steps     Array of LedStep definitions
 * @param length    Number of steps in the array
 * @param pause_ms  Pause between individual steps
 * @param gap_ms    Trailing pause after the whole sequence
 */
void leds_play_pattern(const LedStep steps[], uint8_t length,
                       uint16_t pause_ms, uint16_t gap_ms);

/** Convenience macro — plays a named pattern from config.h. */
#define PLAY_PATTERN(name) \
    leds_play_pattern(PAT_##name, ARRAY_LEN(PAT_##name), \
                      PAT_##name##_PAUSE_MS, PAT_##name##_GAP_MS)

// =============================================================================
// VALUE DISPLAY (humidity / battery)
// =============================================================================

/**
 * Display a percentage value (0-100) using LED flashes.
 * Tens digit: long flashes, ones digit: short flashes.
 * Green LED for humidity, red LED for battery.
 *
 * @param value   Percentage to display (0-100)
 * @param use_red true = red LED (battery), false = green LED (humidity)
 */
void led_display_value(uint8_t value, bool use_red);

/** Display humidity percentage (green LED). */
inline void led_display_humidity(uint8_t humidity) { led_display_value(humidity, false); }

/** Display battery percentage (red LED). */
inline void led_display_battery_percent(uint8_t percent) { led_display_value(percent, true); }

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
