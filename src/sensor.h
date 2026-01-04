/**
 * sensor.h - Soil moisture sensor interface
 * 
 * Handles ADC reading from capacitive soil moisture sensor
 * and conversion to humidity percentage using calibration values.
 */

#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * Initialize soil moisture sensor ADC.
 * Configures pin and ADC attenuation.
 */
void sensor_init();

// =============================================================================
// READING FUNCTIONS
// =============================================================================

/**
 * Read raw ADC value from soil sensor.
 * Takes multiple samples and averages for noise reduction.
 * 
 * @return Raw ADC value (0-4095 for 12-bit)
 */
uint16_t sensor_read_raw();

/**
 * Read soil humidity as percentage (0-100%).
 * Uses stored wet/dry calibration values for conversion.
 * 
 * Calculation: Maps raw value between dry (0%) and wet (100%)
 * Note: Capacitive sensors are inverted (higher ADC = drier)
 * 
 * @return Humidity percentage (0-100), clamped to valid range
 */
uint8_t sensor_read_humidity_percent();

// =============================================================================
// CALIBRATION
// =============================================================================

/**
 * Perform dry calibration.
 * Reads current sensor value and stores it as the 0% reference.
 * Call this with sensor in air or completely dry soil.
 * 
 * @return The raw value that was stored
 */
uint16_t sensor_calibrate_dry();

/**
 * Perform wet calibration.
 * Reads current sensor value and stores it as the 100% reference.
 * Call this with sensor in water or saturated soil.
 * 
 * @return The raw value that was stored
 */
uint16_t sensor_calibrate_wet();

/**
 * Check if sensor reading is within valid/expected range.
 * Detects disconnected sensor or hardware fault.
 * 
 * @param raw_value Raw ADC reading to validate
 * @return true if reading appears valid
 */
bool sensor_reading_valid(uint16_t raw_value);

#endif // SENSOR_H
