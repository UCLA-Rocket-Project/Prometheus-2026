# Usage instructions

1. Setup hotspot on your phone / wireless router. You have to do this since you need to know the IP address of the ESP32
2. Modify the network credentials
    ```C++
    // Replace with your network credentials
    const char *ssid = "";
    const char *password = "";
    ```
3. Flash the ESP32 with the SD card with this code
4. Grab the file you want using the following command: `curl -o file_name.csv "http://172.20.10.6/download-chunked?fileName=file_name.csv"`
