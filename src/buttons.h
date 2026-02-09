/**
 * buttons.h - Button handling interface
 *
 * Three-layer design:
 *   1. Debounce + raw read
 *   2. Press detection (short / long / none per button)
 *   3. Mode-based action dispatch
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

// =============================================================================
// CONSTANTS
// =============================================================================

#define BTN_COUNT 3   // Main, Cal-Wet, Cal-Dry

// =============================================================================
// TYPES
// =============================================================================

/** Per-button press result (produced by the detection layer). */
typedef enum {
    PRESS_NONE,
    PRESS_SHORT,
    PRESS_LONG
} ButtonPress;

/** Interaction modes (state machine for the action layer). */
typedef enum {
    MODE_GENERAL,
    MODE_PLANT_WATERING,
    MODE_DISPLAY_HUMIDITY,
    MODE_DISPLAY_OPTIMAL_HUMIDITY,
    MODE_CALIBRATION,
    MODE_CALIBRATE_DRY,
    MODE_CALIBRATE_WET,
    MODE_SET_OPTIMAL_HUMIDITY,
    MODE_LOWER_OPT_HUMIDITY,
    MODE_ADD_OPT_HUMIDITY
} ButtonMode;

// =============================================================================
// API
// =============================================================================

/**
 * Initialize button GPIO pins with internal pull-ups.
 */
void buttons_init(void);

/**
 * Handle button interaction after wake.
 * Runs a polling loop that detects presses, resolves mode,
 * and dispatches actions. Returns when an action fires or
 * the mode timeout is reached.
 */
void buttons_handle_interaction(void);

#endif // BUTTONS_H
