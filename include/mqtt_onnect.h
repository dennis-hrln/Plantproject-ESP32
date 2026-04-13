#ifndef MQTT_ONNECT_H
#define MQTT_ONNECT_H

#include <Arduino.h>

// Connect to configured Wi-Fi.
// Returns true on success, false on timeout/failure.
bool mqtt_onnect_wifi_connect(uint32_t timeout_ms = 15000);

#endif // MQTT_ONNECT_H