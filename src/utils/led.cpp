#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include "led.h"

// extern dependencies from main.cpp
extern Adafruit_NeoPixel pixels;
extern bool wifiConnected;
extern bool alarmActive;
extern bool extendedWarningActive;
extern bool systemArmed;

// LED state definitions (moved from main.cpp)
extern LedState currentLedState;
LedState previousLedState = LED_OFFLINE;
unsigned long lastLedUpdateTime = 0;
unsigned long ledUpdateInterval = 500;
int ledAnimationStep = 0;

void ledStatus(const char* status) {
    if (strcmp(status, "ready") == 0) {
        currentLedState = wifiConnected ? LED_CONNECTED_DISARMED : LED_OFFLINE;
    } else if (strcmp(status, "armed") == 0) {
        currentLedState = wifiConnected ? LED_ARMED_CONNECTED : LED_ARMED_DISCONNECTED;
    } else if (strcmp(status, "alarm") == 0) {
        previousLedState = currentLedState;
        currentLedState = LED_FULL_ALARM;
    } else if (strcmp(status, "off") == 0) {
        pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Off
        pixels.show();
        return;
    } else {
        pixels.setPixelColor(0, pixels.Color(75, 75, 75)); // White for unknown status
        pixels.show();
        return;
    }
    ledAnimationStep = 0; 
}

void flashRedBlueLed() {
    // For compatibility; ensure we are in FULL_ALARM state
    previousLedState = currentLedState;
    currentLedState = LED_FULL_ALARM;
    ledAnimationStep = 0;
}

void updateLedStatus() {
    unsigned long currentTime = millis();
    
    // Check WiFi connection status
    bool newWifiStatus = (WiFi.status() == WL_CONNECTED);
    if (wifiConnected != newWifiStatus) {
        wifiConnected = newWifiStatus;
        if (!alarmActive && !extendedWarningActive) {
            currentLedState = systemArmed ? 
                (wifiConnected ? LED_ARMED_CONNECTED : LED_ARMED_DISCONNECTED) :
                (wifiConnected ? LED_CONNECTED_DISARMED : LED_OFFLINE);
            ledAnimationStep = 0;
        }
    }
    
    if (alarmActive && currentLedState != LED_FULL_ALARM) {
        previousLedState = currentLedState;
        currentLedState = LED_FULL_ALARM;
        ledAnimationStep = 0;
    } else if (extendedWarningActive && !alarmActive && currentLedState != LED_WARNING_ALARM) {
        previousLedState = currentLedState;
        currentLedState = LED_WARNING_ALARM;
        ledAnimationStep = 0;
    }
    
    if (currentTime - lastLedUpdateTime >= ledUpdateInterval) {
        lastLedUpdateTime = currentTime;
        switch (currentLedState) {
            case LED_OFFLINE:
                if (ledAnimationStep == 0) {
                    pixels.setPixelColor(0, pixels.Color(75, 15, 0)); // Orange
                    ledAnimationStep = 1;
                } else {
                    pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Off
                    ledAnimationStep = 0;
                }
                break;
            case LED_CONNECTED_DISARMED:
                pixels.setPixelColor(0, pixels.Color(0, 75, 0)); // Green
                break;
            case LED_ARMED_CONNECTED:
                pixels.setPixelColor(0, pixels.Color(75, 0, 75)); // Purple
                break;
            case LED_ARMED_DISCONNECTED:
                if (ledAnimationStep == 0) {
                    pixels.setPixelColor(0, pixels.Color(75, 0, 75)); // Purple
                    ledAnimationStep = 1;
                } else {
                    pixels.setPixelColor(0, pixels.Color(75, 15, 0)); // Orange
                    ledAnimationStep = 0;
                }
                break;
            case LED_WARNING_ALARM:
                if (ledAnimationStep == 0) {
                    pixels.setPixelColor(0, pixels.Color(255, 255, 255)); // Bright white
                    ledAnimationStep = 1;
                } else {
                    pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Off
                    ledAnimationStep = 0;
                }
                break;
            case LED_FULL_ALARM:
                if (ledAnimationStep == 0) {
                    pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
                    ledAnimationStep = 1;
                } else {
                    pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
                    ledAnimationStep = 0;
                }
                break;
        }
        pixels.show();
    }
}
