#!/usr/bin/env python3
"""
Capture stepper voltage DATA lines from test_stepper_motor firmware,
store CSV, and create a voltage-over-time graph.

Expected firmware line format:
    DATA,<esp_ms>,<motor_on>,<adc_raw>,<adc_mv>,<measured_v>
"""

from __future__ import annotations

import argparse
import csv
import json
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import List, Optional, Tuple

import matplotlib.pyplot as plt
from matplotlib.axes import Axes
from matplotlib.figure import Figure
from matplotlib.lines import Line2D
import serial
from serial.tools import list_ports


DEFAULT_CONFIG_PATH = Path("test/stepper_motor_test/capture_config.json")


@dataclass
class Sample:
    pc_timestamp_utc: str
    esp_ms: int
    motor_on: int
    adc_raw: int
    adc_mv: int
    measured_v: float


def parse_data_line(line: str) -> Optional[Sample]:
    if not line.startswith("DATA,"):
        return None
    parts = line.strip().split(",")
    if len(parts) != 6:
        return None
    try:
        return Sample(
            pc_timestamp_utc=datetime.now(timezone.utc).isoformat(),
            esp_ms=int(parts[1]),
            motor_on=int(parts[2]),
            adc_raw=int(parts[3]),
            adc_mv=int(parts[4]),
            measured_v=float(parts[5]),
        )
    except ValueError:
        return None


def guess_serial_port() -> Optional[str]:
    ports = list(list_ports.comports())
    for p in ports:
        hwid = (p.hwid or "").upper()
        if "303A:1001" in hwid:
            return p.device
    return ports[0].device if ports else None


def load_config(path: Path) -> dict:
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)
        return data if isinstance(data, dict) else {}


def open_serial_with_retry(port: str, baud: int, retries: int, retry_delay_s: float) -> serial.Serial:
    last_error: Optional[Exception] = None
    attempts = max(1, retries + 1)

    for attempt in range(1, attempts + 1):
        try:
            return serial.Serial(port, baud, timeout=0.5)
        except serial.SerialException as exc:
            last_error = exc
            if attempt < attempts:
                print(f"Open failed ({exc}). Retrying {attempt}/{attempts - 1} in {retry_delay_s:.1f}s...")
                time.sleep(max(0.1, retry_delay_s))

    assert last_error is not None
    raise RuntimeError(
        f"Could not open serial port {port} at {baud} baud. "
        "The port is likely in use by another app (PlatformIO monitor, Arduino Serial Monitor, another Python process). "
        "Close other serial tools and try again."
    ) from last_error


def save_csv(samples: List[Sample], out_csv: Path) -> None:
    out_csv.parent.mkdir(parents=True, exist_ok=True)
    with out_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["pc_timestamp_utc", "esp_ms", "motor_on", "adc_raw", "adc_mv", "measured_v"])
        for s in samples:
            writer.writerow([s.pc_timestamp_utc, s.esp_ms, s.motor_on, s.adc_raw, s.adc_mv, f"{s.measured_v:.6f}"])


def save_plot(samples: List[Sample], out_png: Path, title: str) -> None:
    if not samples:
        raise RuntimeError("No samples to plot")

    t0 = samples[0].esp_ms
    time_s = [(s.esp_ms - t0) / 1000.0 for s in samples]
    volts = [s.measured_v for s in samples]
    state = [s.motor_on for s in samples]

    plt.figure(figsize=(12, 5))
    plt.plot(time_s, volts, linewidth=1.4, label="Measured voltage (V)")

    in_off = False
    off_start = 0.0
    for i, motor_on in enumerate(state):
        t = time_s[i]
        if motor_on == 0 and not in_off:
            in_off = True
            off_start = t
        elif motor_on == 1 and in_off:
            in_off = False
            plt.axvspan(off_start, t, color="red", alpha=0.12)
    if in_off:
        plt.axvspan(off_start, time_s[-1], color="red", alpha=0.12)

    plt.title(title)
    plt.xlabel("Time (s) [ESP millis]")
    plt.ylabel("Voltage (V)")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    out_png.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_png, dpi=140)
    plt.close()


def init_live_plot(title: str) -> Tuple[Figure, Axes, Line2D]:
    plt.ion()
    fig, ax = plt.subplots(figsize=(12, 5))
    (line,) = ax.plot([], [], linewidth=1.4, label="Measured voltage (V)")
    ax.set_title(title)
    ax.set_xlabel("Time (s) [ESP millis]")
    ax.set_ylabel("Voltage (V)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    return fig, ax, line


def update_live_plot(samples: List[Sample], ax: Axes, line: Line2D) -> None:
    if not samples:
        return

    t0 = samples[0].esp_ms
    time_s = [(s.esp_ms - t0) / 1000.0 for s in samples]
    volts = [s.measured_v for s in samples]

    line.set_data(time_s, volts)

    x_min = time_s[0]
    x_max = max(time_s[-1], x_min + 1.0)
    y_min = min(volts)
    y_max = max(volts)
    if y_max - y_min < 0.05:
        y_pad = 0.05
    else:
        y_pad = 0.1 * (y_max - y_min)

    ax.set_xlim(x_min, x_max)
    ax.set_ylim(y_min - y_pad, y_max + y_pad)
    ax.figure.canvas.draw_idle()
    ax.figure.canvas.flush_events()


def main() -> None:
    parser = argparse.ArgumentParser(description="Capture stepper voltage and plot it")
    parser.add_argument("--config", default=str(DEFAULT_CONFIG_PATH), help="Path to JSON config file")
    parser.add_argument("--port", default=None, help="Serial port (for example COM8)")
    parser.add_argument("--baud", type=int, default=None)
    parser.add_argument("--duration", type=float, default=None, help="Capture duration in seconds; 0 means run until live graph is closed")
    parser.add_argument("--csv", default=None)
    parser.add_argument("--png", default=None)
    parser.add_argument("--live", action="store_true", help="Show live graph while capturing (overrides config)")
    parser.add_argument("--live-refresh", type=float, default=None, help="Live graph refresh interval in seconds")
    parser.add_argument("--open-retries", type=int, default=None, help="How many times to retry opening serial port if busy")
    parser.add_argument("--open-retry-delay", type=float, default=None, help="Seconds between serial open retries")
    args = parser.parse_args()

    cfg = load_config(Path(args.config))

    port = args.port if args.port is not None else cfg.get("port")
    if not port:
        port = guess_serial_port()

    baud = int(args.baud if args.baud is not None else cfg.get("baud", 115200))
    duration = float(args.duration if args.duration is not None else cfg.get("measurement_time_s", 120.0))
    out_csv = Path(args.csv if args.csv is not None else cfg.get("output_csv", "test/stepper_motor_test/data/stepper_voltage_capture.csv"))
    out_png = Path(args.png if args.png is not None else cfg.get("output_png", "test/stepper_motor_test/data/stepper_voltage_capture.png"))
    live_refresh = float(args.live_refresh if args.live_refresh is not None else cfg.get("live_refresh_s", 0.2))
    open_retries = int(args.open_retries if args.open_retries is not None else cfg.get("open_retries", 3))
    open_retry_delay = float(args.open_retry_delay if args.open_retry_delay is not None else cfg.get("open_retry_delay_s", 1.0))

    live_cfg = bool(cfg.get("live", False))
    live_enabled = args.live or live_cfg

    if not port:
        raise RuntimeError("No serial port found. Connect ESP32 and pass --port COMx")

    print(f"Opening {port} @ {baud} baud")
    if duration == 0:
        print("Capturing until live graph window is closed")
    else:
        print(f"Capturing for {duration:.1f}s")
    if live_enabled:
        print("Live graph enabled")

    samples: List[Sample] = []
    start = datetime.now(timezone.utc)
    last_live_update = start

    live_fig = None
    live_ax = None
    live_line = None
    if live_enabled:
        live_fig, live_ax, live_line = init_live_plot(f"Live Stepper Voltage ({port})")

    with open_serial_with_retry(port, baud, open_retries, open_retry_delay) as ser:
        while True:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if line:
                sample = parse_data_line(line)
                if sample is not None:
                    samples.append(sample)
                else:
                    print(line)

            if live_enabled and samples and live_ax is not None and live_line is not None:
                now = datetime.now(timezone.utc)
                if (now - last_live_update).total_seconds() >= max(live_refresh, 0.05):
                    update_live_plot(samples, live_ax, live_line)
                    last_live_update = now

            if duration == 0:
                if live_fig is not None and not plt.fignum_exists(live_fig.number):
                    break
            else:
                elapsed = (datetime.now(timezone.utc) - start).total_seconds()
                if elapsed >= duration:
                    break

    if live_enabled and samples and live_ax is not None and live_line is not None:
        update_live_plot(samples, live_ax, live_line)
        plt.ioff()
        plt.show(block=False)

    if not samples:
        raise RuntimeError("No DATA lines received. Check monitor conflicts, baud, and firmware upload")

    save_csv(samples, out_csv)
    save_plot(samples, out_png, f"Stepper Voltage over Time ({port})")

    print(f"Saved {len(samples)} samples to {out_csv}")
    print(f"Saved plot to {out_png}")


if __name__ == "__main__":
    main()
