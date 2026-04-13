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

// Set to 1 to enable serial debug output, 0 to disable.
#ifndef WIFI_TEST_SERIAL
#define WIFI_TEST_SERIAL 1
#endif

#if WIFI_TEST_SERIAL
#define DBG_BEGIN(baud) Serial.begin(baud)
#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINTLN(x) Serial.println(x)
#define DBG_PRINTF(fmt, ...) Serial.printf((fmt), ##__VA_ARGS__)
#else
#define DBG_BEGIN(baud) do { (void)(baud); } while (0)
#define DBG_PRINT(x) do { (void)sizeof(x); } while (0)
#define DBG_PRINTLN(x) do { (void)sizeof(x); } while (0)
#define DBG_PRINTF(...) do {} while (0)
#endif

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
    DBG_PRINTLN("[wifi_test] Starting Wi-Fi connection attempt");
    DBG_PRINT("[wifi_test] SSID: ");
    DBG_PRINTLN(MQTT_WIFI_SSID);

    WiFi.mode(WIFI_STA);
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(MQTT_WIFI_SSID, MQTT_WIFI_PASSWORD);
    }

    uint32_t last_log_ms = 0;
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        blink_red(1, 120, 120);

        const uint32_t now = millis();
        if ((now - last_log_ms) >= 1000) {
            DBG_PRINTF("[wifi_test] waiting... status=%d elapsed=%lu ms\n",
                       (int)WiFi.status(), (unsigned long)(now - start));
            last_log_ms = now;
        }

        delay(120);
    }

    if (WiFi.status() == WL_CONNECTED) {
        DBG_PRINTLN("[wifi_test] Wi-Fi connected");
        DBG_PRINT("[wifi_test] IP: ");
        DBG_PRINTLN(WiFi.localIP());
        show_success();
        return true;
    }

    DBG_PRINTF("[wifi_test] Wi-Fi connect FAILED (status=%d)\n", (int)WiFi.status());
    show_error();
    return false;
}

void setup() {
    DBG_BEGIN(115200);
    delay(150);
    DBG_PRINTLN("\n[wifi_test] Boot");

    leds_init_local();
    delay(200);
}

void loop() {
    const bool connected = connect_wifi_and_signal(15000);

    if (connected) {
        // Keep a visible "connected" heartbeat.
        DBG_PRINTLN("[wifi_test] heartbeat: connected");
        blink_green(1, 120, 120);
        delay(1880);
        return;
    }

    DBG_PRINTLN("[wifi_test] retry in 2500 ms");
    // Retry after a short pause.
    delay(2500);
}