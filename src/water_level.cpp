/**
 * water_level.cpp - Water reservoir level monitoring implementation
 * 
 * Reads a 3D-printed float switch connected between PIN_WATER_LEVEL and GND.
 * Internal pull-up keeps pin HIGH when switch is open (water OK).
 * When water drops below threshold, the float closes the switch → pin goes LOW.
 */

#include "water_level.h"
#include "config.h"

// =============================================================================
// INITIALIZATION
// =============================================================================

void water_level_init() {
    pinMode(PIN_WATER_LEVEL, INPUT_PULLUP);
    
    #ifdef DEBUG_SERIAL
    Serial.println("[WATER_LEVEL] Initialized");
    #endif
}

static bool water_level_low_stable() {
    uint8_t low_count = 0;

    for (uint8_t i = 0; i < 5; ++i) {
        if (digitalRead(PIN_WATER_LEVEL) == LOW) {
            low_count++;
        }
        delay(1);
    }

    return low_count >= 3;
}

// =============================================================================
// STATUS
// =============================================================================

bool water_level_ok() {
    // Switch open (enough water) → pin pulled HIGH
    bool status = !water_level_low_stable();
    
    #ifdef DEBUG_SERIAL
    Serial.print("[WATER_LEVEL] Status: ");
    Serial.println(status ? "OK" : "LOW");
    #endif
    
    return status;
}

bool water_level_low() {
    // Switch closed (water low) → pin pulled LOW
    bool status = water_level_low_stable();
    
    #ifdef DEBUG_SERIAL
    Serial.print("[WATER_LEVEL] Check result: ");
    Serial.println(status ? "LOW" : "OK");
    #endif
    
    return status;
}
