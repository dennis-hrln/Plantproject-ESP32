[![Cirkit Designer Preview](https://app.cirkitdesigner.com/api/project/a6507e17-3860-47a8-9a6b-0286741713fa/screenshot?width=800)](https://app.cirkitdesigner.com/project/a6507e17-3860-47a8-9a6b-0286741713fa?view=interactive_preview)

**Edit this project interactively in [Cirkit Designer](https://app.cirkitdesigner.com/project/a6507e17-3860-47a8-9a6b-0286741713fa).**

# Wiring Guide

This document contains the full step-by-step wiring instructions and a consolidated wiring summary for the ESP32 Single-Plant Automatic Watering System.

## Components Needed

| Quantity | Component | Example Part |
|----------|-----------|--------------|
| 1 | ESP32-C3 Supermini board | HW-466AB |
| 1 | Capacitive soil moisture sensor | v1.2 or v2.0 |
| 1 | Mini water pump 3-6V | DC submersible pump |
| 1 | N-channel MOSFET | IRLZ44N or 2N7000 |
| 1 | 3xAA Alkaline battery | 4.5V nominal |
| 2 | 100kΩ resistors | Voltage divider |
| 2 | 220Ω resistors | LED current limiting |
| 1 | Green LED | 3mm or 5mm |
| 1 | Red LED | 3mm or 5mm |
| 1 | Mini float switch | PP water level sensor |
| 3 | Push buttons | Tactile switches |
| 1 | Breadboard or perfboard | For prototyping |

---

## Wiring Steps (summary)

1) Soil moisture sensor

- Sensor wires: Red (VCC) → ESP32 3V3
- Black (GND) → ESP32 GND
- Yellow (Signal) → ESP32 GPIO4 (analog)

2) Battery voltage monitoring

- Build voltage divider with 2×100kΩ resistors. Middle point → ESP32 GPIO3 (ADC)
- Battery + → ESP32 5V (power input)
- Battery − → ESP32 GND

3) Pump + MOSFET + flyback diode

- MOSFET Gate → ESP32 GPIO5
- MOSFET Source → GND
- MOSFET Drain → Pump (−)
- Pump (+) → Battery +
- Flyback diode (1N400x): Cathode (striped) → Pump +, Anode → MOSFET Drain

4) LEDs

- Green LED anode → ESP32 GPIO6 through 220Ω resistor; cathode → GND
- Red LED anode → ESP32 GPIO7 through 220Ω resistor; cathode → GND

5) Buttons (active LOW, use internal pull-ups)

- Main button → GPIO0 ↔ GND
- Wet calibration → GPIO2 ↔ GND
- Dry calibration → GPIO1 ↔ GND

6) Water level float switch (active LOW = water low, internal pull-up)

- Float switch wire 1 → ESP32 GPIO10
- Float switch wire 2 → ESP32 GND
- Mount at minimum water level inside reservoir
- Switch closed (water LOW) → GPIO reads LOW → red LED blinks every 15 min, watering disabled
- Switch open (water OK) → GPIO reads HIGH via pull-up → normal operation

---

## Complete Wiring Summary (table)

| From | To |
|------|----|
| Soil Sensor VCC | ESP32 3V3 |
| Soil Sensor GND | ESP32 GND |
| Soil Sensor OUT | ESP32 GPIO4 |
| Battery (+) | ESP32 5V |
| Battery (−) | ESP32 GND |
| Divider middle | ESP32 GPIO3 |
| ESP32 GPIO5 | MOSFET Gate |
| MOSFET Source | GND |
| MOSFET Drain | Pump (−) |
| Pump (+) | Battery (+) |
| ESP32 GPIO6 | Green LED (+) via 220Ω |
| ESP32 GPIO7 | Red LED (+) via 220Ω |
| Main Button | GPIO0 ↔ GND |
| Wet Cal Button | GPIO2 ↔ GND |
| Dry Cal Button | GPIO1 ↔ GND |
| Float Switch wire 1 | ESP32 GPIO10 |
| Float Switch wire 2 | ESP32 GND |

---

If you want this file expanded with images or a printable checklist, tell me and I will add them.
