/**
 * watering.h - Core watering decision logic interface
 * 
 * Central decision engine that determines when to water.
 * Checks humidity, water reservoir level, battery, and timing constraints.
 */

#ifndef WATERING_H
#define WATERING_H

#include <Arduino.h>

// =============================================================================
// WATERING RESULT CODES
// =============================================================================

typedef enum {
    WATER_OK,               // Watering completed, max humidity reached
    WATER_PARTIAL,          // Watered but pulse limit reached before max humidity
    WATER_NOT_NEEDED,       // Humidity is above minimal threshold, no watering needed
    WATER_BATTERY_LOW,      // Battery too low to water
    WATER_RESERVOIR_LOW,    // Water reservoir needs refilling
    WATER_TOO_SOON,         // Minimum interval not elapsed
    WATER_SENSOR_ERROR,     // Sensor reading invalid
    WATER_PUMP_FAILED       // Pump failed to run
} WateringResult;

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * Initialize watering system.
 * Should be called after storage, sensor, battery, and pump init.
 */
void watering_init();

// =============================================================================
// DECISION FUNCTIONS
// =============================================================================

/**
 * Check all conditions and perform automatic watering if needed.
 * This is the main function called during timer wake.
 * 
 * Decision logic:
 * 1. Read sensor → calculate humidity %
 * 2. Compare humidity to minimal threshold
 * 3. Check water reservoir level
 * 4. Check battery level
 * 5. Check time since last watering
 * 6. If all conditions met → pulse-pump loop:
 *    pump → wait soak → re-read → repeat until >= max humidity or safety limit
 * 7. Update last watering timestamp
 * 
 * @return Result code indicating what happened
 */
WateringResult watering_check_and_execute();

/**
 * Perform manual watering (triggered by user button).
 * Still enforces battery and timing constraints for safety.
 * 
 * @param force_override If true, skip interval check (use with caution)
 * @return Result code indicating what happened
 */
WateringResult watering_manual(bool force_override);

// =============================================================================
// STATUS QUERIES
// =============================================================================

/**
 * Check if watering is currently allowed.
 * Combines water level, battery, and timing checks (not humidity).
 * 
 * @return true if watering would be permitted
 */
bool watering_is_allowed();

/**
 * Get seconds until next watering is allowed.
 * Based on minimum interval constraint.
 * 
 * @return Seconds remaining, or 0 if allowed now
 */
uint32_t watering_get_seconds_until_allowed();

/**
 * Get current soil humidity reading.
 * 
 * @return Humidity percentage (0-100)
 */
uint8_t watering_get_current_humidity();

/**
 * Check if soil needs water based on humidity threshold.
 * 
 * @return true if humidity < minimal threshold
 */
bool watering_soil_needs_water();

#endif // WATERING_H
