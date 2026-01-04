/**
 * watering.cpp - Core watering decision logic implementation
 * 
 * DECISION FLOW:
 * ┌─────────────────────┐
 * │ Read Soil Sensor    │
 * └──────────┬──────────┘
 *            ▼
 * ┌─────────────────────┐
 * │ Sensor Valid?       │──No──▶ Return SENSOR_ERROR
 * └──────────┬──────────┘
 *           Yes
 *            ▼
 * ┌─────────────────────┐
 * │ Humidity < Optimal? │──No──▶ Return NOT_NEEDED
 * └──────────┬──────────┘
 *           Yes
 *            ▼
 * ┌─────────────────────┐
 * │ Battery OK?         │──No──▶ Return BATTERY_LOW
 * └──────────┬──────────┘
 *           Yes
 *            ▼
 * ┌─────────────────────┐
 * │ Interval Elapsed?   │──No──▶ Return TOO_SOON
 * └──────────┬──────────┘
 *           Yes
 *            ▼
 * ┌─────────────────────┐
 * │ Run Pump            │
 * └──────────┬──────────┘
 *            ▼
 * ┌─────────────────────┐
 * │ Update Timestamp    │
 * └──────────┬──────────┘
 *            ▼
 *      Return OK
 */

#include "watering.h"
#include "config.h"
#include "storage.h"
#include "sensor.h"
#include "battery.h"
#include "pump.h"

// =============================================================================
// INTERNAL STATE
// =============================================================================

// Cache for current readings (refreshed each wake cycle)
static uint8_t current_humidity = 0;
static uint16_t current_raw_sensor = 0;

// =============================================================================
// INITIALIZATION
// =============================================================================

void watering_init() {
    // Nothing special needed; dependencies initialized separately
    // Could add self-test here if desired
}

// =============================================================================
// TIME HELPERS
// =============================================================================

/**
 * Get current persistent timestamp in seconds.
 * This value persists across deep sleep cycles by tracking
 * boot count and sleep intervals in NVS.
 */
static uint32_t get_current_time_sec() {
    return storage_get_persistent_time();
}

/**
 * Check if minimum interval since last watering has elapsed.
 */
static bool interval_elapsed() {
    uint32_t last_watering = storage_get_last_watering_time();
    
    // If never watered (timestamp is 0), interval is considered elapsed
    if (last_watering == 0) {
        return true;
    }
    
    uint32_t current_time = get_current_time_sec();
    
    // Handle edge case where current time is less than last watering
    // (shouldn't happen, but guard against NVS corruption)
    if (current_time < last_watering) {
        return true;
    }
    
    uint32_t elapsed = current_time - last_watering;
    
    return (elapsed >= MIN_WATERING_INTERVAL_SEC);
}

/**
 * Get seconds remaining until watering is allowed again.
 */
static uint32_t get_seconds_until_interval_elapsed() {
    uint32_t last_watering = storage_get_last_watering_time();
    
    if (last_watering == 0) {
        return 0;
    }
    
    uint32_t current_time = get_current_time_sec();
    
    if (current_time < last_watering) {
        return 0;
    }
    
    uint32_t elapsed = current_time - last_watering;
    
    if (elapsed >= MIN_WATERING_INTERVAL_SEC) {
        return 0;
    }
    
    return MIN_WATERING_INTERVAL_SEC - elapsed;
}

// =============================================================================
// MAIN DECISION LOGIC
// =============================================================================

WateringResult watering_check_and_execute() {
    // Step 1: Read sensor
    current_raw_sensor = sensor_read_raw();
    
    // Step 2: Validate sensor reading
    if (!sensor_reading_valid(current_raw_sensor)) {
        return WATER_SENSOR_ERROR;
    }
    
    // Step 3: Calculate humidity percentage
    current_humidity = sensor_read_humidity_percent();
    
    // Step 4: Check if watering is needed (humidity below threshold)
    uint8_t optimal = storage_get_optimal_humidity();
    if (current_humidity >= optimal) {
        return WATER_NOT_NEEDED;
    }
    
    // Step 5: Check battery
    if (!battery_watering_allowed()) {
        return WATER_BATTERY_LOW;
    }
    
    // Step 6: Check minimum interval
    if (!interval_elapsed()) {
        return WATER_TOO_SOON;
    }
    
    // Step 7: All conditions met - run pump
    bool pump_success = pump_run_timed(PUMP_RUN_DURATION_MS);
    
    if (!pump_success) {
        return WATER_PUMP_FAILED;
    }
    
    // Step 8: Update timestamp
    storage_set_last_watering_time(get_current_time_sec());
    
    return WATER_OK;
}

// =============================================================================
// MANUAL WATERING
// =============================================================================

WateringResult watering_manual(bool force_override) {
    // Check battery first (never skip this)
    if (!battery_watering_allowed()) {
        return WATER_BATTERY_LOW;
    }
    
    // Check interval unless forced
    if (!force_override && !interval_elapsed()) {
        return WATER_TOO_SOON;
    }
    
    // Run pump
    bool pump_success = pump_run_timed(PUMP_RUN_DURATION_MS);
    
    if (!pump_success) {
        return WATER_PUMP_FAILED;
    }
    
    // Update timestamp
    storage_set_last_watering_time(get_current_time_sec());
    
    return WATER_OK;
}

// =============================================================================
// STATUS QUERIES
// =============================================================================

bool watering_is_allowed() {
    // Check battery and interval, but not humidity
    return battery_watering_allowed() && interval_elapsed();
}

uint32_t watering_get_seconds_until_allowed() {
    return get_seconds_until_interval_elapsed();
}

uint8_t watering_get_current_humidity() {
    // Return cached value from last check, or read fresh
    if (current_humidity == 0) {
        current_humidity = sensor_read_humidity_percent();
    }
    return current_humidity;
}

bool watering_soil_needs_water() {
    uint8_t humidity = sensor_read_humidity_percent();
    uint8_t optimal = storage_get_optimal_humidity();
    
    return (humidity < optimal);
}
