#include "mqtt_control.h"

#include <WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "watering.h"
#include "sensor.h"
#include "battery.h"
#include "storage.h"
#include "water_level.h"

static WiFiClient s_wifi;
static PubSubClient s_mqtt(s_wifi);
static uint32_t s_last_reconnect_ms = 0;

static void mqtt_publish_status(const char *text) {
    s_mqtt.publish(MQTT_TOPIC_STATUS, text, true);
}

static void mqtt_publish_ack(const char *cmd, bool ok, const char *detail) {
    if (!s_mqtt.connected()) {
        return;
    }

    const uint32_t ts = storage_get_persistent_time();
    char msg[192];
    snprintf(msg, sizeof(msg),
             "{\"ts\":%lu,\"cmd\":\"%s\",\"ok\":%s,\"detail\":\"%s\"}",
             (unsigned long)ts,
             cmd,
             ok ? "true" : "false",
             detail);
    s_mqtt.publish(MQTT_TOPIC_ACK, msg, false);
}

void mqtt_control_publish_telemetry() {
    if (!s_mqtt.connected()) {
        return;
    }

    const uint32_t ts = storage_get_persistent_time();
    const uint8_t humidity = sensor_read_humidity_percent();
    const uint8_t batt = battery_get_percent();
    const bool water_ok = water_level_ok();
    const uint8_t min_h = storage_get_minimal_humidity();
    const uint8_t max_h = storage_get_max_humidity();

    char msg[192];
    snprintf(msg, sizeof(msg),
             "{\"ts\":%lu,\"humidity\":%u,\"battery\":%u,\"water_ok\":%s,\"min\":%u,\"max\":%u}",
             (unsigned long)ts,
             humidity,
             batt,
             water_ok ? "true" : "false",
             min_h,
             max_h);
    s_mqtt.publish(MQTT_TOPIC_TELEMETRY, msg, false);
}

static int parse_int_arg(const String &input, const char *prefix) {
    const String p(prefix);
    if (!input.startsWith(p)) {
        return -1;
    }
    String value = input.substring(p.length());
    value.trim();
    if (value.length() == 0) {
        return -1;
    }
    return value.toInt();
}

static void handle_command(const String &cmd_raw) {
    String cmd = cmd_raw;
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "water") {
        WateringResult r = watering_manual(true);
        if (r == WATER_OK || r == WATER_PARTIAL) {
            mqtt_publish_ack("water", true, "watered");
        } else if (r == WATER_BATTERY_LOW) {
            mqtt_publish_ack("water", false, "battery_low");
        } else if (r == WATER_RESERVOIR_LOW) {
            mqtt_publish_ack("water", false, "reservoir_low");
        } else {
            mqtt_publish_ack("water", false, "watering_failed");
        }
        mqtt_control_publish_telemetry();
        return;
    }

    if (cmd == "calibrate_wet") {
        sensor_calibrate_wet();
        mqtt_publish_ack("calibrate_wet", true, "ok");
        mqtt_control_publish_telemetry();
        return;
    }

    if (cmd == "calibrate_dry") {
        sensor_calibrate_dry();
        mqtt_publish_ack("calibrate_dry", true, "ok");
        mqtt_control_publish_telemetry();
        return;
    }

    int min_h = parse_int_arg(cmd, "set_min:");
    if (min_h >= 0) {
        if (min_h > 100) min_h = 100;
        storage_set_minimal_humidity((uint8_t)min_h);
        mqtt_publish_ack("set_min", true, "ok");
        mqtt_control_publish_telemetry();
        return;
    }

    int max_h = parse_int_arg(cmd, "set_max:");
    if (max_h >= 0) {
        if (max_h > 100) max_h = 100;
        storage_set_max_humidity((uint8_t)max_h);
        mqtt_publish_ack("set_max", true, "ok");
        mqtt_control_publish_telemetry();
        return;
    }

    if (cmd == "status") {
        mqtt_control_publish_telemetry();
        mqtt_publish_ack("status", true, "ok");
        return;
    }

    mqtt_publish_ack("unknown", false, "unknown_command");
}

static void mqtt_callback(char *topic, uint8_t *payload, unsigned int length) {
    (void)topic;
    String cmd;
    cmd.reserve(length);
    for (unsigned int i = 0; i < length; ++i) {
        cmd += (char)payload[i];
    }
    handle_command(cmd);
}

static void wifi_ensure_connected() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(MQTT_WIFI_SSID, MQTT_WIFI_PASSWORD);

    const uint32_t start_ms = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start_ms) < 15000UL) {
        delay(250);
        yield();
    }
}

static void mqtt_try_reconnect() {
    if (s_mqtt.connected()) {
        return;
    }

    const uint32_t now = millis();
    if ((now - s_last_reconnect_ms) < MQTT_RECONNECT_MS) {
        return;
    }
    s_last_reconnect_ms = now;

    wifi_ensure_connected();
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    bool ok = false;
    if (strlen(MQTT_BROKER_USER) > 0) {
        ok = s_mqtt.connect(MQTT_CLIENT_ID, MQTT_BROKER_USER, MQTT_BROKER_PASSWORD);
    } else {
        ok = s_mqtt.connect(MQTT_CLIENT_ID);
    }

    if (!ok) {
        return;
    }

    s_mqtt.subscribe(MQTT_TOPIC_COMMAND);
    const uint32_t ts = storage_get_persistent_time();
    char msg[96];
    snprintf(msg, sizeof(msg), "{\"state\":\"online\",\"ts\":%lu}", (unsigned long)ts);
    mqtt_publish_status(msg);
}

void mqtt_control_init() {
    s_mqtt.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    s_mqtt.setCallback(mqtt_callback);
    wifi_ensure_connected();
    mqtt_try_reconnect();
}

void mqtt_control_process() {
    mqtt_try_reconnect();
    if (s_mqtt.connected()) {
        s_mqtt.loop();
    }
}

void mqtt_control_process_for(uint32_t duration_ms) {
    const uint32_t start_ms = millis();
    while ((millis() - start_ms) < duration_ms) {
        mqtt_control_process();
        delay(10);
        yield();
    }
}
