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
| 1 | Li-ion/LiPo battery | 18650 (3.7V) or similar |
| 2 | 100kΩ resistors | Voltage divider |
| 2 | 220Ω resistors | LED current limiting |
| 1 | Green LED | 3mm or 5mm |
| 1 | Red LED | 3mm or 5mm |
| 3 | Push buttons | Tactile switches |
| 1 | Breadboard or perfboard | For prototyping |
| - | Jumper wires | Male-to-female preferred |

---

## 🔌 WIRING GUIDE - Step by Step

### ESP32-C3 Supermini Pinout Reference
```
     USB-C
    ┌─────┐
5V  │ ● ● │ GND
G21 │ ● ● │ 3V3
G20 │ ● ● │ G10
G10 │ ● ● │ G9   (avoid - strapping pin)
G8  │ ● ● │ G8   (avoid - strapping pin)
G7  │ ● ● │ G7
G6  │ ● ● │ G6
G5  │ ● ● │ G5
G4  │ ● ● │ G4   ← Soil sensor
G3  │ ● ● │ G3   ← Battery voltage
G2  │ ● ● │ G2   (avoid - strapping pin)
G1  │ ● ● │ G1
G0  │ ● ● │ G0
    └─────┘
```

---

### 1️⃣ SOIL MOISTURE SENSOR → ESP32-C3

**Sensor has 3 wires:**
- **Red (VCC)** → ESP32 **3V3** pin
- **Black (GND)** → ESP32 **GND** pin
- **Yellow (Signal)** → ESP32 **GPIO4** pin

✅ That's it! No resistors needed.

---

### 2️⃣ BATTERY VOLTAGE MONITORING → ESP32-C3

**Build voltage divider with 2× 100kΩ resistors:**

```
Battery (+) ────┬──────────────> Battery connector
                │
              [100kΩ]
                │
                ├──────────────> ESP32 GPIO3
                │
              [100kΩ]
                │
               GND ────────────> ESP32 GND
```

**Why?** ESP32 reads 0-3.3V max. Battery is 3.0-4.2V, so we divide by 2.

**Steps:**
1. Solder 100kΩ resistor from Battery+ to a middle point
2. Solder another 100kΩ from that middle point to GND
3. Connect middle point to ESP32 **GPIO3**
4. Connect Battery+ to ESP32 **5V** pin (powers the board)
5. Connect Battery- (GND) to ESP32 **GND**

---

### 3️⃣ WATER PUMP + MOSFET + FLYBACK DIODE → ESP32-C3

**⚠️ Critical: add a flyback diode to protect the MOSFET**
- Use 1N4007 (or 1N4148). 
- **Striped end (cathode)** goes to pump **positive (+)**. 
- **Plain end (anode)** goes to MOSFET **Drain** (and pump −).

**MOSFET legs (flat side facing you):**
- Left = **Gate** ← GPIO5
- Middle = **Drain** ← Pump (−) & diode anode
- Right = **Source** ← GND

**Wiring:**
1) ESP32 **GPIO5** → MOSFET **Gate** (left)  
2) MOSFET **Source** (right) → ESP32 **GND**  
3) MOSFET **Drain** (middle) → Pump **black (−)**  
4) Pump **red (+)** → Battery **+** (or switch → Battery +)  
5) **Flyback diode:** anode (no stripe) to MOSFET Drain; cathode (stripe) to Pump +

```
ESP32 GPIO5 ────────────> MOSFET Gate
                          MOSFET Source ──> GND
                          MOSFET Drain ───> Pump (−)
                                            │
                               Diode anode ─┘
                               Diode cathode (stripe) ──> Pump (+) ──> Battery (+)
```

✅ MOSFET acts as electronic switch. GPIO5 HIGH = pump ON, LOW = pump OFF. The diode clamps the voltage spike when the pump turns off.

---

### 4️⃣ GREEN LED → ESP32-C3

**LED has 2 legs:**
- **Long leg (+)** = Anode
- **Short leg (−)** = Cathode

**Wiring:**
1. ESP32 **GPIO6** → **220Ω resistor** → LED long leg (+)
2. LED short leg (−) → ESP32 **GND**

```
ESP32 GPIO6 ──[220Ω]──> LED (+) ──> GND
```

---

### 5️⃣ RED LED → ESP32-C3

Same as green LED:

1. ESP32 **GPIO7** → **220Ω resistor** → LED long leg (+)
2. LED short leg (−) → ESP32 **GND**

```
ESP32 GPIO7 ──[220Ω]──> LED (+) ──> GND
```

---

### 6️⃣ BUTTONS → ESP32-C3

**Each button has 2 legs. When pressed, they connect together.**

**No resistors needed!** ESP32 has internal pull-ups.

**Main Button:**
- One leg → ESP32 **GPIO10**
- Other leg → ESP32 **GND**

**Wet Calibration Button:**
- One leg → ESP32 **GPIO20**
- Other leg → ESP32 **GND**

**Dry Calibration Button:**
- One leg → ESP32 **GPIO21**
- Other leg → ESP32 **GND**

```
Button layout:
[GPIO10] ──┤ ├── GND    (Main)
[GPIO20] ──┤ ├── GND    (Wet Cal)
[GPIO21] ──┤ ├── GND    (Dry Cal)
```

---

## ✅ COMPLETE WIRING SUMMARY

| From | To | Notes |
|------|-----|-------|
| **Soil Sensor VCC** | ESP32 **3V3** | Red wire |
| **Soil Sensor GND** | ESP32 **GND** | Black wire |
| **Soil Sensor OUT** | ESP32 **GPIO4** | Yellow/Signal wire |
| **Battery (+)** | ESP32 **5V** | Powers board |
| **Battery (−)** | ESP32 **GND** | Common ground |
| **Battery (+)** | Voltage divider top | 100kΩ resistor |
| **Divider middle** | ESP32 **GPIO3** | After first 100kΩ |
| **Divider bottom** | ESP32 **GND** | Second 100kΩ to GND |
| **ESP32 GPIO5** | MOSFET **Gate** | Pump control |
| **MOSFET Source** | ESP32 **GND** | Common ground |
| **MOSFET Drain** | Pump **(−)** | Pump negative wire |
| **Pump (+)** | Battery **(+)** | Pump power |
| **ESP32 GPIO6** | Green LED (+) | Via 220Ω resistor |
| **Green LED (−)** | ESP32 **GND** | Short leg |
| **ESP32 GPIO7** | Red LED (+) | Via 220Ω resistor |
| **Red LED (−)** | ESP32 **GND** | Short leg |
| **Main Button** | GPIO10 ↔ GND | Press = connect |
| **Wet Cal Button** | GPIO20 ↔ GND | Press = connect |
| **Dry Cal Button** | GPIO21 ↔ GND | Press = connect |

---

## 🧪 TESTING BEFORE USE

1. **Power Test**: Plug battery. Green or red LED should NOT light yet (deep sleep).
2. **Button Test**: Press Main button briefly. ESP32 should wake, LEDs may flash.
3. **LED Test**: Code will test LEDs on first boot.
4. **Sensor Test**: Check Serial Monitor (115200 baud) for sensor readings.
5. **Pump Test**: Use manual watering (long-press Main). Pump should run 3 seconds.

---

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
