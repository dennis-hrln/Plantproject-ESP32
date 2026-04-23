#include "motor.h"
#include "config.h"

static bool motor_running = false;

#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
static uint8_t drv8833_phase_idx = 0;
static int8_t drv8833_phase_delta = 1;

static inline void drv8833_set_all_low() {
	digitalWrite(PIN_DRV8833_AN1, LOW);
	digitalWrite(PIN_DRV8833_AN2, LOW);
	digitalWrite(PIN_DRV8833_BN1, LOW);
	digitalWrite(PIN_DRV8833_BN2, LOW);
}

static inline void drv8833_apply_phase(uint8_t idx) {
	switch (idx & 0x03U) {
		case 0:
			digitalWrite(PIN_DRV8833_AN1, HIGH);
			digitalWrite(PIN_DRV8833_AN2, LOW);
			digitalWrite(PIN_DRV8833_BN1, HIGH);
			digitalWrite(PIN_DRV8833_BN2, LOW);
			break;
		case 1:
			digitalWrite(PIN_DRV8833_AN1, LOW);
			digitalWrite(PIN_DRV8833_AN2, HIGH);
			digitalWrite(PIN_DRV8833_BN1, HIGH);
			digitalWrite(PIN_DRV8833_BN2, LOW);
			break;
		case 2:
			digitalWrite(PIN_DRV8833_AN1, LOW);
			digitalWrite(PIN_DRV8833_AN2, HIGH);
			digitalWrite(PIN_DRV8833_BN1, LOW);
			digitalWrite(PIN_DRV8833_BN2, HIGH);
			break;
		default:
			digitalWrite(PIN_DRV8833_AN1, HIGH);
			digitalWrite(PIN_DRV8833_AN2, LOW);
			digitalWrite(PIN_DRV8833_BN1, LOW);
			digitalWrite(PIN_DRV8833_BN2, HIGH);
			break;
	}
}

static inline void drv8833_step_once() {
	if (drv8833_phase_delta > 0) {
		drv8833_phase_idx = (uint8_t)((drv8833_phase_idx + 1U) & 0x03U);
	} else {
		drv8833_phase_idx = (uint8_t)((drv8833_phase_idx + 3U) & 0x03U);
	}
	drv8833_apply_phase(drv8833_phase_idx);
}
#endif

// Driver families can require slightly different minimum STEP pulse widths.
static inline uint16_t step_pulse_us() {
#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8825)
	return 2;  // DRV8825 min is ~1.9us
#else
	return 1;  // A4988 min is ~1.0us
#endif
}

static inline void stepper_enable(bool enable) {
	#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
	digitalWrite(PIN_DRV8833_STBY, enable ? HIGH : LOW);
	if (!enable) {
		drv8833_set_all_low();
	}
	#else
	// A4988/DRV8825 use active-low ENABLE.
	digitalWrite(PIN_STEPPER_EN, enable ? LOW : HIGH);
	#endif
}

static void stepper_pulse_steps(uint32_t steps) {
	#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
	for (uint32_t i = 0; i < steps; ++i) {
		drv8833_step_once();
	}
	#else
	const uint16_t pulse_us = step_pulse_us();
	for (uint32_t i = 0; i < steps; ++i) {
		digitalWrite(PIN_STEPPER_STEP, HIGH);
		delayMicroseconds(pulse_us);
		digitalWrite(PIN_STEPPER_STEP, LOW);
		delayMicroseconds(pulse_us);
	}
	#endif
}

void motor_init() {
	#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
	pinMode(PIN_DRV8833_AN1, OUTPUT);
	pinMode(PIN_DRV8833_AN2, OUTPUT);
	pinMode(PIN_DRV8833_BN1, OUTPUT);
	pinMode(PIN_DRV8833_BN2, OUTPUT);
	pinMode(PIN_DRV8833_STBY, OUTPUT);

	drv8833_phase_idx = 0;
	drv8833_phase_delta = STEPPER_DEFAULT_DIR_CW ? 1 : -1;
	drv8833_set_all_low();
	digitalWrite(PIN_DRV8833_STBY, LOW);
	#else
	pinMode(PIN_STEPPER_STEP, OUTPUT);
	pinMode(PIN_STEPPER_DIR, OUTPUT);
	pinMode(PIN_STEPPER_EN, OUTPUT);

	digitalWrite(PIN_STEPPER_STEP, LOW);
	digitalWrite(PIN_STEPPER_DIR, STEPPER_DEFAULT_DIR_CW ? HIGH : LOW);
	stepper_enable(false);
	#endif
	motor_running = false;
}

void motor_on() {
	stepper_enable(true);
	#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
	drv8833_apply_phase(drv8833_phase_idx);
	#endif
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
	#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
	drv8833_set_all_low();
	#else
	digitalWrite(PIN_STEPPER_STEP, LOW);
	#endif
	motor_running = false;
}

bool motor_is_running() {
	return motor_running;
}
