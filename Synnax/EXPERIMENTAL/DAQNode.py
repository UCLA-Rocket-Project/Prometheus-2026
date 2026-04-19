import os
import time
from typing import Dict, List, Optional

import synnax as sy

try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None

try:
    import serial
except ImportError:
    serial = None


def _env_str(name: str, default: str) -> str:
    return os.getenv(name, default)


def _env_int(name: str, default: int) -> int:
    try:
        return int(os.getenv(name, str(default)))
    except ValueError:
        return default


def _env_float(name: str, default: float) -> float:
    try:
        return float(os.getenv(name, str(default)))
    except ValueError:
        return default


def _env_bool(name: str, default: bool) -> bool:
    value = os.getenv(name)
    if value is None:
        return default
    return value.lower() in {"1", "true", "yes", "on"}


SYNNAX_HOST = _env_str("SYNNAX_HOST", "localhost")
SYNNAX_PORT = _env_int("SYNNAX_PORT", 9091)
SYNNAX_USERNAME = _env_str("SYNNAX_USERNAME", "synnax")
SYNNAX_PASSWORD = _env_str("SYNNAX_PASSWORD", "seldon")
SYNNAX_SECURE = _env_bool("SYNNAX_SECURE", False)
DAQ_SOURCE_MODE = _env_str("DAQ_SOURCE_MODE", "serial").lower()  # mqtt | serial

MQTT_BROKER = _env_str("DAQ_MQTT_BROKER", _env_str("MQTT_BROKER", "127.0.0.1"))
MQTT_PORT = _env_int("DAQ_MQTT_PORT", 1883)
MQTT_TOPIC = _env_str("DAQ_MQTT_TOPIC", "DAQ_transmitter/receiver")
MQTT_CLIENT_ID = _env_str("DAQ_MQTT_CLIENT_ID", "PiDAQNode")

DAQ_SERIAL_PORT = _env_str("DAQ_SERIAL_PORT", "/dev/ttyUSB1")
DAQ_BAUDRATE = _env_int("DAQ_BAUDRATE", 115200)
DAQ_TIMEOUT_SECONDS = _env_float("DAQ_TIMEOUT_SECONDS", 0.2)
DAQ_PARSE_DELIMITER = _env_str("DAQ_PARSE_DELIMITER", ",")

DAQ_PT_CHANNEL_COUNT = 8
DAQ_LC_CHANNEL_COUNT = 2
DAQ_LOOP_HZ = _env_float("DAQ_LOOP_HZ", 200.0)


def _create_channels(client: sy.Synnax) -> None:
    """Create DAQ channels used by the Pi-side data pipeline."""
    daq_time = client.channels.create(
        name="daq_time",
        is_index=True,
        data_type=sy.DataType.TIMESTAMP,
        retrieve_if_name_exists=True,
    )
    pt_channels = [
        sy.Channel(name=f"daq_pt_{i}", data_type=sy.DataType.FLOAT32, index=daq_time.key)
        for i in range(DAQ_PT_CHANNEL_COUNT)
    ]
    lc_channels = [
        sy.Channel(name=f"daq_lc_{i}", data_type=sy.DataType.FLOAT32, index=daq_time.key)
        for i in range(DAQ_LC_CHANNEL_COUNT)
    ]
    uptime_ms = sy.Channel(
        name="daq_uptime_ms",
        data_type=sy.DataType.UINT64,
        index=daq_time.key,
    )
    client.channels.create(pt_channels + lc_channels + [uptime_ms], retrieve_if_name_exists=True)


def _build_writer_channels() -> List[str]:
    """Return channels that are written each DAQ sample."""
    return [
        "daq_time",
        "daq_uptime_ms",
        "daq_pt_0",
        "daq_pt_1",
        "daq_pt_2",
        "daq_pt_3",
        "daq_pt_4",
        "daq_pt_5",
        "daq_pt_6",
        "daq_pt_7",
        "daq_lc_0",
        "daq_lc_1",
    ]


def _parse_rocket_line(payload: str) -> Optional[Dict[str, float]]:
    """Parse ESP32 MQTT payload: rocket_data pt0=...,lc0=...,uptime_ms=..."""
    line = payload.strip()
    if not line:
        return None
    if line.startswith("rocket_data"):
        line = line[len("rocket_data"):].strip()
    if not line:
        return None
    parsed: Dict[str, float] = {}
    for part in line.split(","):
        token = part.strip()
        if "=" not in token:
            continue
        key, raw_value = token.split("=", 1)
        key = key.strip()
        raw_value = raw_value.strip()
        if not key:
            continue
        try:
            parsed[key] = float(raw_value)
        except ValueError:
            continue
    required = [f"pt{i}" for i in range(DAQ_PT_CHANNEL_COUNT)] + ["lc0", "lc1", "uptime_ms"]
    if not all(key in parsed for key in required):
        return None
    return parsed


def _parse_serial_values(raw_line: str) -> Optional[Dict[str, float]]:
    """Parse serial fallback values as pt0..pt7,lc0,lc1,uptime_ms."""
    line = raw_line.strip()
    if not line:
        return None
    rocket_format = _parse_rocket_line(line)
    if rocket_format is not None:
        return rocket_format
    parts = line.split(DAQ_PARSE_DELIMITER)
    if len(parts) < 11:
        parts = line.split()
        if len(parts) < 11:
            return None
    try:
        values = [float(parts[i]) for i in range(11)]
    except ValueError:
        return None
    parsed: Dict[str, float] = {}
    for i in range(8):
        parsed[f"pt{i}"] = values[i]
    parsed["lc0"] = values[8]
    parsed["lc1"] = values[9]
    parsed["uptime_ms"] = values[10]
    return parsed


def _write_sample_to_synnax(writer, parsed: Dict[str, float]) -> None:
    """Write one parsed sample to Synnax."""
    frame = {
        "daq_time": sy.TimeStamp.now(),
        "daq_uptime_ms": int(parsed["uptime_ms"]),
        "daq_pt_0": parsed["pt0"],
        "daq_pt_1": parsed["pt1"],
        "daq_pt_2": parsed["pt2"],
        "daq_pt_3": parsed["pt3"],
        "daq_pt_4": parsed["pt4"],
        "daq_pt_5": parsed["pt5"],
        "daq_pt_6": parsed["pt6"],
        "daq_pt_7": parsed["pt7"],
        "daq_lc_0": parsed["lc0"],
        "daq_lc_1": parsed["lc1"],
    }
    writer.write(frame)


def _run_mqtt_source(writer, stop_flag=None) -> None:
    """Read DAQ samples from MQTT and forward latest sample to Synnax."""
    latest_sample = {"value": None}

    def on_connect(client, _userdata, _flags, rc, _properties=None):
        if rc == 0:
            print(f"DAQ MQTT connected to {MQTT_BROKER}:{MQTT_PORT}; subscribing {MQTT_TOPIC}")
            client.subscribe(MQTT_TOPIC, qos=0)
        else:
            print(f"DAQ MQTT connect failed with rc={rc}")

    def on_message(_client, _userdata, msg):
        payload = msg.payload.decode("utf-8", errors="ignore")
        parsed = _parse_rocket_line(payload)
        if parsed is not None:
            latest_sample["value"] = parsed

    mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    while True:
        if stop_flag is not None and stop_flag.is_set():
            return
        try:
            mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
            break
        except Exception as err:
            print(f"DAQ MQTT connect failed: {err}")
            time.sleep(1.0)

    mqtt_client.loop_start()
    loop = sy.Loop(sy.Rate.HZ * DAQ_LOOP_HZ)
    try:
        while loop.wait():
            if stop_flag is not None and stop_flag.is_set():
                break
            sample = latest_sample["value"]
            if sample is None:
                continue
            _write_sample_to_synnax(writer, sample)
    finally:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()


def _run_serial_source(writer, stop_flag=None) -> None:
    """Read DAQ samples from serial and write them to Synnax."""
    if serial is None:
        raise RuntimeError("pyserial is not installed. Install with: pip install pyserial")
    loop = sy.Loop(sy.Rate.HZ * DAQ_LOOP_HZ)
    serial_conn = None
    try:
        while True:
            if stop_flag is not None and stop_flag.is_set():
                break
            if serial_conn is None or not serial_conn.is_open:
                try:
                    serial_conn = serial.Serial(DAQ_SERIAL_PORT, DAQ_BAUDRATE, timeout=DAQ_TIMEOUT_SECONDS)
                    print(f"Connected to DAQ serial device on {DAQ_SERIAL_PORT}")
                except Exception as err:
                    print(f"DAQ serial connect failed: {err}")
                    time.sleep(1.0)
                    continue
            if not loop.wait():
                continue
            try:
                raw = serial_conn.readline().decode("utf-8", errors="ignore")
            except Exception as err:
                print(f"DAQ read failed: {err}")
                try:
                    serial_conn.close()
                except Exception:
                    pass
                serial_conn = None
                continue
            parsed = _parse_serial_values(raw)
            if parsed is None:
                continue
            _write_sample_to_synnax(writer, parsed)
    finally:
        if serial_conn is not None and serial_conn.is_open:
            serial_conn.close()


def run_daq_node(stop_flag=None) -> None:
    """Main DAQ loop; source can be MQTT (default) or serial."""
    client = sy.Synnax(
        host=SYNNAX_HOST,
        port=SYNNAX_PORT,
        username=SYNNAX_USERNAME,
        password=SYNNAX_PASSWORD,
        secure=SYNNAX_SECURE,
    )
    _create_channels(client)
    print("=" * 70)
    print("Synnax -> DAQ Node")
    print(f"Source mode: {DAQ_SOURCE_MODE}")
    if DAQ_SOURCE_MODE == "mqtt":
        print(f"MQTT: {MQTT_BROKER}:{MQTT_PORT} topic={MQTT_TOPIC}")
    else:
        print(f"Serial: {DAQ_SERIAL_PORT} @ {DAQ_BAUDRATE}")
    print("=" * 70)
    with client.open_writer(
        start=sy.TimeStamp.now(),
        channels=_build_writer_channels(),
        enable_auto_commit=True,
    ) as writer:
        if DAQ_SOURCE_MODE == "mqtt":
            _run_mqtt_source(writer, stop_flag=stop_flag)
        elif DAQ_SOURCE_MODE == "serial":
            _run_serial_source(writer, stop_flag=stop_flag)
        else:
            raise ValueError("DAQ_SOURCE_MODE must be one of: mqtt, serial")


def main() -> None:
    run_daq_node()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nDAQ node shutting down...")
