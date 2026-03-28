#include "motor.h"
#include "config.h"

static bool motor_running = false;

// A4988 and DRV8825 require different minimum pulse widths.
static inline uint16_t step_pulse_us() {
#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8825)
	return 2;  // DRV8825 min is ~1.9us
#else
	return 1;  // A4988 min is ~1.0us
#endif
}

static inline void stepper_enable(bool enable) {
	// Both A4988 and DRV8825 use active-low ENABLE.
	digitalWrite(PIN_STEPPER_EN, enable ? LOW : HIGH);
}

static void stepper_pulse_steps(uint32_t steps) {
	const uint16_t pulse_us = step_pulse_us();
	for (uint32_t i = 0; i < steps; ++i) {
		digitalWrite(PIN_STEPPER_STEP, HIGH);
		delayMicroseconds(pulse_us);
		digitalWrite(PIN_STEPPER_STEP, LOW);
		delayMicroseconds(pulse_us);
	}
}

void motor_init() {
	pinMode(PIN_STEPPER_STEP, OUTPUT);
	pinMode(PIN_STEPPER_DIR, OUTPUT);
	pinMode(PIN_STEPPER_EN, OUTPUT);

	digitalWrite(PIN_STEPPER_STEP, LOW);
	digitalWrite(PIN_STEPPER_DIR, STEPPER_DEFAULT_DIR_CW ? HIGH : LOW);
	stepper_enable(false);
	motor_running = false;
}

void motor_on() {
	stepper_enable(true);
	motor_running = true;
}

void motor_off() {
#if (STEPPER_DISABLE_WHEN_IDLE)
	stepper_enable(false);
#endif
	motor_running = false;
}

bool motor_run_timed(uint32_t duration_ms) {
	if (duration_ms == 0) {
		return true;
	}

	motor_on();

	const uint32_t start_ms = millis();
	while ((millis() - start_ms) < duration_ms) {
		// Emit one step then pace to configured step frequency.
		stepper_pulse_steps(1);
		if (STEPPER_STEP_HZ > 0) {
			delayMicroseconds((uint32_t)(1000000UL / STEPPER_STEP_HZ));
		} else {
			yield();
		}
		yield();
	}

	motor_off();
	return true;
}

void motor_emergency_stop() {
	stepper_enable(false);
	digitalWrite(PIN_STEPPER_STEP, LOW);
	motor_running = false;
}

bool motor_is_running() {
	return motor_running;
}
