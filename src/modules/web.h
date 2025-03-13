#pragma once
#include <WebServer.h>
#include <ArduinoJson.h>
#include "../utils/pushover.h"
#include "../utils/log.h"
#include "../utils/led.h"
#include "../modules/lightsRelay.h"
#include "../settings.h"
#include "../secrets.h"  // Add secrets.h for DEVICE_NAME

// Forward declarations of external functions
void activateAlarm(unsigned long duration);
void deactivateAlarm();

class WebHandlers {
public:
    static void initialize(WebServer* server, LightsRelay* lightsRef);
    static void handleRoot();
    static void handleAlarmOn();
    static void handleStopAlarm();
    static void handleArmDisarm(bool arm);
    static void handleToggleLights();
    static void handleToggleSound();
    static void handleNotFound();
    static void getStatus();

private:
    static WebServer* _server;
    static LightsRelay* _lights;
};
