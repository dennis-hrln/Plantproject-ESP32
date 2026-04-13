/**
 * stepper_motor_test.cpp
 *
 * Stepper diagnostic test:
 * - prints clear serial instructions
 * - enables the driver so you can feel holding torque
 * - runs slow stepping to confirm STEP/DIR wiring
 * - repeats forever with a 15s ON / 5s OFF cycle
 *
 * Toggle SERIAL_DEBUG_ENABLED at the top of the file.
 */

#include <Arduino.h>
#include "config.h"

#include "measure_voltage.h"

// 1) Serial enabled: 1=true, 0=false
#define SERIAL_ENABLED 1

// 2) Voltage measure mode: 1=true, 0=false
// If enabled, serial output is reserved for measure_voltage DATA lines.
#define VOLTAGE_MEASURE 1

// Derived output modes.
#define SERIAL_DEBUG_ACTIVE   (SERIAL_ENABLED && !(VOLTAGE_MEASURE))
#define SERIAL_VOLTAGE_ACTIVE (SERIAL_ENABLED && (VOLTAGE_MEASURE))

#define SERIAL_BAUD 115200

// Set to 1 to try both enable polarities during startup.
// Leave at 0 for normal A4988/DRV8825 active-low enable behavior.
#define TRY_BOTH_ENABLE_POLARITIES 0

#if SERIAL_DEBUG_ACTIVE
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINTLN(...) do { } while (0)
#define DEBUG_PRINTF(...) do { } while (0)
#endif

static constexpr uint32_t RUN_MS = 15000UL;
static constexpr uint32_t OFF_MS = 5000UL;
static constexpr uint32_t STEP_HZ_TEST = STEPPER_STEP_HZ;
static constexpr uint32_t HOLD_TEST_MS = 3000UL;
static constexpr uint32_t BOOT_DELAY_MS = 500UL;

static inline uint16_t step_pulse_us() {
#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8825)
    return 2;  // DRV8825 min step pulse is about 1.9 us
#else
    return 2;  // A4988 is usually happier with 2 us on ESP32
#endif
}

static inline void stepper_enable(bool enable) {
    // A4988/DRV8825 enable is active LOW.
    digitalWrite(PIN_STEPPER_EN, enable ? LOW : HIGH);
}

static inline void stepper_enable_raw(bool enable_level_high) {
    digitalWrite(PIN_STEPPER_EN, enable_level_high ? HIGH : LOW);
}

static void stepper_step_once(uint16_t pulse_us, uint32_t low_delay_us) {
    digitalWrite(PIN_STEPPER_STEP, HIGH);
    delayMicroseconds(pulse_us);
    digitalWrite(PIN_STEPPER_STEP, LOW);
    delayMicroseconds(pulse_us);

    if (low_delay_us > 0) {
        delayMicroseconds(low_delay_us);
    }
}

void setup() {
#if SERIAL_ENABLED
    Serial.begin(SERIAL_BAUD);
    delay(200);
#endif

#if SERIAL_DEBUG_ACTIVE
    DEBUG_PRINTLN("Stepper diagnostic test starting");
    DEBUG_PRINTF("STEP pin=%d, DIR pin=%d, EN pin=%d\n", (int)PIN_STEPPER_STEP, (int)PIN_STEPPER_DIR, (int)PIN_STEPPER_EN);
    DEBUG_PRINTF("Run=%lu ms, Off=%lu ms, StepHz=%lu\n", (unsigned long)RUN_MS, (unsigned long)OFF_MS, (unsigned long)STEP_HZ_TEST);
    DEBUG_PRINTLN("Check: driver VMOT/VDD/GND connected, motor coils on A1/A2 and B1/B2, and SLEEP/RESET held HIGH.");
#endif

    pinMode(PIN_STEPPER_STEP, OUTPUT);
    pinMode(PIN_STEPPER_DIR, OUTPUT);
    pinMode(PIN_STEPPER_EN, OUTPUT);

    digitalWrite(PIN_STEPPER_STEP, LOW);
    digitalWrite(PIN_STEPPER_DIR, STEPPER_DEFAULT_DIR_CW ? HIGH : LOW);
    stepper_enable(false);

#if SERIAL_VOLTAGE_ACTIVE
    voltage_measure_init();
#endif

    delay(BOOT_DELAY_MS);
}

void loop() {
    
    const uint16_t pulse_us = step_pulse_us();
    const uint32_t step_period_us = (STEP_HZ_TEST > 0) ? (1000000UL / STEP_HZ_TEST) : 0UL;
    const uint32_t gap_us = (step_period_us > (2UL * pulse_us)) ? (step_period_us - (2UL * pulse_us)) : 0UL;

#if SERIAL_DEBUG_ACTIVE
    DEBUG_PRINTLN("\nPhase 1: enable driver only");
    DEBUG_PRINTLN("Feel the motor shaft by hand. If it gets hard to turn, driver power and enable are working.");
#endif

    stepper_enable(true);
    delay(HOLD_TEST_MS);

#if SERIAL_DEBUG_ACTIVE
    DEBUG_PRINTLN("Phase 2: slow stepping for motion check");
    DEBUG_PRINTLN("If it holds torque but does not move, check STEP/DIR wiring, coil wiring, and current limit.");
#endif

    if (TRY_BOTH_ENABLE_POLARITIES) {
        DEBUG_PRINTLN("Trying alternate enable polarity for diagnosis.");
        stepper_enable_raw(false);
        delay(500);
        stepper_enable_raw(true);
        delay(500);
    }

    const uint32_t on_start = millis();
    while ((millis() - on_start) < RUN_MS) {
        stepper_step_once(pulse_us, gap_us);
#if SERIAL_VOLTAGE_ACTIVE
        voltage_measure_maybe_emit(true, millis());
#endif
        yield();
    }

#if SERIAL_DEBUG_ACTIVE
    DEBUG_PRINTLN("Motor OFF");
    DEBUG_PRINTLN("If the shaft is still hard to turn while OFF, EN may be wired wrong or the driver may be stuck enabled.");
#endif
    stepper_enable(false);
    const uint32_t off_start = millis();
    while ((millis() - off_start) < OFF_MS) {
#if SERIAL_VOLTAGE_ACTIVE
        voltage_measure_maybe_emit(false, millis());
#endif
        delay(5);
        yield();
    }
}
