#!/usr/bin/env python3
"""
rocket-dash bridge: reads ESP32 serial telemetry and broadcasts over WebSocket.
Parses nosecone_data line-protocol format and forwards to Telegraf + WebSocket.
Optional InfluxDB write if --influx-token is provided.
"""

import argparse
import asyncio
import json
import math
import random
import sys
import time
from contextlib import suppress

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

try:
    import websockets
    HAS_WS = True
except ImportError:
    HAS_WS = False
    print("[WARN] websockets not installed. Run: pip install websockets", file=sys.stderr)

try:
    from influxdb_client import InfluxDBClient, WriteOptions
    from influxdb_client.client.write_api import SYNCHRONOUS
    HAS_INFLUX = True
except ImportError:
    HAS_INFLUX = False


CONNECTED_CLIENTS: set = set()


async def broadcast(message: str):
    dead = set()
    for ws in CONNECTED_CLIENTS:
        try:
            await ws.send(message)
        except Exception:
            dead.add(ws)
    CONNECTED_CLIENTS.difference_update(dead)


async def ws_handler(websocket, path=None):
    CONNECTED_CLIENTS.add(websocket)
    print(f"[WS] Client connected ({len(CONNECTED_CLIENTS)} total)", flush=True)
    try:
        await websocket.wait_closed()
    finally:
        CONNECTED_CLIENTS.discard(websocket)
        print(f"[WS] Client disconnected ({len(CONNECTED_CLIENTS)} total)", flush=True)


def build_influx_line(pkt: dict, measurement: str = "telemetry") -> str:
    tags = f"phase={pkt.get('phase', 'unknown')}"
    fields = ",".join(
        f"{k}={v}"
        for k, v in pkt.items()
        if k not in ("phase",) and isinstance(v, (int, float))
    )
    ts_ns = int(pkt.get("t", time.time()) * 1e9)
    return f"{measurement},{tags} {fields} {ts_ns}"


def write_influx(write_api, bucket: str, org: str, pkt: dict):
    try:
        line = build_influx_line(pkt)
        write_api.write(bucket=bucket, org=org, record=line)
    except Exception as e:
        print(f"[INFLUX] Write error: {e}", flush=True)


def post_telegraf(telegraf_url: str, fields: dict):
    if not HAS_REQUESTS or not telegraf_url:
        return
    try:
        influx_line = "nosecone_data " + ",".join(
            f"{k}={float(v)}" for k, v in fields.items()
        )
        requests.post(telegraf_url, data=influx_line, timeout=0.05)
    except Exception as e:
        print(f"[TELEGRAF] Error: {e}", flush=True)


# ── Nosecone field normalisation ──────────────────────────────────────────────

def _tilt_from_accel(ax: float, ay: float, az: float):
    # IMU X axis points toward the tail (down on pad), so remap:
    # rocket body Z (thrust/up) = -ax, body X = ay, body Y = az
    bx, by, bz = ay, az, -ax
    roll = math.degrees(math.atan2(by, bz))
    pitch = math.degrees(math.atan2(-bx, math.sqrt(by * by + bz * bz)))
    return roll, pitch


def _quat_from_rpy(roll_deg: float, pitch_deg: float, yaw_deg: float = 0.0):
    r = math.radians(roll_deg) / 2
    p = math.radians(pitch_deg) / 2
    y = math.radians(yaw_deg) / 2
    cr, sr = math.cos(r), math.sin(r)
    cp, sp = math.cos(p), math.sin(p)
    cy, sy = math.cos(y), math.sin(y)
    qw = cr * cp * cy + sr * sp * sy
    qx = sr * cp * cy - cr * sp * sy
    qy = cr * sp * cy + sr * cp * sy
    qz = cr * cp * sy - sr * sp * cy
    n = math.sqrt(qw**2 + qx**2 + qy**2 + qz**2) or 1.0
    return qw / n, qx / n, qy / n, qz / n


def _estimate_phase(accel_g: float, alt_m: float, vel: float, ctx: dict) -> str:
    ps = ctx.setdefault('ps', {'state': 'pad', 'pad_alt': 0.0})
    state = ps['state']

    if state == 'pad':
        if accel_g > 2.0:
            ps['pad_alt'] = alt_m
            ps['state'] = 'powered'
        return ps['state']

    agl = alt_m - ps['pad_alt']

    if state == 'powered':
        if accel_g < 2.0:
            ps['state'] = 'coasting'
        return ps['state']

    if state == 'coasting':
        if vel < 1.0:
            ps['state'] = 'apogee'
        return ps['state']

    if state == 'apogee':
        if vel < -3.0:
            ps['state'] = 'drogue'
        return ps['state']

    if state == 'drogue':
        if agl < 300:
            ps['state'] = 'main'
        return ps['state']

    if state == 'main':
        if agl < 30 and abs(vel) < 3:
            ps['state'] = 'landed'
        return ps['state']

    return 'landed'


def normalize_nosecone_packet(fields: dict, ctx: dict) -> dict:
    """Map nosecone serial fields to the web app packet format and compute derived values."""
    t = round(time.time(), 2)

    baro_alt = float(fields.get('baro_alt', 0.0))
    ax = float(fields.get('ax', 0.0))
    ay = float(fields.get('ay', 0.0))
    az = float(fields.get('az', 0.0))
    gx = float(fields.get('gx', 0.0))
    gy = float(fields.get('gy', 0.0))
    gz = float(fields.get('gz', 0.0))

    # Accel magnitude in G (ISM330DHCX outputs m/s²)
    accel_g = math.sqrt(ax * ax + ay * ay + az * az) / 9.81
    ctx['accel_max'] = max(ctx.get('accel_max', 0.0), accel_g)

    # Velocity via finite difference on baro altitude with EMA smoothing
    prev_t = ctx.get('prev_t', t)
    prev_alt = ctx.get('prev_alt', baro_alt)
    prev_vel = ctx.get('prev_vel', 0.0)
    dt = t - prev_t
    if dt > 0.01:
        vel_raw = (baro_alt - prev_alt) / dt
        vel = 0.3 * vel_raw + 0.7 * prev_vel
    else:
        vel = prev_vel
    ctx['prev_t'] = t
    ctx['prev_alt'] = baro_alt
    ctx['prev_vel'] = vel

    phase = _estimate_phase(accel_g, baro_alt, vel, ctx)

    roll, pitch = _tilt_from_accel(ax, ay, az)
    qw, qx, qy, qz = _quat_from_rpy(roll, pitch)

    return {
        't': t,
        'phase': phase,
        'alt': round(baro_alt, 1),
        'vel': round(vel, 1),
        'accel': round(accel_g, 2),
        'accel_max': round(ctx['accel_max'], 2),
        'qw': round(qw, 4), 'qx': round(qx, 4), 'qy': round(qy, 4), 'qz': round(qz, 4),
        'roll': round(roll, 1), 'pitch': round(pitch, 1), 'yaw': 0.0,
        'gyr_x': round(math.degrees(gx), 2),
        'gyr_y': round(math.degrees(gy), 2),
        'gyr_z': round(math.degrees(gz), 2),
        'lat': round(float(fields.get('lat', 0.0)), 6),
        'lon': round(float(fields.get('lon', 0.0)), 6),
        'gps_sats': int(float(fields.get('gps_fix', 0))),
        'hdop': 1.0,
        'baro_alt': round(baro_alt, 1),
        'bat': 0.0,
        'rssi': int(float(fields.get('rssi', -99))),
        'rec_drogue': 0,
        'rec_main': 0,
        'temp': float(fields.get('altTemp', fields.get('temp', 0.0))),
        'snr': round(float(fields.get('snr', 0.0)), 2),
    }


def _open_serial(port: str, baud: int):
    """Open serial port, falling back to raw stty+file if pyserial fails (macOS ARM bug)."""
    if HAS_SERIAL:
        try:
            ser = serial.Serial(port, baud, timeout=1)
            return ser, 'pyserial'
        except Exception:
            pass
    # macOS fallback: configure with stty then open as raw file
    import subprocess
    subprocess.run(['stty', '-f', port, str(baud), 'raw', '-echo', 'cs8',
                    '-cstopb', '-parenb'], capture_output=True)
    fd = open(port, 'rb', buffering=0)
    return fd, 'raw'


def _readline_raw(fd) -> bytes:
    """Read one newline-terminated line from a raw file descriptor."""
    buf = b''
    while True:
        ch = fd.read(1)
        if not ch:
            return buf
        buf += ch
        if ch == b'\n':
            return buf


async def serial_reader_loop(port: str, baud: int, write_api, influx_opts: dict, telegraf_url: str):
    while True:
        try:
            loop = asyncio.get_event_loop()
            fd, mode = await loop.run_in_executor(None, _open_serial, port, baud)
            print(f"[SERIAL] Connected to {port} @ {baud} ({mode})", flush=True)
            latest_fields: dict = {}
            nosecone_ctx: dict = {}

            readline_fn = fd.readline if mode == 'pyserial' else lambda: _readline_raw(fd)

            while True:
                line = await loop.run_in_executor(None, readline_fn)
                if not line:
                    continue
                text = line.decode("utf-8", errors="replace").strip()
                if not text:
                    continue
                print(text, flush=True)

                if text.startswith("SENDING: nosecone_data"):
                    try:
                        data_part = text.split("nosecone_data", 1)[1].strip()
                        latest_fields = dict(
                            item.split("=") for item in data_part.split(",") if "=" in item
                        )
                    except Exception as e:
                        print(f"[PARSE] nosecone_data error: {e}", flush=True)

                elif text.startswith("RSSI"):
                    try:
                        parts = text.replace(":", "").split()
                        latest_fields["rssi"] = float(parts[1])
                        latest_fields["snr"] = float(parts[3])

                        await loop.run_in_executor(None, post_telegraf, telegraf_url, latest_fields)

                        pkt = normalize_nosecone_packet(latest_fields, nosecone_ctx)
                        await broadcast(json.dumps(pkt))
                        print(f"[PKT] phase={pkt['phase']} alt={pkt['alt']}m accel={pkt['accel']}G rssi={pkt['rssi']}", flush=True)

                        if write_api and influx_opts:
                            write_influx(write_api, influx_opts["bucket"], influx_opts["org"], pkt)
                    except Exception as e:
                        print(f"[PARSE] RSSI error: {e}", flush=True)

                else:
                    try:
                        pkt = json.loads(text)
                        print(f"[PKT] t={pkt.get('t')} phase={pkt.get('phase')} alt={pkt.get('alt')}", flush=True)
                        await broadcast(json.dumps(pkt))
                        if write_api and influx_opts:
                            write_influx(write_api, influx_opts["bucket"], influx_opts["org"], pkt)
                    except json.JSONDecodeError:
                        pass

        except Exception as e:
            print(f"[SERIAL] Error: {e}. Retrying in 3s...", flush=True)
            await asyncio.sleep(3)


# ── Demo flight simulator (mirrors dummyFeed.js) ─────────────────────────────

def lerp(a, b, t): return a + (b - a) * t
def clamp(v, lo, hi): return max(lo, min(hi, v))
def ease_in_out(t): return 2 * t * t if t < 0.5 else -1 + (4 - 2 * t) * t

PAD_LAT, PAD_LON = 34.8742, -117.3108


def get_phase(t: float) -> str:
    if t < 3: return "pad"
    if t < 9: return "powered"
    if t < 35: return "coasting"
    if t < 35.5: return "apogee"
    if t < 90: return "drogue"
    if t < 140: return "main"
    return "landed"


def compute_altitude(t: float) -> float:
    if t < 3: return 0.0
    if t < 9:
        s = (t - 3) / 6
        return 1600 * s * s
    if t < 35:
        s = (t - 9) / 26
        sp = 1 - (1 - s) ** 2
        return lerp(640, 2241, sp)
    if t < 35.5: return 2241.0
    if t < 90:
        s = (t - 35.5) / 54.5
        return lerp(2241, 305, s)
    if t < 140:
        s = (t - 90) / 50
        return lerp(305, 5, s)
    return 0.0


def compute_velocity(t: float) -> float:
    if t < 3: return 0.0
    if t < 9: return lerp(0, 142, (t - 3) / 6)
    if t < 35: return lerp(142, 0, ease_in_out((t - 9) / 26))
    if t < 35.5: return 0.0
    if t < 90: return -15.0
    if t < 140: return -5.0
    return 0.0


def compute_accel(t: float) -> float:
    if t < 3: return 0.0
    if t < 9:
        s = (t - 3) / 6
        return lerp(1, 18.3, s * 2) if s < 0.5 else lerp(18.3, 2, (s - 0.5) * 2)
    if t < 35: return -1.0 + math.sin(t * 0.3) * 0.1
    if t < 35.5: return 0.0
    if t < 90: return -0.3 + math.sin(t * 0.2) * 0.05
    if t < 140: return -0.1 + math.sin(t * 0.1) * 0.02
    if t < 141: return 8 * math.exp(-(t - 140) * 3)
    return 0.0


def compute_quaternion(t: float):
    roll = 0 if t < 3 else (t - 3) * 2.1 * math.pi / 180
    pitch = 0 if t < 3 else math.sin(t * 0.15) * 12 * math.pi / 180
    yaw = 0 if t < 3 else (t - 3) * 0.8 * math.pi / 180
    cr, sr = math.cos(roll / 2), math.sin(roll / 2)
    cp, sp = math.cos(pitch / 2), math.sin(pitch / 2)
    cy, sy = math.cos(yaw / 2), math.sin(yaw / 2)
    qw = cr * cp * cy + sr * sp * sy
    qx = sr * cp * cy - cr * sp * sy
    qy = cr * sp * cy + sr * cp * sy
    qz = cr * cp * sy - sr * sp * cy
    n = math.sqrt(qw**2 + qx**2 + qy**2 + qz**2)
    return qw/n, qx/n, qy/n, qz/n


def build_demo_packet(t: float) -> dict:
    phase = get_phase(t)
    alt = max(0.0, compute_altitude(t) + (random.random() - 0.5) * 2)
    vel = compute_velocity(t) + (random.random() - 0.5) * 0.5
    accel = compute_accel(t) + (random.random() - 0.5) * 0.05
    qw, qx, qy, qz = compute_quaternion(t)

    roll_deg = math.degrees(math.atan2(
        2 * (qw * qx + qy * qz), 1 - 2 * (qx**2 + qy**2)
    ))
    pitch_deg = math.degrees(math.asin(clamp(2 * (qw * qy - qz * qx), -1, 1)))
    yaw_deg = math.degrees(math.atan2(
        2 * (qw * qz + qx * qy), 1 - 2 * (qy**2 + qz**2)
    ))

    drift_n = (t - 3) * 0.00003 if t >= 3 else 0
    drift_e = (t - 3) * -0.000012 if t >= 3 else 0
    gyr_scale = 0.01 if t < 3 else (8 if t < 9 else (3 if t < 35 else 0.5))

    return {
        "t": round(t, 2),
        "phase": phase,
        "alt": round(alt, 1),
        "vel": round(vel, 1),
        "accel": round(accel, 2),
        "accel_max": 18.3,
        "qw": round(qw, 4), "qx": round(qx, 4), "qy": round(qy, 4), "qz": round(qz, 4),
        "roll": round(roll_deg, 1), "pitch": round(pitch_deg, 1), "yaw": round(yaw_deg, 1),
        "gyr_x": round((random.random() - 0.5) * gyr_scale, 2),
        "gyr_y": round((random.random() - 0.5) * gyr_scale, 2),
        "gyr_z": round((random.random() - 0.5) * gyr_scale, 2),
        "lat": round(PAD_LAT + drift_n + (random.random() - 0.5) * 0.000005, 6),
        "lon": round(PAD_LON + drift_e + (random.random() - 0.5) * 0.000005, 6),
        "gps_sats": 0 if t < 2 else (6 if t < 3 else 9),
        "hdop": 2.1 if t < 3 else 0.8,
        "baro_alt": round(alt + (random.random() - 0.5) * 5, 1),
        "bat": round(3.92 - t * 0.001, 2),
        "rssi": round(-74 - alt * 0.01 + (random.random() - 0.5) * 3),
        "rec_drogue": 1 if t >= 35.5 else 0,
        "rec_main": 1 if t >= 90 else 0,
        "temp": round(18.4 - alt * 0.006 + (random.random() - 0.5) * 0.2, 1),
    }


async def demo_loop(write_api, influx_opts: dict):
    t = 0.0
    dt = 0.1
    print("[DEMO] Starting flight simulation at 10 Hz...", flush=True)
    while True:
        pkt = build_demo_packet(t)
        print(f"[PKT] t={pkt['t']} phase={pkt['phase']} alt={pkt['alt']}", flush=True)
        await broadcast(json.dumps(pkt))
        if write_api and influx_opts:
            write_influx(write_api, influx_opts["bucket"], influx_opts["org"], pkt)
        t = round(t + dt, 3)
        if t > 160:
            t = 0.0
        await asyncio.sleep(dt)


async def main():
    parser = argparse.ArgumentParser(description="rocket-dash bridge")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket port")
    parser.add_argument("--influx-url", default=None)
    parser.add_argument("--influx-token", default=None)
    parser.add_argument("--influx-org", default="rocket")
    parser.add_argument("--influx-bucket", default="telemetry")
    parser.add_argument("--telegraf-url", default="http://localhost:8094/telegraf", help="Telegraf HTTP listener URL")
    parser.add_argument("--demo", action="store_true", help="Run demo flight simulation")
    args = parser.parse_args()

    write_api = None
    influx_opts = None
    if args.influx_token and HAS_INFLUX:
        client = InfluxDBClient(url=args.influx_url, token=args.influx_token, org=args.influx_org)
        write_api = client.write_api(write_options=SYNCHRONOUS)
        influx_opts = {"bucket": args.influx_bucket, "org": args.influx_org}
        print(f"[INFLUX] Connected to {args.influx_url}", flush=True)
    elif args.influx_token and not HAS_INFLUX:
        print("[WARN] influxdb-client not installed. InfluxDB writes disabled.", flush=True)

    if not HAS_WS:
        print("[ERROR] websockets package required. Run: pip install websockets pyserial", file=sys.stderr)
        sys.exit(1)

    print(f"[WS] Starting WebSocket server on ws://localhost:{args.ws_port}", flush=True)

    async with websockets.serve(ws_handler, "0.0.0.0", args.ws_port):
        if args.demo:
            await demo_loop(write_api, influx_opts)
        else:
            await serial_reader_loop(args.port, args.baud, write_api, influx_opts, args.telegraf_url)


if __name__ == "__main__":
    with suppress(KeyboardInterrupt):
        asyncio.run(main())
