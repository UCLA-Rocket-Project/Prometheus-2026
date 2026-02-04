# This is the Documentation for setting up and usinng the DAQ Grafana pipeline
Note: This was written with the MacOS in mind. If you are using windows, the steps are similar, but the syntax to get there might be different. 
Note: If you don't have brew on Mac installed, install it. 

## What to Install
1. InfluxDB OSS V2
2. Telegraf
3. Python - download this however you want
4. Grafana

## Summary of the Wired (ethernet) Pipeline
1. Data comes into the DAQ via the transmitter board esp32
2. Transmitter board sends data in a formatted string (InfluxDB Line Protocol) via serial
3. Receiver board esp32 reads data via serial and sends it through serial via usb to a computer
4. serialToTelegraf.py reads data and logs it in a local csv, and also sends a copy via HTTP POST to a telegraf instance. 
5. Telegraf stores data in influxDB bucket
6. Grafana reads the bucket and displays the data on the dashboard 
- sample_data.py tests this pipeline by creating random data and sending it to telegraf. It doesn't test the intake of data via usb

## Setting up InfluxDB OSS V2
Run in the terminal:
- To install: `brew install influxdb@2`
- To run: `brew services start influxdb@2`
- To stop: `brew services stop influxdb@2`
Open `http://localhost:8086` to complete account setup in the UI
- Save the operator API Token somewhere safe

## Setting up Telegraf
Run in terminal:
- To install: `brew install telegraf`

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

  afterward MUST set grafana to reload faster
    - Open dashboard 
    - On the top right click edit, and open settings
    - Scroll down to Time Options and at auto-refresh add 500ms as on option.



To create an account, type admin for the passwrod and username and hit enter to start the new account creation process. 

To link it to your influxdb, lowkey you can jut ask AI on how to do that.

