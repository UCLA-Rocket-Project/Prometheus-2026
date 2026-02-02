import serial
import csv
import time
import requests
import os.path
import socketio
from datetime import datetime  # <--- New Import

# --- CONFIGURATION ---
PORT = '/dev/cu.usbserial-0001' 
BAUDRATE = 115200
WEBSOCKET_ADDRESS = "http://localhost:3001/"
TELEGRAF_URL = "http://localhost:8094/telegraf"

# --- SETUP ---
# Determine whether to save to csv
printToCsv = True
print("Save to csv? (y/n) (default is yes)")
response = input()
if response.lower().strip() == "n" or response.lower().strip() == "no":
    printToCsv = False
    print(f"printToCSV = {printToCsv}")    


# 1. Dated CSV Filename logic
today_date = datetime.now().strftime("%Y-%m-%d")
base_name = f"rocket_data_{today_date}"
file_path = f"{base_name}_0.csv"
counter = 0

# Increment the version number if the file already exists
while os.path.exists(file_path):
    counter += 1
    file_path = f"{base_name}_{counter}.csv"

# 2. Setup WebSockets
sio = socketio.Client()
try:
    sio.connect(WEBSOCKET_ADDRESS, wait_timeout=2)
    print("Connected to HUD")
except:
    print("HUD not found (Data will still log to Telegraf and CSV if enabled)")

# 3. Setup Serial
try:
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)
    print(f"Connected to ESP32 on {PORT}")
except Exception as e:
    print(f"SERIAL ERROR: {e}")
    exit()

# 4. Write CSV Header immediately
if(printToCsv == True):
    with open(file_path, mode='w', newline='') as f:
        header = [f'pt{i}' for i in range(8)] + ['lc0', 'lc1', 'timestamp']
        csv.writer(f).writerow(header)

# --- HELPERS ---

def format_for_hud(sensor_dict):
    """Formats data for the HUD display"""
    hud_config = []
    for i in range(8):
        key = f'pt{i}'
        hud_config.append({'dataName': f'PT {i}', 'value': sensor_dict.get(key, 0.0), 'units': 'PSI'})
    for i in range(2):
        key = f'lc{i}'
        hud_config.append({'dataName': f'LC {i}', 'value': sensor_dict.get(key, 0.0), 'units': 'lbs'})
    return hud_config

def parse_line(line):
    """Turns 'rocket_data pt0=1.2,pt1=3.4' into {'pt0': 1.2, 'pt1': 3.4}"""
    try:
        # A 797.9732666016,800.0090332031,801.8237304688,490.4696044922,1.9790169001,21195 Z
        
        parts = line.strip().split(" ")
        if len(parts) < 2: return None
        fields = parts[1].split(",")
        return {f.split("=")[0]: float(f.split("=")[1]) for f in fields if "=" in f}
    except:
        return None

# --- MAIN LOOP ---
print(f"Bridge Active. Logging to: {file_path}")
print("Press Ctrl+C to stop.")

while True:
    try:
        if ser.in_waiting > 0:
            # Read from ESP32
            raw_line = ser.readline().decode('utf-8').strip()
            
            if not raw_line.startswith("rocket_data"):
                continue

            # 1. Forward to Telegraf (via HTTP)
            try:
                requests.post(TELEGRAF_URL, data=raw_line, timeout=0.05)
            except:
                pass 

            # 2. Parse and log to CSV/HUD
            data = parse_line(raw_line)
            if data:
                if printToCsv == True:
                    # Append to CSV
                    with open(file_path, mode='a', newline='') as f:
                        csv.writer(f).writerow([data.get(f'pt{i}', '') for i in range(8)] + 
                                            [data.get('lc0', ''), data.get('lc1',''), time.time()])
                
                # Update HUD
                if sio.connected:
                    formatted_data = format_for_hud(data)
                    sio.emit('live/receive-data-stream-from-mqtt', formatted_data)

    except KeyboardInterrupt:
        print("\nStopping data collection...")
        break
    except Exception as e:
        print(f"General Error: {e}")
