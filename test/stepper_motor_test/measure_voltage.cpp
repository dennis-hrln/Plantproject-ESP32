#include <Arduino.h>
#include "measure_voltage.h"
#include "measure_voltage_config.h"

static constexpr float MEASURE_DIVIDER_RATIO = (R_TOP_OHM + R_BOTTOM_OHM) / R_BOTTOM_OHM;

static uint32_t s_next_sample_ms = 0;

static inline float adc_mv_to_measured_v(uint32_t adc_mv) {
	return (adc_mv * MEASURE_DIVIDER_RATIO) / 1000.0f;
}

static void emit_sample(uint32_t now_ms, bool motor_on) {
	const int raw = analogRead((int)MEASURE_ADC_PIN);
	const uint32_t mv = (uint32_t)analogReadMilliVolts((int)MEASURE_ADC_PIN);
	const float measured_v = adc_mv_to_measured_v(mv);

	// CSV line format for host parser:
	// DATA,<esp_ms>,<motor_on>,<adc_raw>,<adc_mv>,<measured_v>
	Serial.printf("DATA,%lu,%u,%d,%lu,%.4f\n",
			  (unsigned long)now_ms,
			  motor_on ? 1U : 0U,
			  raw,
			  (unsigned long)mv,
			  measured_v);
}

void voltage_measure_init() {
	analogReadResolution(12);
	analogSetPinAttenuation((int)MEASURE_ADC_PIN, ADC_11db);
	s_next_sample_ms = millis();

	Serial.println("# measure_voltage ready");
	Serial.printf("# adc_pin=%d sample_ms=%lu divider_ratio=%.3f r_top=%.0f r_bottom=%.0f\n",
			  (int)MEASURE_ADC_PIN,
			  (unsigned long)SAMPLE_INTERVAL_MS,
			  (double)MEASURE_DIVIDER_RATIO,
			  (double)R_TOP_OHM,
			  (double)R_BOTTOM_OHM);
	Serial.println("# csv_header: DATA,esp_ms,motor_on,adc_raw,adc_mv,measured_v");
}

void voltage_measure_maybe_emit(bool motor_on, uint32_t now_ms) {
	if ((int32_t)(now_ms - s_next_sample_ms) < 0) {
		return;
	}
	emit_sample(now_ms, motor_on);
	s_next_sample_ms += SAMPLE_INTERVAL_MS;
}
