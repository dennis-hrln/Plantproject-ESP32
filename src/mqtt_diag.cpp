#include "mqtt_diag.h"

#include "leds.h"

void mqtt_diag_wifi_connect_start() {
    led_green_on();
    delay(80);
    led_green_off();
}

void mqtt_diag_wifi_connect_ok() {
    led_green_blink(2, 120);
}

void mqtt_diag_wifi_connect_fail() {
    led_red_blink(3, 120);
}

void mqtt_diag_wifi_status_code(int wifi_status) {
    // Encode Wi-Fi status as blink count: 0->1 blink, 1->2 blinks, ...
    uint8_t count = 8;
    if (wifi_status >= 0 && wifi_status <= 6) {
        count = (uint8_t)wifi_status + 1;
    }

    delay(140);
    led_green_blink(1, 220);
    delay(140);
    led_red_blink(count, 80);
    delay(500);
}

void mqtt_diag_mqtt_connect_ok() {
    led_green_blink(3, 80);
}

void mqtt_diag_mqtt_connect_fail() {
    led_red_blink(4, 80);
}
