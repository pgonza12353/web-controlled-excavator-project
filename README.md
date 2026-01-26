## ESP32 Excavator Controller

This repository contains the firmware for an ESP32-based excavator controller. 
The firmware handles motor control for the base and tracks and receives commands over WebSocket.

## Status
### Completed
- Firmware logic for controlling the excavator base and tracks
- WebSocket client commands successfully reach the ESP32 WebSocket server
- ESP32 firmware logic successfully controls motors' speeds
### Planned
- Web-based control UI for remote operation via browser
- Improved UI/UX and command mapping

## Project Structure
```bash
/include/ - header files (including secrets template)
/src/ - firmware source (main.cpp)
platform.ini - PlatformIO configuration
```

## How it Works
1. WebSocket client sends control commands
2. ESP32 WebSocket server receives commands and updates internal state
3. `loop()` executes motor control based on the latest state
4. Motors respond in real time with normalized speed values

## Wi-Fi Setup
Wi-Fi credentials are stored locally and are **not committed** to GitHub. 

To connect the device to WiFi:

1. Copy: `include/secrets.example.h` -> `include/secrets.h`
2. Fill in your SSID and password

> The `include/secrets.h` is gitignored

## How to Run/Test
1. Connect the ESP32 via USB to your computer
2. Build & Upload (VS Code PlatformIO):
    * Use PlatformIO -> Build
    * Use PlatformIO -> Upload 
3. Open Serial Monitor
    * Use PlatformIO -> Monitor
4. Connect WebSocket client
    * Connect to the ESP32 IP address
    * Use the WebSocket endpoint:
    ```ws://<ESP32_IP>:<PORT>```

> Note: A USB-to-serial driver may be required for some systems