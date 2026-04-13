# Stepper Motor Test

This folder contains a standalone stepper driver test for ESP32-C3 SuperMini.

Files:
- `test_stepper_motor.cpp`: Main motor test (15s ON / 5s OFF)
- `measure_voltage.cpp` + `measure_voltage.h`: Optional ADC-based voltage logging helper
- `capture_stepper_voltage.py`: PC script to capture serial data and plot voltage over time
- `requirements.txt`: Python dependencies for the capture script

## What To Run When

### 1) Motor should just spin (no voltage logging)

Use this when you only want to verify motor/driver movement.

1. In `test_stepper_motor.cpp`, keep:
	- `#define ENABLE_VOLTAGE_MEASURE 0`
2. Build + upload `stepper_motor_test` environment.
3. Optional: open serial monitor for diagnostics.

Active firmware file in this mode:
- `test_stepper_motor.cpp`

### 2) Motor test + voltage data over serial

Use this when you want a CSV stream (`DATA,...`) and later a graph.

1. In `test_stepper_motor.cpp`, set:
	- `#define ENABLE_VOLTAGE_MEASURE 1`
2. Build + upload `stepper_motor_test` environment.
3. On PC, run `capture_stepper_voltage.py` to store CSV and create plot.

Active firmware files in this mode:
- `test_stepper_motor.cpp`
- `measure_voltage.cpp` / `measure_voltage.h`

Active PC files in this mode:
- `requirements.txt`
- `capture_stepper_voltage.py`

### 3) Only install Python dependencies

Use this once or when setting up a new venv:

```powershell
pip install -r test/stepper_motor_test/requirements.txt
```

## Pin Mapping

The motor test uses pins from `include/config.h`:
- STEP: `PIN_STEPPER_STEP` (currently GPIO21)
- DIR: `PIN_STEPPER_DIR` (currently GPIO20)
- EN: `PIN_STEPPER_EN` (currently GPIO8, active LOW)
- Measure Voltage `MEASURE_ADC_PIN` (currently GPIO4)
## Wiring

### 1) Motor disconnected (driver-only voltage test)

Use this to safely verify driver switching without the motor attached.

1. ESP32 GND -> Driver GND
2. ESP32 GPIO21 -> Driver STEP
3. ESP32 GPIO20 -> Driver DIR
4. ESP32 GPIO8 -> Driver EN
5. Driver VMOT/GND -> Motor power supply (according to driver rating)
6. Driver SLEEP and RESET must be HIGH (or tied together HIGH)
7. Leave motor outputs A1/A2/B1/B2 disconnected from the motor
8. Connect measurement path to ESP32 ADC pin (GPIO4 by default in `measure_voltage.cpp`):
	- Driver point to measure -> resistor divider -> GPIO4
	- Divider ground -> common GND
	- Keep ADC input within 0-3.3V
	- Divider values used in code: top resistor `10k`, bottom resistor `20k`
	- Conversion in firmware: `V_measured = V_adc * (10k + 20k) / 20k = V_adc * 1.5`
	- This divider is only a measurement tap (parallel), not in series with the motor path

Measurement tap topology:

```text
Driver output node -----+--------------------> original load path (unchanged)
								|
							  R1 (10k)
								|
							ADC pin (GPIO4)
								|
							  R2 (20k)
								|
							  GND
```

The original load path remains direct. Example:

```text
Driver output node ----> motor ----> return/GND (as originally wired)
```

Recommended firmware setting for this mode:
- `#define ENABLE_VOLTAGE_MEASURE 1`

### 2) Motor connected + voltage capture + plot

Use this to test real operation under load and collect voltage data.

1. ESP32 GND -> Driver GND
2. ESP32 GPIO21 -> Driver STEP
3. ESP32 GPIO20 -> Driver DIR
4. ESP32 GPIO8 -> Driver EN
5. Driver VMOT/GND -> Motor power supply
6. Driver A1/A2/B1/B2 -> Stepper motor coils
7. Driver SLEEP and RESET must be HIGH (or tied together HIGH)
8. Also connect voltage measurement divider to GPIO4 as above
	- Divider values used in code: top resistor `10k`, bottom resistor `20k`
	- Conversion in firmware: `V_measured = V_adc * 1.5`
	- Motor wiring stays direct to the driver output; divider is only a high-impedance tap

Recommended firmware setting for this mode:
- `#define ENABLE_VOLTAGE_MEASURE 1`

Important safety note:
- Never connect high driver output voltage directly to ESP32 ADC.
- Use a resistor divider so ADC stays within 0-3.3V.
- Keep common ground between ESP32, driver logic, and motor supply.

## Build and Upload

Single environment is used: `stepper_motor_test`.

Build:
```powershell
C:\Users\denni\.platformio\penv\Scripts\platformio.exe run --environment stepper_motor_test
```

Upload:
```powershell
C:\Users\denni\.platformio\penv\Scripts\platformio.exe run --target upload --environment stepper_motor_test
```

Monitor:
```powershell
C:\Users\denni\.platformio\penv\Scripts\platformio.exe device monitor -p COM8 -b 115200
```

If monitor fails with "Access is denied", close other serial monitors first.

## Python Capture and Plot (Optional)

Install dependencies:
```powershell
pip install -r test/stepper_motor_test/requirements.txt
```

Run capture:
```powershell
python test/stepper_motor_test/capture_stepper_voltage.py --port COM8 --duration 120
```

Outputs:
- `test/stepper_motor_test/data/stepper_voltage_capture.csv`
- `test/stepper_motor_test/data/stepper_voltage_capture.png`
