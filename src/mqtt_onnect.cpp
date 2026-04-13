#include "mqtt_onnect.h"

#include <WiFi.h>

#include "config.h"

bool mqtt_onnect_wifi_connect(uint32_t timeout_ms) {
    WiFi.mode(WIFI_STA);

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(MQTT_WIFI_SSID, MQTT_WIFI_PASSWORD);
    }

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        delay(200);
    }

    return WiFi.status() == WL_CONNECTED;
}