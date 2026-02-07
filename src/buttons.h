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
// BUTTON EVENT TYPES
// =============================================================================

typedef enum {
    BTN_EVENT_NONE,         // No event
    BTN_EVENT_SHORT_PRESS,  // Short press (<2s)
    BTN_EVENT_LONG_PRESS,   // Long press (>=2s)
    BTN_EVENT_RELEASED      // Button was released
} ButtonEvent;

// =============================================================================
// BUTTON STATE STRUCTURE
// =============================================================================

typedef struct {
    ButtonId button;            // Which button
    ButtonEvent event;          // Event type
    uint32_t press_duration_ms; // How long button was held
} ButtonState;

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * Initialize button GPIO pins with internal pull-ups.
 */
void buttons_init();

// =============================================================================
// READING FUNCTIONS
// =============================================================================

/**
 * Check if a specific button is currently pressed.
 * Includes debouncing.
 * 
 * @param button Button to check
 * @return true if button is pressed
 */
bool button_is_pressed(ButtonId button);

/**
 * Wait for a button event (press/release) with timeout.
 * Blocking call that waits for user interaction.
 * 
 * @param timeout_ms Maximum time to wait (0 = forever)
 * @return Button state with event information
 */
ButtonState button_wait_event(uint32_t timeout_ms);

/**
 * Check for any button press (non-blocking).
 * Returns immediately with current state.
 * 
 * @return Button state (BTN_NONE if no button pressed)
 */
ButtonState button_check();

// =============================================================================
// LONG PRESS DETECTION
// =============================================================================

/**
 * Wait for a specific button to be held for long press duration.
 * Returns true if button was held long enough.
 * 
 * @param button Button to monitor
 * @return true if long press detected
 */
bool button_wait_long_press(ButtonId button);

/**
 * Wait for button release after detecting a press.
 * 
 * @param button Button to monitor
 * @return Duration the button was held (ms)
 */
uint32_t button_wait_release(ButtonId button);

// =============================================================================
// COMBINATION DETECTION
// =============================================================================

/**
 * Check if multiple buttons are pressed simultaneously.
 * Used for protected calibration entry.
 * 
 * @return true if both calibration buttons are pressed
 */
bool buttons_calibration_combo_pressed();

/**
 * Check if main button is held during calibration combo.
 * Three-button safety for critical operations.
 * 
 * @return true if all three buttons are pressed
 */
bool buttons_full_combo_pressed();

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
