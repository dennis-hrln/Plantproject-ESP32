/**
 * stepper_motor_test.cpp
 *
 * Simple DRV8833-only stepper diagnostic test:
 * - runs slow stepping for 5s
 * - runs fast stepping for 5s
 * - turns motor output off for 5s
 * - repeats forever
 */

#include <Arduino.h>

#define SERIAL_BAUD 115200

static constexpr uint32_t PHASE_MS = 5000UL;
static constexpr uint32_t OFF_MS = PHASE_MS;
static constexpr uint32_t BOOT_DELAY_MS = 500UL;
 uint32_t STEP_HZ_TEST = 50U;


static uint8_t drv8833_phase_idx = 0;
static int8_t drv8833_phase_delta = 1;

#define PIN_DRV8833_AN2          GPIO_NUM_21
#define PIN_DRV8833_AN1          GPIO_NUM_20
#define PIN_DRV8833_BN2          GPIO_NUM_9
#define PIN_DRV8833_BN1          GPIO_NUM_8
#define PIN_DRV8833_STBY         GPIO_NUM_5

enum Drv8833BridgeCmd : uint8_t {
    DRV8833_BRIDGE_COAST = 0,
    DRV8833_BRIDGE_FWD,
    DRV8833_BRIDGE_REV,
};

typedef struct {
    Drv8833BridgeCmd a;
    Drv8833BridgeCmd b;
} Drv8833Phase;

static const Drv8833Phase DRV8833_FULLSTEP_SEQ[] = {
    {DRV8833_BRIDGE_FWD, DRV8833_BRIDGE_FWD},
    {DRV8833_BRIDGE_REV, DRV8833_BRIDGE_FWD},
    {DRV8833_BRIDGE_REV, DRV8833_BRIDGE_REV},
    {DRV8833_BRIDGE_FWD, DRV8833_BRIDGE_REV},
};

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
    drv8833_write_bridge(PIN_DRV8833_AN1, PIN_DRV8833_AN2, DRV8833_BRIDGE_COAST);
    drv8833_write_bridge(PIN_DRV8833_BN1, PIN_DRV8833_BN2, DRV8833_BRIDGE_COAST);
}

static inline void drv8833_apply_phase(uint8_t idx) {
    const uint8_t seq_len = (uint8_t)(sizeof(DRV8833_FULLSTEP_SEQ) / sizeof(DRV8833_FULLSTEP_SEQ[0]));
    const Drv8833Phase phase = DRV8833_FULLSTEP_SEQ[idx % seq_len];

    drv8833_set_all_coast();
    delayMicroseconds(20);
    drv8833_write_bridge(PIN_DRV8833_AN1, PIN_DRV8833_AN2, phase.a);
    drv8833_write_bridge(PIN_DRV8833_BN1, PIN_DRV8833_BN2, phase.b);
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

static inline void stepper_enable(bool enable) {
    if (!enable) {
        drv8833_set_all_coast();
    }
    digitalWrite(PIN_DRV8833_STBY, enable ? HIGH : LOW);
    if (enable) {
        delayMicroseconds(100);
    }
}

static void stepper_step_once(uint32_t low_delay_us) {
    drv8833_step_once();
    if (low_delay_us > 0) {
        delayMicroseconds(low_delay_us);
    }
}

static void run_step_phase(uint32_t duration_ms, uint32_t step_hz) {
    const uint32_t step_period_us = (step_hz > 0U) ? (1000000UL / step_hz) : 0U;
    const uint32_t gap_us = step_period_us;

    const uint32_t phase_start = millis();
    while ((millis() - phase_start) < duration_ms) {
        stepper_step_once(gap_us);
        yield();
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(200);

    Serial.println("Stepper diagnostic test starting");
    Serial.println("Phase 1: slow stepping");
    Serial.println("Phase 2: fast stepping");
    Serial.println("Phase 3: motor off");

    pinMode(PIN_DRV8833_AN2, OUTPUT);
    pinMode(PIN_DRV8833_AN1, OUTPUT);
    pinMode(PIN_DRV8833_BN2, OUTPUT);
    pinMode(PIN_DRV8833_BN1, OUTPUT);
    pinMode(PIN_DRV8833_STBY, OUTPUT);

    drv8833_phase_idx = 0;
    drv8833_phase_delta = 1;
    drv8833_set_all_coast();
    digitalWrite(PIN_DRV8833_STBY, LOW);
    stepper_enable(false);

    delay(BOOT_DELAY_MS);
}

void loop() {
    
    STEP_HZ_TEST = STEP_HZ_TEST + 50U;
    stepper_enable(true);


    run_step_phase(PHASE_MS, STEP_HZ_TEST);

    stepper_enable(false);
    const uint32_t off_start = millis();
    while ((millis() - off_start) < OFF_MS) {
        delay(5);
        yield();
    }
}




