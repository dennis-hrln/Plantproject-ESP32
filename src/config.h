/**
 * config.h - All hardware pins and tunable constants
 * 
 * ESP32 Single-Plant Automatic Watering System
 * All configurable values in one place for easy tuning.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// HARDWARE PIN DEFINITIONS - ESP32-C3 Supermini (HW-466AB)
// =============================================================================
// ESP32-C3 has ADC1 on GPIO0-4, safe digital GPIO on 5-10, 18-21
// Avoid GPIO2, GPIO8, GPIO9 (strapping/boot pins)

// Soil moisture sensor (capacitive, analog output)
#define PIN_SOIL_SENSOR         GPIO_NUM_4    // ADC1_CH4, analog input

// Battery voltage via voltage divider
#define PIN_BATTERY_ADC         GPIO_NUM_3    // ADC1_CH3, analog input

// Pump control (N-MOSFET gate)
#define PIN_PUMP                GPIO_NUM_5    // Digital output to MOSFET

// LEDs
#define PIN_LED_GREEN           GPIO_NUM_6    // Status / humidity display
#define PIN_LED_RED             GPIO_NUM_7    // Low battery / error

// Buttons (connect to GND when pressed, internal pull-ups enabled)
#define PIN_BTN_MAIN            GPIO_NUM_0   // Main interaction button
#define PIN_BTN_CAL_WET         GPIO_NUM_1   // Wet calibration button
#define PIN_BTN_CAL_DRY         GPIO_NUM_2   // Dry calibration button

// Button wake-up mask for deep sleep (ext1)
#define BUTTON_WAKE_MASK        ((1ULL << PIN_BTN_MAIN) | (1ULL << PIN_BTN_CAL_WET) | (1ULL << PIN_BTN_CAL_DRY))

// =============================================================================
// ADC CONFIGURATION
// =============================================================================

#define ADC_RESOLUTION          12                  // 12-bit (0-4095)
#define ADC_MAX_VALUE           4095
#define ADC_ATTENUATION         ADC_11db            // Full 0-3.3V range
#define ADC_SAMPLES             16                  // Samples to average

// =============================================================================
// BATTERY THRESHOLDS (in millivolts)
// =============================================================================

// Voltage divider ratio: R1/(R1+R2), adjust based on your resistors
// Example: 100k / (100k + 100k) = 0.5 → multiply ADC voltage by 2
#define BATTERY_DIVIDER_RATIO   2.0

#define BATTERY_FULL_MV         4600    // Fully charged 
#define BATTERY_WARNING_MV      3600    // Show warning LED
#define BATTERY_CRITICAL_MV     3200    // Disable watering
#define BATTERY_EMPTY_MV        3000    // Absolute minimum

// =============================================================================
// SOIL SENSOR DEFAULTS (raw ADC values, will be overwritten by calibration)
// =============================================================================

// Capacitive sensors: higher value = drier soil (inverted)
#define DEFAULT_SENSOR_DRY      3200    // Raw ADC when sensor is in air
#define DEFAULT_SENSOR_WET      1400    // Raw ADC when sensor is in water

// =============================================================================
// WATERING PARAMETERS
// =============================================================================

#define DEFAULT_OPTIMAL_HUMIDITY    40      // Target humidity percentage (0-100)
#define PUMP_RUN_DURATION_MS        3000    // Pump on time per watering (3 sec)
#define PUMP_MAX_DURATION_MS        10000   // Absolute safety limit (10 sec)

// Minimum time between watering events (in seconds)
#define MIN_WATERING_INTERVAL_SEC   (3 * 60 * 60)   // 3 hours

// =============================================================================
// SLEEP / TIMING CONFIGURATION
// =============================================================================

// Temporary debug: keep device awake to test buttons
// Set to 1 to disable deep sleep and poll buttons in loop()
#define DEBUG_NO_SLEEP           0

// How often to wake and check soil moisture (in seconds)
#define MEASUREMENT_INTERVAL_SEC    (1 * 60 * 60)   // 1 hour

// Conversion factor for deep sleep (microseconds)
#define SEC_TO_US                   1000000ULL

// =============================================================================
// BUTTON TIMING (in milliseconds)
// =============================================================================

#define BTN_DEBOUNCE_MS             50      // Debounce time
#define BTN_LONG_PRESS_MS           2000    // Long press threshold
#define BTN_COMBO_WINDOW_MS         500     // Window for multi-button combo

// =============================================================================
// LED TIMING (in milliseconds)
// =============================================================================

#define LED_FLASH_LONG_MS           600     // Long flash for tens digit
#define LED_FLASH_SHORT_MS          200     // Short flash for ones digit
#define LED_PAUSE_MS                300     // Pause between flashes
#define LED_DIGIT_PAUSE_MS          1000    // Pause between digits

// =============================================================================
// NVS STORAGE KEYS
// =============================================================================

#define NVS_NAMESPACE               "plant"
#define NVS_KEY_SENSOR_DRY          "sensor_dry"
#define NVS_KEY_SENSOR_WET          "sensor_wet"
#define NVS_KEY_OPTIMAL_HUMIDITY    "opt_humid"
#define NVS_KEY_LAST_WATERING       "last_water"
#define NVS_KEY_BOOT_COUNT          "boot_count"
#define NVS_KEY_TOTAL_TIME          "total_time"

#endif // CONFIG_H
