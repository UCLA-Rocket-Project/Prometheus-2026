import serial
import csv
import time
import requests
import os.path
import socketio
from datetime import datetime  

# --- CONFIGURATION ---
PORT = '/dev/cu.usbserial-0001' 
BAUDRATE = 115200
WEBSOCKET_ADDRESS = "http://localhost:3001/"
TELEGRAF_URL = "http://localhost:8094/telegraf"

# --- USER INPUT TO DETERMINE WHETHER TO TURN ON CSV ---
printToCsv = True
print("Save to csv? (y/n) (default is yes)")
response = input()
if response in ["n", "no"]:
    printToCsv = False
    print(f"printToCSV = {printToCsv}")    

# --- SETUP ---
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
http_session = requests.Session() # Keeps connection open for speed
sio = socketio.Client()

try:
    sio.connect(WEBSOCKET_ADDRESS, wait_timeout=2)
    print("Connected to HUD")
except:
    print("HUD not found (Data will still log to Telegraf and CSV if enabled)")

# 3. Setup Serial
try:
    ser = serial.Serial(PORT, BAUDRATE, timeout=0.1)
    ser.reset_input_buffer()
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
def format_for_hud(fields):
    """Formats data for the HUD display using the split dictionary"""
    hud_config = []
    for i in range(8):
        key = f'pt{i}'
        hud_config.append({'dataName': f'PT {i}', 'value': float(fields.get(key, 0.0)), 'units': 'PSI'})
    for i in range(2):
        key = f'lc{i}'
        hud_config.append({'dataName': f'LC {i}', 'value': float(fields.get(key, 0.0)), 'units': 'lbs'})
    return hud_config

# --- MAIN LOOP ---
print(f"Bridge Active. Press Ctrl+C to stop.")

# Open file context only if needed
# We use a dummy context manager if printToCsv is False
class DummyFile:
    def __enter__(self): return None
    def __exit__(self, *args): pass

file_context = open(file_path, mode='w', newline='') if printToCsv else DummyFile()

with file_context as f:
    writer = csv.writer(f) if f else None
    if writer:
        writer.writerow([f'pt{i}' for i in range(8)] + ['lc0', 'lc1', 'timestamp'])

    try:
        while True:
            if ser.in_waiting > 0:
                raw_line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                if not raw_line.startswith("rocket_data"):
                    continue

                # 1. High-Speed Telegraf POST
                try:
                    http_session.post(TELEGRAF_URL, data=raw_line, timeout=0.02)
                except:
                    pass 

                # 2. Parse and log
                try:
                    # Optimized parsing: split the 'rocket_data ' prefix, then split fields
                    _, data_part = raw_line.split(" ", 1)
                    fields = dict(item.split("=") for item in data_part.split(","))

                    if printToCsv and writer:
                        writer.writerow([fields.get(f'pt{i}', '') for i in range(8)] + 
                                        [fields.get('lc0', ''), fields.get('lc1',''), time.time()])
                    
                    if sio.connected:
                        formatted_data = format_for_hud(fields)
                        sio.emit('live/receive-data-stream-from-mqtt', formatted_data)
                except:
                    continue

    except KeyboardInterrupt:
        print("\nStopping data collection...")
    except Exception as e:
        print(f"General Error: {e}")
