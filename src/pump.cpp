/**
 * pump.cpp - Water pump control implementation
 */

#include "pump.h"
#include "config.h"
#include "battery.h"

// Track pump state
static bool pump_running = false;

// =============================================================================
// INITIALIZATION
// =============================================================================

void pump_init() {
    // Configure pump control pin as output
    pinMode(PIN_PUMP, OUTPUT);
    
    // Ensure pump starts OFF (LOW = MOSFET off = pump off)
    digitalWrite(PIN_PUMP, LOW);
    pump_running = false;
}

// =============================================================================
// BASIC CONTROL
// =============================================================================

void pump_on() {
    digitalWrite(PIN_PUMP, HIGH);  // HIGH = MOSFET on = pump runs
    pump_running = true;
}

void pump_off() {
    digitalWrite(PIN_PUMP, LOW);   // LOW = MOSFET off = pump stops
    pump_running = false;
}

// =============================================================================
// TIMED OPERATION
// =============================================================================

bool pump_run_timed(uint32_t duration_ms) {
    // Enforce safety maximum
    if (duration_ms > PUMP_MAX_DURATION_MS) {
        duration_ms = PUMP_MAX_DURATION_MS;
    }
    
    // Check battery before starting
    if (!battery_watering_allowed()) {
        return false;
    }
    
    // Start pump
    pump_on();
    
    // Wait for duration (simple blocking delay)
    // Could be improved with non-blocking approach if needed
    uint32_t start_time = millis();
    
    while ((millis() - start_time) < duration_ms) {
        // Periodically check battery during operation
        if (!battery_watering_allowed()) {
            pump_emergency_stop();
            return false;
        }
        yield();  // let watchdog / background tasks run
    }
    
    // Stop pump
    pump_off();
    
    return true;
}

// =============================================================================
// SAFETY
// =============================================================================

void pump_emergency_stop() {
    // Immediately stop pump, no checks
    digitalWrite(PIN_PUMP, LOW);
    pump_running = false;
}

bool pump_is_running() {
    return pump_running;
}
