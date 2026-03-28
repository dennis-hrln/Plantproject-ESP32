#ifndef MQTT_CONTROL_H
#define MQTT_CONTROL_H

#include <Arduino.h>

// Initialize WiFi and MQTT command channel.
void mqtt_control_init();

// Keep MQTT connection alive and process incoming commands.
void mqtt_control_process();

// Process MQTT for a bounded time window (ms), then return.
void mqtt_control_process_for(uint32_t duration_ms);

// Publish current telemetry snapshot.
void mqtt_control_publish_telemetry();

#endif // MQTT_CONTROL_H
