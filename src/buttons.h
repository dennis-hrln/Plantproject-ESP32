/**
 * buttons.h - Button handling interface
 * 
 * Handles button debouncing, press detection, and special sequences.
 * Supports short press, long press, and multi-button combinations.
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

// =============================================================================
// BUTTON IDENTIFIERS
// =============================================================================

typedef enum {
    BTN_NONE,           // No button pressed
    BTN_MAIN,           // Main interaction button
    BTN_CAL_WET,        // Wet calibration button
    BTN_CAL_DRY         // Dry calibration button
} ButtonId;

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * Initialize button GPIO pins with internal pull-ups.
 */
void buttons_init();

// =============================================================================
// HIGH-LEVEL ACTIONS
// =============================================================================

/**
 * Handle button wake and determine user intent.
 * Call this after waking from button interrupt.
 * 
 * Detects:
 * - Short press on main → display humidity
 * - Long press on main → manual watering
 * - Long press on cal button (with combo) → calibration
 * - Combination sequence → settings adjustment
 */
void buttons_handle_interaction();

/**
 * Handle button wake with GPIO wake mask for single-press detection.
 * Use the wake mask from esp_sleep_get_gpio_wakeup_status().
 * 
 * @param wake_mask GPIO wake bitmask (0 if unknown)
 */
void buttons_handle_interaction_with_wake(uint64_t wake_mask);

#endif // BUTTONS_H
