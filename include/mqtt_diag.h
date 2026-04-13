#ifndef MQTT_DIAG_H
#define MQTT_DIAG_H

void mqtt_diag_wifi_connect_start();
void mqtt_diag_wifi_connect_ok();
void mqtt_diag_wifi_connect_fail();
void mqtt_diag_wifi_status_code(int wifi_status);

void mqtt_diag_mqtt_connect_ok();
void mqtt_diag_mqtt_connect_fail();

#endif // MQTT_DIAG_H
