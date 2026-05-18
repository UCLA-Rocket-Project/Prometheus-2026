For running noseconegui.py through bridge.py : 

python3 /Users/meghjoshi/Prometheus-2026/Web_app/dashboard/bridge/bridge.py --port /dev/cu.usbserial-310 --baud 115200
[change to your path]


For running the Web App on your machine and on the same network [HOTSPOT] : 

cd /Users/meghjoshi/Prometheus-2026/Web_app/dashboard && npm run dev -- --host
