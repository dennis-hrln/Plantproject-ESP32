/**
 * battery.cpp - Battery voltage monitoring implementation
 *
 * Reads battery voltage once per wake cycle via a voltage divider
 * on an ADC pin.  The result is cached so that repeated calls to
 * battery_get_state() / battery_watering_allowed() / battery_get_percent()
 * do not trigger additional ADC reads.
 */

#include "battery.h"
#include "config.h"

// ADC reference voltage in millivolts (ESP32 with 11dB attenuation)
#define ADC_REF_VOLTAGE_MV  3300

// Cached voltage (0 = not yet read this wake cycle)
static uint16_t cached_voltage_mv = 0;

// =============================================================================
// INITIALIZATION
// =============================================================================

void battery_init() {
    pinMode(PIN_BATTERY_ADC, INPUT);
    cached_voltage_mv = 0;
}

// =============================================================================
// VOLTAGE READING (cached)
// =============================================================================

uint16_t battery_read_voltage_mv() {
    if (cached_voltage_mv != 0) {
        return cached_voltage_mv;
    }

    uint32_t sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        sum += analogRead(PIN_BATTERY_ADC);
        delayMicroseconds(100);
    }

    uint16_t raw = sum / ADC_SAMPLES;
    uint32_t voltage_at_adc = ((uint32_t)raw * ADC_REF_VOLTAGE_MV) / ADC_MAX_VALUE;
    cached_voltage_mv = (uint16_t)(voltage_at_adc * BATTERY_DIVIDER_RATIO);

    return cached_voltage_mv;
}

// =============================================================================
// STATE CHECKING
// =============================================================================

BatteryState battery_get_state() {
    uint16_t voltage = battery_read_voltage_mv();

    if (voltage < BATTERY_CRITICAL_MV) return BATTERY_CRITICAL;
    if (voltage < BATTERY_WARNING_MV)  return BATTERY_WARNING;
    return BATTERY_OK;
}

bool battery_watering_allowed() {
    return (battery_read_voltage_mv() >= BATTERY_CRITICAL_MV);
}

// =============================================================================
// PERCENTAGE CALCULATION
// =============================================================================

uint8_t battery_get_percent() {
    uint16_t voltage = battery_read_voltage_mv();

    if (voltage <= BATTERY_EMPTY_MV) return 0;
    if (voltage >= BATTERY_FULL_MV)  return 100;

    return (uint8_t)(((uint32_t)(voltage - BATTERY_EMPTY_MV) * 100)
                     / (BATTERY_FULL_MV - BATTERY_EMPTY_MV));
}
