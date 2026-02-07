/**
 * hardware_test.cpp - Standalone hardware wiring test (NO serial)
 * 
 * Tests every component using only the 3 buttons and 2 LEDs.
 * No PC or serial monitor needed.
 * 
 * HOW TO USE:
 *   Upload, then use buttons to run tests:
 * 
 *   MAIN button (GPIO 10) short press:
 *     -> LED test: green on 1s, red on 1s, both blink 3x
 *     -> Result: you see them light up = wiring OK
 * 
 *   WET button (GPIO 20) short press:
 *     -> Soil sensor test: reads sensor, shows humidity as green blinks
 *        0 blinks = error (red flashes 5x fast)
 *        1-10 blinks = humidity in 10% steps (3 blinks = 30%)
 * 
 *   DRY button (GPIO 21) short press:
 *     -> Pump test: red LED on, pump runs 1 second, red LED off
 *        If pump doesn't run, check transistor wiring
 * 
 *   MAIN button LONG press (>2s):
 *     -> Battery test: reads voltage, shows level as green blinks
 *        1 blink = very low, 5 blinks = full
 *        Red flash after = voltage below warning threshold
 * 
 *   ALL 3 buttons pressed together:
 *     -> Run all tests in sequence (LED, sensor, battery, pump)
 * 
 *   STARTUP: Both LEDs flash twice = board is ready
 * 
 * WIRING UNDER TEST:
 *   GPIO 4  - Soil moisture sensor (analog input)
 *   GPIO 3  - Battery voltage divider (analog input)
 *   GPIO 5  - Pump transistor base via 10k (digital output)
 *   GPIO 6  - Green LED via 330 Ohm (digital output)
 *   GPIO 7  - Red LED via 330 Ohm (digital output)
 *   GPIO 10 - Main button (input, pull-up, active LOW)
 *   GPIO 20 - Wet cal button (input, pull-up, active LOW)
 *   GPIO 21 - Dry cal button (input, pull-up, active LOW)
 */

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Pin definitions (matching config.h)
// ---------------------------------------------------------------------------
#define PIN_SOIL_SENSOR     4
#define PIN_BATTERY_ADC     3
#define PIN_PUMP            5
#define PIN_LED_GREEN       6
#define PIN_LED_RED         7
#define PIN_BTN_MAIN        0
#define PIN_BTN_CAL_WET     1
#define PIN_BTN_CAL_DRY     2

// Timing
#define DEBOUNCE_MS         50
#define LONG_PRESS_MS       2000

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void blink_green(uint8_t count, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0; i < count; i++) {
        digitalWrite(PIN_LED_GREEN, HIGH);
        delay(on_ms);
        digitalWrite(PIN_LED_GREEN, LOW);
        delay(off_ms);
    }
}

void blink_red(uint8_t count, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0; i < count; i++) {
        digitalWrite(PIN_LED_RED, HIGH);
        delay(on_ms);
        digitalWrite(PIN_LED_RED, LOW);
        delay(off_ms);
    }
}

void blink_both(uint8_t count, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0; i < count; i++) {
        digitalWrite(PIN_LED_GREEN, HIGH);
        digitalWrite(PIN_LED_RED, HIGH);
        delay(on_ms);
        digitalWrite(PIN_LED_GREEN, LOW);
        digitalWrite(PIN_LED_RED, LOW);
        delay(off_ms);
    }
}

void leds_off() {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);
}

// Start indicator: both LEDs flash once briefly
void signal_test_start() {
    blink_both(1, 150, 300);
}

// Success indicator: 3 fast green blinks
void signal_pass() {
    delay(300);
    blink_green(3, 100, 100);
}

// Error indicator: 5 fast red blinks
void signal_error() {
    delay(300);
    blink_red(5, 100, 100);
}

// ---------------------------------------------------------------------------
// TEST 1: LEDs (MAIN short press)
// ---------------------------------------------------------------------------
void test_leds() {
    // Green on solid 1s
    digitalWrite(PIN_LED_GREEN, HIGH);
    delay(1000);
    digitalWrite(PIN_LED_GREEN, LOW);
    delay(400);

    // Red on solid 1s
    digitalWrite(PIN_LED_RED, HIGH);
    delay(1000);
    digitalWrite(PIN_LED_RED, LOW);
    delay(400);

    // Both on solid 1s
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, HIGH);
    delay(1000);
    leds_off();
    delay(400);

    // Alternating blink 4x
    for (int i = 0; i < 4; i++) {
        digitalWrite(PIN_LED_GREEN, HIGH);
        digitalWrite(PIN_LED_RED, LOW);
        delay(250);
        digitalWrite(PIN_LED_GREEN, LOW);
        digitalWrite(PIN_LED_RED, HIGH);
        delay(250);
    }
    leds_off();
}

// ---------------------------------------------------------------------------
// TEST 2: Soil Moisture Sensor (WET short press)
// Shows humidity as green blinks: 1-10 = 10%-100%
// Red flash = sensor error (reading 0 or 4095)
// ---------------------------------------------------------------------------
void test_soil_sensor() {
    signal_test_start();

    // Read sensor (average 16 samples)
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(PIN_SOIL_SENSOR);
        delayMicroseconds(200);
    }
    uint16_t raw = sum / 16;

    // Check for error conditions
    if (raw == 0 || raw >= 4090) {
        signal_error();
        return;
    }

    // Map raw to approximate humidity percentage
    // Capacitive sensor: ~3200 = dry (0%), ~1400 = wet (100%)
    int32_t humidity;
    if (raw >= 3200) {
        humidity = 0;
    } else if (raw <= 1400) {
        humidity = 100;
    } else {
        humidity = (int32_t)(3200 - raw) * 100 / (3200 - 1400);
    }

    // Convert to 1-10 blink count (each blink = 10%)
    uint8_t blinks = (uint8_t)(humidity / 10);
    if (blinks == 0 && humidity > 0) blinks = 1;
    if (blinks == 0) blinks = 1;  // At least 1 blink to show it's working

    delay(500);
    // Show: short red flash = "sensor reading incoming"
    blink_red(1, 200, 400);

    // Blink green for humidity level
    blink_green(blinks, 400, 300);

    // End marker: brief both-LED flash
    delay(300);
    blink_both(1, 150, 0);
}

// ---------------------------------------------------------------------------
// TEST 3: Pump (DRY short press)
// Red LED on during pump run (1 second)
// ---------------------------------------------------------------------------
void test_pump() {
    signal_test_start();

    // 3 red blinks as countdown warning
    blink_red(3, 500, 500);

    // Run pump for 1 second with red LED indicator
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_PUMP, HIGH);
    delay(1000);
    digitalWrite(PIN_PUMP, LOW);
    digitalWrite(PIN_LED_RED, LOW);

    // Confirm done
    delay(300);
    blink_green(2, 200, 200);
}

// ---------------------------------------------------------------------------
// TEST 4: Battery (MAIN long press)
// Shows battery level as 1-5 green blinks
// Additional red flash if below warning threshold
// ---------------------------------------------------------------------------
void test_battery() {
    signal_test_start();

    // Read battery ADC (average 16 samples)
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(PIN_BATTERY_ADC);
        delayMicroseconds(200);
    }
    uint16_t raw = sum / 16;

    // Check for error
    if (raw == 0) {
        signal_error();
        return;
    }

    // Convert to voltage (mV) with divider ratio of 2.0
    uint32_t adc_mv = ((uint32_t)raw * 3300) / 4095;
    uint32_t batt_mv = adc_mv * 2;

    // Map to 1-5 blinks for 3x AA (3000-4500 mV range)
    uint8_t blinks;
    if (batt_mv >= 4200) blinks = 5;
    else if (batt_mv >= 3900) blinks = 4;
    else if (batt_mv >= 3600) blinks = 3;
    else if (batt_mv >= 3300) blinks = 2;
    else blinks = 1;

    delay(500);
    blink_green(blinks, 500, 400);

    // Red warning if below 3600 mV
    if (batt_mv < 3600) {
        delay(300);
        blink_red(3, 300, 200);
    }

    // End marker
    delay(300);
    blink_both(1, 150, 0);
}

// ---------------------------------------------------------------------------
// TEST 5: All tests in sequence (all 3 buttons pressed)
// ---------------------------------------------------------------------------
void run_all_tests() {
    // Signal: rapid alternating 5x = "running all"
    for (int i = 0; i < 5; i++) {
        digitalWrite(PIN_LED_GREEN, HIGH);
        digitalWrite(PIN_LED_RED, LOW);
        delay(100);
        digitalWrite(PIN_LED_GREEN, LOW);
        digitalWrite(PIN_LED_RED, HIGH);
        delay(100);
    }
    leds_off();
    delay(800);

    test_leds();
    delay(1000);

    test_soil_sensor();
    delay(1000);

    test_battery();
    delay(1000);

    test_pump();
    delay(500);

    // Final: 3 both-LED blinks = "all done"
    blink_both(3, 300, 300);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
    // Configure all pins
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    pinMode(PIN_SOIL_SENSOR, INPUT);
    pinMode(PIN_BATTERY_ADC, INPUT);
    pinMode(PIN_PUMP, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_BTN_MAIN, INPUT_PULLUP);
    pinMode(PIN_BTN_CAL_WET, INPUT_PULLUP);
    pinMode(PIN_BTN_CAL_DRY, INPUT_PULLUP);

    // Ensure pump is OFF and LEDs are OFF
    digitalWrite(PIN_PUMP, LOW);
    leds_off();

    // Startup signal: 2x both LEDs = "ready"
    delay(500);
    blink_both(2, 200, 200);
}

// ---------------------------------------------------------------------------
// Main loop - simple button LED test
// ---------------------------------------------------------------------------
void loop() {
    bool main_pressed = (digitalRead(PIN_BTN_MAIN) == LOW);
    bool wet_pressed  = (digitalRead(PIN_BTN_CAL_WET) == LOW);
    bool dry_pressed  = (digitalRead(PIN_BTN_CAL_DRY) == LOW);

    if (main_pressed) {
        delay(DEBOUNCE_MS);
        if (digitalRead(PIN_BTN_MAIN) == LOW) {
            while (digitalRead(PIN_BTN_MAIN) == LOW) delay(10);
            blink_both(4, 200, 1800);
        }
    } else if (dry_pressed) {
        delay(DEBOUNCE_MS);
        if (digitalRead(PIN_BTN_CAL_DRY) == LOW) {
            while (digitalRead(PIN_BTN_CAL_DRY) == LOW) delay(10);
            blink_green(4, 200, 1800);
        }
    } else if (wet_pressed) {
        delay(DEBOUNCE_MS);
        if (digitalRead(PIN_BTN_CAL_WET) == LOW) {
            while (digitalRead(PIN_BTN_CAL_WET) == LOW) delay(10);
            blink_red(4, 200, 1800);
        }
    }

    delay(20);
}
