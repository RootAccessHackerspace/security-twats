#include "web.h"
// #include "../settings.h"
#include "../secrets.h"  // Add secrets.h for DEVICE_NAME

// Static member initialization
WebServer* WebHandlers::_server = nullptr;
LightsRelay* WebHandlers::_lights = nullptr;

// External variables we need access to (declare as extern)
extern bool systemArmed;
extern bool alarmActive;
extern bool extendedWarningActive;
extern bool motionDetected;
extern int motionCount;
extern bool soundActivated;
extern unsigned long alarmDuration;
extern unsigned long alarmStartTime;
extern unsigned long alarmDurationDefault;
extern unsigned long continuousMotionThreshold;
extern int motionCountThreshold;
extern unsigned long motionCountWindow;
extern unsigned long warningDuration;
extern bool wifiConnected;
extern bool notificationsEnabled;
extern LedState currentLedState;

void WebHandlers::initialize(WebServer* server, LightsRelay* lightsRef) {
    _server = server;
    _lights = lightsRef;
    
    _server->on("/", handleRoot);
    _server->on("/alarmOn", handleAlarmOn);
    _server->on("/stopalarm", handleStopAlarm);
    _server->on("/arm", []() { handleArmDisarm(true); });
    _server->on("/disarm", []() { handleArmDisarm(false); });
    _server->on("/toggleLights", handleToggleLights);
    _server->on("/toggleSound", handleToggleSound);    
    _server->on("/status", getStatus);
    _server->onNotFound(handleNotFound);
}

void WebHandlers::handleRoot() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Trespass Warning and Access Threat System</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; }
        .button { padding: 10px 20px; margin: 5px; border: 1px solid #ccc; border-radius: 5px; text-decoration: none; color: #333; display: inline-block; }
        .button:hover { background-color: #eee; }
        .status { margin-top: 20px; }
    </style>
</head>
<body>
    <h3>)" + String(DEVICE_NAME) + R"( System Status</h3>
    <div class='status'>
        System Status: <strong>)" + String(systemArmed ? "Armed" : "Disarmed") + R"(</strong><br>
        Alarm Status: <strong>)" + String(alarmActive ? "Active" : "Inactive") + R"(</strong>
        )";

    if(motionDetected) {
        html += R"(<br/>Motion Detected: <strong>Yes</strong>)";
    }
        
    html += R"(
    </div>
    <a href="/alarmOn" class="button">Trigger Alarm</a><br/>
    <a href="/stopalarm" class="button">Stop Alarm</a><br/>
    <a href="/)" + String(systemArmed ? "disarm" : "arm") + R"(" class="button">)" + String(systemArmed ? "Disarm" : "Arm") + R"( System</a><br/>
    <a href="/status" class="button">Get Full Status</a>
</body>
</html>
)";
    _server->send(200, "text/html", html);
}

void WebHandlers::handleAlarmOn() {
    // Call external alarm activation function
    activateAlarm(alarmDuration);
    _server->send(200, "application/json", R"({"status":"success","message":"Alarm Activated"})");
}

void WebHandlers::handleStopAlarm() {
    if (alarmActive || extendedWarningActive) {
        deactivateAlarm();
        extendedWarningActive = false;
        _server->send(200, "application/json", R"({"status":"success","message":"Alarm Stopped"})");
    } else {
        _server->send(200, "application/json", R"({"status":"info","message":"No Alarm is Active"})");
    }
}

void WebHandlers::handleArmDisarm(bool arm) {
    systemArmed = arm;
    currentLedState = arm ? 
        (wifiConnected ? LED_ARMED_CONNECTED : LED_ARMED_DISCONNECTED) : 
        (wifiConnected ? LED_CONNECTED_DISARMED : LED_OFFLINE);
    logEvent(arm ? "System Armed" : "System Disarmed");
    _server->send(200, "application/json", 
        arm ? R"({"status":"success","message":"System Armed"})" : 
              R"({"status":"success","message":"System Disarmed"})");
}

void WebHandlers::handleToggleLights() {
    _lights->setEnabled(!_lights->isEnabled());
    _server->send(200, "application/json", _lights->isEnabled() ? 
        R"({"status":"success","message":"Lights Enabled"})" : 
        R"({"status":"success","message":"Lights Disabled"})");
}

void WebHandlers::handleToggleSound() {
    soundActivated = !soundActivated;
    logEvent(soundActivated ? "Sound Enabled" : "Sound Disabled", true);
    _server->send(200, "application/json", soundActivated ? 
        R"({"status":"success","message":"Sound Enabled"})" : 
        R"({"status":"success","message":"Sound Disabled"})");
}

void WebHandlers::handleNotFound() {
    _server->send(404, "application/json", R"({"status":"error","message":"404: Not Found"})");
    logEvent("404 Not Found");
}

void WebHandlers::getStatus() {
    JsonDocument doc;

    doc["system"]["armed"] = systemArmed;
    doc["system"]["alarm"]["active"] = alarmActive;
    doc["system"]["alarm"]["time_left"] = alarmActive ? (alarmDuration - (millis() - alarmStartTime)) : 0;
    doc["system"]["lights"]["activated"] = _lights->isEnabled();
    doc["system"]["lights"]["currently_on"] = _lights->isOn();

    doc["motion"]["detected"] = motionDetected;
    doc["motion"]["count"] = motionCount;

    doc["warning"]["active"] = extendedWarningActive;

    doc["settings"]["lights"] = _lights->isEnabled();
    doc["settings"]["sound"] = soundActivated;
    doc["settings"]["notifications"] = Settings::pushNotificationsEnabled;
    doc["settings"]["alarm"]["duration"] = alarmDuration;
    doc["settings"]["alarm"]["default_duration"] = alarmDurationDefault;
    doc["settings"]["motion"]["continuous_threshold"] = continuousMotionThreshold;
    doc["settings"]["motion"]["count_threshold"] = motionCountThreshold;
    doc["settings"]["motion"]["count_window"] = motionCountWindow;
    doc["settings"]["warning"]["duration"] = warningDuration;
    doc["settings"]["device_name"] = DEVICE_NAME;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    _server->send(200, "application/json", jsonResponse);
    logEvent("Status requested");
}
