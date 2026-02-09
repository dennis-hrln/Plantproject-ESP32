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
    // ADC resolution + attenuation are configured globally in init_hardware()
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

uint8_t sensor_raw_to_humidity_percent(uint16_t raw) {
    // Load calibration values from NVS
    uint16_t dry_value = storage_get_sensor_dry();   // High ADC = dry = 0%
    uint16_t wet_value = storage_get_sensor_wet();   // Low ADC = wet = 100%
    
    // Prevent division by zero
    if (dry_value == wet_value) {
        return 50;
    }
    
    int32_t percent;
    
    if (dry_value > wet_value) {
        // Normal case: dry ADC > wet ADC (inverted capacitive sensor)
        if (raw >= dry_value)      percent = 0;
        else if (raw <= wet_value) percent = 100;
        else percent = (int32_t)(dry_value - raw) * 100 / (dry_value - wet_value);
    } else {
        // Unusual case: dry ADC < wet ADC (non-inverted sensor)
        if (raw <= dry_value)      percent = 0;
        else if (raw >= wet_value) percent = 100;
        else percent = (int32_t)(raw - dry_value) * 100 / (wet_value - dry_value);
    }
    
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    
    return (uint8_t)percent;
}

uint8_t sensor_read_humidity_percent() {
    return sensor_raw_to_humidity_percent(sensor_read_raw());
}

// =============================================================================
// CALIBRATION
// =============================================================================

uint16_t sensor_calibrate_dry() {
    // Average sensor value over adjustable calibration time
    unsigned long calibrating_time = SENSOR_CALIBRATION_TIME_MS;
    unsigned long start_time = millis();
    uint32_t sum = 0;
    uint32_t count = 0;
    #ifdef DEBUG_SERIAL
    Serial.println("Dry calibration started. Keep sensor dry for " + String(calibrating_time / 1000) + " seconds.");
    #endif
    while (millis() - start_time < calibrating_time) {
        sum += sensor_read_raw();
        count++;
        delay(10);
    }
    uint16_t avg_val = (count > 0) ? (uint16_t)(sum / count) : 0;
    storage_set_sensor_dry(avg_val);
    #ifdef DEBUG_SERIAL
    Serial.print("Dry calibration complete. Value: ");
    Serial.println(avg_val);
    #endif
    return avg_val;
}

uint16_t sensor_calibrate_wet() {
    // Average sensor value over adjustable calibration time
    unsigned long calibrating_time = SENSOR_CALIBRATION_TIME_MS;
    unsigned long start_time = millis();
    uint32_t sum = 0;
    uint32_t count = 0;
    #ifdef DEBUG_SERIAL
    Serial.println("Wet calibration started. Keep sensor wet for " + String(calibrating_time / 1000) + " seconds.");
    #endif
    while (millis() - start_time < calibrating_time) {
        sum += sensor_read_raw();
        count++;
        delay(10);
    }
    uint16_t avg_val = (count > 0) ? (uint16_t)(sum / count) : 0;
    storage_set_sensor_wet(avg_val);
    #ifdef DEBUG_SERIAL
    Serial.print("Wet calibration complete. Value: ");
    Serial.println(avg_val);
    #endif
    return avg_val;
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
