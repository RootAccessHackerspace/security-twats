# Trespass Warning and Access Threat System (TWATS)

A motion-detection security alarm system built for ESP8266/ESP32 microcontrollers. This system detects motion, provides audible warnings, and sends push notifications when suspicious activity is detected.

## Features

- Motion detection with PIR sensor
- Escalating alert levels (warning tone, extended warning, full alarm)
- Push notifications via Pushover service
- Web interface for remote control
- Arming/disarming functionality
- Configurable alarm durations

## Hardware Requirements

- ESP8266 or ESP32 development board
- PIR motion sensor
- Piezo buzzer
- Breadboard and jumper wires
- Micro USB cable for power and programming

## Wiring

- PIR Sensor: Connect to pin D7
- Piezo Buzzer: Connect to pin D2
- Both sensors need to be connected to VCC (3.3V) and GND

## Software Setup

### Prerequisites

1. [Arduino IDE](https://www.arduino.cc/en/software) installed
2. ESP8266 board support installed in Arduino IDE
3. Required libraries:
   - ESP8266WiFi (or ESP32WiFi if using ESP32)
   - ESP8266WebServer (or WebServer if using ESP32)
   - ESP8266HTTPClient (or HTTPClient if using ESP32)

### Installation

1. Clone or download this repository
2. Create a `secrets.h` file in the `src` folder (see below)
3. Open the `Alarm.ino` file in Arduino IDE
4. Select the correct board and port
5. Upload the sketch to your ESP8266/ESP32

### Setting Up secrets.h

You need to create a `secrets.h` file containing your WiFi credentials and Pushover API keys. This file is not included in the repository for security reasons.

Create a new file named `secrets.h` in the `src` folder with the following content:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "YourWiFiName"
#define WIFI_PASSWORD "YourWiFiPassword"
#define PUSHOVER_USER_KEY "YourPushoverUserKey"
#define PUSHOVER_API_TOKEN "YourPushoverAPIToken"

#endif
```

Replace the placeholder values with your actual credentials:
- `YourWiFiName`: Your WiFi network name
- `YourWiFiPassword`: Your WiFi password
- `YourPushoverUserKey`: Your Pushover user key (obtained from Pushover account)
- `YourPushoverAPIToken`: Your Pushover API token (create an application at Pushover)

### Pushover Setup

1. Create an account at [Pushover](https://pushover.net/)
2. After logging in, you'll find your User Key in the dashboard
3. Create a new application to get an API token
4. Add these values to your `secrets.h` file

## Usage

1. Power on the device
2. Connect to the device's web interface using its IP address (shown in Serial Monitor during startup)
3. Use the web interface to:
   - Arm/disarm the system
   - Trigger a full alarm manually
   - Check system status and motion detection count

When armed, the system will:
1. Sound a short warning tone when motion is first detected
2. Trigger an extended warning if multiple motions are detected in a short period
3. Send push notifications for significant events

## Troubleshooting

- If notifications aren't working, verify your Pushover credentials and internet connection
- If the motion sensor is too sensitive, try adjusting the sensor's sensitivity knob or the detection thresholds in the code
- Check the Serial Monitor (9600 baud) for debugging information

## License

This project is available for hackerspace members and contributors.

## Security Note

The `secrets.h` file contains sensitive information. Never commit this file to public repositories or share it in public forums.
