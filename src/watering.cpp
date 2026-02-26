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
 * ┌─────────────────────────┐
 * │ Humidity < Min Humidity? │──No──▶ Return NOT_NEEDED
 * └──────────┬───────────────┘
 *           Yes
 *            ▼
 * ┌─────────────────────┐
 * │ Water Level OK?     │──No──▶ Return RESERVOIR_LOW
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
 * ┌─────────────────────────────────────────┐
 * │ PULSE LOOP:                             │
 * │  1. Run pump (PUMP_RUN_DURATION_MS)     │
 * │  2. Wait soak time (SOAK_WAIT_TIME_MS)  │
 * │  3. Re-read sensor                      │
 * │  4. Humidity >= Max? → stop (OK)        │
 * │  5. Pulses >= MAX_PUMP_PULSES? → stop   │
 * │  6. Battery/reservoir still OK? → loop  │
 * └──────────┬──────────────────────────────┘
 *            ▼
 * ┌─────────────────────┐
 * │ Update Timestamp    │
 * └──────────┬──────────┘
 *            ▼
 *   Return OK or PARTIAL
 */

#include "watering.h"
#include "config.h"
#include "storage.h"
#include "sensor.h"
#include "battery.h"
#include "pump.h"
#include "water_level.h"

// =============================================================================
// INTERNAL STATE
// =============================================================================

// Cache for current readings (refreshed each wake cycle)
static int16_t  current_humidity   = -1;   // -1 = not yet read
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
// PULSE-PUMP LOOP
// =============================================================================

/**
 * Run the pulse-pump loop: pump → soak → re-read → repeat
 * until humidity >= max_humidity or safety limit reached.
 *
 * @return WATER_OK if max humidity reached,
 *         WATER_PARTIAL if pulse limit / safety stop,
 *         WATER_PUMP_FAILED if first pulse fails
 */
static WateringResult pump_until_max() {
    uint8_t max_hum = DEFAULT_MAX_HUMIDITY;
    uint8_t pulses  = 0;
    bool    reached_max = false;

    while (pulses < MAX_PUMP_PULSES) {
        // Run pump for one pulse
        bool pump_success = pump_run_timed(PUMP_RUN_DURATION_MS);
        if (!pump_success) {
            // First pulse failed → pump error; later failure → partial success
            if (pulses == 0) return WATER_PUMP_FAILED;
            break;
        }
        pulses++;

        // Wait for water to soak into soil before re-reading
        delay(SOAK_WAIT_TIME_MS);

        // Re-read sensor
        uint16_t raw = sensor_read_raw();
        if (!sensor_reading_valid(raw)) {
            // Sensor error mid-loop — we already delivered water, stop safely
            break;
        }
        current_humidity = sensor_raw_to_humidity_percent(raw);

        // Target reached?
        if (current_humidity >= max_hum) {
            reached_max = true;
            break;
        }

        // Safety re-checks before next pulse
        if (!battery_watering_allowed()) break;
        if (water_level_low())           break;
    }

    return reached_max ? WATER_OK : WATER_PARTIAL;
}

// =============================================================================
// MAIN DECISION LOGIC
// =============================================================================

WateringResult watering_check_and_execute() {
    // Step 1: Read sensor once
    current_raw_sensor = sensor_read_raw();
    
    // Step 2: Validate sensor reading
    if (!sensor_reading_valid(current_raw_sensor)) {
        return WATER_SENSOR_ERROR;
    }
    
    // Step 3: Convert to humidity using the same raw reading
    current_humidity = sensor_raw_to_humidity_percent(current_raw_sensor);
    
    // Step 4: Check if watering is needed (humidity below minimal threshold)
    uint8_t minimal = storage_get_minimal_humidity();
    if (current_humidity >= minimal) {
        return WATER_NOT_NEEDED;
    }
    
    // Step 5: Check water reservoir level
    if (water_level_low()) {
        return WATER_RESERVOIR_LOW;
    }
    
    // Step 6: Check battery
    if (!battery_watering_allowed()) {
        return WATER_BATTERY_LOW;
    }
    
    // Step 7: Check minimum interval
    if (!interval_elapsed()) {
        return WATER_TOO_SOON;
    }
    
    // Step 8: Pulse-pump loop — water until max humidity or safety limit
    WateringResult result = pump_until_max();
    
    // Step 9: Update timestamp (even for partial — water was delivered)
    if (result == WATER_OK || result == WATER_PARTIAL) {
        storage_set_last_watering_time(get_current_time_sec());
    }
    
    return result;
}

// =============================================================================
// MANUAL WATERING
// =============================================================================

WateringResult watering_manual(bool force_override) {
    // Check water reservoir first (never skip this)
    if (water_level_low()) {
        return WATER_RESERVOIR_LOW;
    }
    
    // Check battery (never skip this)
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
    // Check water level, battery, and interval (not humidity)
    return water_level_ok() && battery_watering_allowed() && interval_elapsed();
}

uint32_t watering_get_seconds_until_allowed() {
    return get_seconds_until_interval_elapsed();
}

uint8_t watering_get_current_humidity() {
    if (current_humidity < 0) {
        current_humidity = sensor_read_humidity_percent();
    }
    return (uint8_t)current_humidity;
}

bool watering_soil_needs_water() {
    uint8_t humidity = sensor_read_humidity_percent();
    uint8_t minimal = storage_get_minimal_humidity();
    
    return (humidity < minimal);
}
