from __future__ import annotations

import json
import queue
import signal
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


def persist_event(event: Dict[str, Any]) -> None:
    base_dir = Path(__file__).with_name("data")
    base_dir.mkdir(parents=True, exist_ok=True)

    envelope = {
        "received_at": time.time(),
        "event": event,
    }

    events_path = base_dir / "events.jsonl"
    with events_path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(envelope, separators=(",", ":")) + "\n")

    etype = event.get("type")
    if etype in ("telemetry", "ack", "status"):
        latest_path = base_dir / f"latest_{etype}.json"
        with latest_path.open("w", encoding="utf-8") as f:
            json.dump(envelope, f, separators=(",", ":"), ensure_ascii=False)

    if etype == "ack":
        data = event.get("data")
        if isinstance(data, dict) and data.get("cmd") == "set_name" and bool(data.get("ok")):
            new_name = data.get("plant")
            if isinstance(new_name, str) and new_name:
                migrate_stored_plant_name(base_dir, new_name)


def _replace_plant_name_in_obj(obj: Any, new_name: str) -> Any:
    if isinstance(obj, dict):
        updated: Dict[str, Any] = {}
        for key, value in obj.items():
            if key == "plant" and isinstance(value, str):
                updated[key] = new_name
            else:
                updated[key] = _replace_plant_name_in_obj(value, new_name)
        return updated
    if isinstance(obj, list):
        return [_replace_plant_name_in_obj(x, new_name) for x in obj]
    return obj


def migrate_stored_plant_name(base_dir: Path, new_name: str) -> None:
    events_path = base_dir / "events.jsonl"
    if events_path.exists():
        rewritten_lines = []
        with events_path.open("r", encoding="utf-8") as f:
            for line in f:
                raw = line.strip()
                if not raw:
                    continue
                try:
                    item = json.loads(raw)
                except json.JSONDecodeError:
                    continue

                item = _replace_plant_name_in_obj(item, new_name)

                event = item.get("event")
                if isinstance(event, dict):
                    payload = event.get("payload")
                    if isinstance(payload, str) and payload.strip().startswith("{"):
                        try:
                            payload_obj = json.loads(payload)
                            payload_obj = _replace_plant_name_in_obj(payload_obj, new_name)
                            event["payload"] = json.dumps(payload_obj, separators=(",", ":"), ensure_ascii=False)
                        except json.JSONDecodeError:
                            pass

                rewritten_lines.append(json.dumps(item, separators=(",", ":"), ensure_ascii=False))

        with events_path.open("w", encoding="utf-8") as f:
            for line in rewritten_lines:
                f.write(line + "\n")

    for name in ("latest_telemetry.json", "latest_ack.json", "latest_status.json"):
        latest_path = base_dir / name
        if not latest_path.exists():
            continue
        try:
            with latest_path.open("r", encoding="utf-8") as f:
                item = json.load(f)
        except json.JSONDecodeError:
            continue

        item = _replace_plant_name_in_obj(item, new_name)

        event = item.get("event")
        if isinstance(event, dict):
            payload = event.get("payload")
            if isinstance(payload, str) and payload.strip().startswith("{"):
                try:
                    payload_obj = json.loads(payload)
                    payload_obj = _replace_plant_name_in_obj(payload_obj, new_name)
                    event["payload"] = json.dumps(payload_obj, separators=(",", ":"), ensure_ascii=False)
                except json.JSONDecodeError:
                    pass

        with latest_path.open("w", encoding="utf-8") as f:
            json.dump(item, f, separators=(",", ":"), ensure_ascii=False)


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
                persist_event(event)
            except queue.Empty:
                pass
    finally:
        logic.stop()
        print(f"[{ts()}] receiver stopped")


if __name__ == "__main__":
    main()
