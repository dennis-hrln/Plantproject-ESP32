# ESP32 Single-Plant Automatic Watering System

A production-quality, battery-powered automatic plant watering system using ESP32 with deep sleep for ultra-low power consumption.

## Features

- **Automatic Watering**: Waters plant when soil moisture drops below threshold
- **Deep Sleep**: ~10µA sleep current for months of battery life
- **Safe Operation**: Multiple safety checks prevent accidental watering
- **LED Feedback**: Visual humidity display and status indicators
- **Persistent Storage**: Settings survive power cycles (NVS)
- **Protected Calibration**: Requires button combinations to prevent accidents

## Hardware Requirements

| Component | Pin | Notes |
|-----------|-----|-------|
| Soil Moisture Sensor | GPIO34 | Capacitive sensor, ADC1 |
| Battery Voltage | GPIO35 | Via voltage divider (1:1 ratio) |
| Water Pump | GPIO25 | Via N-MOSFET (logic level) |
| Green LED | GPIO26 | 220Ω resistor to GND |
| Red LED | GPIO27 | 220Ω resistor to GND |
| Main Button | GPIO32 | To GND, internal pull-up |
| Wet Cal Button | GPIO33 | To GND, internal pull-up |
| Dry Cal Button | GPIO14 | To GND, internal pull-up |

### Schematic Notes

```
BATTERY (+) ──┬── Voltage Divider ──> GPIO35
              │   (100k + 100k)
              └── VIN or 3.3V regulator

PUMP: GPIO25 ──> MOSFET Gate ──> Pump (-)
                              └── GND
      Pump (+) ──> Battery (+)

SENSOR: VCC ──> 3.3V
        GND ──> GND
        OUT ──> GPIO34

BUTTONS: Each button connects GPIO to GND when pressed
         Internal pull-ups used (no external resistors needed)
```

## Button Operations

### Basic Operations

| Action | How To | LED Feedback |
|--------|--------|--------------|
| **Show Humidity** | Short press MAIN | Long flashes (tens) + short flashes (ones) |
| **Manual Watering** | Long press MAIN (2s) | Green blinks, then watering |

### Protected Operations (require button combinations)

| Action | How To | LED Feedback |
|--------|--------|--------------|
| **Enter Calibration** | Hold ALL 3 buttons for 2+ seconds | Both LEDs pulse, then green blinks |
| **Wet Calibration** | (In cal mode) Long press WET button | Green progress, then success |
| **Dry Calibration** | (In cal mode) Long press DRY button | Red progress, then success |
| **Adjust Humidity** | Hold MAIN + WET for 2s | Shows current value, then adjust mode |
| **Increase Target** | (In adjust mode) Press WET | Green blink (+5%) |
| **Decrease Target** | (In adjust mode) Press DRY | Red blink (-5%) |

### LED Patterns

| Pattern | Meaning |
|---------|---------|
| Both LEDs flash → Long flashes → Short flashes | Humidity display (tens.ones) |
| 2 green blinks | Success |
| 3 rapid red blinks | Error |
| 2 slow red blinks | Battery warning |
| 5 rapid red blinks | Battery critical |
| Green-Red-Green sequence | Calibration mode entered |

## Calibration Procedure

### Initial Setup

1. **Dry Calibration** (do this first):
   - Hold sensor in air (or completely dry soil)
   - Press and hold ALL 3 buttons for 2+ seconds
   - Release all buttons when both LEDs pulse
   - Long-press the DRY button (red flashing indicates progress)
   - Success confirmed with green blinks

2. **Wet Calibration**:
   - Place sensor in water (or saturated soil)
   - Press and hold ALL 3 buttons for 2+ seconds
   - Release all buttons
   - Long-press the WET button (green flashing indicates progress)
   - Success confirmed with green blinks

### Setting Target Humidity

1. Hold MAIN + WET buttons together for 2+ seconds
2. LED will flash current target (e.g., 4 long + 0 short = 40%)
3. Press WET to increase by 5% (green blink)
4. Press DRY to decrease by 5% (red blink)
5. Wait 5 seconds to save (success blinks confirm)

## Configuration

Edit `src/config.h` to customize:

```cpp
// Measurement interval (how often to check soil)
#define MEASUREMENT_INTERVAL_SEC    (1 * 60 * 60)   // 1 hour

// Watering parameters
#define DEFAULT_OPTIMAL_HUMIDITY    40      // Target humidity %
#define PUMP_RUN_DURATION_MS        3000    // Pump time per watering
#define MIN_WATERING_INTERVAL_SEC   (6 * 60 * 60)   // 6 hours minimum

// Battery thresholds (adjust for your battery)
#define BATTERY_WARNING_MV      3500    // Show warning LED
#define BATTERY_CRITICAL_MV     3300    // Disable watering
```

## Building

### PlatformIO (Recommended)

```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor serial output (debug mode)
pio device monitor
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
   - Requires ALL 3 buttons held for 2+ seconds
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
