from __future__ import annotations

import json
import queue
import signal
import sys
import time
from pathlib import Path
from typing import Any, Dict

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
        client_id=raw.get("client_id", "plant-host-receiver"),
        topic_command=raw.get("topic_command", "plant/cmd"),
        topic_status=raw.get("topic_status", "plant/status"),
        topic_telemetry=raw.get("topic_telemetry", "plant/telemetry"),
        topic_ack=raw.get("topic_ack", "plant/ack"),
    )


def ts() -> str:
    return time.strftime("%Y-%m-%d %H:%M:%S")


def print_event(event: Dict[str, Any]) -> None:
    etype = event.get("type", "unknown")

    if etype == "connection":
        state = "connected" if event.get("connected") else "disconnected"
        print(f"[{ts()}] connection {state} rc={event.get('rc')}")
        return

    if etype in ("telemetry", "ack", "status"):
        data = event.get("data")
        if isinstance(data, dict):
            print(f"[{ts()}] {etype} {json.dumps(data, separators=(',', ':'))}")
        else:
            print(f"[{ts()}] {etype} {event.get('payload', '')}")
        return

    if etype == "error":
        print(f"[{ts()}] error {event.get('message', 'unknown')}")
        return

    print(f"[{ts()}] event {event}")


def main() -> None:
    events: "queue.Queue[Dict[str, Any]]" = queue.Queue()
    cfg = load_config()
    logic = PlantMqttLogic(cfg, on_event=events.put)

    running = True

    def _stop_handler(signum, frame) -> None:
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, _stop_handler)
    signal.signal(signal.SIGTERM, _stop_handler)

    logic.start()
    print(f"[{ts()}] receiver started")

    last_status_request = 0.0

    try:
        while running:
            now = time.time()
            if now - last_status_request > 60:
                logic.request_status()
                last_status_request = now

            try:
                event = events.get(timeout=0.5)
                print_event(event)
            except queue.Empty:
                pass
    finally:
        logic.stop()
        print(f"[{ts()}] receiver stopped")


if __name__ == "__main__":
    main()
