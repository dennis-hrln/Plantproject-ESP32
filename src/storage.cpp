/**
 * storage.cpp - NVS persistent storage implementation
 */

#include "storage.h"
#include "config.h"
#include <Preferences.h>

// Preferences instance (ESP32 NVS wrapper)
static Preferences prefs;

// =============================================================================
// INITIALIZATION
// =============================================================================

bool storage_init() {
    // Open NVS namespace in read-write mode
    // Second param: false = read-write, true = read-only
    return prefs.begin(NVS_NAMESPACE, false);
}

void storage_close() {
    prefs.end();
}

// =============================================================================
// CALIBRATION VALUES
// =============================================================================

uint16_t storage_get_sensor_dry() {
    // Return stored value, or default if key doesn't exist
    return prefs.getUShort(NVS_KEY_SENSOR_DRY, DEFAULT_SENSOR_DRY);
}

uint16_t storage_get_sensor_wet() {
    return prefs.getUShort(NVS_KEY_SENSOR_WET, DEFAULT_SENSOR_WET);
}

void storage_set_sensor_dry(uint16_t value) {
    prefs.putUShort(NVS_KEY_SENSOR_DRY, value);
}

void storage_set_sensor_wet(uint16_t value) {
    prefs.putUShort(NVS_KEY_SENSOR_WET, value);
}

// =============================================================================
// HUMIDITY SETPOINT
// =============================================================================

uint8_t storage_get_minimal_humidity() {
    return prefs.getUChar(NVS_KEY_MINIMAL_HUMIDITY, DEFAULT_MINIMAL_HUMIDITY);
}

void storage_set_minimal_humidity(uint8_t percent) {
    // Clamp to valid range
    if (percent > 100) percent = 100;
    prefs.putUChar(NVS_KEY_MINIMAL_HUMIDITY, percent);
}

// =============================================================================
// WATERING TIMESTAMP
// =============================================================================

uint32_t storage_get_last_watering_time() {
    return prefs.getULong(NVS_KEY_LAST_WATERING, 0);
}

void storage_set_last_watering_time(uint32_t timestamp) {
    prefs.putULong(NVS_KEY_LAST_WATERING, timestamp);
}

// =============================================================================
// PERSISTENT TIME TRACKING
// =============================================================================

/**
 * Persistent time is calculated by accumulating:
 *   sleep_duration_sec  (time spent in deep sleep before this wake)
 *   awake_duration_sec  (time spent awake during this cycle)
 *
 * Caller passes the appropriate sleep duration:
 *   Timer wake  → MEASUREMENT_INTERVAL_SEC
 *   Power-on    → 0 (no prior sleep)
 *   Button wake → not called (no time added)
 */

uint32_t storage_get_persistent_time() {
    return prefs.getULong(NVS_KEY_TOTAL_TIME, 0);
}

void storage_increment_boot_count(uint32_t sleep_duration_sec,
                                  uint32_t awake_duration_sec) {
    uint32_t boot_count = prefs.getULong(NVS_KEY_BOOT_COUNT, 0);
    uint32_t total_time = prefs.getULong(NVS_KEY_TOTAL_TIME, 0);
    
    boot_count++;
    prefs.putULong(NVS_KEY_BOOT_COUNT, boot_count);
    
    total_time += sleep_duration_sec + awake_duration_sec;
    prefs.putULong(NVS_KEY_TOTAL_TIME, total_time);
}

uint32_t storage_get_boot_count() {
    return prefs.getULong(NVS_KEY_BOOT_COUNT, 0);
}

void storage_reset_time_tracking() {
    prefs.putULong(NVS_KEY_BOOT_COUNT, 0);
    prefs.putULong(NVS_KEY_TOTAL_TIME, 0);
    prefs.putULong(NVS_KEY_LAST_WATERING, 0);
}
