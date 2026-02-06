# This is the documentation for setting up and using the Grafana-Telegraf pipeline on Windows

Note: This was written with connecting to the DAQ & Windows in mind. If you are using MacOS, refer to the documentation for the MacOS equivalent for this guide.

## What to Install
1. InfluxDB OSS V2
2. Telegraf
4. Python
5. Grafana



## Setting up InfluxDB OSS V2
To install, visit this website: `https://docs.influxdata.com/influxdb/v2/install/?t=Windows`
  - Extract the downloaded zip
  - In the PowerShell, navigate to the directory you extracted the downloaded zip in and run: `.\influxd.exe` [KEEP THIS RUNNING AT ALL TIMES if you plan to collect data]
    
Open `http://localhost:8086` to complete account setup in the UI
- Save the API Token somewhere safe
- Create a bucket

Note: Your organization name is the name of the tab if you forget it

## Setting up Telegraf
Run in PowerShell (everything inside the || ||): 
|| wget `
https://dl.influxdata.com/telegraf/releases/telegraf-1.37.1_windows_amd64.zip `
-UseBasicParsing `
-OutFile telegraf-1.37.1_windows_amd64.zip
Expand-Archive .\telegraf-1.37.1_windows_amd64.zip `
-DestinationPath 'C:\Program Files\InfluxData\telegraf\' ||


Run this after:
`cd "C:\Program Files\InfluxData\telegraf";
mv .\telegraf-1.37.1\telegraf.* .`

In your File Explorer, navigate to the directory `C:\Program Files\InfluxData\telegraf` and open `telegraf.conf` (easiest if you use VSCode to open)
 - Replace the contents of the file with the telegrafConfig.conf file in the github data visualization pipeline/ directory
 - Add your InfluxDB API token from earlier to the `token = ""` line
 - Add your InfluxDB organization name to the `organization = ""` line
 - Add the name of the InfluxDB bucket you created earlier to the `bucket = ""` line
   
Note: the `interval` & `flush_interval` should be at a rate >= the rate at which the [Arduino code] collects data

In an Adminstrative PowerShell, run (everything in the || ||): || .\telegraf.exe --service install `
--config "C:\Program Files\InfluxData\telegraf\telegraf.conf" ||
- To stop it, simply press Ctrl + C
- This will be running in that PowerShell window. Open a new window if you want to run other commands.

## Setting up Grafana
- To install, go to this website & download the Windows Installer version: `https://grafana.com/grafana/download?platform=windows`
- After it's finished downloading, run it --> this will automatically start running Grafana as a background process.
    - If you want to stop or restart it:
      1. Right click on the Windows (bottom-left corner of screen)
      2. Click `Run`
      3. Enter `servicesmsc`
      4. Scroll down & click Grafana
         


Open: `[http](http://localhost:3000)`
- Here, you'll be prompted to enter a username and password. Enter `admin` for both fields. You'll be prompted to create a new password right after.

## Setting up Python
If you've already downloaded VSCode with Python, feel free to skip to the "Create a new folder..." section

Install Visual Studio Code (VSCode)

Install the Python Install Manager in the Microsoft Store
- After installation, run it and install most preferrably the 3.12 package (the latest 3.14 version is fine too)
  
Open VSCode and on the left-hand side click the Extensions tab
- Download the Python extension (doing this should also download the other two needed Python extensions)
- If at any time during this guide VSCode prompts you to download some extension at the bottom-right of the screen, go ahead
  
Press `Ctrl+Shift+P` and enter `Python: Select Interpreter` --> choose the recommended one

Create a new folder to store the Python script(s) for this guide and open it in VSCode
- Create a new file named `requirements.txt` and paste in it the contents of the requirements.txt file found in this Github repo directory
    - If you downloaded a 3.14 Python package, change `netifaces==0.10.6` to `netifaces2`
- Create a new [name].py file (name it whatever you want).
    - If you want to run sample data to test your system, paste in the contents of sampledata.py
    - If you want to connect to the DAQ, paste in the contents of the serialToTelegraf.py
      
In the python file, click on the Run icon on the top-right
- Enter `python -m venv venv`
- After, enter `pip install -r requirements.txt`

If you Run the file again (`./[name of file]`), data should be outputting into your VSCode terminal (if you're properly connected to the according device or you're using sample data)

## To adjust Grafana UI refresh rate
- Navigate to the directory where telegraf is installed (likely: `C:\Program Files\InfluxData\telegraf\`)
- Locate a file named `default` and run it (easiest if you use VSCode to open it)
- Find the section `[dashboards]` (you can use Ctrl+F to find [dashboards])
- Adjust the line `min_refresh_interval: 5s` to your desirable Grafana dashboard refresh rate (eg. 500ms)
- Save the file (Ctrl+S)
- Restart grafana
- Go back to grafana web browser (http://localhost:3000)
  -- Open the Dashboards tab
  -- Click New Dashboard on the top right, and then click import
  -- ** Insert the code from Cold_Flow_Grafana_GUI.json into the import section **
    - Change each instance of a bucket name in the json to the name of your own influxdb bucket. Be sure to remove the [] brackets too. Should be: `"bucket name"`

Afterward, you MUST set grafana to reload faster
    - Open the dashboard UI
    - On the top right click edit, and open settings (you should already be in the General tab after clicking Settings)
    - Scroll down to Time Options and at auto-refresh add your desired refresh rate as on option (eg. 500ms). If this doesn't work, restart grafana and try again.


## If you run into any difficult issues, ask anybody from Software DAQ GUI team for assistance
