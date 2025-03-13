#ifndef LED_H
#define LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

enum LedState {
    LED_OFFLINE,
    LED_CONNECTED_DISARMED,
    LED_ARMED_CONNECTED,
    LED_ARMED_DISCONNECTED,
    LED_WARNING_ALARM,
    LED_FULL_ALARM
};

extern LedState currentLedState;
extern LedState previousLedState;
extern unsigned long lastLedUpdateTime;
extern unsigned long ledUpdateInterval;
extern int ledAnimationStep;

void ledStatus(const char* status);
void flashRedBlueLed();
void updateLedStatus();
void setupLed();

#endif
