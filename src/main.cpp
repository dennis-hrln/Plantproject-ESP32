/**
 * main.cpp - ESP32 Single-Plant Automatic Watering System
 * 
 * SYSTEM OVERVIEW:
 * - Wakes periodically from deep sleep to check soil moisture
 * - Waters plant if humidity is below threshold
 * - Respects battery level and minimum watering interval
 * - Can also wake on button press for user interaction
 * 
 * WAKE CYCLE:
 * 1. Wake from deep sleep (timer or button)
 * 2. Initialize hardware
 * 3. Determine wake reason
 * 4. Execute appropriate action
 * 5. Return to deep sleep
 * 
 * POWER CONSUMPTION:
 * - Deep sleep: ~10 µA
 * - Active: ~50-80 mA (brief)
 * - Pump running: depends on pump, typically 100-500 mA
 */

#include <Arduino.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "soc/soc_caps.h"
#include "config.h"
#include "storage.h"
#include "sensor.h"
#include "battery.h"
#include "pump.h"
#include "watering.h"
#include "leds.h"
#include "buttons.h"
#include "water_level.h"

// =============================================================================
// WAKE REASON TRACKING
// =============================================================================

typedef enum {
    WAKE_TIMER,         // Woke from periodic timer
    WAKE_BUTTON,        // Woke from button press
    WAKE_POWER_ON,      // First boot / power cycle
    WAKE_UNKNOWN        // Unexpected wake source
} WakeReason;

/**
 * Determine why the ESP32 woke up.
 * Uses esp_sleep_get_wakeup_cause() API.
 */
WakeReason determine_wake_reason() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            return WAKE_TIMER;
            
        case ESP_SLEEP_WAKEUP_GPIO:      // ESP32-C3 GPIO deep/light sleep wake
#if SOC_PM_SUPPORT_EXT0_WAKEUP
        case ESP_SLEEP_WAKEUP_EXT0:      // Original ESP32 only
#endif
#if SOC_PM_SUPPORT_EXT1_WAKEUP
        case ESP_SLEEP_WAKEUP_EXT1:      // Original ESP32 only
#endif
            return WAKE_BUTTON;
            
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            // No wake cause = first boot or reset
            return WAKE_POWER_ON;
            
        default:
            return WAKE_UNKNOWN;
    }
}

// =============================================================================
// DEEP SLEEP CONFIGURATION
// =============================================================================

/**
 * Configure and enter deep sleep.
 * Sets up timer wake and GPIO wake sources.
 * 
 * @param sleep_seconds  How long to sleep; defaults to MEASUREMENT_INTERVAL_SEC.
 *                       Pass ALERT_INTERVAL_SEC for faster wake when alerts are active.
 *
 * Note: ESP32-C3 uses per-pin gpio_wakeup_enable() configuration
 */
void enter_deep_sleep(uint32_t sleep_seconds = MEASUREMENT_INTERVAL_SEC) {
    // Ensure pump is off before sleeping
    pump_emergency_stop();
    
    // Close NVS cleanly
    storage_close();
    
    // Turn off LEDs before sleep
    leds_all_off();
    
    // Configure timer wake (use provided interval)
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_seconds * SEC_TO_US);
    
    // Configure GPIO wake for buttons (ESP32-C3 deep sleep compatible)
    // ESP32-C3 does NOT support ext0/ext1 wake. It uses esp_deep_sleep_enable_gpio_wakeup()
    // which is available in ESP-IDF 5.0+ / Arduino ESP32 core 3.0+.
    uint64_t wake_mask = (1ULL << PIN_BTN_MAIN) | 
                         (1ULL << PIN_BTN_CAL_WET) | 
                         (1ULL << PIN_BTN_CAL_DRY);
    esp_deep_sleep_enable_gpio_wakeup(wake_mask, ESP_GPIO_WAKEUP_GPIO_LOW);
    
    // Hold GPIO pull-up configuration during deep sleep
    gpio_hold_en(PIN_BTN_MAIN);
    gpio_hold_en(PIN_BTN_CAL_WET);
    gpio_hold_en(PIN_BTN_CAL_DRY);
    gpio_deep_sleep_hold_en();
    
    // Enter deep sleep (function does not return)
    esp_deep_sleep_start();
}

// =============================================================================
// HARDWARE INITIALIZATION
// =============================================================================

/**
 * Initialize all hardware peripherals.
 * Called once after each wake from deep sleep.
 */
void init_hardware() {
    // Release GPIO holds from deep sleep so pins can be reconfigured
    gpio_hold_dis(PIN_BTN_MAIN);
    gpio_hold_dis(PIN_BTN_CAL_WET);
    gpio_hold_dis(PIN_BTN_CAL_DRY);
    gpio_deep_sleep_hold_dis();
    
    // Initialize storage first (needed by other modules)
    if (!storage_init()) {
        // NVS failure — flash error LED and continue with defaults
        led_red_blink(5, 100);
    }
    
    // Configure ADC once (shared by sensor + battery)
    analogReadResolution(ADC_RESOLUTION);
    analogSetAttenuation(ADC_ATTENUATION);
    
    // Initialize sensors and actuators
    sensor_init();
    battery_init();
    pump_init();
    watering_init();
    water_level_init();
    
    // Initialize LEDs and buttons using their modules
    leds_init();
    buttons_init();
}

// =============================================================================
// TIMER WAKE HANDLER
// =============================================================================

/**
 * Handle periodic timer wake.
 * Main purpose: check soil moisture and water if needed.
 * Also signals alerts for low water reservoir or low battery.
 */
void handle_timer_wake() {
    // Check water reservoir level
    bool reservoir_low = water_level_low();
    
    if (reservoir_low) {
        // Red LED: 3 slow blinks to signal low water
        led_red_blink(3, 800);
        delay(500);
    }
    
    // Quick battery check
    BatteryState batt = battery_get_state();
    
    if (batt == BATTERY_CRITICAL) {
        // Flash red LED to indicate critically low battery
        led_show_battery_critical();
        // Don't try to water, go back to sleep (alert interval)
        return;
    }
    
    if (batt == BATTERY_WARNING) {
        // Brief red LED warning
        led_show_battery_warning();
    }
    
    // Only attempt watering if reservoir has water
    if (reservoir_low) {
        // Skip watering entirely, alert already shown above
        return;
    }
    
    // Execute main watering logic
    WateringResult result = watering_check_and_execute();
    
    // LED feedback based on result
    switch (result) {
        case WATER_OK:
            // Watered successfully - green blinks
            led_show_success();
            break;
            
        case WATER_NOT_NEEDED:
            // Soil is moist enough - no indication needed
            break;
            
        case WATER_BATTERY_LOW:
            // Red blinks
            led_show_battery_warning();
            break;
            
        case WATER_RESERVOIR_LOW:
            // Already signalled above; no extra indication needed
            break;
            
        case WATER_TOO_SOON:
            // Too soon since last watering - no indication
            break;
            
        case WATER_SENSOR_ERROR:
            // Sensor error
            led_show_error();
            break;
            
        case WATER_PUMP_FAILED:
            // Pump error - long red
            led_red_blink(1, 1000);
            break;
    }
}

// =============================================================================
// BUTTON WAKE HANDLER
// =============================================================================

/**
 * Handle button press wake.
 * Delegates to button module for full interaction handling.
 */
void handle_button_wake() {
    buttons_handle_interaction(true);
}

// =============================================================================
// FIRST BOOT HANDLER
// =============================================================================

/**
 * Handle first power-on or reset.
 * Runs self-test and shows initial status.
 */
void handle_first_boot() {
    #ifdef DEBUG_SERIAL
    Serial.println("First boot - running initialization...");
    #endif
    
    // Visual indication of power on
    led_green_blink(2, 200);
    delay(300);
    
    // Check battery on first boot
    BatteryState batt = battery_get_state();
    if (batt == BATTERY_CRITICAL) {
        led_show_battery_critical();
    } else if (batt == BATTERY_WARNING) {
        led_show_battery_warning();
    }
    
    // Check sensor
    uint16_t raw = sensor_read_raw();
    if (!sensor_reading_valid(raw)) {
        led_show_error();
        #ifdef DEBUG_SERIAL
        Serial.println("WARNING: Sensor reading invalid!");
        #endif
    }
    
    // Show current humidity (convert from raw to avoid a second ADC read)
    uint8_t humidity = sensor_raw_to_humidity_percent(raw);
    #ifdef DEBUG_SERIAL
    Serial.print("Current humidity: ");
    Serial.print(humidity);
    Serial.println("%");
    #endif
    
    // Brief delay to show status
    delay(500);
    
    // Optionally do first watering check
    // Uncomment if you want auto-check on power-on:
    // handle_timer_wake();
}

// =============================================================================
// ARDUINO FRAMEWORK ENTRY POINTS
// =============================================================================

// Track wake start time for debug logging
static uint32_t wake_start_ms = 0;

void setup() {
    // Record wake time immediately
    wake_start_ms = millis();
    
    // Initialize serial for debugging (optional, uses power)
    #ifdef DEBUG_SERIAL
    Serial.begin(115200);
    delay(100);
    Serial.println("\n========================================");
    Serial.println("ESP32 Plant Watering System - Wake");
    Serial.println("========================================");
    #endif
    
    // Initialize all hardware
    init_hardware();
    
    // Determine why we woke up
    WakeReason reason = determine_wake_reason();
    
    #ifdef DEBUG_SERIAL
    Serial.print("Wake reason: ");
    switch(reason) {
        case WAKE_TIMER:    Serial.println("TIMER");    break;
        case WAKE_BUTTON:   Serial.println("BUTTON");   break;
        case WAKE_POWER_ON: Serial.println("POWER_ON"); break;
        default:            Serial.println("UNKNOWN");  break;
    }
    Serial.print("Boot count: ");
    Serial.println(storage_get_boot_count());
    Serial.print("Persistent time: ");
    Serial.print(storage_get_persistent_time() / 3600);
    Serial.println(" hours");
    #endif
    
    // Update persistent time tracking BEFORE handling wake reason.
    // This ensures watering logic sees the correct current time
    // (including the sleep duration that just elapsed).
    // NOTE: awake_duration is negligible (<1 s) vs. sleep (3600 s)
    // and is omitted here to keep time accurate for the watering check.
    uint32_t sleep_sec = (reason == WAKE_TIMER || reason == WAKE_BUTTON) 
                         ? MEASUREMENT_INTERVAL_SEC : 0;
    if (reason != WAKE_UNKNOWN) {
        storage_increment_boot_count(sleep_sec, 0);
    }

    // Handle based on wake reason
    switch (reason) {
        case WAKE_TIMER:
            handle_timer_wake();
            break;
            
        case WAKE_BUTTON:
            // Brief wake indicator is handled inside buttons_handle_interaction.
            handle_button_wake();
            break;
            
        case WAKE_POWER_ON:
            // First boot - run initialization and self-test
            handle_first_boot();
            break;
            
        case WAKE_UNKNOWN:
            // Unexpected - just go back to sleep
            #ifdef DEBUG_SERIAL
            Serial.println("Unknown wake reason, returning to sleep");
            #endif
            break;
    }
    
    // All done, go to deep sleep
    #ifdef DEBUG_SERIAL
    Serial.print("Awake for ");
    Serial.print(millis() - wake_start_ms);
    Serial.println(" ms");
    Serial.println("Entering deep sleep...");
    Serial.flush();
    #endif
    
    #if DEBUG_NO_SLEEP
    // Stay awake for button testing
    return;
    #else
    // Use shorter alert interval if water reservoir or battery needs attention
    bool need_alert = water_level_low() || 
                      (battery_get_state() != BATTERY_OK);
    enter_deep_sleep(need_alert ? ALERT_INTERVAL_SEC : MEASUREMENT_INTERVAL_SEC);
    #endif
}

void loop() {
    #if DEBUG_NO_SLEEP
    // Poll buttons while awake for troubleshooting.
    // buttons_handle_interaction() blocks for up to MODE_TIMEOUT_MS.
    buttons_handle_interaction();
    #else
    // This should never execute because we enter deep sleep in setup()
    enter_deep_sleep();
    #endif
}
