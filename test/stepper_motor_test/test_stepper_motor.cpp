/**
 * stepper_motor_test.cpp
 *
 * Stepper diagnostic test:
 * - prints clear serial instructions
 * - runs slow stepping for 5s
 * - runs fast stepping for 5s
 * - turns motor output off for 5s
 * - in DRV8833 mode, uses the fixed A1/A2/B1/B2 pin assignment
 * - repeats forever
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
#define VOLTAGE_MEASURE 0

// Derived output modes.
#define SERIAL_DEBUG_ACTIVE   (SERIAL_ENABLED && !(VOLTAGE_MEASURE))
#define SERIAL_VOLTAGE_ACTIVE (SERIAL_ENABLED && (VOLTAGE_MEASURE))

#define SERIAL_BAUD 115200

// Set to 1 to try both enable polarities during startup.
// Leave at 0 for normal A4988/DRV8825/DRV8833 active-low enable behavior.
#define TRY_BOTH_ENABLE_POLARITIES 0

#if SERIAL_DEBUG_ACTIVE
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINTLN(...) do { } while (0)
#define DEBUG_PRINTF(...) do { } while (0)
#endif

static constexpr uint32_t PHASE_MS = 5000UL;
static constexpr uint32_t OFF_MS = PHASE_MS;
#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
// DRV8833 + pump load is more reliable with conservative startup speeds.
static constexpr uint32_t STEP_HZ_SLOW_TEST = 1U;
static constexpr uint32_t STEP_HZ_FAST_TEST = 12U;
#else
static constexpr uint32_t STEP_HZ_FAST_TEST = (STEPPER_STEP_HZ > 0U) ? STEPPER_STEP_HZ : 400U;
static constexpr uint32_t STEP_HZ_SLOW_TEST = (STEP_HZ_FAST_TEST >= 4U) ? (STEP_HZ_FAST_TEST / 4U) : 1U;
#endif
static constexpr uint32_t BOOT_DELAY_MS = 500UL;

#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
static uint8_t drv8833_phase_idx = 0;
static int8_t drv8833_phase_delta = 1;

typedef struct {
    gpio_num_t a1;
    gpio_num_t a2;
    gpio_num_t b1;
    gpio_num_t b2;
} Drv8833PinMap;
static Drv8833PinMap drv8833_map = {
    PIN_DRV8833_AN2,
    PIN_DRV8833_AN1,
    PIN_DRV8833_BN2,
    PIN_DRV8833_BN1,
};

enum Drv8833BridgeCmd : uint8_t {
    DRV8833_BRIDGE_COAST = 0,
    DRV8833_BRIDGE_FWD,
    DRV8833_BRIDGE_REV,
};

typedef struct {
    Drv8833BridgeCmd a;
    Drv8833BridgeCmd b;
} Drv8833Phase;

// Full-step sequence (both coils energized) for maximum torque.
static const Drv8833Phase DRV8833_FULLSTEP_SEQ[] = {
    {DRV8833_BRIDGE_FWD, DRV8833_BRIDGE_FWD},
    {DRV8833_BRIDGE_REV, DRV8833_BRIDGE_FWD},
    {DRV8833_BRIDGE_REV, DRV8833_BRIDGE_REV},
    {DRV8833_BRIDGE_FWD, DRV8833_BRIDGE_REV},
};

static void drv8833_advance_case() {
    // Case testing removed; keep the verified wiring mapping fixed.
}

static void drv8833_log_case() {
    DEBUG_PRINTF("DRV8833 wiring: A1=GPIO%d A2=GPIO%d B1=GPIO%d B2=GPIO%d\n",
                 (int)drv8833_map.a1,
                 (int)drv8833_map.a2,
                 (int)drv8833_map.b1,
                 (int)drv8833_map.b2);
}

static void drv8833_reset_map() {
    drv8833_map.a1 = PIN_DRV8833_AN2;
    drv8833_map.a2 = PIN_DRV8833_AN1;
    drv8833_map.b1 = PIN_DRV8833_BN2;
    drv8833_map.b2 = PIN_DRV8833_BN1;
}

static inline void drv8833_set_all_outputs_low() {
    digitalWrite(drv8833_map.a1, LOW);
    digitalWrite(drv8833_map.a2, LOW);
    digitalWrite(drv8833_map.b1, LOW);
    digitalWrite(drv8833_map.b2, LOW);
}

static inline void drv8833_write_bridge(gpio_num_t in1, gpio_num_t in2, Drv8833BridgeCmd cmd) {
    switch (cmd) {
        case DRV8833_BRIDGE_FWD:
            digitalWrite(in1, HIGH);
            digitalWrite(in2, LOW);
            break;
        case DRV8833_BRIDGE_REV:
            digitalWrite(in1, LOW);
            digitalWrite(in2, HIGH);
            break;
        default:
            digitalWrite(in1, LOW);
            digitalWrite(in2, LOW);
            break;
    }
}

static inline void drv8833_set_all_coast() {
    drv8833_write_bridge(drv8833_map.a1, drv8833_map.a2, DRV8833_BRIDGE_COAST);
    drv8833_write_bridge(drv8833_map.b1, drv8833_map.b2, DRV8833_BRIDGE_COAST);
}

static inline void drv8833_apply_phase(uint8_t idx) {
    const uint8_t seq_len = (uint8_t)(sizeof(DRV8833_FULLSTEP_SEQ) / sizeof(DRV8833_FULLSTEP_SEQ[0]));
    const Drv8833Phase phase = DRV8833_FULLSTEP_SEQ[idx % seq_len];

    // Short coast break-before-make helps prevent harsh direction transitions.
    drv8833_set_all_coast();
    delayMicroseconds(20);
    drv8833_write_bridge(drv8833_map.a1, drv8833_map.a2, phase.a);
    drv8833_write_bridge(drv8833_map.b1, drv8833_map.b2, phase.b);
}

static inline void drv8833_step_once() {
    const uint8_t seq_len = (uint8_t)(sizeof(DRV8833_FULLSTEP_SEQ) / sizeof(DRV8833_FULLSTEP_SEQ[0]));
    if (drv8833_phase_delta > 0) {
        drv8833_phase_idx = (uint8_t)((drv8833_phase_idx + 1U) % seq_len);
    } else {
        drv8833_phase_idx = (uint8_t)((drv8833_phase_idx + seq_len - 1U) % seq_len);
    }
    drv8833_apply_phase(drv8833_phase_idx);
}
#endif

static inline uint16_t step_pulse_us() {
#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8825)
    return 2;  // DRV8825 min step pulse is about 1.9 us
#else
    return 2;  // A4988 is usually happier with 2 us on ESP32
#endif
}

static inline void stepper_enable(bool enable) {
    #if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
    if (!enable) {
        drv8833_set_all_coast();
    }
    digitalWrite(PIN_DRV8833_STBY, enable ? HIGH : LOW);
    if (enable) {
        // Wake-up settling time before applying coil phases.
        delayMicroseconds(100);
    }
    #else
    // A4988/DRV8825 STEP-DIR breakouts usually use active LOW enable.
    digitalWrite(PIN_STEPPER_EN, enable ? LOW : HIGH);
    #endif
}

static inline void stepper_enable_raw(bool enable_level_high) {
    #if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
    digitalWrite(PIN_DRV8833_STBY, enable_level_high ? HIGH : LOW);
    #else
    digitalWrite(PIN_STEPPER_EN, enable_level_high ? HIGH : LOW);
    #endif
}

static void stepper_step_once(uint16_t pulse_us, uint32_t low_delay_us) {
    #if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
    (void)pulse_us;
    drv8833_step_once();
    #else
    digitalWrite(PIN_STEPPER_STEP, HIGH);
    delayMicroseconds(pulse_us);
    digitalWrite(PIN_STEPPER_STEP, LOW);
    delayMicroseconds(pulse_us);
    #endif

    if (low_delay_us > 0) {
        delayMicroseconds(low_delay_us);
    }
}

static void run_step_phase(uint32_t duration_ms, uint32_t step_hz, uint16_t pulse_us) {
    const uint32_t step_period_us = (step_hz > 0U) ? (1000000UL / step_hz) : 0U;
    #if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
    const uint32_t gap_us = step_period_us;
    #else
    const uint32_t gap_us = (step_period_us > (2UL * pulse_us)) ? (step_period_us - (2UL * pulse_us)) : 0U;
    #endif

    const uint32_t phase_start = millis();
    while ((millis() - phase_start) < duration_ms) {
        stepper_step_once(pulse_us, gap_us);
#if SERIAL_VOLTAGE_ACTIVE
        voltage_measure_maybe_emit(true, millis());
#endif
        yield();
    }
}

void setup() {
#if SERIAL_ENABLED
    Serial.begin(SERIAL_BAUD);
    delay(200);
#endif

#if SERIAL_DEBUG_ACTIVE
    DEBUG_PRINTLN("Stepper diagnostic test starting");
    #if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
    DEBUG_PRINTF("Fixed DRV8833 mapping: A1=GPIO%d A2=GPIO%d B1=GPIO%d B2=GPIO%d\n",
                 (int)PIN_DRV8833_AN2,
                 (int)PIN_DRV8833_AN1,
                 (int)PIN_DRV8833_BN2,
                 (int)PIN_DRV8833_BN1);
    DEBUG_PRINTF("STBY pin=%d\n", (int)PIN_DRV8833_STBY);
    #else
    DEBUG_PRINTF("STEP pin=%d, DIR pin=%d, EN pin=%d\n", (int)PIN_STEPPER_STEP, (int)PIN_STEPPER_DIR, (int)PIN_STEPPER_EN);
    #endif
    DEBUG_PRINTF("Slow=%lu Hz for %lu ms, Fast=%lu Hz for %lu ms, Off=%lu ms\n",
                 (unsigned long)STEP_HZ_SLOW_TEST,
                 (unsigned long)PHASE_MS,
                 (unsigned long)STEP_HZ_FAST_TEST,
                 (unsigned long)PHASE_MS,
                 (unsigned long)OFF_MS);
    DEBUG_PRINTLN("Sequence repeats forever:");
    DEBUG_PRINTLN("  1) Slow stepping");
    DEBUG_PRINTLN("  2) Fast stepping");
    DEBUG_PRINTLN("  3) Motor OFF");
    DEBUG_PRINTLN("Check: driver VMOT/VDD/GND connected, motor coils on A+/A- and B+/B-.");
#endif

    #if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
    pinMode(PIN_DRV8833_AN2, OUTPUT);
    pinMode(PIN_DRV8833_AN1, OUTPUT);
    pinMode(PIN_DRV8833_BN2, OUTPUT);
    pinMode(PIN_DRV8833_BN1, OUTPUT);
    pinMode(PIN_DRV8833_STBY, OUTPUT);

    drv8833_reset_map();
    drv8833_phase_idx = 0;
    drv8833_phase_delta = STEPPER_DEFAULT_DIR_CW ? 1 : -1;
    drv8833_set_all_outputs_low();
    drv8833_set_all_coast();
    digitalWrite(PIN_DRV8833_STBY, LOW);
    #if SERIAL_DEBUG_ACTIVE
    drv8833_log_case();
    #endif
    #else
    pinMode(PIN_STEPPER_STEP, OUTPUT);
    pinMode(PIN_STEPPER_DIR, OUTPUT);
    pinMode(PIN_STEPPER_EN, OUTPUT);

    digitalWrite(PIN_STEPPER_STEP, LOW);
    digitalWrite(PIN_STEPPER_DIR, STEPPER_DEFAULT_DIR_CW ? HIGH : LOW);
    #endif
    stepper_enable(false);

#if SERIAL_VOLTAGE_ACTIVE
    voltage_measure_init();
#endif

    delay(BOOT_DELAY_MS);
}

void loop() {
    const uint16_t pulse_us = step_pulse_us();

#if SERIAL_DEBUG_ACTIVE
    #if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
    DEBUG_PRINTLN("");
    drv8833_log_case();
    #endif
#endif

    stepper_enable(true);

#if SERIAL_DEBUG_ACTIVE
    DEBUG_PRINTLN("\nPhase 1/3: slow stepping");
#endif
    run_step_phase(PHASE_MS, STEP_HZ_SLOW_TEST, pulse_us);

#if SERIAL_DEBUG_ACTIVE
    DEBUG_PRINTLN("Phase 2/3: fast stepping");
    #if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
    DEBUG_PRINTLN("If it does not move, check AN/BN wiring, motor coils, and supply voltage.");
    #else
    DEBUG_PRINTLN("If it holds torque but does not move, check STEP/DIR wiring, coil wiring, and current limit.");
    #endif
#endif
    run_step_phase(PHASE_MS, STEP_HZ_FAST_TEST, pulse_us);

    if (TRY_BOTH_ENABLE_POLARITIES) {
        DEBUG_PRINTLN("Trying alternate enable polarity for diagnosis.");
        stepper_enable_raw(false);
        delay(100);
        stepper_enable_raw(true);
        delay(100);
    }

#if SERIAL_DEBUG_ACTIVE
    DEBUG_PRINTLN("Phase 3/3: motor OFF");
    #if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
    DEBUG_PRINTLN("If shaft remains strongly held while OFF, check STBY wiring and enable logic.");
    #else
    DEBUG_PRINTLN("If the shaft is still hard to turn while OFF, EN may be wired wrong or the driver may be stuck enabled.");
    #endif
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




