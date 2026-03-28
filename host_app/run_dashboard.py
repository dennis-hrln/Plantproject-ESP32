from __future__ import annotations

import json
import os
import queue
from pathlib import Path

from gui.plant_dashboard_gui import PlantDashboardGui
from logic.plant_mqtt_logic import MqttConfig, PlantMqttLogic


def load_config() -> MqttConfig:
    config_path = Path(__file__).with_name("host_config.json")
    if not config_path.exists():
        return MqttConfig()

    with config_path.open("r", encoding="utf-8") as f:
        raw = json.load(f)

    return MqttConfig(
        broker_host=raw.get("broker_host", "192.168.1.10"),
        broker_port=int(raw.get("broker_port", 1883)),
        username=raw.get("username", ""),
        password=raw.get("password", ""),
        client_id=raw.get("client_id", "plant-host-dashboard"),
        topic_command=raw.get("topic_command", "plant/cmd"),
        topic_status=raw.get("topic_status", "plant/status"),
        topic_telemetry=raw.get("topic_telemetry", "plant/telemetry"),
        topic_ack=raw.get("topic_ack", "plant/ack"),
    )


def main() -> None:
    events: "queue.Queue[dict]" = queue.Queue()
    cfg = load_config()
    logic = PlantMqttLogic(cfg, on_event=events.put)
    logic.start()

    app = PlantDashboardGui(
        event_queue=events,
        on_request_status=logic.request_status,
        on_water=logic.command_water,
        on_cal_wet=logic.command_calibrate_wet,
        on_cal_dry=logic.command_calibrate_dry,
        on_set_min=logic.set_min_humidity,
        on_set_max=logic.set_max_humidity,
        on_set_name=logic.set_plant_name,
        on_close=logic.stop,
    )
    app.mainloop()


if __name__ == "__main__":
    main()
