STEPS FOR SETTING UP MOSQUITTO SERVER

1. Download Mosquitto install and install with default configurations
2. Copy the mosquitto.conf file from github and paste over your own mosquitto.conf file (ONLY IF ROUTER HAS NO INTERNET ACCESS)
3. run mosquitto with this command in terminal: mosquitto -c mosquitto.conf -v
4. check services to see if mosquitto is running (on windows hit [windows key + r], then enter services.msc)
5. Allow firewall access (2 options): temporarily turn off firewall for public domains | OR | open firewall advanced settings -> add inbound rule (port: 1883, protocol: TCP, Allow)

YOU ONLY NEED TO DO STEPS 1-5 ONCE, 6-9 MUST BE REPEATED EVERY TIME SERVER IS SET UP

6. power and turn on the router
7. connect mosquitto server host (most likely your computer) to WiFi router. You can find username and password information in code (ssid is username, psasword is the password)
8. After connecting to router, run the command "ipconfig" in terminal and read the ipv4 under "Wireless LAN adapter Wi-Fi:"
9. Change the code for both esp32's involved in communication such that the variable mqtt_server = the host IP you just found
