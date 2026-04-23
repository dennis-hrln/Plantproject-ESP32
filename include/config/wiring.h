#ifndef CONFIG_WIRING_H
#define CONFIG_WIRING_H

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
#define PIN_WATER_LEVEL         GPIO_NUM_10   // Float switch, active LOW = water LOW

// Buttons (connect to GND when pressed, internal pull-ups enabled)
#define PIN_BTN_MAIN            GPIO_NUM_0    // Main interaction button
#define PIN_BTN_CAL_WET         GPIO_NUM_2    // Wet calibration button
// WARNING: GPIO 2 is a strapping pin (MTMS) on ESP32-C3.
// If this button is held during power-on/reset, the chip enters download mode.
// Consider moving to GPIO 10 or another non-strapping GPIO if intermittent
// boot failures occur.
#define PIN_BTN_CAL_DRY         GPIO_NUM_1    // Dry calibration button

// Button wake-up mask for deep sleep (ext1)
#define BUTTON_WAKE_MASK        ((1ULL << PIN_BTN_MAIN) | (1ULL << PIN_BTN_CAL_WET) | (1ULL << PIN_BTN_CAL_DRY))

// Select per-driver wiring mapping by configured stepper driver type.
#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_A4988)
#include "wiring/wiring_a4988.h"
#elif (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8825)
#include "wiring/wiring_a4988.h"
#elif (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
#include "wiring/wiring_DRV988.h"
#else
#error "No wiring mapping available for selected STEPPER_DRIVER_TYPE."
#endif

#endif // CONFIG_WIRING_H
