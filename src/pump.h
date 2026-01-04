/**
 * pump.h - Water pump control interface
 * 
 * Controls pump via N-MOSFET with safety features.
 * Enforces maximum run time and battery checks.
 */

#ifndef PUMP_H
#define PUMP_H

#include <Arduino.h>

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * Initialize pump control GPIO.
 * Sets pin as output and ensures pump is OFF.
 */
void pump_init();

// =============================================================================
// PUMP CONTROL
// =============================================================================

/**
 * Turn pump ON.
 * Direct control - caller must handle timing.
 * WARNING: Always call pump_off() after use!
 */
void pump_on();

/**
 * Turn pump OFF.
 * Safe to call multiple times.
 */
void pump_off();

/**
 * Run pump for specified duration with safety timeout.
 * Blocks until pump cycle complete.
 * 
 * @param duration_ms How long to run pump (clamped to MAX)
 * @return true if pump ran successfully
 */
bool pump_run_timed(uint32_t duration_ms);

/**
 * Emergency stop - immediately turn off pump.
 * Use in error conditions or battery critical situations.
 */
void pump_emergency_stop();

/**
 * Check if pump is currently running.
 * 
 * @return true if pump is ON
 */
bool pump_is_running();

#endif // PUMP_H
