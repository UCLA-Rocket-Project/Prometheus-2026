import serial
import csv
import time
import requests
import os.path
from datetime import datetime  

# --- CONFIGURATION ---
PORT = 'COM5'
BAUDRATE = 115200
TELEGRAF_URL = "http://localhost:8094/telegraf"

printToCsv = True

# --- SETUP ---
# 1. Dated CSV Filename logic
today_date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
base_name = f"rocket_data_{today_date}"
file_path = f"{base_name}_0.csv"
counter = 0

# Increment the version number if the file already exists
while os.path.exists(file_path):
    counter += 1
    file_path = f"{base_name}_{counter}.csv"

# 2. Setup WebSockets
http_session = requests.Session() # Keeps connection open for speed

# 4. Write CSV Header immediately
# if(printToCsv == True):
#     with open(file_path, mode='w', newline='') as f:
#         header = [f'pt{i}' for i in range(8)] + ['lc0', 'lc1', 'timestamp']
#         csv.writer(f).writerow(header)


# --- MAIN LOOP ---
print(f"Bridge Active. Press Ctrl+C to stop.")

file_context = open(file_path, mode='w', newline='')

import time

buffer = ""

TELEGRAF_HZ = 4  # max posts per second
last_telegraf_post = 0
def handleTelegrafPost(raw_data):
    global last_telegraf_post

    now = time.time()

    # Minimum time between posts
    min_interval = 1.0 / TELEGRAF_HZ

    # Skip if called too soon
    if now - last_telegraf_post < min_interval:
        return

    try:
        http_session.post(TELEGRAF_URL, data=raw_data, timeout=1)
        last_telegraf_post = now
        print("Posted")
    except Exception as e:
        print(e)
        print("Fail");
        pass

def handleLine(raw_line, writer):
    print(raw_line)

    if not raw_line.startswith("rocket_data"):
        return

    handleTelegrafPost(raw_line)

    try:
        _, data_part = raw_line.split(" ", 1)
        fields = dict(item.split("=") for item in data_part.split(","))

        if printToCsv and writer:
            writer.writerow(
                [fields.get(f'pt{i}', '') for i in range(8)] +
                [fields.get('lc0', ''), fields.get('lc1', ''), time.time()]
            )
    except Exception:
        pass


try:
    with open(file_path, mode='a', newline='') as f, \
         serial.Serial(PORT, BAUDRATE, timeout=0.1) as ser:

        writer = csv.writer(f)

        header = [f'pt{i}' for i in range(8)] + ['lc0', 'lc1', 'timestamp']
        writer.writerow(header)
        f.flush()


        print("Logging started. Press Ctrl+C to stop.")

        while True:

            if ser.in_waiting:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')

                buffer += data
                lines = buffer.split('\n')

                # Keep incomplete line
                buffer = lines.pop()

                for line in lines:
                    line = line.strip()

                    if line:
                        handleLine(line, writer)

                f.flush()

except KeyboardInterrupt:
    pass
