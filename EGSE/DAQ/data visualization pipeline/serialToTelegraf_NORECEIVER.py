import csv
import time
import requests
import paho.mqtt.client as mqtt
from datetime import datetime

BROKER = "192.168.0.100"
TOPIC = "DAQ_transmitter/receiver"
TELEGRAF_URL = "http://localhost:8094/telegraf"

today_date = datetime.now().strftime("%Y-%m-%d")
file_path = f"rocket_data_{today_date}.csv"

f = open(file_path, "w", newline="")
writer = csv.writer(f)
writer.writerow(['pt0','pt1','pt2','pt3','lc0','uptime','timestamp'])

http_session = requests.Session()

def on_message(client, userdata, msg):
    raw_line = msg.payload.decode()

    if not raw_line.startswith("rocket_data"):
        return

    #send to Telegraf
    try:
        http_session.post(TELEGRAF_URL, data=raw_line, timeout=0.02)
    except:
        pass

    #parse
    try:
        _, data_part = raw_line.split(" ", 1)

        fields = {}
        for item in data_part.split(","):
            if "=" in item:
                k,v = item.split("=",1)
                fields[k] = v

        writer.writerow([
            fields.get('pt0',''),
            fields.get('pt1',''),
            fields.get('pt2',''),
            fields.get('pt3',''),
            fields.get('lc0',''),
            fields.get('uptime_ms',''),
            time.time()
        ])
        f.flush()

        print("Logged:", fields)

    except Exception as e:
        print("Parse error:", e)

client = mqtt.Client()
client.on_message = on_message
client.reconnect_delay_set(min_delay=1, max_delay=5)

client.connect(BROKER, 1883, 60)
client.subscribe(TOPIC)

print("MQTT Bridge Active...")
client.loop_forever()
