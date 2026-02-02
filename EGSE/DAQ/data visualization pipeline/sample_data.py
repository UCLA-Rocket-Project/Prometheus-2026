import random
import time
import requests # We use requests for HTTP instead of socket

# --- CONFIGURATION ---
# This matches your Telegraf http_listener_v2 config
TELEGRAF_URL = "http://localhost:8094/telegraf" 

print(f"Sending data to {TELEGRAF_URL}...")

while True:
    # 1. Generate fake sensor data
    pt0 = random.uniform(0, 100)
    pt1 = random.uniform(0, 100)
    pt2 = random.uniform(0, 100)
    pt3 = random.uniform(0, 100)
    pt4 = random.uniform(0, 100)
    pt5 = random.uniform(0, 100)
    pt6 = random.uniform(0, 100)
    pt7 = random.uniform(0, 100)
    lc0 = random.uniform(0, 100)
    lc1 = random.uniform(0, 100)
    
    # Calculate uptime in ms (mocking the Arduino logic)
    uptime_ms = int(time.time() * 1000)

    # 2. Format as InfluxDB Line Protocol
    # Format: measurement_name field1=value,field2=value
    # Note: No spaces after commas!
    payload = (
        f"rocket_data "
        f"pt0={pt0:.2f},pt1={pt1:.2f},pt2={pt2:.2f},pt3={pt3:.2f},"
        f"pt4={pt4:.2f},pt5={pt5:.2f},pt6={pt6:.2f},pt7={pt7:.2f},"
        f"lc0={lc0:.2f},lc1={lc1:.2f},uptime_ms={uptime_ms}"
    )

    try:
        # 3. Send via HTTP POST
        response = requests.post(TELEGRAF_URL, data=payload)
        
        # Check if successful (204 means success)
        if response.status_code == 204:
            print(f"Sent: {payload[:50]}... (Success)")
        else:
            print(f"Error: Telegraf returned {response.status_code}")
            
    except requests.exceptions.ConnectionError:
        print("Connection Error: Is Telegraf running? (brew services start telegraf)")

    time.sleep(1)
