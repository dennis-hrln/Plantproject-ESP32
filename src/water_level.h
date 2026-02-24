/**
 * water_level.h - Water reservoir level monitoring interface
 * 
 * Uses a 3D-printed float switch to detect when the water reservoir
 * is below the minimum level. When low, watering is disabled
 * and LEDs signal every 15 minutes that a refill is needed.
 * 
 * Hardware: Float switch between PIN_WATER_LEVEL and GND.
 *   - Switch CLOSED (LOW)  = water level LOW (refill needed)
 *   - Switch OPEN   (HIGH) = water level OK
 */

#ifndef WATER_LEVEL_H
#define WATER_LEVEL_H

#include <Arduino.h>

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * Initialize water level sensor GPIO with internal pull-up.
 */
void water_level_init();

// =============================================================================
// STATUS
// =============================================================================

/**
 * Check if the water reservoir has enough water.
 * 
 * @return true if water level is OK (switch open, pin HIGH via pull-up)
 */
bool water_level_ok();

/**
 * Check if the water reservoir is low.
 * 
 * @return true if water level is below threshold (switch closed, pin LOW)
 */
bool water_level_low();

#endif // WATER_LEVEL_H
