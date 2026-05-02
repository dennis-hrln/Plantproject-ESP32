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
        #ifdef DEBUG_SERIAL
        Serial.print("[BATTERY] Cached voltage: ");
        Serial.print(cached_voltage_mv);
        Serial.println(" mV");
        #endif
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

    #ifdef DEBUG_SERIAL
    Serial.print("[BATTERY] Raw ADC: ");
    Serial.print(raw);
    Serial.print(", Voltage: ");
    Serial.print(cached_voltage_mv);
    Serial.println(" mV");
    #endif

    return cached_voltage_mv;
}

// =============================================================================
// STATE CHECKING
// =============================================================================

BatteryState battery_get_state() {
    uint16_t voltage = battery_read_voltage_mv();
    BatteryState state;

    if (voltage < BATTERY_CRITICAL_MV) state = BATTERY_CRITICAL;
    else if (voltage < BATTERY_WARNING_MV) state = BATTERY_WARNING;
    else state = BATTERY_OK;

    #ifdef DEBUG_SERIAL
    Serial.print("[BATTERY] State: ");
    switch(state) {
        case BATTERY_OK:       Serial.print("OK"); break;
        case BATTERY_WARNING:  Serial.print("WARNING"); break;
        case BATTERY_CRITICAL: Serial.print("CRITICAL"); break;
    }
    Serial.print(" (");
    Serial.print(voltage);
    Serial.println(" mV)");
    #endif

    return state;
}

bool battery_watering_allowed() {
    return (battery_read_voltage_mv() >= BATTERY_CRITICAL_MV);
}

// =============================================================================
// PERCENTAGE CALCULATION
// =============================================================================

uint8_t battery_get_percent() {
    uint16_t voltage = battery_read_voltage_mv();
    uint8_t percent;

    if (voltage <= BATTERY_EMPTY_MV) percent = 0;
    else if (voltage >= BATTERY_FULL_MV) percent = 100;
    else percent = (uint8_t)(((uint32_t)(voltage - BATTERY_EMPTY_MV) * 100)
                     / (BATTERY_FULL_MV - BATTERY_EMPTY_MV));

    #ifdef DEBUG_SERIAL
    Serial.print("[BATTERY] Percentage: ");
    Serial.print(percent);
    Serial.println("%");
    #endif

    return percent;
}
