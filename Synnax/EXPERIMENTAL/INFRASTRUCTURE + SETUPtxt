The system is built around a Raspberry Pi that acts as the central hub for everything. 
The Pi has two USB cables coming out of it. One goes to the Controls ESP32, which is the microcontroller that physically drives the valves. 
The other goes to the DAQ ESP32, which is the microcontroller that reads all the sensors. 
The Pi talks to both of these over serial at 115200 baud, meaning it is just sending and receiving raw text over a USB connection like a very simple wire.

The Pi also runs Synnax, which is a time-series database with a web interface. 
Synnax is what the operator actually interacts with. From any computer on the same network, you open a browser, go to the Pi's IP address on port 9091, and you get the Synnax console. 
From there you can flip valves, arm the system, and watch sensor data in real time. The Pi is the only machine that needs to be physically near the rocket hardware. 
The operator's computer just needs to be on the same WiFi or Ethernet network.

When the operator flips a valve in the Synnax console, that command goes from the browser to the Synnax server running on the Pi. 
Python scripts running on the Pi pick up that command, package it up, and send it over the USB serial cable to the Controls ESP32. 
The ESP32 then drives the GPIO pins that are wired to the relay board, which actually opens or closes the valves. 
The Pi also sends back a confirmation to Synnax so the console always shows the real applied state, not just what was requested.

On the sensor side, the DAQ ESP32 is constantly reading 8 pressure transducers and 2 load cells. 
It formats those readings into a line of text and sends them over the USB serial cable to the Pi. 
A Python script on the Pi reads those lines as fast as they come in and writes the values into Synnax. 
That is how sensor data ends up visible in the console in real time.

To run the whole thing, you SSH into the Pi from any computer, navigate to the project folder, and run one Python script. 
That script starts the controls side and the DAQ side at the same time. 
When you want to stop, you press Ctrl+C in the SSH terminal and both sides shut down cleanly.

The only things that need to be physically present at the test site are the Pi, the two ESP32s, and whatever relay board and sensors are wired up to them. 
Everything else, including monitoring and commanding, happens over the network from wherever the operator is sitting.
---

SETUP WORKFLOW

First, flash the two ESP32s with their respective firmware files using the Arduino IDE or PlatformIO on your development machine. 
The controls firmware goes on the Controls ESP32 and the DAQ firmware goes on the DAQ ESP32. 
Do this before connecting anything to the Pi.

Next, plug both ESP32s into the Pi via USB. 
Plug them in a consistent order every time because Linux assigns ttyUSB0 and ttyUSB1 based on the order they enumerate. 
The Controls ESP32 should be ttyUSB0 and the DAQ ESP32 should be ttyUSB1. 
If they end up swapped you can fix it with environment variables before running the software.

On the Pi, install Synnax and start the Synnax server so it is running on port 9091. 
Then copy the project files to the Pi, either with scp from your development machine or by cloning from a git repo. 
Once the files are there, run pip3 install -r requirements.txt to install the Python dependencies.

If you want Grafana dashboards, also install InfluxDB 2 and Grafana on the Pi. 
Set up an InfluxDB organization called rocket and a bucket called daq, generate an API token, and point Grafana at InfluxDB as a data source. 
Then set the INFLUXDB_ENABLED and INFLUXDB_TOKEN environment variables before starting the software.

Open the necessary firewall ports. Port 9091 for Synnax, port 3000 for Grafana, and port 8086 for InfluxDB if you are using it. 
Port 22 for SSH is usually already open.

To start everything, SSH into the Pi, navigate to the project folder, and run python3 PiNode.py. 
That is the only command you need. It starts both the controls side and the DAQ side at the same time.

Once it is running, open a browser on any computer on the network. 
Go to the Pi's IP address on port 9091 to get the Synnax console. 
From there you arm the system by setting the arm channel to 1, then you can command valves and watch sensor data live. 
If you set up Grafana, go to port 3000 for the dashboards.

To shut down, press Ctrl+C in the SSH terminal. Both nodes will stop cleanly within a few seconds.
