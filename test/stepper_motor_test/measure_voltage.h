#ifndef STEPPER_MEASURE_VOLTAGE_H
#define STEPPER_MEASURE_VOLTAGE_H

#include <Arduino.h>

void voltage_measure_init();
void voltage_measure_maybe_emit(bool motor_on, uint32_t now_ms);

#endif
