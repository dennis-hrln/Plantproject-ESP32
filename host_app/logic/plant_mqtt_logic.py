from __future__ import annotations

import json
import threading
import time
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
        self._client.connect(self._config.broker_host, self._config.broker_port, keepalive=30)
        self._client.loop_start()

    def stop(self) -> None:
        self._stop_event.set()
        try:
            self._client.loop_stop()
            self._client.disconnect()
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
            self._emit({"type": "ack", "topic": topic, "payload": payload, "data": parsed})
            return

        self._emit({"type": "status", "topic": topic, "payload": payload, "data": parsed})

    def _publish_command(self, command: str) -> None:
        if not self._connected:
            self._emit({"type": "error", "message": "Not connected to MQTT broker"})
            return
        result = self._client.publish(self._config.topic_command, command, qos=0, retain=False)
        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            self._emit({"type": "error", "message": f"Publish failed: rc={result.rc}"})

    def request_status(self) -> None:
        self._publish_command("status")

    def command_water(self) -> None:
        self._publish_command("water")

    def command_calibrate_wet(self) -> None:
        self._publish_command("calibrate_wet")

    def command_calibrate_dry(self) -> None:
        self._publish_command("calibrate_dry")

    def set_min_humidity(self, value: int) -> None:
        value = max(0, min(100, int(value)))
        self._publish_command(f"set_min:{value}")

    def set_max_humidity(self, value: int) -> None:
        value = max(0, min(100, int(value)))
        self._publish_command(f"set_max:{value}")

    def set_plant_name(self, name: str) -> None:
        cleaned = (name or "").strip()
        if not cleaned:
            self._emit({"type": "error", "message": "Plant name must not be empty"})
            return
        self._publish_command(f"set_name:{cleaned}")
