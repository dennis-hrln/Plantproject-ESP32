/**
 * battery.h - Battery voltage monitoring interface
 * 
 * Reads battery voltage via ADC through voltage divider.
 * Provides threshold checking for warning and critical states.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

// =============================================================================
// BATTERY STATE ENUMERATION
// =============================================================================

typedef enum {
    BATTERY_OK,         // Above warning threshold, normal operation
    BATTERY_WARNING,    // Below warning but above critical, show LED warning
    BATTERY_CRITICAL    // Below critical threshold, disable watering
} BatteryState;

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * Initialize battery ADC reading.
 * Configures ADC pin and attenuation.
 */
void battery_init();

// =============================================================================
// READING FUNCTIONS
// =============================================================================

/**
 * Read current battery voltage in millivolts.
 * Compensates for voltage divider ratio.
 * 
 * @return Battery voltage in mV (e.g., 3700 = 3.7V)
 */
uint16_t battery_read_voltage_mv();

/**
 * Get current battery state based on voltage thresholds.
 * 
 * @return BATTERY_OK, BATTERY_WARNING, or BATTERY_CRITICAL
 */
BatteryState battery_get_state();

/**
 * Check if battery level permits watering.
 * Watering should be disabled when battery is critical.
 * 
 * @return true if voltage is above critical threshold
 */
bool battery_watering_allowed();

/**
 * Get battery level as percentage (0-100%).
 * Maps between empty and full voltage thresholds.
 * 
 * @return Approximate battery percentage
 */
uint8_t battery_get_percent();

#endif // BATTERY_H
