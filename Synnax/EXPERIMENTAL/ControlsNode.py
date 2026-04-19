import os
import time
from typing import Optional

import synnax as sy

try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None

try:
    import serial
except ImportError:
    serial = None

CLOSED = 0
OPEN = 1
NUM_CHANNELS = 8
SAFE_STATES = [
    CLOSED,  # 1 - Abort
    CLOSED,  # 2 - QD
    CLOSED,  # 3 - Vent
    CLOSED,  # 4 - Ignite
    CLOSED,  # 5 - Fill
    CLOSED,  # 6 - Dump
    CLOSED,  # 7 - MPV
    CLOSED,  # 8 - Purge
]
MAP_SYN_TO_PACKET = {
    1: 1,  # Abort
    2: 2,  # QD
    3: 3,  # Vent
    4: 4,  # Ignite
    5: 5,  # Fill
    6: 6,  # Dump
    7: 7,  # MPV
    8: 8,  # Purge
}


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

MQTT_ENABLED = _env_bool("MQTT_ENABLED", False)
MQTT_BROKER = _env_str("MQTT_BROKER", "127.0.0.1")
MQTT_PORT = _env_int("MQTT_PORT", 1883)
MQTT_TOPIC = _env_str("MQTT_TOPIC", "switchbox/commands")
CONTROLS_TRANSPORT = _env_str("CONTROLS_TRANSPORT", "serial").lower()  # serial | mqtt
CONTROLS_SERIAL_PORT = _env_str("CONTROLS_SERIAL_PORT", "/dev/ttyUSB0")
CONTROLS_SERIAL_BAUDRATE = _env_int("CONTROLS_SERIAL_BAUDRATE", 115200)
CONTROLS_SERIAL_TIMEOUT_SECONDS = _env_float("CONTROLS_SERIAL_TIMEOUT_SECONDS", 0.2)

LOOP_HZ = _env_float("CONTROLS_LOOP_HZ", 7.5)
LOOP = sy.Loop(sy.Rate.HZ * LOOP_HZ)

ARMED = 0
CHANNEL_STATES = SAFE_STATES.copy()
mqtt_client = None
serial_client = None


def build_packet(states: list[int]) -> str:
    """Build the control packet expected by the switchbox firmware."""
    packet = ["0"] * 11
    packet[0] = "A"
    packet[10] = "Z"
    for syn_channel, packet_index in MAP_SYN_TO_PACKET.items():
        if 1 <= syn_channel <= NUM_CHANNELS:
            packet[packet_index] = str(states[syn_channel - 1])
    packet[9] = str(ARMED)
    return "".join(packet)


def connect_mqtt_with_retry(stop_flag=None) -> bool:
    """Connect to MQTT and retry until connected or stop is requested."""
    global mqtt_client
    if mqtt is None:
        raise RuntimeError("paho-mqtt is not installed. Install with: pip install paho-mqtt")
    mqtt_client = mqtt.Client(client_id="SynnaxControls")
    while True:
        if stop_flag is not None and stop_flag.is_set():
            return False
        try:
            mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
            mqtt_client.loop_start()
            print(f"Connected to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
            return True
        except Exception as err:
            print(f"MQTT connection failed: {err}")
            print("Retrying in 3 seconds (Ctrl+C to quit)")
            time.sleep(3)


def connect_serial_with_retry(stop_flag=None) -> bool:
    """Connect to controls serial port and retry until connected or stop requested."""
    global serial_client
    if serial is None:
        raise RuntimeError("pyserial is not installed. Install with: pip install pyserial")
    while True:
        if stop_flag is not None and stop_flag.is_set():
            return False
        try:
            serial_client = serial.Serial(
                CONTROLS_SERIAL_PORT,
                CONTROLS_SERIAL_BAUDRATE,
                timeout=CONTROLS_SERIAL_TIMEOUT_SECONDS,
            )
            print(f"Connected to controls serial at {CONTROLS_SERIAL_PORT}")
            return True
        except Exception as err:
            print(f"Controls serial connection failed: {err}")
            print("Retrying in 2 seconds (Ctrl+C to quit)")
            time.sleep(2)


def send_mqtt_command(states: list[int]) -> bool:
    """Send one control packet to MQTT when enabled."""
    packet = build_packet(states)
    print(f"Packet ready: {packet}")
    if not MQTT_ENABLED:
        return False
    global mqtt_client
    if mqtt_client is None or not mqtt_client.is_connected():
        return False
    try:
        mqtt_client.publish(MQTT_TOPIC, packet, qos=0)
        return True
    except Exception as err:
        print(f"Publish failed: {err}")
        return False


def send_serial_command(states: list[int]) -> bool:
    """Send one control packet over serial. Reconnects automatically on failure."""
    global serial_client
    if serial_client is None or not serial_client.is_open:
        print("Serial disconnected, attempting reconnect...")
        connect_serial_with_retry()
    packet = build_packet(states)
    print(f"Packet ready: {packet}")
    try:
        serial_client.write(packet.encode("utf-8"))
        return True
    except Exception as err:
        print(f"Serial write failed: {err}")
        try:
            serial_client.close()
        except Exception:
            pass
        serial_client = None
        return False


def send_controls_command(states: list[int]) -> bool:
    """Dispatch controls packet over the configured transport."""
    if CONTROLS_TRANSPORT == "serial":
        return send_serial_command(states)
    if CONTROLS_TRANSPORT == "mqtt":
        if not MQTT_ENABLED:
            return False
        return send_mqtt_command(states)
    raise ValueError("CONTROLS_TRANSPORT must be one of: serial, mqtt")


def init_synnax_channels(client: sy.Synnax) -> None:
    """Create all control command and state channels if missing."""
    command_channels = [
        sy.Channel(name=f"controls_cmd_{i + 1}", data_type=sy.DataType.UINT8, virtual=True)
        for i in range(NUM_CHANNELS)
    ]
    client.channels.create(command_channels, retrieve_if_name_exists=True)
    state_time = client.channels.create(
        name="controls_state_time",
        is_index=True,
        data_type=sy.DataType.TIMESTAMP,
        retrieve_if_name_exists=True,
    )
    state_channels = [
        sy.Channel(name=f"controls_state_{i + 1}", data_type=sy.DataType.UINT8, index=state_time.key)
        for i in range(NUM_CHANNELS)
    ]
    client.channels.create(state_channels, retrieve_if_name_exists=True)
    arm_time = client.channels.create(
        name="controls_arm_time",
        is_index=True,
        data_type=sy.DataType.TIMESTAMP,
        retrieve_if_name_exists=True,
    )
    client.channels.create(
        name="controls_arm_cmd",
        data_type=sy.DataType.UINT8,
        virtual=True,
        retrieve_if_name_exists=True,
    )
    client.channels.create(
        name="controls_arm_state",
        data_type=sy.DataType.UINT8,
        index=arm_time.key,
        retrieve_if_name_exists=True,
    )


def write_state_frame(writer, states: list[int]) -> None:
    frame = {"controls_state_time": sy.TimeStamp.now()}
    for i, state in enumerate(states):
        frame[f"controls_state_{i + 1}"] = state
    writer.write(frame)


def run_controls_node(stop_flag=None) -> None:
    """Read Synnax commands, enforce safety gates, send controls packet, and mirror states."""
    global ARMED
    global CHANNEL_STATES
    client = sy.Synnax(
        host=SYNNAX_HOST,
        port=SYNNAX_PORT,
        username=SYNNAX_USERNAME,
        password=SYNNAX_PASSWORD,
        secure=SYNNAX_SECURE,
    )
    init_synnax_channels(client)
    print("=" * 70)
    print("Synnax -> Controls Box")
    print(f"Transport = {CONTROLS_TRANSPORT}")
    if CONTROLS_TRANSPORT == "mqtt":
        print(f"MQTT_ENABLED = {MQTT_ENABLED}")
        print(f"MQTT target = {MQTT_BROKER}:{MQTT_PORT} topic={MQTT_TOPIC}")
    if CONTROLS_TRANSPORT == "serial":
        print(f"Serial target = {CONTROLS_SERIAL_PORT} @ {CONTROLS_SERIAL_BAUDRATE}")
    print("=" * 70)
    if CONTROLS_TRANSPORT == "mqtt":
        if MQTT_ENABLED and not connect_mqtt_with_retry(stop_flag=stop_flag):
            return
    elif CONTROLS_TRANSPORT == "serial":
        if not connect_serial_with_retry(stop_flag=stop_flag):
            return
    else:
        raise ValueError("CONTROLS_TRANSPORT must be one of: serial, mqtt")
    print("Setting initial safe states...")
    send_controls_command(SAFE_STATES)
    stream_channels = [f"controls_cmd_{i + 1}" for i in range(NUM_CHANNELS)] + ["controls_arm_cmd"]
    write_channels = [f"controls_state_{i + 1}" for i in range(NUM_CHANNELS)] + [
        "controls_state_time",
        "controls_arm_time",
        "controls_arm_state",
    ]
    with client.open_streamer(stream_channels) as reader:
        with client.open_writer(
            start=sy.TimeStamp.now(),
            channels=write_channels,
            enable_auto_commit=True,
        ) as writer:
            write_state_frame(writer, CHANNEL_STATES)
            print("\n" + "=" * 60)
            print("READY - Flip any valve in the Synnax console")
            print(f"Sending packets at {LOOP_HZ:.2f} Hz")
            print("=" * 60 + "\n")
            while LOOP.wait():
                if stop_flag is not None and stop_flag.is_set():
                    break
                incoming = reader.read(timeout=0)
                if incoming is not None:
                    next_states = CHANNEL_STATES.copy()
                    for i in range(NUM_CHANNELS):
                        channel_values = incoming[f"controls_cmd_{i + 1}"]
                        if len(channel_values) > 0:
                            next_states[i] = int(channel_values[-1])
                    arm_values = incoming["controls_arm_cmd"]
                    if len(arm_values) > 0:
                        ARMED = int(arm_values[-1])
                    if not ARMED:
                        next_states[6] = SAFE_STATES[6]  # MPV requires arm.
                        next_states[3] = SAFE_STATES[3]  # Ignite requires arm.
                    CHANNEL_STATES = next_states
                send_controls_command(CHANNEL_STATES)
                write_state_frame(writer, CHANNEL_STATES)
                writer.write(
                    {
                        "controls_arm_time": sy.TimeStamp.now(),
                        "controls_arm_state": ARMED,
                    }
                )
    if mqtt_client is not None:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
    if serial_client is not None and serial_client.is_open:
        serial_client.close()


def main() -> None:
    run_controls_node()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nControls node shutting down...")
