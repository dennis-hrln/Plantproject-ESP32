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

// Water level float switch (connects to GND when water is below threshold)
#define PIN_WATER_LEVEL         GPIO_NUM_10  // Float switch, active LOW = water LOW

// Buttons (connect to GND when pressed, internal pull-ups enabled)
#define PIN_BTN_MAIN            GPIO_NUM_0   // Main interaction button
#define PIN_BTN_CAL_WET         GPIO_NUM_2   // Wet calibration button
// WARNING: GPIO 2 is a strapping pin (MTMS) on ESP32-C3.
// If this button is held during power-on/reset, the chip enters download mode.
// Consider moving to GPIO 10 or another non-strapping GPIO if intermittent
// boot failures occur.
#define PIN_BTN_CAL_DRY         GPIO_NUM_1   // Dry calibration button

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

// 3× Alkaline AA: fresh ~4.8V (3×1.6V), nominal 4.5V (3×1.5V), dead ~2.7V (3×0.9V)
#define BATTERY_FULL_MV         4500    // Nominal fresh (3 × 1.5V)
#define BATTERY_WARNING_MV      3600    // Getting low  (3 × 1.2V)
#define BATTERY_CRITICAL_MV     3000    // Disable watering (3 × 1.0V)
#define BATTERY_EMPTY_MV        2700    // Dead (3 × 0.9V)

// =============================================================================
// SOIL SENSOR DEFAULTS (raw ADC values, will be overwritten by calibration)
// =============================================================================

// Capacitive sensors: higher value = drier soil (inverted)
#define DEFAULT_SENSOR_DRY      3200    // Raw ADC when sensor is in dry earth
#define DEFAULT_SENSOR_WET      1400    // Raw ADC when sensor is in water

// =============================================================================
// WATERING PARAMETERS
// =============================================================================

#define DEFAULT_MINIMAL_HUMIDITY    40      // Start watering below this % (0-100)
#define DEFAULT_MAX_HUMIDITY        70      // Stop watering at this % (0-100)

#define PUMP_RUN_DURATION_MS        3000    // Pump on time per pulse (3 sec)
#define PUMP_MAX_DURATION_MS        10000   // Absolute safety limit per pulse (10 sec)
#define SOAK_WAIT_TIME_MS           1* 60 *1000   // Wait after each pump pulse for water to soak (1 min)
#define MAX_PUMP_PULSES             8       // Max pump pulses per watering cycle

// Minimum time between watering cycles (in seconds)
#define MIN_WATERING_INTERVAL_SEC   (3 * 60 * 60)   // 3 hours

// =============================================================================
// SLEEP / TIMING CONFIGURATION
// =============================================================================

// Temporary debug: keep device awake to test buttons
// Set to 1 to disable deep sleep and poll buttons in loop()
#define DEBUG_NO_SLEEP           0

// How often to wake and check soil moisture (in seconds)
#define MEASUREMENT_INTERVAL_SEC    (1 * 60 * 60)   // 1 hour

// How often to wake and flash LEDs when water or battery is low (in seconds)
#define ALERT_INTERVAL_SEC          (15 * 60)        // 15 minutes

// Conversion factor for deep sleep (microseconds)
#define SEC_TO_US                   1000000ULL


// =============================================================================
// CALIBRATION TIMING (in milliseconds)
// =============================================================================

#define SENSOR_CALIBRATION_TIME_MS  15000   // Default: 15 seconds

// =============================================================================
// BUTTON TIMING (in milliseconds)
// =============================================================================

#define BTN_DEBOUNCE_MS             50      // Debounce time
#define BTN_LONG_PRESS_MS           2000    // Long press threshold
#define MODE_TIMEOUT_MS             8000    // Mode timeout before returning to general
#define HUMIDITY_STEP               5       // Minimal humidity adjustment step (%)

// =============================================================================
// LED TIMING (in milliseconds)
// =============================================================================

// --- Base durations ---
#define LED_LONG           1200    // Long flash for tens digit
#define LED_SHORT          400     // Short flash for ones digit
#define LED_RAPID          200     // Shared rapid blink for status patterns
#define LED_PAUSE_MS                400     // Pause between flashes
#define LED_DIGIT_PAUSE_MS          1500    // Pause between tens and ones digit

// --- Number display (humidity / battery percentage) ---
#define LED_NUMBER_START_MS         100     // Start indicator (both LEDs on)

// =============================================================================
// LED PATTERN SYSTEM
// =============================================================================
//
// Format per pattern:
//   1) PAT_<NAME>[]            step list
//   2) PAT_<NAME>_PAUSE_MS     pause between steps
//   3) PAT_<NAME>_GAP_MS       pause after pattern ends

typedef struct { uint16_t green_ms; uint16_t red_ms; } LedStep;

#define LED_G(ms)    {(ms), 0}
#define LED_R(ms)    {0, (ms)}
#define LED_GR(ms)   {(ms), (ms)}
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#define PAT_PAUSE_DEFAULT   LED_PAUSE_MS
#define PAT_GAP_NONE        0
#define PAT_GAP_WATER_LOW   500
#define PAT_GAP_BOOT        300

// Water low alert (red, long)
static const LedStep PAT_WATER_LOW[] = {
	LED_R(LED_RAPID),
	LED_R(LED_RAPID),
	LED_R(LED_RAPID),
    LED_R(LED_RAPID),
    LED_R(LED_RAPID),
    LED_R(LED_RAPID),
	LED_R(LED_RAPID),
	LED_R(LED_RAPID),
    LED_R(LED_RAPID),
    LED_R(LED_RAPID),
};
#define PAT_WATER_LOW_PAUSE_MS   PAT_PAUSE_DEFAULT
#define PAT_WATER_LOW_GAP_MS     PAT_GAP_WATER_LOW

// Battery warning (red)
static const LedStep PAT_BATT_WARN[] = {
	LED_R(LED_SHORT),
	LED_R(LED_SHORT),
};
#define PAT_BATT_WARN_PAUSE_MS   PAT_PAUSE_DEFAULT
#define PAT_BATT_WARN_GAP_MS     PAT_GAP_NONE

// Battery critical (red, rapid)
static const LedStep PAT_BATT_CRIT[] = {
	LED_R(LED_SHORT),
	LED_R(LED_SHORT),
	LED_R(LED_SHORT),
	LED_R(LED_SHORT),
	LED_R(LED_SHORT),
};
#define PAT_BATT_CRIT_PAUSE_MS   PAT_PAUSE_DEFAULT
#define PAT_BATT_CRIT_GAP_MS     PAT_GAP_NONE

// Generic error (red)
static const LedStep PAT_ERROR[] = {
	LED_R(LED_RAPID),
	LED_R(LED_RAPID),
	LED_R(LED_RAPID),
};
#define PAT_ERROR_PAUSE_MS       PAT_PAUSE_DEFAULT
#define PAT_ERROR_GAP_MS         PAT_GAP_NONE

// Success / confirmation (green)
static const LedStep PAT_SUCCESS[] = {
	LED_G(LED_RAPID),
	LED_G(LED_RAPID),
};
#define PAT_SUCCESS_PAUSE_MS     PAT_PAUSE_DEFAULT
#define PAT_SUCCESS_GAP_MS       PAT_GAP_NONE

// Pump failed (single long red)
static const LedStep PAT_PUMP_FAIL[] = {
	LED_R(LED_LONG),
};
#define PAT_PUMP_FAIL_PAUSE_MS   PAT_PAUSE_DEFAULT
#define PAT_PUMP_FAIL_GAP_MS     PAT_GAP_NONE

// Storage init failed (red, rapid)
static const LedStep PAT_NVS_FAIL[] = {
	LED_R(LED_RAPID),
	LED_R(LED_RAPID),
	LED_R(LED_RAPID),
	LED_R(LED_RAPID),
	LED_R(LED_RAPID),
};
#define PAT_NVS_FAIL_PAUSE_MS    PAT_PAUSE_DEFAULT
#define PAT_NVS_FAIL_GAP_MS      PAT_GAP_NONE

// Boot indicator (green)
static const LedStep PAT_BOOT[] = {
	LED_G(LED_RAPID),
	LED_G(LED_RAPID),
};
#define PAT_BOOT_PAUSE_MS        PAT_PAUSE_DEFAULT
#define PAT_BOOT_GAP_MS          PAT_GAP_BOOT

// Calibration confirm (green -> red -> green)
static const LedStep PAT_CAL_CONFIRM[] = {
	LED_G(LED_RAPID),
	LED_R(LED_RAPID),
	LED_G(LED_RAPID),
};
#define PAT_CAL_CONFIRM_PAUSE_MS PAT_GAP_NONE
#define PAT_CAL_CONFIRM_GAP_MS   PAT_GAP_NONE

// Button acknowledge (green)
static const LedStep PAT_BTN_ACK[] = {
	LED_G(LED_RAPID),
	LED_G(LED_RAPID),
};
#define PAT_BTN_ACK_PAUSE_MS     PAT_PAUSE_DEFAULT
#define PAT_BTN_ACK_GAP_MS       PAT_GAP_NONE

// Button invalid combo (red)
static const LedStep PAT_BTN_BAD[] = {
	LED_R(LED_RAPID),
};
#define PAT_BTN_BAD_PAUSE_MS     PAT_PAUSE_DEFAULT
#define PAT_BTN_BAD_GAP_MS       PAT_GAP_NONE

// Number display end indicator (double flash)
static const LedStep PAT_NUM_END[] = {
	LED_GR(LED_RAPID),
	LED_GR(LED_RAPID),
};
#define PAT_NUM_END_PAUSE_MS     LED_RAPID
#define PAT_NUM_END_GAP_MS       PAT_GAP_NONE

// =============================================================================
// NVS STORAGE KEYS
// =============================================================================

#define NVS_NAMESPACE               "plant"
#define NVS_KEY_SENSOR_DRY          "sensor_dry"
#define NVS_KEY_SENSOR_WET          "sensor_wet"
#define NVS_KEY_MINIMAL_HUMIDITY    "min_humid"
#define NVS_KEY_LAST_WATERING       "last_water"
#define NVS_KEY_BOOT_COUNT          "boot_count"
#define NVS_KEY_TOTAL_TIME          "total_time"

#endif // CONFIG_H
