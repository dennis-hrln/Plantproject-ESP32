

[![Cirkit Designer Preview](https://app.cirkitdesigner.com/api/project/a6507e17-3860-47a8-9a6b-0286741713fa/screenshot?width=800)](https://app.cirkitdesigner.com/project/a6507e17-3860-47a8-9a6b-0286741713fa?view=interactive_preview)

**Edit this project interactively in [Cirkit Designer](https://app.cirkitdesigner.com/project/a6507e17-3860-47a8-9a6b-0286741713fa).**

![Cirkit Designer Wiring Preview](./wiring-preview.png)

# ESP32 Single-Plant Automatic Watering System

A production-quality, battery-powered automatic plant watering system using ESP32 with deep sleep for ultra-low power consumption.

## Features

- **Automatic Watering**: Waters plant when soil moisture drops below threshold
- **Deep Sleep**: ~10µA sleep current for months of battery life
- **Safe Operation**: Multiple safety checks prevent accidental watering
- **LED Feedback**: Visual humidity display and status indicators
- **Persistent Storage**: Settings survive power cycles (NVS)
- **Protected Calibration**: Requires button combinations to prevent accidents

## Hardware - ESP32-C3 Supermini (HW-466AB)

### Components Needed

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
| 3 | Push buttons | Tactile switches |
| 1 | Breadboard or perfboard | For prototyping |
| - | Jumper wires | Male-to-female preferred |

---

## Wiring

The full step-by-step wiring guide has been moved to a dedicated document: [Wiring Guide](wiring.md).

This keeps the README focused — open `wiring.md` for detailed wiring steps, diagrams and the complete wiring summary.

## 🧪 TESTING BEFORE USE

1. **Power Test**: Plug battery. Green or red LED should NOT light yet (deep sleep).
2. **Button Test**: Press Main button briefly. ESP32 should wake, LEDs may flash.
3. **LED Test**: Code will test LEDs on first boot.
4. **Sensor Test**: Check Serial Monitor (115200 baud) for sensor readings.
5. **Pump Test**: Long-press the Main button for manual watering. The pump should run for 3 seconds.
---

## Button Operations

### Basic Operations

| Action | How To | LED Feedback |
|--------|--------|--------------|
| **Show Humidity** | Short press MAIN | Long flashes (tens) + short flashes (ones) |
| **Manual Watering** | Long press MAIN (2s) | Green blinks, then watering |
| **Show Target Humidity** | Short press DRY | Same two-digit pattern as humidity |

### Protected Operations (require button combinations)

| Action | How To | LED Feedback |
|--------|--------|--------------|
| **Enter Calibration** | Hold WET + DRY for 2+ seconds | Both LEDs heartbeat blink (150ms on, 300ms off) |
| **Wet Calibration** | (In cal mode) Long press WET button | Red steady during calibrate, then success |
| **Dry Calibration** | (In cal mode) Long press DRY button | Green steady during calibrate, then success |
| **Set Target Humidity** | Hold ALL 3 buttons for 2+ seconds | Both LEDs stay on while in adjust mode |
| **Increase Target** | (In adjust mode) Press WET | Brief green off (about 300ms), +5% |
| **Decrease Target** | (In adjust mode) Press DRY | Brief red off (about 300ms), -5% |

### LED Patterns

| Pattern | Meaning |
|---------|---------|
| Both LEDs flash → Long green flashes (tens) → Short green flashes (ones) | Humidity display (tens.ones) |
| 2 solid green blinks | Success |
| 3 rapid red blinks | Error |
| 2 slow red blinks | Battery warning |
| 5 rapid red blinks | Battery critical |
| Both LEDs heartbeat blink (150ms on, 300ms off) | Calibration mode waiting for action |

## Button LED Behavior (quick reference)

This describes what the LEDs will do when you press the Main, Wet and Dry buttons in two contexts: the `hardware_test` sketch and the main firmware (normal operation / calibration mode).

### Hardware Test (upload the `hardware_test` environment)

- Startup: 2× both-LED flashes → ready.
- Main (short press): LED test sequence — Green ON 1s → Red ON 1s → Both ON 1s → Alternating green/red 4× (250ms).
- Main (long press >2s): Battery test — Start marker, N× green blinks (1–5) representing battery level, optional red warning blinks if low, end marker.
- Wet (short press): Soil sensor test — Start marker, short red flash (reading incoming), N× green blinks (each ≈10%), end marker; error = 5 rapid red blinks.
- Dry (short press): Pump test — 3 red blinks countdown, red LED ON while pump runs (1s), confirmation 2 green blinks.
- All 3 buttons together: Run all tests sequence — rapid alternating green/red 5× then runs all tests; final: 3 both-LED blinks.

### Main Firmware (normal operation / calibration)
- Main (short press): Display humidity using the two‑digit LED pattern (`led_display_number`):
   - Start indicator: both LEDs briefly ON (~100 ms).
   - Pause (~LED_DIGIT_PAUSE_MS = 1000 ms).
   - TENS digit: long green flashes (each ~LED_FLASH_LONG_MS = 600 ms, 300 ms pause); skipped if 0.
   - Pause (~LED_DIGIT_PAUSE_MS = 1000 ms).
   - ONES digit: short green flashes (each ~LED_FLASH_SHORT_MS = 200 ms, 300 ms pause); if ones == 0 a single very brief flash (~80 ms) indicates zero.
   - End indicator: two very brief green flashes (50 ms each, short gap).
   - Special: 100% is shown as three long green flashes.

- Dry (short press): Display target humidity using the same two‑digit LED pattern (`led_display_number`).

- Main (long press): Manual watering (`perform_manual_watering`): two quick green blinks (2 × 100 ms) then the watering attempt. Result visual:
   - `WATER_OK`: `led_show_success()` → 2 solid green blinks (300 ms each).
   - `WATER_BATTERY_LOW`: battery critical pattern (rapid red blinks).
   - other errors: `led_show_error()` → 3 rapid red blinks (100 ms on).

- Enter calibration: Hold WET + DRY for 2+ seconds → both LEDs heartbeat blink (150 ms on, 300 ms off) while waiting for calibration action.

- In calibration mode:
   - Long-press WET: `perform_calibrate_wet()` lights red steady while calibrating, then `led_show_success()` on success (2 solid green blinks).
   - Long-press DRY: `perform_calibrate_dry()` lights green steady while calibrating, then `led_show_success()` on success.

- Set optimal humidity: hold ALL 3 buttons for 2+ seconds; both LEDs stay on while in adjust mode. Press WET to increase or DRY to decrease. The relevant LED briefly turns off (~300 ms) to confirm the step.

- Battery/status indicators used elsewhere:
   - `led_show_battery_warning()` → 2 slow red blinks (300 ms on).
   - `led_show_battery_critical()` → 5 rapid red blinks (100 ms on).

Refer to the `hardware_test` patterns above when using the standalone test sketch; both sets of patterns are useful during debugging and setup.

### Main Firmware LED Pattern Table

Summary of LED patterns used by the main firmware (functions in `src/leds.cpp`).

| Pattern | Meaning | Function / Notes |
|--------|---------|------------------|
| Brief both-LED flash (~100 ms) | Start indicator for numeric display | `led_display_number`: start marker |
| Long green flash (~LED_FLASH_LONG_MS) | Tens digit (each = 10%) | `led_display_number` |
| Short green flash (~LED_FLASH_SHORT_MS) | Ones digit (each = 1%) | `led_display_number` |
| Very brief green flash (~80 ms) | Zero ones indicator | `led_display_number` special case |
| Two very brief green flashes (50 ms) | End indicator for numeric display | `led_display_number` end marker |
| 2 solid green blinks (300 ms) | Success | `led_show_success()` |
| 3 rapid red blinks (100 ms) | Error | `led_show_error()` |
| 2 slow red blinks (300 ms) | Battery warning | `led_show_battery_warning()` |
| 5 rapid red blinks (100 ms) | Battery critical | `led_show_battery_critical()` |
| Both LEDs blink simultaneously (150 ms ON, 300 ms OFF) | Calibration mode active (heartbeat until action/timeout) | `buttons.cpp` heartbeat |
| N green blinks (500 ms) | Battery level indicator (1..5) | `test_battery()` mapping |
| Single long red blink (1000 ms) | Pump/power error indicator | used for pump failure in runtime |


**Hardware Test LED Patterns**

The hardware test firmware (`src/hardware_test.cpp`) uses additional patterns for diagnostics. These are visible when you upload the `hardware_test` environment and run the button-driven tests.

| Pattern | Meaning |
|---------|---------|
| Startup: 2 both-LED flashes (200ms) | Test sketch ready/boot completed |
| Start marker: 1 both-LED flash (150ms) | Beginning of an individual test |
| 3 fast green blinks (100ms) | Test success / pass |
| 5 fast red blinks (100ms) | Test error / sensor disconnected |
| LED test: green on 1s → red on 1s → both on 1s → alternating 4× (250ms) | Visual LED wiring / polarity check |
| Soil sensor test: short red flash → N× green blinks (each ~400ms) → both-LED end marker | Shows humidity in 10% steps (1–10 blinks). If reading invalid (0 or near-max) you get error pattern |
| Pump test: 3 red blinks (500ms) → red LED on while pump runs 1s → 2 green blinks | Pump/transistor check and confirmation |
| Battery test: N green blinks (500ms) then optional 3 red blinks if low | Battery level 1–5 (low→full); red blinks indicate below warning threshold |
| Run-all signal: rapid alternating green/red 5× | Running full test sequence (LED, sensor, battery, pump) |
| Final-all: 3 both-LED blinks (300ms) | All tests completed |


## Calibration Procedure

### Initial Setup

1. **Dry Calibration** (do this first):
   - Hold sensor in completely dry soil
   - Press and hold WET + DRY for 2+ seconds
   - Release both buttons when both LEDs start the heartbeat blink
   - Long-press the DRY button (green steady indicates progress)
   - Success confirmed with green blinks

2. **Wet Calibration**:
   - Place sensor in water (or saturated soil)
   - Press and hold WET + DRY for 2+ seconds
   - Release both buttons
   - Long-press the WET button (red steady indicates progress)
   - Success confirmed with green blinks

### Setting Target Humidity

1. Hold ALL 3 buttons for 2+ seconds (enter adjust mode)
2. Both LEDs stay on while in adjust mode
3. Press WET to increase by 5% (green LED briefly turns off)
4. Press DRY to decrease by 5% (red LED briefly turns off)
5. Wait for timeout to save and exit

## Configuration

Edit `src/config.h` to customize:

```cpp
// Measurement interval (how often to check soil)
#define MEASUREMENT_INTERVAL_SEC    (1 * 60 * 60)   // 1 hour

// Watering parameters
#define DEFAULT_OPTIMAL_HUMIDITY    40      // Target humidity %
#define PUMP_RUN_DURATION_MS        3000    // Pump time per watering
#define MIN_WATERING_INTERVAL_SEC   (6 * 60 * 60)   // 6 hours minimum

// Battery thresholds (for 3xAA Alkaline)
#define BATTERY_WARNING_MV      3600    // Show warning LED
#define BATTERY_CRITICAL_MV     3000    // Disable watering
```

## Building

### PlatformIO (Recommended)

```bash
pio run           # Build
pio run --target upload   # Upload
pio device monitor        # Monitor serial output (debug mode)
```

### Arduino IDE

1. Install ESP32 board support
2. Copy all `.cpp` and `.h` files from `src/` to your sketch folder
3. Rename `main.cpp` to `YourSketchName.ino`
4. Select board: "ESP32 Dev Module"
5. Upload

## Debug Mode

Enable serial output by ensuring `DEBUG_SERIAL` is defined in `platformio.ini`:

```ini
build_flags = 
    -D DEBUG_SERIAL
```

Serial output (115200 baud) shows:
- Wake reason
- Boot count
- Humidity readings
- Watering decisions
- Calibration values

## Power Consumption

| State | Current |
|-------|---------|
| Deep Sleep | ~10 µA |
| Active (measuring) | ~50 mA |
| Pump Running | 100-500 mA |

Expected battery life with 2000mAh battery, 1-hour measurement interval:
- Sleep: 23.9 hours × 10µA = 0.239 mAh/day
- Active: 0.1 hours × 50mA = 5 mAh/day (assuming 6 min total active/day)
- **Theoretical: Several months** (actual depends on watering frequency)

## Safety Features

1. **Watering Protection**:
   - Battery must be above critical threshold
   - Minimum 6 hours between waterings (configurable)
   - Manual watering requires 2-second long press

2. **Calibration Protection**:
   - Requires WET + DRY held for 2+ seconds
   - Then individual calibration requires another long press
   - Accidental calibration is virtually impossible

3. **Pump Safety**:
   - Maximum pump duration enforced in hardware/software
   - Battery checked during pump operation
   - Emergency stop on low battery

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No wake from button | Check button wiring (should connect GPIO to GND) |
| Sensor reads 0% or 100% | Re-calibrate with proper dry/wet references |
| Pump doesn't run | Check MOSFET wiring, battery voltage |
| Won't water | Check battery level, wait for minimum interval |
| Erratic readings | Add 100nF capacitor across sensor output |

## File Structure

```
src/
├── config.h        # All pins and constants
├── main.cpp        # Entry point, sleep management
├── storage.h/cpp   # NVS persistent storage
├── sensor.h/cpp    # Soil moisture sensor
├── battery.h/cpp   # Battery monitoring
├── pump.h/cpp      # Pump control
├── watering.h/cpp  # Decision logic
├── leds.h/cpp      # LED patterns
└── buttons.h/cpp   # Button handling
```

## License

Free for personal use.
