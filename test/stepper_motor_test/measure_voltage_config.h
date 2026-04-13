#ifndef STEPPER_MEASURE_VOLTAGE_CONFIG_H
#define STEPPER_MEASURE_VOLTAGE_CONFIG_H

#include <Arduino.h>

// ADC pin used for the divider measurement tap.
static constexpr gpio_num_t MEASURE_ADC_PIN = GPIO_NUM_4;

// Divider values (R1 top, R2 bottom):
// measured_node -- R1 -- ADC -- R2 -- GND
static constexpr float R_TOP_OHM = 10000.0f;
static constexpr float R_BOTTOM_OHM = 20000.0f;

// Sampling interval in milliseconds.
// Possible lower bound with this serial CSV approach is about 10 ms (~100 Hz).
// Recommended minimum for stable logging while stepping is 20 ms (~50 Hz).
// Default: 50 ms (20 Hz).
static constexpr uint32_t SAMPLE_INTERVAL_MS = 50UL;

#endif
