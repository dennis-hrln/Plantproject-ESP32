/**
 * stepper_motor_test.cpp
 *
 * Simple stepper test:
 * - ON for 15 seconds (step pulses generated)
 * - OFF for 5 seconds
 * - repeats forever
 */

#include <Arduino.h>
#include "config.h"

static inline uint16_t step_pulse_us() {
#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8825)
    return 2;  // DRV8825 min step pulse ~1.9us
#else
    return 1;  // A4988 min step pulse ~1.0us
#endif
}

static inline void stepper_enable(bool enable) {
    // A4988/DRV8825: EN is active LOW
    digitalWrite(PIN_STEPPER_EN, enable ? LOW : HIGH);
}

void setup() {
    pinMode(PIN_STEPPER_STEP, OUTPUT);
    pinMode(PIN_STEPPER_DIR, OUTPUT);
    pinMode(PIN_STEPPER_EN, OUTPUT);

    digitalWrite(PIN_STEPPER_STEP, LOW);
    digitalWrite(PIN_STEPPER_DIR, STEPPER_DEFAULT_DIR_CW ? HIGH : LOW);
    stepper_enable(false);
}

void loop() {
    // ON: run motor for 15s
    stepper_enable(true);

    const uint32_t on_start = millis();
    while ((millis() - on_start) < 15000UL) {
        digitalWrite(PIN_STEPPER_STEP, HIGH);
        delayMicroseconds(step_pulse_us());
        digitalWrite(PIN_STEPPER_STEP, LOW);
        delayMicroseconds(step_pulse_us());

        if (STEPPER_STEP_HZ > 0) {
            delayMicroseconds((uint32_t)(1000000UL / STEPPER_STEP_HZ));
        } else {
            yield();
        }
        yield();
    }

    // OFF: disable motor for 5s
    stepper_enable(false);
    delay(5000);
}
