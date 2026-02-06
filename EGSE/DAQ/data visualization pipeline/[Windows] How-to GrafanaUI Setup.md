# This is the documentation for setting up and using the Grafana-Telegraf pipeline on Windows
THIS IS NOT FINISHED, PLEASE DO NOT FOLLOW THIS
Note: This was written with Windows in mind. If you are using MacOS, refer to the documentation for the MacOS equivalent for this guide.

## What to Install
1. InfluxDB OSS V2
2. Telegraf
4. Python - download this however you want
5. Grafana

## Summary of the Wired (ethernet) Pipeline
1. Data comes into the DAQ via the transmitter board esp32
2. Transmitter board sends data in a formatted string (InfluxDB Line Protocol) via serial
3. Receiver board esp32 reads data via serial and sends it through serial via usb to a computer
4. serialToTelegraf.py reads data and logs it in a local csv, and also sends a copy via HTTP POST to a telegraf instance. 
5. Telegraf stores data in influxDB bucket
6. Grafana reads the bucket and displays the data on the dashboard 
- sample_data.py tests this pipeline by creating random data and sending it to telegraf. It doesn't test the intake of data via usb

## Setting up InfluxDB OSS V2
To install, visit this website: `https://docs.influxdata.com/influxdb/v2/install/?t=Windows`
  - Extract the downloaded zip
  - In the PowerShell, navigate to the directory you extracted the downloaded zip in and run: `.\influxd.exe` [KEEP THIS RUNNING AT ALL TIMES if you plan to collect data]
Open `http://localhost:8086` to complete account setup in the UI
- Save the API Token somewhere safe

## Setting up Telegraf
Run in PowerShell: 
`wget `
https://dl.influxdata.com/telegraf/releases/telegraf-1.37.1_windows_amd64.zip `
-UseBasicParsing `
-OutFile telegraf-1.37.1_windows_amd64.zip
Expand-Archive .\telegraf-1.37.1_windows_amd64.zip `
-DestinationPath 'C:\Program Files\InfluxData\telegraf\'`


Run this after:
`cd "C:\Program Files\InfluxData\telegraf";
mv .\telegraf-1.37.1\telegraf.* .`

In your File Explorer, navigate to the direction `C:\Program Files\InfluxData\telegraf` and open `telegraf.conf` (easiest if you use VSCode to open)

Create a configuration file in the file path using the telegrafConfig.conf and name it something like `telegraf.conf`. Add the missing values as necessary based on your influxdb configurations.
It's best to create a local project specific config file.
In the terminal, navigate to the folder that contains the config file and run: ` telegraf --config telegraf.conf`
- To stop it, simply press Ctrl + C
- This will be running in that terminal window. Open a new window if you want to run other commands.

## Setting up Grafana
- To install: `brew install grafana`
- To run: `brew services start grafana`
- To stop: `brew services stop grafana`
Open: `[http](http://localhost:3000)`

## To adjust Grafana UI refresh rate
Open in terminal `nano /opt/homebrew/etc/grafana/grafana.ini` (apple silicon) or `nano /usr/local/etc/grafana/grafana.ini` (intel mac)
- Find the section `[dashboards]` (you can use Ctrl+W to find [dashboards])
- Adjust the line `min_refresh_interval: 5s` to your desirable Grafana dashboard refresh rate (eg. 500ms)
- Save the file (Ctrl+O --> `y` --> Enter)
- Restart grafana
- Go back to grafana web browser (http://localhost:3000)
  -- Open dashboard
  -- Click New Dashboard on the top righ, and then clich import
  -- ** Insert the code from Cold_Flow_Grafana_GUI.json into the import section **
    - remember to change the bucket to the name of your own influxdb bucket. remove the [] brackets too. Should be: `"bucket name"`

  Afterward, you MUST set grafana to reload faster
    - Open the dashboard UI
    - On the top right click edit, and open settings (you should already be in the General tab after clicking Settings)
    - Scroll down to Time Options and at auto-refresh add 500ms as on option. If this doesn't work, do `brew services restart grafana` and try again



To create an account, type admin for the passwrod and username and hit enter to start the new account creation process. 

To link it to your influxdb, lowkey you can jut ask AI on how to do that.

