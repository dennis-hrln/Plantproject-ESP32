Host Dashboard (Laptop / Raspberry Pi)

Purpose
- Receives MQTT telemetry and ACK messages from the ESP32.
- Displays current plant values in a desktop GUI.
- Sends commands and setpoint updates back to the ESP32.

Architecture (strict separation)
- logic/plant_mqtt_logic.py: MQTT transport + command/telemetry logic only.
- gui/plant_dashboard_gui.py: GUI only (no MQTT/network code).
- run_dashboard.py: wiring/composition layer connecting GUI and logic.

Setup
1. Install Python 3.10+.
2. Install dependencies:
   pip install -r requirements.txt
3. Copy host_config.example.json to host_config.json and adjust values.

Run
- From this folder:
  python run_dashboard.py

Headless Receiver (no GUI)
- From this folder:
  python run_receiver.py

This mode is intended for laptop or Raspberry Pi service usage.
It subscribes to status/telemetry/ack, prints incoming data, and requests status every 60 seconds.

Supported Commands from GUI
- Request Status
- Water Now
- Calibrate Wet
- Calibrate Dry
- Set Min Humidity
- Set Max Humidity
- Set Plant Name

MQTT Topics (default)
- Command publish: plant/cmd
- Status subscribe: plant/status
- Telemetry subscribe: plant/telemetry
- Ack subscribe: plant/ack

Command Payload Examples
- status
- water
- calibrate_wet
- calibrate_dry
- set_min:45
- set_max:70
- set_name:Office Fern

Receiver Data Persistence
- The headless receiver stores all incoming events to:
  host_app/data/events.jsonl
- Latest snapshots are written to:
  host_app/data/latest_telemetry.json
  host_app/data/latest_ack.json
  host_app/data/latest_status.json

Raspberry Pi service (systemd)
1. Copy host_app/systemd/plant_receiver.service.example to /etc/systemd/system/plant_receiver.service
2. Adjust User, WorkingDirectory, and ExecStart paths.
3. Run:
  sudo systemctl daemon-reload
  sudo systemctl enable plant_receiver
  sudo systemctl start plant_receiver
4. Check logs:
  journalctl -u plant_receiver -f
