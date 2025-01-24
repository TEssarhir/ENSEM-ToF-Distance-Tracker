# ESP32 Distance Tracker

This project is an ESP32-based distance tracker that uses a VL53L0X time-of-flight sensor to measure distances and display the data on a web interface. The project includes real-time data updates via WebSocket and provides metrics such as maximum, minimum, and average distances.

## Features

- Real-time distance measurement using VL53L0X sensor
- Web interface for live data visualization
- WebSocket for real-time data updates
- Metrics calculation (max, min, average distances)
- CSV data download
- OTA updates using ElegantOTA

## Hardware Requirements

- ESP32 development board
- VL53L0X time-of-flight sensor
- WiFi network

## Software Requirements

- PlatformIO
- Arduino framework

## Installation

1. Clone this repository.
2. Open the project in PlatformIO.
3. Modify the WiFi credentials in `src/main.cpp`:
    ```cpp
    const char* ssid = "your-ssid";
    const char* password = "your-password";
    ```
4. Build and upload the project to your ESP32 board.

## Usage

1. Connect to the ESP32's WiFi network or ensure it is connected to your local WiFi.
2. Open a web browser and navigate to the ESP32's IP address.
3. The web interface will display the live distance data and metrics.
4. Use the "Download CSV Data" button to download the recorded data.

## File Structure

- `src/main.cpp`: Main application code.
- `data/index.html`: Web interface HTML file.
- `data/style.css`: Web interface CSS file.
- `data/script.js`: Web interface JavaScript file.
- `platformio.ini`: PlatformIO project configuration file.

## Libraries Used

- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA)
- [Adafruit_VL53L0X](https://github.com/adafruit/Adafruit_VL53L0X)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)