#include "motor.h"
#include "config.h"

static bool motor_running = false;

#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
static uint8_t drv8833_phase_idx = 0;
static int8_t drv8833_phase_delta = 1;

enum Drv8833BridgeCmd : uint8_t {
	DRV8833_BRIDGE_COAST = 0,
	DRV8833_BRIDGE_FWD,
	DRV8833_BRIDGE_REV,
};

typedef struct {
	Drv8833BridgeCmd a;
	Drv8833BridgeCmd b;
} Drv8833Phase;

// Half-step sequence for bipolar stepper on dual H-bridge.
static const Drv8833Phase DRV8833_HALFSTEP_SEQ[] = {
	{DRV8833_BRIDGE_FWD,   DRV8833_BRIDGE_COAST},
	{DRV8833_BRIDGE_FWD,   DRV8833_BRIDGE_FWD},
	{DRV8833_BRIDGE_COAST, DRV8833_BRIDGE_FWD},
	{DRV8833_BRIDGE_REV,   DRV8833_BRIDGE_FWD},
	{DRV8833_BRIDGE_REV,   DRV8833_BRIDGE_COAST},
	{DRV8833_BRIDGE_REV,   DRV8833_BRIDGE_REV},
	{DRV8833_BRIDGE_COAST, DRV8833_BRIDGE_REV},
	{DRV8833_BRIDGE_FWD,   DRV8833_BRIDGE_REV},
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
	const uint8_t seq_len = (uint8_t)(sizeof(DRV8833_HALFSTEP_SEQ) / sizeof(DRV8833_HALFSTEP_SEQ[0]));
	const Drv8833Phase phase = DRV8833_HALFSTEP_SEQ[idx % seq_len];

	// Short coast break-before-make helps prevent harsh direction transitions.
	drv8833_set_all_coast();
	delayMicroseconds(20);
	drv8833_write_bridge(PIN_DRV8833_AN1, PIN_DRV8833_AN2, phase.a);
	drv8833_write_bridge(PIN_DRV8833_BN1, PIN_DRV8833_BN2, phase.b);
}

static inline void drv8833_step_once() {
	const uint8_t seq_len = (uint8_t)(sizeof(DRV8833_HALFSTEP_SEQ) / sizeof(DRV8833_HALFSTEP_SEQ[0]));
	if (drv8833_phase_delta > 0) {
		drv8833_phase_idx = (uint8_t)((drv8833_phase_idx + 1U) % seq_len);
	} else {
		drv8833_phase_idx = (uint8_t)((drv8833_phase_idx + seq_len - 1U) % seq_len);
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
		drv8833_set_all_coast();
	} else {
		// Give bridge logic time to wake from standby.
		delayMicroseconds(100);
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
	drv8833_set_all_coast();
	digitalWrite(PIN_DRV8833_STBY, LOW);
	#ifdef DEBUG_SERIAL
	Serial.println("[MOTOR] DRV8833 initialized");
	#endif
	#else
	pinMode(PIN_STEPPER_STEP, OUTPUT);
	pinMode(PIN_STEPPER_DIR, OUTPUT);
	pinMode(PIN_STEPPER_EN, OUTPUT);

	digitalWrite(PIN_STEPPER_STEP, LOW);
	digitalWrite(PIN_STEPPER_DIR, STEPPER_DEFAULT_DIR_CW ? HIGH : LOW);
	stepper_enable(false);
	#ifdef DEBUG_SERIAL
	Serial.println("[MOTOR] Stepper driver initialized");
	#endif
	#endif
	motor_running = false;
}

void motor_on() {
	stepper_enable(true);
	#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
	drv8833_apply_phase(drv8833_phase_idx);
	#endif
	motor_running = true;
	#ifdef DEBUG_SERIAL
	Serial.println("[MOTOR] Enabled");
	#endif
}

void motor_off() {
#if (MOTOR_DISABLE_WHEN_IDLE)
	stepper_enable(false);
#endif
	motor_running = false;
	#ifdef DEBUG_SERIAL
	Serial.println("[MOTOR] Disabled");
	#endif
}

bool motor_run_timed(uint32_t duration_ms) {
	if (duration_ms == 0) {
		#ifdef DEBUG_SERIAL
		Serial.println("[MOTOR] Zero duration requested");
		#endif
		return true;
	}

	#ifdef DEBUG_SERIAL
	Serial.print("[MOTOR] Running for ");
	Serial.print(duration_ms);
	Serial.println(" ms");
	#endif

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
	
	#ifdef DEBUG_SERIAL
	Serial.println("[MOTOR] Run complete");
	#endif
	
	return true;
}

void motor_emergency_stop() {
	stepper_enable(false);
	#if (STEPPER_DRIVER_TYPE == STEPPER_DRIVER_DRV8833)
	drv8833_set_all_coast();
	#else
	digitalWrite(PIN_STEPPER_STEP, LOW);
	#endif
	motor_running = false;
	
	#ifdef DEBUG_SERIAL
	Serial.println("[MOTOR] EMERGENCY STOP");
	#endif
}

bool motor_is_running() {
	return motor_running;
}
