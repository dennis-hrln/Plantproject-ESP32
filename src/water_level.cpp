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
    return !water_level_low_stable();
}

bool water_level_low() {
    // Switch closed (water low) → pin pulled LOW
    return water_level_low_stable();
}
