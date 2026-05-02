/**
 * pump.cpp - Water pump control implementation
 */

#include "pump.h"
#include "config.h"

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
    
    #ifdef DEBUG_SERIAL
    Serial.println("[PUMP] Initialized");
    #endif
}

// =============================================================================
// BASIC CONTROL
// =============================================================================

void pump_on() {
    digitalWrite(PIN_PUMP, HIGH);  // HIGH = MOSFET on = pump runs
    pump_running = true;
    
    #ifdef DEBUG_SERIAL
    Serial.println("[PUMP] Turned ON");
    #endif
}

void pump_off() {
    digitalWrite(PIN_PUMP, LOW);   // LOW = MOSFET off = pump stops
    pump_running = false;
    
    #ifdef DEBUG_SERIAL
    Serial.println("[PUMP] Turned OFF");
    #endif
}

// =============================================================================
// TIMED OPERATION
// =============================================================================

bool pump_run_timed(uint32_t duration_ms) {
    // Enforce safety maximum
    if (duration_ms > PUMP_MAX_DURATION_MS) {
        #ifdef DEBUG_SERIAL
        Serial.print("[PUMP] Duration capped to ");
        Serial.print(PUMP_MAX_DURATION_MS);
        Serial.println(" ms");
        #endif
        duration_ms = PUMP_MAX_DURATION_MS;
    }

    #ifdef DEBUG_SERIAL
    Serial.print("[PUMP] Running for ");
    Serial.print(duration_ms);
    Serial.println(" ms");
    #endif

    // Start pump
    pump_on();

    // Wait for duration (simple blocking delay)
    uint32_t start_time = millis();

    while ((millis() - start_time) < duration_ms) {
        yield();
    }

    // Stop pump
    pump_off();

    #ifdef DEBUG_SERIAL
    Serial.println("[PUMP] Run complete");
    #endif

    return true;
}

// =============================================================================
// SAFETY
// =============================================================================

void pump_emergency_stop() {
    // Immediately stop pump, no checks
    digitalWrite(PIN_PUMP, LOW);
    pump_running = false;
    
    #ifdef DEBUG_SERIAL
    Serial.println("[PUMP] EMERGENCY STOP");
    #endif
}

bool pump_is_running() {
    return pump_running;
}
