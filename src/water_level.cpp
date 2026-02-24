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
}

// =============================================================================
// STATUS
// =============================================================================

bool water_level_ok() {
    // Switch open (enough water) → pin pulled HIGH
    return digitalRead(PIN_WATER_LEVEL) == HIGH;
}

bool water_level_low() {
    // Switch closed (water low) → pin pulled LOW
    return digitalRead(PIN_WATER_LEVEL) == LOW;
}
