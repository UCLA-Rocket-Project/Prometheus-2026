import serial
import time
import requests
import socketio

# --- CONFIGURATION ---
PORT = '/dev/cu.usbserial-120' # double check, may randomly change
BAUDRATE = 115200
WEBSOCKET_ADDRESS = "http://localhost:3001/"
TELEGRAF_URL = "http://localhost:8094/telegraf"

# --- SETUP ---
http_session = requests.Session()
sio = socketio.Client()

try:
    sio.connect(WEBSOCKET_ADDRESS, wait_timeout=2)
    print("Connected to HUD")
except:
    print("HUD not found (Data will still log to Telegraf)")

try:
    ser = serial.Serial(PORT, BAUDRATE, timeout=0.1)
    ser.reset_input_buffer()
    print(f"Connected to ESP32 on {PORT}")
except Exception as e:
    print(f"SERIAL ERROR: {e}")
    exit()

print("Bridge Active. Press Ctrl+C to stop.")

# store last telemetry
latest_fields = {}

# --- MAIN LOOP ---
try:
    while True:
        if ser.in_waiting > 0:
            raw_line = ser.readline().decode('utf-8', errors='ignore').strip()

            print(raw_line)

            # --- CASE 1: telemetry line ---
            if raw_line.startswith("SENDING: nosecone_data"):
                try:
                    data_part = raw_line.split("nosecone_data", 1)[1].strip()

                    latest_fields = dict(
                        item.split("=") for item in data_part.split(",") if "=" in item
                    )

                except Exception as e:
                    print("Parse error:", e)

            # --- CASE 2: RSSI/SNR line ---
            elif raw_line.startswith("RSSI"):
                try:
                    parts = raw_line.replace(":", "").split()
                    # RSSI -57 SNR 12.50

                    latest_fields["rssi"] = float(parts[1])
                    latest_fields["snr"] = float(parts[3])

                    # Build Influx line with EVERYTHING
                    influx_line = "nosecone_data " + ",".join(
                        f"{k}={float(v)}" for k, v in latest_fields.items()
                    )

                    print("→ Influx:", influx_line)

                    # send to Telegraf
                    try:
                        http_session.post(TELEGRAF_URL, data=influx_line, timeout=0.02)
                    except:
                        pass

                    # optional HUD
                    if sio.connected:
                        sio.emit('live/receive-data-stream-from-mqtt', latest_fields)

                except Exception as e:
                    print("RSSI parse error:", e)

except KeyboardInterrupt:
    print("\nStopping...")
