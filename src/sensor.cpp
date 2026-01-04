/**
 * sensor.cpp - Soil moisture sensor implementation
 */

#include "sensor.h"
#include "config.h"
#include "storage.h"

// =============================================================================
// INITIALIZATION
// =============================================================================

void sensor_init() {
    // Configure ADC for soil sensor pin
    // ADC1 channels work even when WiFi is active
    analogReadResolution(ADC_RESOLUTION);
    analogSetAttenuation(ADC_ATTENUATION);  // 11dB = 0-3.3V range
    
    // Set pin mode (input-only pins like GPIO34/35 don't need this,
    // but it doesn't hurt)
    pinMode(PIN_SOIL_SENSOR, INPUT);
}

// =============================================================================
// RAW READING
// =============================================================================

uint16_t sensor_read_raw() {
    // Take multiple samples and average for noise reduction
    uint32_t sum = 0;
    
    for (int i = 0; i < ADC_SAMPLES; i++) {
        sum += analogRead(PIN_SOIL_SENSOR);
        delayMicroseconds(100);  // Small delay between samples
    }
    
    return (uint16_t)(sum / ADC_SAMPLES);
}

// =============================================================================
// PERCENTAGE CONVERSION
// =============================================================================

uint8_t sensor_read_humidity_percent() {
    uint16_t raw = sensor_read_raw();
    
    // Load calibration values from NVS
    uint16_t dry_value = storage_get_sensor_dry();   // High ADC = dry = 0%
    uint16_t wet_value = storage_get_sensor_wet();   // Low ADC = wet = 100%
    
    // Capacitive sensors are INVERTED:
    // - Higher ADC value = less moisture (drier)
    // - Lower ADC value = more moisture (wetter)
    
    // Prevent division by zero
    if (dry_value == wet_value) {
        return 50;  // Return midpoint if calibration is invalid
    }
    
    // Map from [dry_value, wet_value] to [0, 100]
    // Since dry > wet (inverted sensor), we calculate:
    // percent = (dry - raw) / (dry - wet) * 100
    
    int32_t percent;
    
    if (dry_value > wet_value) {
        // Normal case: dry ADC > wet ADC (inverted sensor)
        if (raw >= dry_value) {
            percent = 0;
        } else if (raw <= wet_value) {
            percent = 100;
        } else {
            percent = (int32_t)(dry_value - raw) * 100 / (dry_value - wet_value);
        }
    } else {
        // Unusual case: dry ADC < wet ADC (non-inverted sensor)
        if (raw <= dry_value) {
            percent = 0;
        } else if (raw >= wet_value) {
            percent = 100;
        } else {
            percent = (int32_t)(raw - dry_value) * 100 / (wet_value - dry_value);
        }
    }
    
    // Clamp to 0-100 range
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    return (uint8_t)percent;
}

// =============================================================================
// CALIBRATION
// =============================================================================

uint16_t sensor_calibrate_dry() {
    // Read current raw value
    uint16_t raw = sensor_read_raw();
    
    // Store as dry reference
    storage_set_sensor_dry(raw);
    
    return raw;
}

uint16_t sensor_calibrate_wet() {
    // Read current raw value
    uint16_t raw = sensor_read_raw();
    
    // Store as wet reference
    storage_set_sensor_wet(raw);
    
    return raw;
}

// =============================================================================
// VALIDATION
// =============================================================================

bool sensor_reading_valid(uint16_t raw_value) {
    // Check for extreme values that indicate hardware problems
    
    // Value of 0 or max usually indicates disconnected sensor
    if (raw_value < 100) return false;
    if (raw_value > ADC_MAX_VALUE - 100) return false;
    
    // Value is within reasonable range
    return true;
}
