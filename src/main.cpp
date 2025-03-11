#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "secrets.h"
#include "settings.h"

// --- Configuration ---
const int baudRate = 115200;
const int port = 80;
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// --- Pins ---
const int pirPin = 10;
const int alarmPin = 8;
const int lightRelayPin = 6;
Adafruit_NeoPixel pixels(1, 7, NEO_GRB + NEO_KHZ800);

// --- Pushover ---
const char* pushoverUserKey = PUSHOVER_USER_KEY;
const char* pushoverApiToken = PUSHOVER_API_TOKEN;

// --- Constants ---
const bool notificationsEnabled = Settings::pushNotificationsEnabled;
const int motionCountThreshold = 3;
const unsigned long motionCountWindow = 30000;
const unsigned long continuousMotionThreshold = 10000;
const unsigned long warningDuration = 800;     // Short warning duration
const unsigned long alarmDurationDefault = 60000; //  default alarm duration

// --- LED State Management ---
enum LedState {
  LED_OFFLINE,              // Flashing orange
  LED_CONNECTED_DISARMED,   // Solid green
  LED_ARMED_CONNECTED,      // Solid purple
  LED_ARMED_DISCONNECTED,   // Alternate purple/orange
  LED_WARNING_ALARM,        // Flashing bright white
  LED_FULL_ALARM            // Alternate red and blue
};

// LED state variables
LedState currentLedState = LED_OFFLINE;
LedState previousLedState = LED_OFFLINE;
unsigned long lastLedUpdateTime = 0;
unsigned long ledUpdateInterval = 500; // Default interval for LED animations
int ledAnimationStep = 0;
bool wifiConnected = false;

// --- Global Variables ---

// Alarm related variables
unsigned long alarmDuration = alarmDurationDefault;
unsigned long alarmStartTime = 0; 
bool alarmActive = false;
bool extendedWarningActive = false;

// System state variables
bool systemArmed = false;
bool motionDetected = false;
bool lightsCurrentlyOn = false;

// Settings related variables
bool soundActivated = Settings::soundEnabled;
bool lightsActivated = Settings::lightsEnabled;

// Motion detection variables
unsigned long lastMotionTime = 0;
unsigned long motionStartTime = 0; // For continuous motion
int motionCount = 0;

// Notification flag
bool notificationSent = false; 

// --- Web Server ---
WebServer server(port);

// --- Function Prototypes ---
void connectToWiFi();
void handleRoot();
void handleAlarmOn();
void handleStopAlarm();
void handleArmDisarm(bool arm);
void handleToggleLights();
void handleToggleSound();
void handleNotFound();
void getStatus();

void activateAlarm(unsigned long duration);
void deactivateAlarm();
void playWarning();
void playAlarm();
void stopAlarm();
void setLights(bool on);
void sendNotification(const char* message);
void logEvent(const char* message, bool force = false);

void ledStatus(const char* status);
void flashRedBlueLed();
void updateLedStatus(); // function to update LED based on state

void setup() {
    Serial.begin(baudRate);
    pinMode(pirPin, INPUT);
    pinMode(alarmPin, OUTPUT);
    pinMode(lightRelayPin, OUTPUT);
    digitalWrite(alarmPin, LOW);  // Ensure alarm is off
    digitalWrite(lightRelayPin, LOW); // Ensure lights are off

    pixels.begin();
    pixels.setPixelColor(0, pixels.Color(255, 165, 0)); // Orange for offline
    pixels.show();

    connectToWiFi();

    // --- Web Server Routes ---
    server.on("/", handleRoot);
    server.on("/alarmOn", handleAlarmOn);
    server.on("/stopalarm", handleStopAlarm);
    server.on("/arm", []() { handleArmDisarm(true); });
    server.on("/disarm", []() { handleArmDisarm(false); });
    server.on("/toggleLights", handleToggleLights);
    server.on("/toggleSound", handleToggleSound);    
    server.on("/status", getStatus);
    server.onNotFound(handleNotFound);
    server.begin();
    logEvent("HTTP server started");
}

void loop() {
    server.handleClient();
    updateLedStatus(); // Update LED based on current state
    unsigned long currentTime = millis();

    // --- Motion Detection Logic ---
    if (systemArmed) {
        int pirValue = digitalRead(pirPin);

        if (pirValue == HIGH) {
            if (!motionDetected) {
                logEvent("Motion detected!");
                motionDetected = true;
                lastMotionTime = currentTime;
                motionStartTime = currentTime; // For continuous motion
                motionCount++;

                if (!alarmActive && !extendedWarningActive) { // Only play warning if no full alarm
                  if (motionCount >= motionCountThreshold)
                  {
                    logEvent("Motion count threshold exceeded. Activating extended warning.");
                    extendedWarningActive = true;
                    if (notificationsEnabled) {
                        sendNotification((String(DEVICE_NAME) + " Multiple motion events! Extended warning activated!").c_str());
                    }
                    playAlarm();
                    // flashRedBlueLed();
                    currentLedState = LED_WARNING_ALARM;
                    

                    // DO NOT reset motion count here. The count should
                    // reset only after the motionCountWindow.
                    // If you reset it here, rapid succession motion will
                    // retrigger this every time, instead of after 3 times.

                  } else {
                    logEvent("Playing warning alarm.");
                    playWarning();
                  }
                }
            } else {
                // Continuous motion
                if (currentTime - motionStartTime >= continuousMotionThreshold) {
                    if (!alarmActive && !extendedWarningActive) {
                        logEvent("Continuous motion detected. Activating FULL ALARM!");
                        activateAlarm(alarmDuration);
                        flashRedBlueLed();
                        if (notificationsEnabled) {
                            sendNotification((String(DEVICE_NAME) + " Continuous motion detected! Alarm activated!").c_str());
                        }
                    }
                }
            }
        } else { // PIR is LOW
            if (motionDetected) {
              if(currentTime - lastMotionTime > motionCountWindow)
              {
                motionCount = 0;
                logEvent("Motion count window expired, resetting.", true);
              }

                motionDetected = false;
                logEvent("Motion stopped.");
            }
        }
    }

    // --- Alarm Duration Handling ---
    if (alarmActive && (millis() - alarmStartTime >= alarmDuration)) {
        deactivateAlarm();
        logEvent("Alarm duration expired, deactivating alarm.");
        // ledStatus("ready");
        // currentLedState = systemArmed ? LED_ARMED_CONNECTED : LED_CONNECTED_DISARMED;
    }

    // --- Extended warning
    if(extendedWarningActive && (currentTime - (lastMotionTime + continuousMotionThreshold) >= 0))
    {
      logEvent("Extended warning stopped", true);
      stopAlarm();
    //   ledStatus("ready");
        currentLedState = systemArmed ? LED_ARMED_CONNECTED : LED_CONNECTED_DISARMED;
      extendedWarningActive = false;
    }
}

// --- Helper Functions ---

void connectToWiFi() {
    // Set initial LED state as offline (flashing orange handled by updateLedStatus)
    currentLedState = LED_OFFLINE;
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        // We update LED state manually here during connection process
        updateLedStatus();
    }
    
    Serial.println();
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Update LED state based on armed status now that WiFi is connected
    wifiConnected = true;
    currentLedState = systemArmed ? LED_ARMED_CONNECTED : LED_CONNECTED_DISARMED;
    ledAnimationStep = 0;
}

void logEvent(const char* message, bool force) {
    static unsigned long lastLogTime = 0;
    unsigned long now = millis();

    // Log immediately if forced, or if enough time has passed since the last log
    if (force || (now - lastLogTime >= 1000)) { // Log every second max, unless forced
        Serial.println(message);
        lastLogTime = now;
    }
}

void activateAlarm(unsigned long duration) {
  if(!alarmActive) { // only set start time if the alarm was not already active
    alarmActive = true;
    alarmStartTime = millis();  // set alarm activation time
    logEvent("Alarm Activated!", true);
    
    // Save previous LED state and set to full alarm
    previousLedState = currentLedState;
    currentLedState = LED_FULL_ALARM;
    ledAnimationStep = 0;
    
    if (soundActivated) {
        playAlarm();
    }
    if (lightsActivated) {
        setLights(true);
    }
    if (notificationsEnabled) {
        sendNotification((String(DEVICE_NAME) + " Alarm Activated!").c_str());
    }
  } else {
    logEvent("Alarm was already active, extending time.", true);
    // Do not update alarmStartTime here to preserve original activation time
  }
}

void deactivateAlarm() {
    alarmActive = false;
    stopAlarm();
    setLights(false);
    
    // Restore previous LED state
    currentLedState = systemArmed ? 
        (wifiConnected ? LED_ARMED_CONNECTED : LED_ARMED_DISCONNECTED) :
        (wifiConnected ? LED_CONNECTED_DISARMED : LED_OFFLINE);
    ledAnimationStep = 0;
    
    logEvent("Alarm Deactivated!", true);
}

void playWarning() {
    if (soundActivated) {
      digitalWrite(alarmPin, HIGH);
      delay(warningDuration);
      digitalWrite(alarmPin, LOW);
    }
}

void playAlarm() {
    //  alarm controlled by setting pin HIGH
    digitalWrite(alarmPin, HIGH);
}

void stopAlarm() {
    //  alarm controlled by setting pin LOW
    digitalWrite(alarmPin, LOW);
}

void setLights(bool on) {
    digitalWrite(lightRelayPin, on ? HIGH : LOW);
    lightsCurrentlyOn = on;
    logEvent(on ? "Lights ON" : "Lights OFF", true);
}

void sendNotification(const char* message) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();
        http.begin(client, "https://api.pushover.net/1/messages.json");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String postData = "token=" + String(pushoverApiToken) + "&user=" + String(pushoverUserKey) + "&message=" + String(message);
        int httpResponseCode = http.POST(postData);
        if (httpResponseCode > 0) {
            logEvent(("Pushover Response: " + http.getString()).c_str());
        } else {
            logEvent("Error sending Pushover notification");
        }
        http.end();
    } else {
        logEvent("WiFi not connected. Cannot send notification.");
    }
}

// --- Web Handlers ---

void handleRoot() {
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

        if(motionDetected)
        {
          html += R"(
          <br/>
          Motion Detected: <strong>Yes</strong>
          )";
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
    server.send(200, "text/html", html);
    // logEvent("Root page requested");
}

void handleAlarmOn() {
    activateAlarm(alarmDuration);
    server.send(200, "application/json", R"({"status":"success","message":"Alarm Activated"})");
}

void handleStopAlarm() {
    if (alarmActive || extendedWarningActive) {
        deactivateAlarm();
        extendedWarningActive = false;
        server.send(200, "application/json", R"({"status":"success","message":"Alarm Stopped"})");
    } else {
        server.send(200, "application/json", R"({"status":"info","message":"No Alarm is Active"})");
    }
}

void handleArmDisarm(bool arm) {
    systemArmed = arm;
    currentLedState = arm ? (wifiConnected ? LED_ARMED_CONNECTED : LED_ARMED_DISCONNECTED) : (wifiConnected ? LED_CONNECTED_DISARMED : LED_OFFLINE);
    logEvent(arm ? "System Armed" : "System Disarmed");
    server.send(200, "application/json", arm ? R"({"status":"success","message":"System Armed"})" : R"({"status":"success","message":"System Disarmed"})");
}

void handleToggleLights() {
    lightsActivated = !lightsActivated;
    logEvent(lightsActivated ? "Lights Enabled" : "Lights Disabled");
     if (!lightsActivated && lightsCurrentlyOn) {
        setLights(false); // Turn off lights if they are currently on and lights are disabled.
    }
    server.send(200, "application/json", lightsActivated ? R"({"status":"success","message":"Lights Enabled"})" : R"({"status":"success","message":"Lights Disabled"})");
}

void handleToggleSound()
{
  soundActivated = !soundActivated;
  logEvent(soundActivated ? "Sound Enabled" : "Sound Disabled", true);
  server.send(200, "application/json", soundActivated ? R"({"status":"success", "message":"Sound Enabled"})" : R"({"status":"success", "message":"Sound Disabled"})");
}

void handleNotFound() {
    server.send(404, "application/json", R"({"status":"error","message":"404: Not Found"})");
    logEvent("404 Not Found");
}

void getStatus() {
    JsonDocument doc;

    doc["system"]["armed"] = systemArmed;
    doc["system"]["alarm"]["active"] = alarmActive;
    doc["system"]["alarm"]["time_left"] = alarmActive ? (alarmDuration - (millis() - alarmStartTime)) : 0;
    doc["system"]["lights"]["activated"] = lightsActivated;
    doc["system"]["lights"]["currently_on"] = lightsCurrentlyOn;

    doc["motion"]["detected"] = motionDetected;
    doc["motion"]["count"] = motionCount;

    doc["warning"]["active"] = extendedWarningActive;

    doc["settings"]["lights"] = lightsActivated;
    doc["settings"]["sound"] = soundActivated;
    doc["settings"]["notifications"] = notificationsEnabled;
    doc["settings"]["alarm"]["duration"] = alarmDuration;
    doc["settings"]["alarm"]["default_duration"] = alarmDurationDefault;
    doc["settings"]["motion"]["continuous_threshold"] = continuousMotionThreshold;
    doc["settings"]["motion"]["count_threshold"] = motionCountThreshold;
    doc["settings"]["motion"]["count_window"] = motionCountWindow;
    doc["settings"]["warning"]["duration"] = warningDuration;
    doc["settings"]["device_name"] = DEVICE_NAME;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
    logEvent("Status requested");
}

// Updated LED status function to work with state machine
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
        return; // Direct control for "off" state
    } else {
        pixels.setPixelColor(0, pixels.Color(75, 75, 75)); // White for unknown status
        pixels.show();
        return; // Direct control for unknown state
    }
    
    ledAnimationStep = 0; 
}

// Replace blocking flashRedBlueLed with non-blocking updateLedStatus
void flashRedBlueLed() {
    // This function remains for compatibility but now just ensures we're in FULL_ALARM state
    previousLedState = currentLedState;
    currentLedState = LED_FULL_ALARM;
    ledAnimationStep = 0;
}

// New function to handle LED state updates non-blockingly
void updateLedStatus() {
    unsigned long currentTime = millis();
    
    // Check WiFi connection status
    bool newWifiStatus = (WiFi.status() == WL_CONNECTED);
    if (wifiConnected != newWifiStatus) {
        wifiConnected = newWifiStatus;
        
        // Update LED state based on new WiFi status
        if (!alarmActive && !extendedWarningActive) {
            currentLedState = systemArmed ? 
                (wifiConnected ? LED_ARMED_CONNECTED : LED_ARMED_DISCONNECTED) :
                (wifiConnected ? LED_CONNECTED_DISARMED : LED_OFFLINE);
            ledAnimationStep = 0;
        }
    }
    
    // Update LED state based on alarm conditions
    if (alarmActive && currentLedState != LED_FULL_ALARM) {
        previousLedState = currentLedState;
        currentLedState = LED_FULL_ALARM;
        ledAnimationStep = 0;
    } else if (extendedWarningActive && !alarmActive && currentLedState != LED_WARNING_ALARM) {
        previousLedState = currentLedState;
        currentLedState = LED_WARNING_ALARM;
        ledAnimationStep = 0;
    }
    
    // Handle LED animation based on current state
    if (currentTime - lastLedUpdateTime >= ledUpdateInterval) {
        lastLedUpdateTime = currentTime;
        
        switch (currentLedState) {
            case LED_OFFLINE:
                // Flashing orange
                if (ledAnimationStep == 0) {
                    pixels.setPixelColor(0, pixels.Color(255, 165, 0)); // Orange
                    ledAnimationStep = 1;
                } else {
                    pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Off
                    ledAnimationStep = 0;
                }
                break;
                
            case LED_CONNECTED_DISARMED:
                // Solid green
                pixels.setPixelColor(0, pixels.Color(0, 75, 0)); // Green
                break;
                
            case LED_ARMED_CONNECTED:
                // Solid purple
                pixels.setPixelColor(0, pixels.Color(75, 0, 75)); // Purple
                break;
                
            case LED_ARMED_DISCONNECTED:
                // Alternate purple/orange
                if (ledAnimationStep == 0) {
                    pixels.setPixelColor(0, pixels.Color(75, 0, 75)); // Purple
                    ledAnimationStep = 1;
                } else {
                    pixels.setPixelColor(0, pixels.Color(255, 165, 0)); // Orange
                    ledAnimationStep = 0;
                }
                break;
                
            case LED_WARNING_ALARM:
                // Flashing bright white
                if (ledAnimationStep == 0) {
                    pixels.setPixelColor(0, pixels.Color(255, 255, 255)); // Bright white
                    ledAnimationStep = 1;
                } else {
                    pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Off
                    ledAnimationStep = 0;
                }
                break;
                
            case LED_FULL_ALARM:
                // Alternate red and blue
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