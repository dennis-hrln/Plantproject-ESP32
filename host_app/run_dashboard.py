from __future__ import annotations

import json
import os
import queue
from pathlib import Path

from gui.plant_dashboard_gui import PlantDashboardGui
from logic.plant_mqtt_logic import MqttConfig, PlantMqttLogic


def _read_env_or_raw(raw: dict, env_name: str, raw_key: str, default: str) -> str:
    value = os.getenv(env_name)
    if value is not None:
        return value
    return str(raw.get(raw_key, default))


def _read_env_int_or_raw(raw: dict, env_name: str, raw_key: str, default: int) -> int:
    value = os.getenv(env_name)
    if value is not None and value.strip() != "":
        return int(value)
    return int(raw.get(raw_key, default))


def load_config() -> MqttConfig:
    config_path = Path(__file__).with_name("host_config.json")
    raw = {}

    if not config_path.exists():
        raw = {}
    else:
        with config_path.open("r", encoding="utf-8") as f:
            raw = json.load(f)

    return MqttConfig(
        broker_host=_read_env_or_raw(raw, "PLANT_MQTT_BROKER_HOST", "broker_host", "192.168.1.10"),
        broker_port=_read_env_int_or_raw(raw, "PLANT_MQTT_BROKER_PORT", "broker_port", 1883),
        username=_read_env_or_raw(raw, "PLANT_MQTT_USERNAME", "username", ""),
        password=_read_env_or_raw(raw, "PLANT_MQTT_PASSWORD", "password", ""),
        client_id=_read_env_or_raw(raw, "PLANT_MQTT_CLIENT_ID", "client_id", "plant-host-dashboard"),
        topic_command=_read_env_or_raw(raw, "PLANT_MQTT_TOPIC_COMMAND", "topic_command", "plant/cmd"),
        topic_status=_read_env_or_raw(raw, "PLANT_MQTT_TOPIC_STATUS", "topic_status", "plant/status"),
        topic_telemetry=_read_env_or_raw(raw, "PLANT_MQTT_TOPIC_TELEMETRY", "topic_telemetry", "plant/telemetry"),
        topic_ack=_read_env_or_raw(raw, "PLANT_MQTT_TOPIC_ACK", "topic_ack", "plant/ack"),
    )


def main() -> None:
    events: "queue.Queue[dict]" = queue.Queue()
    cfg = load_config()
    logic = PlantMqttLogic(cfg, on_event=events.put)
    logic.start()
    logic.request_deep_sleep_status()

    app = PlantDashboardGui(
        event_queue=events,
        on_request_status=logic.request_status,
        on_water=logic.command_water,
        on_cal_wet=logic.command_calibrate_wet,
        on_cal_dry=logic.command_calibrate_dry,
        on_toggle_sleep=logic.command_toggle_deep_sleep,
        on_set_min=logic.set_min_humidity,
        on_set_max=logic.set_max_humidity,
        on_set_name=logic.set_plant_name,
        on_close=logic.stop,
    )
    app.mainloop()


if __name__ == "__main__":
    main()
