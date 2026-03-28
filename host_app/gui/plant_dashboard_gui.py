from __future__ import annotations

import queue
import tkinter as tk
from tkinter import ttk, messagebox
from typing import Callable, Dict


class PlantDashboardGui(tk.Tk):
    """Pure GUI layer.

    It only consumes incoming event dictionaries and invokes injected callbacks.
    No MQTT/network code belongs here.
    """

    def __init__(
        self,
        event_queue: "queue.Queue[Dict]",
        on_request_status: Callable[[], None],
        on_water: Callable[[], None],
        on_cal_wet: Callable[[], None],
        on_cal_dry: Callable[[], None],
        on_set_min: Callable[[int], None],
        on_set_max: Callable[[int], None],
        on_set_name: Callable[[str], None],
        on_close: Callable[[], None],
    ) -> None:
        super().__init__()

        self.title("Plant MQTT Dashboard")
        self.geometry("560x500")
        self.minsize(520, 460)

        self._event_queue = event_queue
        self._on_request_status = on_request_status
        self._on_water = on_water
        self._on_cal_wet = on_cal_wet
        self._on_cal_dry = on_cal_dry
        self._on_set_min = on_set_min
        self._on_set_max = on_set_max
        self._on_set_name = on_set_name
        self._on_close = on_close

        self.connection_var = tk.StringVar(value="Disconnected")
        self.humidity_var = tk.StringVar(value="-")
        self.battery_var = tk.StringVar(value="-")
        self.water_ok_var = tk.StringVar(value="-")
        self.timestamp_var = tk.StringVar(value="-")
        self.plant_name_var = tk.StringVar(value="-")
        self.min_var = tk.StringVar(value="-")
        self.max_var = tk.StringVar(value="-")
        self.last_ack_var = tk.StringVar(value="-")

        self._build_ui()
        self.protocol("WM_DELETE_WINDOW", self._handle_close)
        self.after(100, self._drain_events)

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=12)
        root.pack(fill=tk.BOTH, expand=True)

        status_frame = ttk.LabelFrame(root, text="Plant Telemetry", padding=10)
        status_frame.pack(fill=tk.X)

        rows = [
            ("Connection", self.connection_var),
            ("Timestamp", self.timestamp_var),
            ("Plant", self.plant_name_var),
            ("Humidity", self.humidity_var),
            ("Battery", self.battery_var),
            ("Water OK", self.water_ok_var),
            ("Min", self.min_var),
            ("Max", self.max_var),
            ("Last ACK", self.last_ack_var),
        ]

        for i, (label, value_var) in enumerate(rows):
            ttk.Label(status_frame, text=label + ":", width=14).grid(row=i, column=0, sticky=tk.W, pady=2)
            ttk.Label(status_frame, textvariable=value_var).grid(row=i, column=1, sticky=tk.W, pady=2)

        cmd_frame = ttk.LabelFrame(root, text="Commands", padding=10)
        cmd_frame.pack(fill=tk.X, pady=(12, 0))

        ttk.Button(cmd_frame, text="Request Status", command=self._on_request_status).grid(row=0, column=0, padx=4, pady=4, sticky=tk.EW)
        ttk.Button(cmd_frame, text="Water Now", command=self._on_water).grid(row=0, column=1, padx=4, pady=4, sticky=tk.EW)
        ttk.Button(cmd_frame, text="Calibrate Wet", command=self._on_cal_wet).grid(row=1, column=0, padx=4, pady=4, sticky=tk.EW)
        ttk.Button(cmd_frame, text="Calibrate Dry", command=self._on_cal_dry).grid(row=1, column=1, padx=4, pady=4, sticky=tk.EW)

        for col in range(2):
            cmd_frame.grid_columnconfigure(col, weight=1)

        set_frame = ttk.LabelFrame(root, text="Set Values", padding=10)
        set_frame.pack(fill=tk.X, pady=(12, 0))

        ttk.Label(set_frame, text="Min Humidity (0-100)").grid(row=0, column=0, sticky=tk.W, pady=4)
        self.min_entry = ttk.Entry(set_frame)
        self.min_entry.grid(row=0, column=1, sticky=tk.EW, padx=6)
        ttk.Button(set_frame, text="Set Min", command=self._submit_min).grid(row=0, column=2, padx=4)

        ttk.Label(set_frame, text="Max Humidity (0-100)").grid(row=1, column=0, sticky=tk.W, pady=4)
        self.max_entry = ttk.Entry(set_frame)
        self.max_entry.grid(row=1, column=1, sticky=tk.EW, padx=6)
        ttk.Button(set_frame, text="Set Max", command=self._submit_max).grid(row=1, column=2, padx=4)

        ttk.Label(set_frame, text="Plant Name").grid(row=2, column=0, sticky=tk.W, pady=4)
        self.name_entry = ttk.Entry(set_frame)
        self.name_entry.grid(row=2, column=1, sticky=tk.EW, padx=6)
        ttk.Button(set_frame, text="Set Name", command=self._submit_name).grid(row=2, column=2, padx=4)

        set_frame.grid_columnconfigure(1, weight=1)

        log_frame = ttk.LabelFrame(root, text="Event Log", padding=10)
        log_frame.pack(fill=tk.BOTH, expand=True, pady=(12, 0))

        self.log = tk.Text(log_frame, height=10, state=tk.DISABLED)
        self.log.pack(fill=tk.BOTH, expand=True)

    def _append_log(self, text: str) -> None:
        self.log.configure(state=tk.NORMAL)
        self.log.insert(tk.END, text + "\n")
        self.log.see(tk.END)
        self.log.configure(state=tk.DISABLED)

    def _submit_min(self) -> None:
        try:
            value = int(self.min_entry.get().strip())
        except ValueError:
            messagebox.showerror("Invalid value", "Min humidity must be an integer")
            return
        self._on_set_min(value)

    def _submit_max(self) -> None:
        try:
            value = int(self.max_entry.get().strip())
        except ValueError:
            messagebox.showerror("Invalid value", "Max humidity must be an integer")
            return
        self._on_set_max(value)

    def _submit_name(self) -> None:
        name = self.name_entry.get().strip()
        if not name:
            messagebox.showerror("Invalid value", "Plant name must not be empty")
            return
        self._on_set_name(name)

    def _handle_close(self) -> None:
        self._on_close()
        self.destroy()

    def _drain_events(self) -> None:
        while True:
            try:
                event = self._event_queue.get_nowait()
            except queue.Empty:
                break
            self._handle_event(event)

        self.after(100, self._drain_events)

    def _handle_event(self, event: Dict) -> None:
        etype = event.get("type", "unknown")

        if etype == "connection":
            self.connection_var.set("Connected" if event.get("connected") else "Disconnected")
            self._append_log(f"connection: {self.connection_var.get()} (rc={event.get('rc')})")
            return

        if etype == "telemetry":
            data = event.get("data") or {}
            self.timestamp_var.set(str(data.get("ts", "-")))
            self.plant_name_var.set(str(data.get("plant", "-")))
            self.humidity_var.set(str(data.get("humidity", "-")))
            self.battery_var.set(str(data.get("battery", "-")))
            self.water_ok_var.set(str(data.get("water_ok", "-")))
            self.min_var.set(str(data.get("min", "-")))
            self.max_var.set(str(data.get("max", "-")))
            self._append_log("telemetry updated")
            return

        if etype == "ack":
            data = event.get("data") or {}
            ack_line = f"{data.get('cmd', '?')} ok={data.get('ok', False)} detail={data.get('detail', '')}"
            self.last_ack_var.set(ack_line)
            self._append_log("ack: " + ack_line)
            return

        if etype == "status":
            self._append_log("status: " + str(event.get("payload", "")))
            return

        if etype == "error":
            msg = str(event.get("message", "Unknown error"))
            self._append_log("error: " + msg)
            messagebox.showerror("MQTT Error", msg)
            return

        self._append_log(str(event))
