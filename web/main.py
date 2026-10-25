"""SOLACE route demo and live ESP telemetry server."""
from __future__ import annotations

import asyncio
import json
import math
import threading
import time
from collections import deque
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException, Query, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse, StreamingResponse

ROOT = Path(__file__).resolve().parent
MAX_READINGS = 5000
LIVE_READING_MAX_AGE_SECONDS = 35
SLOT_COORDS = {"A": (.15, .49), "B": (.49, .19), "C": (.49, .79), "D": (.85, .49)}
SIMULATED_LAMPS = {
    "A": {"temperature": 78., "solar_energy_per_min": 2.8},
    "B": {"temperature": 82., "solar_energy_per_min": 3.5},
    "C": {"temperature": 76., "solar_energy_per_min": 3.2},
    "D": {"temperature": 88., "solar_energy_per_min": 4.1},
}
PATHS = [
    {"start": "A", "end": "B", "distance": 100}, {"start": "A", "end": "C", "distance": 140},
    {"start": "B", "end": "D", "distance": 120}, {"start": "C", "end": "D", "distance": 100},
]


class ReadingStore:
    def __init__(self) -> None:
        self.items: deque[dict[str, Any]] = deque(maxlen=MAX_READINGS)
        self.subscribers: set[asyncio.Queue] = set()
        self.lock = threading.Lock()

    def add(self, item: dict[str, Any]) -> None:
        with self.lock:
            self.items.append(item)
            subscribers = tuple(self.subscribers)
        for subscriber in subscribers:
            if subscriber.full():
                try: subscriber.get_nowait()
                except asyncio.QueueEmpty: pass
            subscriber.put_nowait(item)

    def snapshot(self, device: str | None = None, limit: int = 1000) -> list[dict[str, Any]]:
        with self.lock: items = list(self.items)
        if device: items = [item for item in items if item["device"] == device]
        return items[-limit:]

    def devices(self) -> list[str]:
        with self.lock: return sorted({item["device"] for item in self.items})

    def clear(self) -> None:
        with self.lock: self.items.clear()

    def subscribe(self) -> asyncio.Queue:
        subscriber = asyncio.Queue(maxsize=100)
        with self.lock: self.subscribers.add(subscriber)
        return subscriber

    def unsubscribe(self, subscriber: asyncio.Queue) -> None:
        with self.lock: self.subscribers.discard(subscriber)


STORE = ReadingStore()
app = FastAPI(title="SOLACE", version="1.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["Content-Type"])


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def finite_number(value: Any, name: str) -> float:
    if isinstance(value, bool): raise ValueError(f"{name} must be a number")
    try: result = float(value)
    except (TypeError, ValueError) as exc: raise ValueError(f"{name} must be a number") from exc
    if not math.isfinite(result) or not -1e12 < result < 1e12: raise ValueError(f"{name} is out of range")
    return result


def nearest_slot(x: float, y: float) -> str:
    return min(SLOT_COORDS, key=lambda slot: (x-SLOT_COORDS[slot][0])**2 + (y-SLOT_COORDS[slot][1])**2)


def normalize_reading(payload: Any) -> dict[str, Any]:
    if not isinstance(payload, dict): raise ValueError("reading must be a JSON object")
    device = str(payload.get("device") or payload.get("nodeId") or "esp32")[:80]
    if not device: raise ValueError("device cannot be empty")
    default_slot = device.upper() if device.upper() in SLOT_COORDS else None
    default_x, default_y = SLOT_COORDS.get(default_slot, (.5, .5))
    x = max(0., min(1., finite_number(payload.get("x", payload.get("longitude", default_x)), "x")))
    y = max(0., min(1., finite_number(payload.get("y", payload.get("latitude", default_y)), "y")))
    value, unit = payload.get("value", payload.get("temperatureC")), str(payload.get("unit", "°C"))[:16]
    if value is None and "heatWarning" in payload: value, unit = (1 if payload["heatWarning"] else 0), "heat"
    requested_slot = str(payload.get("slot", default_slot or "")).upper()
    return {"id": f"{time.time_ns():x}", "device": device,
            "slot": requested_slot if requested_slot in SLOT_COORDS else nearest_slot(x, y),
            "x": x, "y": y, "value": finite_number(value, "value"), "unit": unit,
            "timestamp": str(payload.get("timestamp") or utc_now())[:64],
            "receivedEpoch": time.time(),
            "wifiConnected": bool(payload.get("wifiConnected", True)),
            "wifiIp": str(payload.get("wifiIp", ""))[:64],
            "wifiRssi": int(payload.get("wifiRssi", 0))}


def value_fahrenheit(reading: dict[str, Any]) -> float | None:
    unit, value = reading["unit"].strip().lower().replace("°", ""), float(reading["value"])
    if unit in {"c", "celsius"}: return value * 9 / 5 + 32
    if unit in {"f", "fahrenheit"}: return value
    return None


def current_lamps() -> list[dict[str, Any]]:
    latest: dict[str, dict[str, Any]] = {}
    for reading in STORE.snapshot(limit=MAX_READINGS): latest[reading["slot"]] = reading
    lamps = []
    for slot, simulated in SIMULATED_LAMPS.items():
        reading = latest.get(slot)
        age_seconds = max(0, int(time.time() - reading["receivedEpoch"])) if reading else None
        fresh = reading is not None and age_seconds <= LIVE_READING_MAX_AGE_SECONDS
        live_temp = value_fahrenheit(reading) if fresh else None
        temperature = live_temp if live_temp is not None else simulated["temperature"]
        display = f"{temperature:.1f}°F"
        if fresh and reading["unit"].lower() == "heat": display = "HOT" if reading["value"] >= .5 else "clear"
        lamps.append({"id": slot, "temperature": round(temperature, 1),
                      "solar_energy_per_min": simulated["solar_energy_per_min"], "live": fresh,
                      "numeric_temperature_live": live_temp is not None, "display_value": display,
                      "device": reading["device"] if reading else None,
                      "timestamp": reading["timestamp"] if reading else None,
                      "last_seen_seconds": age_seconds,
                      "wifi_ip": reading["wifiIp"] if reading else None,
                      "wifi_rssi": reading["wifiRssi"] if reading else None})
    return lamps


@app.get("/")
def home(): return FileResponse(ROOT / "index.html")

@app.get("/style.css")
def stylesheet():
    return FileResponse(ROOT / "style.css", media_type="text/css",
                        headers={"Cache-Control": "no-store"})

@app.get("/script.js")
def javascript():
    return FileResponse(ROOT / "script.js", media_type="text/javascript",
                        headers={"Cache-Control": "no-store"})

@app.get("/assets/solace-mark.png")
def logo(): return FileResponse(ROOT / "assets" / "solace-mark.png", media_type="image/png")

@app.get("/streetlamps")
def get_streetlamps(): return current_lamps()

@app.get("/paths")
def get_paths(): return PATHS


@app.get("/best-route")
def get_best_route(start: str = "A", end: str = "D"):
    start, end = start.upper(), end.upper()
    if start not in SLOT_COORDS or end not in SLOT_COORDS:
        raise HTTPException(status_code=400, detail="start and end must be A, B, C, or D")

    lamps = {lamp["id"]: lamp for lamp in current_lamps()}
    adjacency: dict[str, list[tuple[str, int]]] = {node: [] for node in SLOT_COORDS}
    for edge in PATHS:
        adjacency[edge["start"]].append((edge["end"], edge["distance"]))
        adjacency[edge["end"]].append((edge["start"], edge["distance"]))

    candidates: list[tuple[list[str], int]] = []

    def walk(node: str, nodes: list[str], distance: int) -> None:
        if node == end:
            candidates.append((nodes, distance))
            return
        for neighbor, edge_distance in adjacency[node]:
            if neighbor not in nodes:
                walk(neighbor, nodes + [neighbor], distance + edge_distance)

    walk(start, [start], 0)

    def route(nodes: list[str], distance: int):
        path = "-".join(nodes)
        temperature = sum(lamps[node]["temperature"] for node in nodes) / len(nodes)
        eta = distance / 1.4 / 60
        energy = sum(lamps[node]["solar_energy_per_min"] for node in nodes)
        return {"path": path, "distance": distance, "average_temperature": round(temperature, 1),
                "score": round(distance + 20*(temperature-75), 1), "eta": round(eta, 1),
                "estimated_energy": round(energy*eta, 1)}

    routes = [route(nodes, distance) for nodes, distance in candidates]
    best = min(routes, key=lambda item: item["score"])
    return {"routes": routes, "selected_route": best,
            "recommended_route": best["path"],
            "using_live_temperature": any(l["numeric_temperature_live"] for l in lamps.values())}


@app.get("/api/health")
def health(): return {"ok": True, "readings": len(STORE.snapshot(limit=MAX_READINGS)), "devices": STORE.devices()}

@app.get("/api/readings")
def readings(limit: int = Query(1000, ge=1, le=MAX_READINGS), device: str | None = None):
    return {"readings": STORE.snapshot(device, limit), "devices": STORE.devices()}


@app.post("/api/reading", status_code=201)
@app.post("/api/readings", status_code=201)
@app.post("/api/telemetry", status_code=201)
@app.post("/api/nodes/telemetry", status_code=201)
async def ingest(request: Request):
    try:
        payload = await request.json()
        raw = payload.get("readings") if isinstance(payload, dict) and "readings" in payload else [payload]
        if not isinstance(raw, list) or not raw or len(raw) > 1000: raise ValueError("readings must contain 1–1000 items")
        accepted = [normalize_reading(item) for item in raw]
    except (ValueError, json.JSONDecodeError) as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    for reading in accepted: STORE.add(reading)
    return JSONResponse({"ok": True, "accepted": len(accepted), "readings": accepted}, status_code=201)


@app.delete("/api/readings")
def clear_readings(): STORE.clear(); return {"ok": True}


@app.get("/api/events")
async def events():
    subscriber = STORE.subscribe()
    async def stream():
        try:
            yield "event: ready\ndata: {}\n\n"
            while True:
                try:
                    reading = await asyncio.wait_for(subscriber.get(), timeout=15)
                    yield f"event: reading\ndata: {json.dumps(reading, separators=(',', ':'))}\n\n"
                except asyncio.TimeoutError: yield ": keepalive\n\n"
        finally: STORE.unsubscribe(subscriber)
    return StreamingResponse(stream(), media_type="text/event-stream", headers={"Cache-Control": "no-cache"})
