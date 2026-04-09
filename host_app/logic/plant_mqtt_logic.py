from __future__ import annotations

import json
import threading
import time
import uuid
from dataclasses import dataclass
from typing import Callable, Dict, Optional

import paho.mqtt.client as mqtt


@dataclass
class MqttConfig:
    broker_host: str = "192.168.1.10"
    broker_port: int = 1883
    username: str = ""
    password: str = ""
    client_id: str = "plant-host-dashboard"
    topic_command: str = "plant/cmd"
    topic_status: str = "plant/status"
    topic_telemetry: str = "plant/telemetry"
    topic_ack: str = "plant/ack"


class PlantMqttLogic:
    """MQTT communication and command logic.

    This module is intentionally GUI-agnostic. It communicates via callbacks.
    """

    def __init__(
        self,
        config: MqttConfig,
        on_event: Callable[[Dict], None],
    ) -> None:
        self._config = config
        self._on_event = on_event
        self._stop_event = threading.Event()
        self._connected = False
        self._pending_lock = threading.Lock()
        self._pending_commands: Dict[str, Dict] = {}
        self._retry_thread: Optional[threading.Thread] = None

        self._client = mqtt.Client(client_id=self._config.client_id, protocol=mqtt.MQTTv311)
        if self._config.username:
            self._client.username_pw_set(self._config.username, self._config.password)

        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message

    @property
    def connected(self) -> bool:
        return self._connected

    def start(self) -> None:
        try:
            # Non-blocking connect prevents startup crashes when broker is down.
            self._client.connect_async(self._config.broker_host, self._config.broker_port, keepalive=30)
            self._client.loop_start()
            self._retry_thread = threading.Thread(target=self._retry_pending_loop, daemon=True)
            self._retry_thread.start()
        except Exception as exc:
            self._connected = False
            self._emit({"type": "error", "message": f"MQTT start failed: {exc}"})

    def stop(self) -> None:
        self._stop_event.set()
        try:
            self._client.loop_stop()
            self._client.disconnect()
            if self._retry_thread and self._retry_thread.is_alive():
                self._retry_thread.join(timeout=1.0)
        finally:
            self._connected = False

    def _emit(self, event: Dict) -> None:
        self._on_event(event)

    def _on_connect(self, client: mqtt.Client, userdata, flags, rc: int) -> None:
        self._connected = rc == 0
        self._emit({"type": "connection", "connected": self._connected, "rc": rc})
        if self._connected:
            client.subscribe(self._config.topic_status)
            client.subscribe(self._config.topic_telemetry)
            client.subscribe(self._config.topic_ack)
            self._flush_pending(force=True)

    def _on_disconnect(self, client: mqtt.Client, userdata, rc: int) -> None:
        self._connected = False
        self._emit({"type": "connection", "connected": False, "rc": rc})

    def _on_message(self, client: mqtt.Client, userdata, msg: mqtt.MQTTMessage) -> None:
        payload = msg.payload.decode("utf-8", errors="replace")
        parsed: Optional[Dict] = None
        try:
            parsed = json.loads(payload)
        except json.JSONDecodeError:
            parsed = None

        topic = msg.topic
        if topic == self._config.topic_telemetry:
            self._emit({"type": "telemetry", "topic": topic, "payload": payload, "data": parsed})
            return

        if topic == self._config.topic_ack:
            self._handle_ack(parsed)
            self._emit({"type": "ack", "topic": topic, "payload": payload, "data": parsed})
            return

        self._emit({"type": "status", "topic": topic, "payload": payload, "data": parsed})

    def _publish_packet(self, payload: str) -> bool:
        result = self._client.publish(self._config.topic_command, payload, qos=1, retain=False)
        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            self._emit({"type": "error", "message": f"Publish failed: rc={result.rc}"})
            return False
        return True

    def _publish_command(self, command: str, ack_cmd: str) -> None:
        cmd_id = f"{int(time.time() * 1000)}-{uuid.uuid4().hex[:8]}"
        packet = json.dumps({"id": cmd_id, "cmd": command}, separators=(",", ":"))

        with self._pending_lock:
            self._pending_commands[cmd_id] = {
                "id": cmd_id,
                "command": command,
                "ack_cmd": ack_cmd,
                "packet": packet,
                "last_sent": 0.0,
                "retries": 0,
            }

        self._emit({"type": "status", "payload": f"queued command {command} ({cmd_id})"})
        self._flush_pending(force=True)

    def _retry_pending_loop(self) -> None:
        while not self._stop_event.is_set():
            self._flush_pending(force=False)
            time.sleep(1.0)

    def _flush_pending(self, force: bool) -> None:
        if not self._connected:
            return

        now = time.time()
        to_send = []
        with self._pending_lock:
            for cmd_id, item in self._pending_commands.items():
                if force or (now - float(item.get("last_sent", 0.0)) >= 2.0):
                    to_send.append((cmd_id, item.copy()))

        for cmd_id, item in to_send:
            sent = self._publish_packet(str(item.get("packet", "")))
            if not sent:
                continue
            with self._pending_lock:
                current = self._pending_commands.get(cmd_id)
                if current is None:
                    continue
                current["last_sent"] = now
                current["retries"] = int(current.get("retries", 0)) + 1

    def _handle_ack(self, parsed: Optional[Dict]) -> None:
        if not isinstance(parsed, dict):
            return

        ack_id = parsed.get("id")
        ack_cmd = parsed.get("cmd")

        if isinstance(ack_id, str) and ack_id:
            with self._pending_lock:
                removed = self._pending_commands.pop(ack_id, None)
            if removed is not None:
                return

        if not isinstance(ack_cmd, str) or not ack_cmd:
            return

        # Fallback for older firmware ACK payloads without command id.
        with self._pending_lock:
            pending_id = None
            for cmd_id, item in self._pending_commands.items():
                if item.get("ack_cmd") == ack_cmd:
                    pending_id = cmd_id
                    break
            if pending_id:
                self._pending_commands.pop(pending_id, None)

    def request_status(self) -> None:
        self._publish_command("status", ack_cmd="status")

    def command_water(self) -> None:
        self._publish_command("water", ack_cmd="water")

    def command_calibrate_wet(self) -> None:
        self._publish_command("calibrate_wet", ack_cmd="calibrate_wet")

    def command_calibrate_dry(self) -> None:
        self._publish_command("calibrate_dry", ack_cmd="calibrate_dry")

    def command_toggle_deep_sleep(self) -> None:
        self._publish_command("toggle_sleep", ack_cmd="set_sleep")

    def command_set_deep_sleep(self, enabled: bool) -> None:
        state = "on" if enabled else "off"
        self._publish_command(f"set_sleep:{state}", ack_cmd="set_sleep")

    def request_deep_sleep_status(self) -> None:
        self._publish_command("sleep_status", ack_cmd="set_sleep")

    def set_min_humidity(self, value: int) -> None:
        value = max(0, min(100, int(value)))
        self._publish_command(f"set_min:{value}", ack_cmd="set_min")

    def set_max_humidity(self, value: int) -> None:
        value = max(0, min(100, int(value)))
        self._publish_command(f"set_max:{value}", ack_cmd="set_max")

    def set_plant_name(self, name: str) -> None:
        cleaned = (name or "").strip()
        if not cleaned:
            self._emit({"type": "error", "message": "Plant name must not be empty"})
            return
        self._publish_command(f"set_name:{cleaned}", ack_cmd="set_name")
