# Usage instructions

1. Setup hotspot on your phone / wireless router. You have to do this since you need to know the IP address of the ESP32
2. Modify the network credentials
    ```C++
    // Replace with your network credentials
    const char *ssid = "";
    const char *password = "";
    ```
3. Flash the ESP32 with the SD card with this code
4. Grab the file you want using the following command: `curl -o <output file name> "http://<IP address of ESP32>:80/download-chunked?fileName=<name of file on ESP 32>`. 
    a. **IMPORTANT: This assumes that the file is named <file>.txt**. Modify the file accordingly if your file has a different extension