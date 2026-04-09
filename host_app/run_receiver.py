from __future__ import annotations

import json
import os
import queue
import re
import signal
import time
from pathlib import Path
from typing import Any, Dict

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
        client_id=_read_env_or_raw(raw, "PLANT_MQTT_CLIENT_ID", "client_id", "plant-host-receiver"),
        topic_command=_read_env_or_raw(raw, "PLANT_MQTT_TOPIC_COMMAND", "topic_command", "plant/cmd"),
        topic_status=_read_env_or_raw(raw, "PLANT_MQTT_TOPIC_STATUS", "topic_status", "plant/status"),
        topic_telemetry=_read_env_or_raw(raw, "PLANT_MQTT_TOPIC_TELEMETRY", "topic_telemetry", "plant/telemetry"),
        topic_ack=_read_env_or_raw(raw, "PLANT_MQTT_TOPIC_ACK", "topic_ack", "plant/ack"),
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


def _slugify_plant_name(name: str) -> str:
    slug = re.sub(r"[^A-Za-z0-9._-]+", "_", name.strip())
    slug = slug.strip("._-")
    return slug or "unknown_plant"


def _extract_plant_name(event: Dict[str, Any]) -> str:
    data = event.get("data")
    if isinstance(data, dict):
        plant = data.get("plant")
        if isinstance(plant, str) and plant.strip():
            return plant.strip()

    payload = event.get("payload")
    if isinstance(payload, str) and payload.strip().startswith("{"):
        try:
            payload_data = json.loads(payload)
        except json.JSONDecodeError:
            return ""
        if isinstance(payload_data, dict):
            plant = payload_data.get("plant")
            if isinstance(plant, str) and plant.strip():
                return plant.strip()

    return ""


def _is_plant_data_event(event: Dict[str, Any]) -> bool:
    if event.get("type") != "telemetry":
        return False

    data = event.get("data")
    if not isinstance(data, dict):
        return False

    plant = data.get("plant")
    if not isinstance(plant, str) or not plant.strip():
        return False

    # Telemetry measurements should include at least one value besides plant name.
    measurement_keys = ("humidity", "ts", "battery", "water_ok", "min", "max")
    return any(key in data for key in measurement_keys)


def _append_jsonl(path: Path, item: Dict[str, Any]) -> None:
    with path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(item, separators=(",", ":"), ensure_ascii=False) + "\n")


def _persist_plant_data(base_dir: Path, plant_name: str, envelope: Dict[str, Any]) -> None:
    plants_dir = base_dir / "plants"
    plants_dir.mkdir(parents=True, exist_ok=True)

    slug = _slugify_plant_name(plant_name)
    plant_file = plants_dir / f"{slug}.json"

    if plant_file.exists():
        try:
            with plant_file.open("r", encoding="utf-8") as f:
                plant_payload = json.load(f)
        except json.JSONDecodeError:
            plant_payload = {"plant": plant_name, "events": []}
    else:
        plant_payload = {"plant": plant_name, "events": []}

    if not isinstance(plant_payload, dict):
        plant_payload = {"plant": plant_name, "events": []}

    events = plant_payload.get("events")
    if not isinstance(events, list):
        events = []

    plant_payload["plant"] = plant_name
    plant_payload["updated_at"] = envelope.get("received_at", time.time())
    events.append(envelope)
    plant_payload["events"] = events

    with plant_file.open("w", encoding="utf-8") as f:
        json.dump(plant_payload, f, separators=(",", ":"), ensure_ascii=False)


def persist_event(event: Dict[str, Any]) -> None:
    base_dir = Path(__file__).with_name("data")
    base_dir.mkdir(parents=True, exist_ok=True)

    envelope = {
        "received_at": time.time(),
        "event": event,
    }

    events_path = base_dir / "events.jsonl"
    if _is_plant_data_event(event):
        plant_name = _extract_plant_name(event)
        if plant_name:
            _persist_plant_data(base_dir, plant_name, envelope)
        else:
            _append_jsonl(events_path, envelope)
    else:
        _append_jsonl(events_path, envelope)

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

    plants_dir = base_dir / "plants"
    if not plants_dir.exists():
        return

    target_path = plants_dir / f"{_slugify_plant_name(new_name)}.json"
    target_payload: Dict[str, Any] = {"plant": new_name, "events": []}

    if target_path.exists():
        try:
            with target_path.open("r", encoding="utf-8") as f:
                loaded_target = json.load(f)
            if isinstance(loaded_target, dict):
                target_payload = loaded_target
        except json.JSONDecodeError:
            pass

    target_events = target_payload.get("events")
    if not isinstance(target_events, list):
        target_events = []

    changed = False
    for plant_file in plants_dir.glob("*.json"):
        if plant_file == target_path:
            continue

        try:
            with plant_file.open("r", encoding="utf-8") as f:
                content = json.load(f)
        except json.JSONDecodeError:
            continue

        if not isinstance(content, dict):
            continue

        content = _replace_plant_name_in_obj(content, new_name)
        source_events = content.get("events")
        if isinstance(source_events, list):
            target_events.extend(source_events)
            changed = True

        if plant_file != target_path:
            try:
                plant_file.unlink()
            except OSError:
                pass

    if changed:
        target_payload["plant"] = new_name
        target_payload["updated_at"] = time.time()
        target_payload["events"] = target_events
        with target_path.open("w", encoding="utf-8") as f:
            json.dump(target_payload, f, separators=(",", ":"), ensure_ascii=False)


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
