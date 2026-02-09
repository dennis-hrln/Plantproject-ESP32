/**
 * storage.h - NVS persistent storage interface
 * 
 * Wraps ESP32 Preferences library for storing calibration values,
 * settings, and timestamps that survive deep sleep and power cycles.
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

// =============================================================================
// STORAGE INITIALIZATION
// =============================================================================

/**
 * Initialize NVS storage system.
 * Call once at startup before any read/write operations.
 * 
 * @return true if initialization successful
 */
bool storage_init();

/**
 * Close NVS storage (call before deep sleep if needed).
 */
void storage_close();

// =============================================================================
// CALIBRATION VALUES
// =============================================================================

/**
 * Get dry calibration value (raw ADC reading for 0% humidity).
 * Returns stored value or DEFAULT_SENSOR_DRY if not set.
 */
uint16_t storage_get_sensor_dry();

/**
 * Get wet calibration value (raw ADC reading for 100% humidity).
 * Returns stored value or DEFAULT_SENSOR_WET if not set.
 */
uint16_t storage_get_sensor_wet();

/**
 * Store dry calibration value to NVS.
 * 
 * @param value Raw ADC reading when sensor is dry
 */
void storage_set_sensor_dry(uint16_t value);

/**
 * Store wet calibration value to NVS.
 * 
 * @param value Raw ADC reading when sensor is wet
 */
void storage_set_sensor_wet(uint16_t value);

// =============================================================================
// HUMIDITY SETPOINT
// =============================================================================

/**
 * Get optimal humidity target percentage.
 * Returns stored value or DEFAULT_OPTIMAL_HUMIDITY if not set.
 */
uint8_t storage_get_optimal_humidity();

/**
 * Store optimal humidity target percentage.
 * 
 * @param percent Humidity percentage (0-100)
 */
void storage_set_optimal_humidity(uint8_t percent);

// =============================================================================
// WATERING TIMESTAMP
// =============================================================================

/**
 * Get timestamp of last watering event.
 * Returns persistent time in seconds (accumulated across boots).
 * Returns 0 if never watered.
 */
uint32_t storage_get_last_watering_time();

/**
 * Store timestamp of last watering event.
 * 
 * @param timestamp Persistent time in seconds
 */
void storage_set_last_watering_time(uint32_t timestamp);

// =============================================================================
// PERSISTENT TIME TRACKING
// =============================================================================

/**
 * Get current persistent time in seconds.
 * Accumulates time across deep sleep cycles and reboots.
 * Uses boot count and measurement interval to track total elapsed time.
 * 
 * @return Total elapsed seconds since first boot
 */
uint32_t storage_get_persistent_time();

/**
 * Increment boot counter and update persistent time.
 * Call this once per wake from deep sleep.
 *
 * @param sleep_duration_sec  How long the ESP was asleep (e.g. MEASUREMENT_INTERVAL_SEC on timer wake, 0 on power-on)
 * @param awake_duration_sec  How long the ESP was awake this cycle
 */
void storage_increment_boot_count(uint32_t sleep_duration_sec,
                                  uint32_t awake_duration_sec);

/**
 * Get total boot count.
 * 
 * @return Number of times system has booted
 */
uint32_t storage_get_boot_count();

/**
 * Reset all persistent time tracking.
 * Use for factory reset or initial setup.
 */
void storage_reset_time_tracking();

#endif // STORAGE_H
