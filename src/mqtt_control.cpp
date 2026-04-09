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
static String s_plant_name = MQTT_PLANT_NAME;
static String s_last_command_id;

typedef struct {
    String command;
    String command_id;
} ParsedCommand;

static void normalize_plant_name(String &name) {
    name.trim();
    if (name.length() == 0) {
        name = MQTT_PLANT_NAME;
    }
    if (name.length() > MQTT_PLANT_NAME_MAX_LEN) {
        name = name.substring(0, MQTT_PLANT_NAME_MAX_LEN);
    }

    for (size_t i = 0; i < name.length(); ++i) {
        const char c = name.charAt(i);
        const bool ok = (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') ||
                        c == ' ' || c == '_' || c == '-';
        if (!ok) {
            name.setCharAt(i, '_');
        }
    }
}

static void mqtt_publish_status(const char *text) {
    s_mqtt.publish(MQTT_TOPIC_STATUS, text, true);
}

static void mqtt_publish_ack(const char *cmd, bool ok, const char *detail, const String &cmd_id = "") {
    if (!s_mqtt.connected()) {
        return;
    }

    const uint32_t ts = storage_get_persistent_time();
    char msg[240];
    snprintf(msg, sizeof(msg),
             "{\"plant\":\"%s\",\"ts\":%lu,\"id\":\"%s\",\"cmd\":\"%s\",\"ok\":%s,\"detail\":\"%s\"}",
             s_plant_name.c_str(),
             (unsigned long)ts,
             cmd_id.c_str(),
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
    const bool deep_sleep_enabled = storage_get_deep_sleep_enabled();

    char msg[224];
    snprintf(msg, sizeof(msg),
             "{\"plant\":\"%s\",\"ts\":%lu,\"humidity\":%u,\"battery\":%u,\"water_ok\":%s,\"min\":%u,\"max\":%u,\"deep_sleep\":%s}",
             s_plant_name.c_str(),
             (unsigned long)ts,
             humidity,
             batt,
             water_ok ? "true" : "false",
             min_h,
             max_h,
             deep_sleep_enabled ? "true" : "false");
    s_mqtt.publish(MQTT_TOPIC_TELEMETRY, msg, false);
}

static bool extract_json_string_field(const String &json, const char *key, String &out) {
    const String pattern = String("\"") + key + "\"";
    int key_pos = json.indexOf(pattern);
    if (key_pos < 0) {
        return false;
    }

    int colon_pos = json.indexOf(':', key_pos + pattern.length());
    if (colon_pos < 0) {
        return false;
    }

    int first_quote = json.indexOf('"', colon_pos + 1);
    if (first_quote < 0) {
        return false;
    }

    int second_quote = json.indexOf('"', first_quote + 1);
    if (second_quote < 0) {
        return false;
    }

    out = json.substring(first_quote + 1, second_quote);
    return true;
}

static ParsedCommand parse_command(const String &raw_payload) {
    ParsedCommand parsed;
    parsed.command = raw_payload;
    parsed.command_id = "";
    parsed.command.trim();

    if (!parsed.command.startsWith("{")) {
        return parsed;
    }

    String command_from_json;
    if (extract_json_string_field(parsed.command, "cmd", command_from_json)) {
        parsed.command = command_from_json;
        parsed.command.trim();
    }

    String id_from_json;
    if (extract_json_string_field(raw_payload, "id", id_from_json)) {
        parsed.command_id = id_from_json;
        parsed.command_id.trim();
    }

    return parsed;
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

static void handle_command(const ParsedCommand &parsed_cmd) {
    String raw = parsed_cmd.command;
    raw.trim();

    String cmd = raw;
    cmd.toLowerCase();

    if (parsed_cmd.command_id.length() > 0 && parsed_cmd.command_id == s_last_command_id) {
        mqtt_publish_ack("duplicate", true, "already_processed", parsed_cmd.command_id);
        return;
    }

    auto mark_processed = [&](void) {
        if (parsed_cmd.command_id.length() == 0) {
            return;
        }
        s_last_command_id = parsed_cmd.command_id;
        storage_set_last_command_id(s_last_command_id);
    };

    if (cmd == "water") {
        WateringResult r = watering_manual(true);
        if (r == WATER_OK || r == WATER_PARTIAL) {
            mqtt_publish_ack("water", true, "watered", parsed_cmd.command_id);
        } else if (r == WATER_BATTERY_LOW) {
            mqtt_publish_ack("water", false, "battery_low", parsed_cmd.command_id);
        } else if (r == WATER_RESERVOIR_LOW) {
            mqtt_publish_ack("water", false, "reservoir_low", parsed_cmd.command_id);
        } else {
            mqtt_publish_ack("water", false, "watering_failed", parsed_cmd.command_id);
        }
        mark_processed();
        mqtt_control_publish_telemetry();
        return;
    }

    if (cmd == "calibrate_wet") {
        sensor_calibrate_wet();
        mqtt_publish_ack("calibrate_wet", true, "ok", parsed_cmd.command_id);
        mark_processed();
        mqtt_control_publish_telemetry();
        return;
    }

    if (cmd == "calibrate_dry") {
        sensor_calibrate_dry();
        mqtt_publish_ack("calibrate_dry", true, "ok", parsed_cmd.command_id);
        mark_processed();
        mqtt_control_publish_telemetry();
        return;
    }

    if (cmd == "toggle_sleep") {
        const bool new_state = !storage_get_deep_sleep_enabled();
        storage_set_deep_sleep_enabled(new_state);
        mqtt_publish_ack("set_sleep", true, new_state ? "on" : "off", parsed_cmd.command_id);
        mark_processed();
        mqtt_control_publish_telemetry();
        return;
    }

    if (cmd == "sleep_status") {
        const bool state = storage_get_deep_sleep_enabled();
        mqtt_publish_ack("set_sleep", true, state ? "on" : "off", parsed_cmd.command_id);
        mark_processed();
        mqtt_control_publish_telemetry();
        return;
    }

    if (cmd.startsWith("set_sleep:")) {
        bool new_state = storage_get_deep_sleep_enabled();
        bool known = true;
        const String value = cmd.substring(String("set_sleep:").length());
        if (value == "on" || value == "1" || value == "true") {
            new_state = true;
        } else if (value == "off" || value == "0" || value == "false") {
            new_state = false;
        } else {
            known = false;
        }

        if (!known) {
            mqtt_publish_ack("set_sleep", false, "invalid_value", parsed_cmd.command_id);
            mark_processed();
            return;
        }

        storage_set_deep_sleep_enabled(new_state);
        mqtt_publish_ack("set_sleep", true, new_state ? "on" : "off", parsed_cmd.command_id);
        mark_processed();
        mqtt_control_publish_telemetry();
        return;
    }

    int min_h = parse_int_arg(cmd, "set_min:");
    if (min_h >= 0) {
        if (min_h > 100) min_h = 100;
        storage_set_minimal_humidity((uint8_t)min_h);
        mqtt_publish_ack("set_min", true, "ok", parsed_cmd.command_id);
        mark_processed();
        mqtt_control_publish_telemetry();
        return;
    }

    int max_h = parse_int_arg(cmd, "set_max:");
    if (max_h >= 0) {
        if (max_h > 100) max_h = 100;
        storage_set_max_humidity((uint8_t)max_h);
        mqtt_publish_ack("set_max", true, "ok", parsed_cmd.command_id);
        mark_processed();
        mqtt_control_publish_telemetry();
        return;
    }

    if (cmd.startsWith("set_name:")) {
        String name = raw.substring(String("set_name:").length());
        normalize_plant_name(name);
        s_plant_name = name;
        storage_set_plant_name(s_plant_name);
        mqtt_publish_ack("set_name", true, "ok", parsed_cmd.command_id);
        mark_processed();
        mqtt_control_publish_telemetry();
        return;
    }

    if (cmd == "status") {
        mqtt_control_publish_telemetry();
        mqtt_publish_ack("status", true, "ok", parsed_cmd.command_id);
        mark_processed();
        return;
    }

    mqtt_publish_ack("unknown", false, "unknown_command", parsed_cmd.command_id);
    mark_processed();
}

static void mqtt_callback(char *topic, uint8_t *payload, unsigned int length) {
    (void)topic;
    String raw_payload;
    raw_payload.reserve(length);
    for (unsigned int i = 0; i < length; ++i) {
        raw_payload += (char)payload[i];
    }
    ParsedCommand parsed = parse_command(raw_payload);
    handle_command(parsed);
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
    const bool deep_sleep_enabled = storage_get_deep_sleep_enabled();
    char msg[128];
    snprintf(msg, sizeof(msg), "{\"plant\":\"%s\",\"state\":\"online\",\"ts\":%lu,\"deep_sleep\":%s}",
             s_plant_name.c_str(),
             (unsigned long)ts,
             deep_sleep_enabled ? "true" : "false");
    mqtt_publish_status(msg);
}

void mqtt_control_init() {
    s_mqtt.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    s_mqtt.setCallback(mqtt_callback);

    String persisted_name = storage_get_plant_name();
    normalize_plant_name(persisted_name);
    s_plant_name = persisted_name;
    s_last_command_id = storage_get_last_command_id();

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
