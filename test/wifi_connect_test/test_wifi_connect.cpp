/**
 * test_wifi_connect.cpp
 *
 * Standalone Wi-Fi connection test for ESP32-C3.
 *
 * Behavior:
 * - On boot, tries to connect to Wi-Fi from secrets.h
 * - While connecting: short red pulses
 * - On success: success LED pattern, then green heartbeat every 2s
 * - On failure: error LED pattern, wait, retry
 */

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

static void leds_init_local() {
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);
}

static void blink_green(uint8_t count, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0; i < count; ++i) {
        digitalWrite(PIN_LED_GREEN, HIGH);
        delay(on_ms);
        digitalWrite(PIN_LED_GREEN, LOW);
        delay(off_ms);
    }
}

static void blink_red(uint8_t count, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0; i < count; ++i) {
        digitalWrite(PIN_LED_RED, HIGH);
        delay(on_ms);
        digitalWrite(PIN_LED_RED, LOW);
        delay(off_ms);
    }
}

static void show_success() {
    blink_green(3, 120, 120);
}

static void show_error() {
    blink_red(3, 180, 120);
}

static bool connect_wifi_and_signal(uint32_t timeout_ms) {
    WiFi.mode(WIFI_STA);
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(MQTT_WIFI_SSID, MQTT_WIFI_PASSWORD);
    }

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        blink_red(1, 120, 120);
        delay(120);
    }

    if (WiFi.status() == WL_CONNECTED) {
        show_success();
        return true;
    }

    show_error();
    return false;
}

void setup() {
    leds_init_local();
    delay(200);
}

void loop() {
    const bool connected = connect_wifi_and_signal(15000);

    if (connected) {
        // Keep a visible "connected" heartbeat.
        blink_green(1, 120, 120);
        delay(1880);
        return;
    }

    // Retry after a short pause.
    delay(2500);
}