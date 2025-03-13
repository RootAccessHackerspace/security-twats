#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "secrets.h"
#include "settings.h"

using namespace Settings;
#include "utils/led.h"
#include "utils/log.h"
#include "utils/pushover.h"
#include "modules/lightsRelay.h"
#include "modules/web.h"

// --- Configuration ---
const int baudRate = 115200;
const int port = 80;
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// --- Pins ---
const int pirPin = 10;
const int alarmPin = 8;
LightsRelay lights(6); // Using pin 6
Adafruit_NeoPixel pixels(1, 7, NEO_GRB + NEO_KHZ800);

// --- Pushover ---
const char* pushoverUserKey = PUSHOVER_USER_KEY;
const char* pushoverApiToken = PUSHOVER_API_TOKEN;
// Removed duplicate definition of notificationsEnabled
// const bool notificationsEnabled = pushNotificationsEnabled;

// --- Constants ---
const bool notificationsEnabled = Settings::pushNotificationsEnabled;
int motionCountThreshold = 3;
unsigned long motionCountWindow = 15000;
unsigned long continuousMotionThreshold = 10000;
unsigned long warningDuration = 800;     // Short warning duration
unsigned long alarmDurationDefault = 60000; //  default alarm duration

// --- Global Variables ---

// Alarm related variables
unsigned long alarmDuration = alarmDurationDefault;
unsigned long alarmStartTime = 0;
unsigned long warningStartTime = 0;
bool warningInProgress = false;
bool alarmActive = false;
bool extendedWarningActive = false;

// System state variables
bool systemArmed = false;
bool motionDetected = false;
bool wifiConnected = false;

// Settings related variables
bool soundActivated = Settings::soundEnabled;

// Motion detection variables
unsigned long lastMotionTime = 0;
unsigned long motionCountStartTime = 0;
unsigned long motionStartTime = 0; // For continuous motion
int motionCount = 0;

// Notification flag
bool notificationSent = false; 

// --- Web Server ---
WebServer server(port);

// --- Function Prototypes ---
void connectToWiFi();
void activateAlarm(unsigned long duration);
void deactivateAlarm();
void playWarning();
void playAlarm();
void stopAlarm();
void setLights(bool on);
void sendNotification(const char* message);

// Make LED state accessible to web handlers
LedState currentLedState = LED_OFFLINE;

void setup() {
    Serial.begin(baudRate);
    pinMode(pirPin, INPUT);
    pinMode(alarmPin, OUTPUT);
    digitalWrite(alarmPin, LOW);  // Ensure alarm is off

    // Initialize LED
    setupLed();

    Pushover::initialize(pushoverUserKey, pushoverApiToken);
    connectToWiFi();

    lights.begin();
    lights.setEnabled(Settings::lightsEnabled);

    // --- Web Server Routes ---
    WebHandlers::initialize(&server, &lights);
    server.begin();
    logEvent("HTTP server started");
}

void loop() {
    server.handleClient();
    updateLedStatus(); // now provided by led.cpp
    unsigned long currentTime = millis();

    // --- Motion Detection Logic ---
    if (systemArmed) {

        if(motionCount > 0 && currentTime - motionCountStartTime > motionCountWindow) {
            motionCount = 0;
            logEvent("Motion count window expired, resetting.", true);
        }
        int pirValue = digitalRead(pirPin);

        if (pirValue == HIGH) {
            if (!motionDetected) {
                logEvent("Motion detected!");
                motionDetected = true;
                lastMotionTime = currentTime;
                motionStartTime = currentTime; // For continuous motion
                
                if (motionCount == 0) {
                    // First motion in a potential sequence
                    motionCountStartTime = currentTime;
                }
                motionCount++;

                if (!alarmActive && !extendedWarningActive) { // Only play warning if no full alarm
                  if (motionCount >= motionCountThreshold){
                    logEvent("Motion count threshold exceeded. Activating extended warning.");
                    if (notificationsEnabled) {
                        sendNotification((String(DEVICE_NAME) + " Multiple motion events! Extended warning activated!").c_str());
                    }
                    playAlarm();
                    // flashRedBlueLed();
                    currentLedState = LED_WARNING_ALARM;
                    extendedWarningActive = true;
                    

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
                // if(currentTime - motionCountStartTime > motionCountWindow) {
                //     motionCount = 0;
                //     logEvent("Motion count window expired, resetting.", true);
                // }
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

    // --- Warning Duration Handling ---
    if (warningInProgress && (millis() - warningStartTime >= warningDuration)) {
        digitalWrite(alarmPin, LOW);
        warningInProgress = false;
    }

    // --- Extended warning
    if(extendedWarningActive && (currentTime >= lastMotionTime + continuousMotionThreshold)) {
      logEvent("Extended warning stopped", true);
      stopAlarm();
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
    if (lights.isEnabled()) {
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
        Serial.println("Playing warning alarm for " + String(warningDuration) + " ms");
        warningInProgress = true;
        warningStartTime = millis();
        digitalWrite(alarmPin, HIGH);
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
    lights.setState(on);
}

void sendNotification(const char* message) {
    if (notificationsEnabled) {
        Pushover::sendNotification(message);
    }
}