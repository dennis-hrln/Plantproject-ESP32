/**
 * motor.h - Stepper motor backend (A4988 / DRV8825 / DRV8833)
 *
 * Independent from pump backend and used only when
 * ACTUATOR_TYPE == ACTUATOR_TYPE_STEPPER.
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

// Initialize stepper driver pins.
void motor_init();

// Turn stepper output on/off.
// motor_on() enables driver; motor_off() disables/idles based on config.
void motor_on();
void motor_off();

// Run stepper for a fixed duration (ms) by generating step pulses.
bool motor_run_timed(uint32_t duration_ms);

// Emergency stop: immediate output disable/off.
void motor_emergency_stop();

// Current run state maintained by motor backend.
bool motor_is_running();

#endif // MOTOR_H
