/**
 * battery.cpp - Battery voltage monitoring implementation
 */

#include "battery.h"
#include "config.h"

// ADC reference voltage in millivolts (ESP32 with 11dB attenuation)
// Actual value may vary; calibrate if precision needed
#define ADC_REF_VOLTAGE_MV  3300

// =============================================================================
// INITIALIZATION
// =============================================================================

void battery_init() {
    // Configure ADC for battery voltage pin
    analogReadResolution(ADC_RESOLUTION);
    analogSetAttenuation(ADC_ATTENUATION);
    
    pinMode(PIN_BATTERY_ADC, INPUT);
}

// =============================================================================
// VOLTAGE READING
// =============================================================================

uint16_t battery_read_voltage_mv() {
    // Take multiple samples for accuracy
    uint32_t sum = 0;
    
    for (int i = 0; i < ADC_SAMPLES; i++) {
        sum += analogRead(PIN_BATTERY_ADC);
        delayMicroseconds(100);
    }
    
    uint16_t raw = sum / ADC_SAMPLES;
    
    // Convert ADC reading to voltage at divider output
    // voltage_at_adc = (raw / ADC_MAX) * ADC_REF_VOLTAGE
    uint32_t voltage_at_adc = ((uint32_t)raw * ADC_REF_VOLTAGE_MV) / ADC_MAX_VALUE;
    
    // Compensate for voltage divider to get actual battery voltage
    // actual_voltage = voltage_at_adc * divider_ratio
    uint16_t actual_voltage = (uint16_t)(voltage_at_adc * BATTERY_DIVIDER_RATIO);
    
    return actual_voltage;
}

// =============================================================================
// STATE CHECKING
// =============================================================================

BatteryState battery_get_state() {
    uint16_t voltage = battery_read_voltage_mv();
    
    if (voltage < BATTERY_CRITICAL_MV) {
        return BATTERY_CRITICAL;
    } else if (voltage < BATTERY_WARNING_MV) {
        return BATTERY_WARNING;
    } else {
        return BATTERY_OK;
    }
}

bool battery_watering_allowed() {
    // Only allow watering if battery is above critical threshold
    uint16_t voltage = battery_read_voltage_mv();
    return (voltage >= BATTERY_CRITICAL_MV);
}

// =============================================================================
// PERCENTAGE CALCULATION
// =============================================================================

uint8_t battery_get_percent() {
    uint16_t voltage = battery_read_voltage_mv();
    
    // Clamp to valid range
    if (voltage <= BATTERY_EMPTY_MV) {
        return 0;
    }
    if (voltage >= BATTERY_FULL_MV) {
        return 100;
    }
    
    // Linear interpolation between empty and full
    // percent = (voltage - empty) / (full - empty) * 100
    uint32_t percent = (uint32_t)(voltage - BATTERY_EMPTY_MV) * 100 
                       / (BATTERY_FULL_MV - BATTERY_EMPTY_MV);
    
    return (uint8_t)percent;
}
