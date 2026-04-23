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
#include <string.h>

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

static const char *wl_status_text(wl_status_t status) {
    switch (status) {
        case WL_NO_SHIELD: return "NO_SHIELD";
        case WL_IDLE_STATUS: return "IDLE";
        case WL_NO_SSID_AVAIL: return "NO_SSID";
        case WL_SCAN_COMPLETED: return "SCAN_DONE";
        case WL_CONNECTED: return "CONNECTED";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "CONNECTION_LOST";
        case WL_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

static int s_last_disconnect_reason = -1;

static const char *wifi_disconnect_reason_text(int reason) {
    switch (reason) {
        case 2: return "AUTH_EXPIRE";
        case 4: return "ASSOC_EXPIRE";
        case 5: return "ASSOC_TOOMANY";
        case 6: return "NOT_AUTHED";
        case 7: return "NOT_ASSOCED";
        case 8: return "ASSOC_LEAVE";
        case 15: return "4WAY_HANDSHAKE_TIMEOUT";
        case 16: return "GROUP_KEY_UPDATE_TIMEOUT";
        case 17: return "IE_IN_4WAY_DIFFERS";
        case 23: return "802_1X_AUTH_FAILED";
        case 201: return "NO_AP_FOUND";
        case 202: return "AUTH_FAIL";
        case 203: return "ASSOC_FAIL";
        case 204: return "HANDSHAKE_TIMEOUT";
        default: return "UNKNOWN";
    }
}

static void on_wifi_event(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        s_last_disconnect_reason = (int)info.wifi_sta_disconnected.reason;
        DBG_PRINTF("[wifi_test] event: STA_DISCONNECTED reason=%d (%s)\n",
                   s_last_disconnect_reason,
                   wifi_disconnect_reason_text(s_last_disconnect_reason));
    }
}

typedef struct {
    bool found;
    int32_t rssi;
    int32_t channel;
    wifi_auth_mode_t auth;
    uint8_t bssid[6];
} TargetAp;

static TargetAp scan_target_ap() {
    TargetAp ap = {false, 0, 0, WIFI_AUTH_OPEN, {0, 0, 0, 0, 0, 0}};
    int networks = WiFi.scanNetworks(false, true);
    if (networks < 0) {
        DBG_PRINTLN("[wifi_test] scan failed");
        return ap;
    }

    for (int i = 0; i < networks; ++i) {
        if (WiFi.SSID(i) == String(MQTT_WIFI_SSID)) {
            ap.found = true;
            ap.rssi = WiFi.RSSI(i);
            ap.channel = WiFi.channel(i);
            ap.auth = WiFi.encryptionType(i);
            const uint8_t *bssid = WiFi.BSSID(i);
            if (bssid != nullptr) {
                memcpy(ap.bssid, bssid, 6);
            }
            DBG_PRINTF("[wifi_test] target SSID visible, RSSI=%d dBm, channel=%d, auth=%d\n",
                       ap.rssi, ap.channel, (int)ap.auth);
            break;
        }
    }

    if (!ap.found) {
        DBG_PRINTLN("[wifi_test] target SSID NOT visible in scan");
    }

    WiFi.scanDelete();
    return ap;
}

static void configure_wpa2_safe_sta_profile() {
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);
    WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);
}

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
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    DBG_PRINTLN("[wifi_test] Starting Wi-Fi connection attempt");
    DBG_PRINT("[wifi_test] SSID: ");
    DBG_PRINTLN(MQTT_WIFI_SSID);

    WiFi.disconnect(false, true);
    delay(100);
    WiFi.mode(WIFI_STA);
    configure_wpa2_safe_sta_profile();
    WiFi.setAutoReconnect(true);

    TargetAp ap = scan_target_ap();

    if (ap.found && ap.auth > WIFI_AUTH_WPA2_PSK) {
        DBG_PRINTF("[wifi_test] forcing WPA2-safe connect path for auth=%d\n", (int)ap.auth);
    }

    DBG_PRINTF("[wifi_test] begin, pre-status=%d (%s)\n",
               (int)WiFi.status(), wl_status_text((wl_status_t)WiFi.status()));
    if (ap.found && ap.channel > 0) {
        WiFi.begin(MQTT_WIFI_SSID, MQTT_WIFI_PASSWORD, ap.channel, ap.bssid, true);
    } else {
        WiFi.begin(MQTT_WIFI_SSID, MQTT_WIFI_PASSWORD);
    }

    uint32_t last_log_ms = 0;
    bool idle_fallback_retry_done = false;
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        blink_red(1, 120, 120);

        const uint32_t now = millis();
        if ((now - last_log_ms) >= 1000) {
            DBG_PRINTF("[wifi_test] waiting... status=%d (%s) elapsed=%lu ms\n",
                       (int)WiFi.status(), wl_status_text((wl_status_t)WiFi.status()),
                       (unsigned long)(now - start));
            last_log_ms = now;
        }

        if (!idle_fallback_retry_done && WiFi.status() == WL_IDLE_STATUS && (now - start) > 5000UL) {
            DBG_PRINTLN("[wifi_test] idle fallback: retrying plain begin()");
            WiFi.disconnect(false, true);
            delay(120);
            WiFi.mode(WIFI_STA);
            WiFi.setSleep(false);
            WiFi.begin(MQTT_WIFI_SSID, MQTT_WIFI_PASSWORD);
            idle_fallback_retry_done = true;
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

    DBG_PRINTF("[wifi_test] Wi-Fi connect FAILED (status=%d, %s)\n",
               (int)WiFi.status(), wl_status_text((wl_status_t)WiFi.status()));
    if (s_last_disconnect_reason >= 0) {
        DBG_PRINTF("[wifi_test] last disconnect reason=%d (%s)\n",
                   s_last_disconnect_reason,
                   wifi_disconnect_reason_text(s_last_disconnect_reason));
    }
    show_error();
    return false;
}

void setup() {
    DBG_BEGIN(115200);
    delay(150);
    DBG_PRINTLN("\n[wifi_test] Boot");

    WiFi.onEvent(on_wifi_event);

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