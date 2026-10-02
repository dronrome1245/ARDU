#!/usr/bin/env python3
"""ARDU ESP8266 v1 stateful development mock.

Run on a PC in the same LAN as the Android phone:
    python mock_esp_v1.py

Then enter <PC_IP>:8080 in ARDU Android -> Settings -> ARDU address.
No external Python packages are required.
"""

from __future__ import annotations

import argparse
import json
import threading
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import urlparse


MUSIC_IDS = ("M01", "M02", "M03", "M04", "M05", "M08", "M09")
AMBIENT_IDS = ("F01", "F02", "F03")


def default_music() -> dict[str, Any]:
    out: dict[str, Any] = {}
    for mode in MUSIC_IDS:
        out[mode] = {
            "brightness": 128,
            "background_brightness": 20,
            "smoothing": 40,
            "sensitivity": 100,
            "speed": 30,
            "aux": 10,
            "submode": 0,
        }
    out["M05"]["submode"] = 1
    out["M08"]["submode"] = 2
    out["M09"]["speed"] = 8
    out["M09"]["aux"] = 6
    return out


class MockState:
    def __init__(self) -> None:
        self.lock = threading.RLock()
        self.mode = "light"
        self.light = {
            "color_mode": "kelvin",
            "kelvin": 2700,
            "rgb": {"r": 255, "g": 170, "b": 87},
            "brightness": 64,
            "saved": True,
            "dirty": False,
        }
        self.clap = {
            "enabled": True,
            "threshold": 70,
            "timeout_ms": 500,
            "saved": True,
            "calibration": {
                "active": False,
                "finished": False,
                "good_pairs": 0,
                "target_pairs": 0,
                "quiet_p99_derivative": 18,
                "suggested_threshold": 0,
            },
        }
        self.night = {
            "enabled": False,
            "hue": 24,
            "saturation": 180,
            "brightness": 18,
            "schedule_enabled": False,
            "schedule_on": "22:00",
            "schedule_off": "07:00",
        }
        self.alarm = {
            "enabled": False,
            "hour": 7,
            "minute": 0,
            "fade_minutes": 30,
            "max_brightness": 120,
            "start_hue": 8,
            "end_hue": 32,
            "dawn_phase": "idle",
            "recovered": False,
        }
        self.ambient = {
            "effect": "F01",
            "auto_cycle": False,
            "auto_period_s": 10,
            "F01": {"hue": 32, "saturation": 255, "brightness": 80},
            "F02": {"hue": 40, "saturation": 220, "brightness": 75, "speed": 30},
            "F03": {"hue": 0, "brightness": 90, "speed": 25, "rainbow_step": 0.5},
        }
        self.music = {"selected": "M01", "modes": default_music()}
        self.system = {
            "current_limit_ma": 3000,
            "audio_calibrated": False,
            "mic_dc": 234,
            "vu_low_pass": 280,
            "spectrum_low_pass": 36,
        }
        self.events: list[dict[str, Any]] = []
        self.started = datetime.now()

    def settings(self) -> dict[str, Any]:
        with self.lock:
            return {
                "ok": True,
                "schema": 1,
                "mode": self.mode,
                "light": json.loads(json.dumps(self.light)),
                "clap": json.loads(json.dumps(self.clap)),
                "night": json.loads(json.dumps(self.night)),
                "alarm": json.loads(json.dumps(self.alarm)),
                "ambient": json.loads(json.dumps(self.ambient)),
                "music": json.loads(json.dumps(self.music)),
                "system": json.loads(json.dumps(self.system)),
            }

    def add_event(self, event_type: str, code: int, *args: int) -> None:
        raw = "V " + " ".join(str(x) for x in (code, *args))
        self.events.append({
            "type": event_type,
            "code": code,
            "args": list(args),
            "raw": raw,
        })
        del self.events[:-16]


STATE = MockState()


class Handler(BaseHTTPRequestHandler):
    server_version = "ARDU-Mock/1.0"

    def log_message(self, fmt: str, *args: Any) -> None:
        print("%s - %s" % (self.address_string(), fmt % args))

    def _json(self, status: int, payload: dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _body_json(self) -> dict[str, Any]:
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length else b"{}"
        try:
            value = json.loads(raw.decode("utf-8"))
        except Exception as exc:
            raise ValueError("BAD_JSON") from exc
        if not isinstance(value, dict):
            raise ValueError("BAD_JSON")
        return value

    def _body_text(self) -> str:
        length = int(self.headers.get("Content-Length", "0"))
        return (self.rfile.read(length) if length else b"").decode("utf-8").strip()

    def do_OPTIONS(self) -> None:
        self._json(200, {"ok": True})

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        with STATE.lock:
            if path == "/api/ping":
                self._json(200, {
                    "ok": True,
                    "device": "ARDU-ESP8266-MOCK",
                    "fw": "ARDU_ESP_V1_MOCK",
                    "uart_protocol": 1,
                    "wifi_connected": True,
                    "ip": self.server.server_address[0],
                    "rssi": -42,
                    "ota_ready": False,
                    "ota_hostname": "ardu-mock",
                    "ota_port": 8266,
                })
                return

            if path == "/api/status":
                now = datetime.now()
                uptime_ms = int((now - STATE.started).total_seconds() * 1000)
                mode_id = {
                    "off": 0, "light": 1, "night": 2,
                    "dawn": 3, "ambient": 4, "music": 5,
                }.get(STATE.mode, 0)
                selected_music = MUSIC_IDS.index(STATE.music["selected"])
                ambient_effect = AMBIENT_IDS.index(STATE.ambient["effect"])
                self._json(200, {
                    "ok": True,
                    "nano_fw": "ARDU_V1_MOCK",
                    "uart_protocol": 1,
                    "mode": STATE.mode,
                    "mode_id": mode_id,
                    "rtc_valid": True,
                    "clap_enabled": STATE.clap["enabled"],
                    "clap_threshold": STATE.clap["threshold"],
                    "clap_timeout_ms": STATE.clap["timeout_ms"],
                    "current_limit_ma": STATE.system["current_limit_ma"],
                    "music_mode": STATE.music["selected"],
                    "ambient_effect": STATE.ambient["effect"],
                    "reset_flags": 0,
                    "uptime_ms": uptime_ms,
                    "rtc_time": now.strftime("%Y-%m-%d %H:%M:%S"),
                    "nano": "D 2 %d 1 %d %d %d %d %d %d 0 %d" % (
                        mode_id,
                        1 if STATE.clap["enabled"] else 0,
                        STATE.clap["threshold"],
                        STATE.clap["timeout_ms"],
                        STATE.system["current_limit_ma"],
                        selected_music,
                        ambient_effect,
                        uptime_ms,
                    ),
                })
                return

            if path == "/api/settings":
                self._json(200, STATE.settings())
                return

            if path == "/api/time":
                now = datetime.now()
                self._json(200, {
                    "ok": True,
                    "valid": True,
                    "date": now.strftime("%Y-%m-%d"),
                    "time": now.strftime("%H:%M:%S"),
                    "nano": "D 4 1 " + now.strftime("%Y %m %d %H %M %S"),
                })
                return

            if path == "/api/light/status":
                l = STATE.light
                self._json(200, {
                    "ok": True,
                    "enabled": STATE.mode == "light",
                    **l,
                    "clap_enabled": STATE.clap["enabled"],
                })
                return

            if path == "/api/light/clap/calibration":
                self._json(200, {"ok": True, **STATE.clap["calibration"]})
                return

            if path == "/api/system/current-limit":
                self._json(200, {
                    "ok": True,
                    "milliamps": STATE.system["current_limit_ma"],
                    "hard_max_ma": 4500,
                })
                return

            if path == "/api/events":
                self._json(200, {"ok": True, "events": list(STATE.events)})
                return

        self._json(404, {"ok": False, "error": "NOT_FOUND"})

    def do_POST(self) -> None:
        path = urlparse(self.path).path
        try:
            with STATE.lock:
                if path == "/api/dev/nano":
                    self._developer(self._body_text())
                    return

                body = self._body_json()

                if path == "/api/time/sync":
                    self._json(200, {"ok": True, "applied": True})
                    return

                if path == "/api/power":
                    STATE.mode = "light" if bool(body.get("on")) else "off"
                    self._json(200, {"ok": True, "on": STATE.mode == "light", "applied": True})
                    return

                if path == "/api/mode":
                    mode = body.get("mode")
                    if mode not in ("off", "light", "night", "ambient", "music"):
                        self._json(400, {"ok": False, "error": "BAD_MODE"})
                        return
                    STATE.mode = mode
                    self._json(200, {"ok": True, "applied": True})
                    return

                if path == "/api/light/settings":
                    self._light(body)
                    return

                if path == "/api/light/clap/calibration/start":
                    target = int(body.get("pairs", 0))
                    if not 3 <= target <= 12:
                        self._json(400, {"ok": False, "error": "BAD_PAIRS"})
                        return
                    cal = STATE.clap["calibration"]
                    cal.update({
                        "active": True,
                        "finished": False,
                        "good_pairs": 0,
                        "target_pairs": target,
                        "quiet_p99_derivative": 18,
                        "suggested_threshold": 0,
                    })
                    self._json(200, {
                        "ok": True,
                        "quiet_p99_derivative": 18,
                        "target_pairs": target,
                    })
                    return

                if path == "/api/light/clap/calibration/sample":
                    cal = STATE.clap["calibration"]
                    if not cal["active"] or cal["good_pairs"] >= cal["target_pairs"]:
                        self._json(409, {"ok": False, "error": "NANO_REJECTED"})
                        return
                    cal["good_pairs"] += 1
                    self._json(200, {
                        "ok": True,
                        "accepted": True,
                        "good_pairs": cal["good_pairs"],
                        "target_pairs": cal["target_pairs"],
                        "weak_strength": 110 + cal["good_pairs"],
                        "strong_strength": 150 + cal["good_pairs"],
                    })
                    return

                if path == "/api/light/clap/calibration/finish":
                    cal = STATE.clap["calibration"]
                    if cal["good_pairs"] < cal["target_pairs"] or cal["target_pairs"] == 0:
                        self._json(409, {"ok": False, "error": "NANO_REJECTED"})
                        return
                    cal["active"] = False
                    cal["finished"] = True
                    cal["suggested_threshold"] = 72
                    STATE.clap["threshold"] = 72
                    STATE.clap["saved"] = False
                    self._json(200, {
                        "ok": True,
                        "quiet_p99_derivative": 18,
                        "clap_p20": 105,
                        "suggested_threshold": 72,
                    })
                    return

                if path == "/api/light/clap/calibration/save":
                    STATE.clap["saved"] = True
                    STATE.clap["calibration"]["finished"] = False
                    self._json(200, {"ok": True, "saved": True})
                    return

                if path == "/api/light/clap/calibration/cancel":
                    STATE.clap["threshold"] = 70
                    STATE.clap["saved"] = True
                    STATE.clap["calibration"].update({
                        "active": False, "finished": False,
                        "good_pairs": 0, "target_pairs": 0,
                        "suggested_threshold": 0,
                    })
                    self._json(200, {"ok": True, "cancelled": True})
                    return

                if path == "/api/music/mode":
                    mode = body.get("id")
                    if mode not in MUSIC_IDS:
                        self._json(400, {"ok": False, "error": "BAD_MUSIC_MODE"})
                        return
                    STATE.music["selected"] = mode
                    STATE.mode = "music"
                    self._json(200, {"ok": True, "applied": True})
                    return

                if path == "/api/music/settings":
                    self._music(body)
                    return

                if path == "/api/music/calibrate":
                    STATE.system["audio_calibrated"] = True
                    self._json(200, {
                        "ok": True,
                        "mic_dc": STATE.system["mic_dc"],
                        "vu_low_pass": STATE.system["vu_low_pass"],
                        "spectrum_low_pass": STATE.system["spectrum_low_pass"],
                    })
                    return

                if path == "/api/ambient/effect":
                    effect = body.get("id")
                    if effect not in AMBIENT_IDS:
                        self._json(400, {"ok": False, "error": "BAD_EFFECT"})
                        return
                    STATE.ambient["effect"] = effect
                    STATE.mode = "ambient"
                    self._json(200, {"ok": True, "applied": True, "persisted": True})
                    return

                if path == "/api/ambient/settings":
                    self._ambient(body)
                    return

                if path == "/api/night/settings":
                    self._night(body)
                    return

                if path == "/api/alarm/settings":
                    self._alarm(body)
                    return

                if path == "/api/alarm/stop-dawn":
                    STATE.alarm["dawn_phase"] = "idle"
                    STATE.mode = "off"
                    STATE.add_event("dawn_stop", 3)
                    self._json(200, {"ok": True, "stopped": True})
                    return

                if path == "/api/system/current-limit":
                    value = int(body.get("milliamps", 0))
                    if not 500 <= value <= 4500:
                        self._json(400, {"ok": False, "error": "BAD_CURRENT_LIMIT"})
                        return
                    STATE.system["current_limit_ma"] = value
                    self._json(200, {"ok": True, "applied": True})
                    return

        except (ValueError, TypeError):
            self._json(400, {"ok": False, "error": "BAD_REQUEST"})
            return

        self._json(404, {"ok": False, "error": "NOT_FOUND"})

    def _light(self, body: dict[str, Any]) -> None:
        if "enabled" in body:
            STATE.mode = "light" if bool(body["enabled"]) else "off"
        if "brightness" in body:
            STATE.light["brightness"] = int(body["brightness"])
            STATE.light["dirty"] = True
        if "kelvin" in body:
            STATE.light["kelvin"] = int(body["kelvin"])
            STATE.light["color_mode"] = "kelvin"
            STATE.light["dirty"] = True
        if "rgb" in body:
            rgb = body["rgb"]
            STATE.light["rgb"] = {
                "r": int(rgb["r"]), "g": int(rgb["g"]), "b": int(rgb["b"])
            }
            STATE.light["color_mode"] = "rgb"
            STATE.light["dirty"] = True
        if "clap_enabled" in body:
            STATE.clap["enabled"] = bool(body["clap_enabled"])
        if body.get("persist_startup_profile"):
            STATE.light["saved"] = True
            STATE.light["dirty"] = False
        self._json(200, {"ok": True, "applied": True})

    def _music(self, body: dict[str, Any]) -> None:
        cfg = STATE.music["modes"][STATE.music["selected"]]
        mapping = {
            "active_brightness": "brightness",
            "background_brightness": "background_brightness",
            "smoothing": "smoothing",
            "sensitivity": "sensitivity",
            "speed": "speed",
        }
        for src, dst in mapping.items():
            if src in body:
                cfg[dst] = int(body[src])
        if "submode" in body:
            cfg["submode"] = {"three": 0, "low": 1, "mid": 2, "high": 3}[body["submode"]]
        if "rainbow_step10" in body:
            cfg["aux"] = int(body["rainbow_step10"])
        if "hue_step" in body:
            cfg["aux"] = int(body["hue_step"])
        if "hue_start" in body:
            cfg["speed"] = int(body["hue_start"])
        self._json(200, {"ok": True, "applied": True})

    def _ambient(self, body: dict[str, Any]) -> None:
        effect = STATE.ambient["effect"]
        cfg = STATE.ambient[effect]
        if "auto_cycle" in body:
            STATE.ambient["auto_cycle"] = bool(body["auto_cycle"])
        if "auto_period_s" in body:
            STATE.ambient["auto_period_s"] = int(body["auto_period_s"])
        for key in ("hue", "saturation", "brightness", "speed", "rainbow_step"):
            if key in body:
                cfg[key] = body[key]
        self._json(200, {"ok": True, "applied": True})

    def _night(self, body: dict[str, Any]) -> None:
        for key in (
            "enabled", "hue", "saturation", "brightness",
            "schedule_enabled", "schedule_on", "schedule_off",
        ):
            if key in body:
                STATE.night[key] = body[key]
        if "enabled" in body:
            STATE.mode = "night"
        self._json(200, {"ok": True, "applied": True})

    def _alarm(self, body: dict[str, Any]) -> None:
        for key in (
            "enabled", "hour", "minute", "fade_minutes",
            "max_brightness", "start_hue", "end_hue",
        ):
            if key in body:
                STATE.alarm[key] = body[key]
        self._json(200, {"ok": True, "applied": True})

    def _developer(self, command: str) -> None:
        parts = command.split()
        if not parts or not all(part.isdigit() for part in parts):
            self._json(400, {"ok": False, "error": "NUMERIC_UART_REQUIRED"})
            return
        op = int(parts[0])
        if op == 1:
            nano = "O 1"
        elif op == 2:
            nano = "D 2 1 1 1 70 500 3000 0 0 0 1234"
        elif op == 4:
            nano = "D 4 1 " + datetime.now().strftime("%Y %m %d %H %M %S")
        elif op == 3:
            nano = "D 3 0 0 2700 255 170 87 64 1 0\nO 3"
        else:
            nano = "O %d" % op
        self._json(200, {
            "ok": True,
            "command": command,
            "nano": nano,
            "timed_out": False,
            "nano_error_code": 0,
        })


def main() -> None:
    parser = argparse.ArgumentParser(description="ARDU ESP8266 v1 stateful mock")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()

    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print("ARDU ESP v1 mock listening on http://%s:%d" % (args.host, args.port))
    print("On the phone enter: <THIS_PC_LAN_IP>:%d" % args.port)
    print("Ctrl+C to stop.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        httpd.server_close()


if __name__ == "__main__":
    main()
