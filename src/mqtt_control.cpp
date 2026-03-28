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

    char msg[160];
    snprintf(msg, sizeof(msg),
             "telemetry:ts=%lu,hum=%u,batt=%u,water_ok=%u,min=%u,max=%u",
             (unsigned long)ts,
             humidity,
             batt,
             water_ok ? 1 : 0,
             min_h,
             max_h);
    mqtt_publish_status(msg);
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
            mqtt_publish_status("ok:water");
        } else if (r == WATER_BATTERY_LOW) {
            mqtt_publish_status("err:battery_low");
        } else if (r == WATER_RESERVOIR_LOW) {
            mqtt_publish_status("err:reservoir_low");
        } else {
            mqtt_publish_status("err:watering_failed");
        }
        return;
    }

    if (cmd == "calibrate_wet") {
        sensor_calibrate_wet();
        mqtt_publish_status("ok:calibrate_wet");
        return;
    }

    if (cmd == "calibrate_dry") {
        sensor_calibrate_dry();
        mqtt_publish_status("ok:calibrate_dry");
        return;
    }

    int min_h = parse_int_arg(cmd, "set_min:");
    if (min_h >= 0) {
        if (min_h > 100) min_h = 100;
        storage_set_minimal_humidity((uint8_t)min_h);
        mqtt_publish_status("ok:set_min");
        return;
    }

    int max_h = parse_int_arg(cmd, "set_max:");
    if (max_h >= 0) {
        if (max_h > 100) max_h = 100;
        storage_set_max_humidity((uint8_t)max_h);
        mqtt_publish_status("ok:set_max");
        return;
    }

    if (cmd == "status") {
        const uint8_t humidity = sensor_read_humidity_percent();
        const uint8_t batt = battery_get_percent();
        const bool water_ok = water_level_ok();
        char msg[96];
        snprintf(msg, sizeof(msg), "status:hum=%u,batt=%u,water_ok=%u", humidity, batt, water_ok ? 1 : 0);
        mqtt_publish_status(msg);
        return;
    }

    mqtt_publish_status("err:unknown_command");
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
    mqtt_publish_status("online");
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
