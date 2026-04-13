#include <Arduino.h>
void setup() {
    Serial.begin(115200);
  delay(3000); // give time for serial monitor to open

  Serial.println("ESP32-C3 Serial Test Start");
}

void loop() {
  Serial.println("Hello from ESP32-C3 SuperMini!");
  delay(1000);
}