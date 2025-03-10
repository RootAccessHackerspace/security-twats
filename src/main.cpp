#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <Adafruit_NeoPixel.h>

#include "secrets.h" // Include your secrets.h file for secret variables
#include "settings.h"  // Added settings include

// --- Configuration ---
const int baudRate = 115200; // Serial baud rate
const int port = 80; // Web server port
const char* ssid = WIFI_SSID;  // Use WiFi SSID from secrets.h
const char* password = WIFI_PASSWORD; // Use WiFi password from secrets.h

const int pirPin = 10;       // PIR sensor connected to GPIO 10
const int piezoPin = 8;      // Piezo buzzer connected to GPIO 8
const int lightRelayPin = 6;  // Relay for lights connected to GPIO 7

Adafruit_NeoPixel pixels(1, 7, NEO_GRB + NEO_KHZ800); // LED pin for status indication

// --- Pushover Configuration ---
const char* pushoverUserKey = PUSHOVER_USER_KEY; // Use Pushover User Key from secrets.h
const char* pushoverApiToken = PUSHOVER_API_TOKEN; // Use Pushover API Token from secrets.h

// --- Constants ---
constexpr bool notificationsEnabled = Settings::pushNotificationsEnabled;
const int motionCountThreshold = 3;           // Number of motion events to trigger extended warning
const unsigned long motionCountWindow = 30000; // Time window for counting (30 seconds)
const unsigned long continuousMotionThreshold = 10000; // Time threshold for continuous motion detection
const unsigned long extendedWarningDuration = 60000; // Extended warning duration (60 seconds)

// --- Global Variables ---
unsigned long alarmDuration = 60000; // Alarm duration in milliseconds (60 seconds)
unsigned long motionDuration = 10000; // Max duration for motion detection before alert
unsigned long alarmStartTime = 0;  // Timestamp of when the full alarm started
unsigned long motionCounter = 0; // Counter for motion detection
unsigned long motionStartTime = 0; // Timestamp of when motion was detected
bool systemArmed = false;    // Flag to indicate if the system is armed
bool alarmActive = false;     // Flag to indicate if the full alarm is active
bool motionDetected = false; // Flag to prevent multiple short alarms.
bool soundActivated = Settings::soundEnabled; // Flag to indicate if sound is activated
bool lightsActivated = Settings::lightsEnabled; // Flag to indicate if lights are activated
bool lightsCurrentlyOn = false; // Flag to track if lights are currently on

// --- Motion Detection Counter Variables ---
int motionCount = 0;                     // Counter for motion events in the window
unsigned long lastMotionTime = 0;        // Time of the last detected motion
bool extendedWarningActive = false;      // Flag for extended warning mode
unsigned long extendedWarningStartTime = 0;   // When extended warning started

bool notificationSent = false; // Flag to indicate if the notification has been sent

// --- Web Server ---
WebServer server(port);

// --- Function Prototypes ---
void connectToWiFi();
void handleAlarmOn();
void handleArm();
void handleDisarm();
void handleEnableLights();
void handleEnableSound();
void handleNotFound();
void handleRoot();
void handleStopAlarm();
void playAlarmSound();
void playExtendedWarning();
void playWarningTone(unsigned int dur = 800);
void sendPushoverNotification(const char* message);
void stopAlarmSound();
void getStatus();
void setLights(bool on); // Simplified function to control lights
void toggleLightsEnabled(); // Function to toggle whether lights are enabled

void setup() {
  Serial.begin(baudRate);

  pixels.begin(); // Initialize the LED
  pixels.setPixelColor(0, pixels.Color(0, 75, 0));  // Green
  pixels.show();

  pinMode(pirPin, INPUT);
  pinMode(piezoPin, OUTPUT);
  pinMode(lightRelayPin, OUTPUT);
  digitalWrite(piezoPin, LOW); // Ensure piezo is off initially
  digitalWrite(lightRelayPin, LOW); // Ensure lights are off initially
  Serial.println("Serial Monitor Initialized"); // Add a test message

  connectToWiFi();

  // --- Web Server Route Setup ---
  server.on("/", handleRoot);
  server.on("/alarmOn", handleAlarmOn);
  server.on("/enableLights", handleEnableLights);
  server.on("/enableSound", handleEnableSound);
  server.on("/arm", handleArm);
  server.on("/disarm", handleDisarm);
  server.on("/stopalarm", handleStopAlarm);
  server.on("/status", getStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

}

void loop() {
  server.handleClient();
  unsigned long currentTime = millis();

  // --- Reset motion counter if window expires ---
  if (motionCount > 0 && currentTime - lastMotionTime > motionCountWindow) {
    Serial.println("Motion count window expired, resetting counter");
    motionCount = 0;
  }

  // --- Extended Warning Logic ---
  if (extendedWarningActive) {
    if (currentTime - extendedWarningStartTime < extendedWarningDuration) {
      playExtendedWarning();
    } else {
      digitalWrite(piezoPin, LOW);
      extendedWarningActive = false;
      Serial.println("Extended warning stopped");
    }
  }

  // --- Motion Detection ---
  if (systemArmed && !alarmActive && !extendedWarningActive) {
    int pirValue = digitalRead(pirPin);
    
    if (pirValue == HIGH) {
      if (!motionDetected) {
        // Initial motion detected
        Serial.println("Motion detected!");
        motionDetected = true;
        lastMotionTime = currentTime;
        motionStartTime = currentTime; // Record when continuous motion started
        motionCount++;
        
        Serial.print("Motion count: ");
        Serial.print(motionCount);
        Serial.print(" of ");
        Serial.println(motionCountThreshold);
        
        if (motionCount >= motionCountThreshold) {
          // Trigger extended warning due to count threshold
          if (!notificationSent && notificationsEnabled) {
            sendPushoverNotification((String(DEVICE_NAME) + " Multiple motion events! Extended warning activated!").c_str());
          }
          extendedWarningActive = true;
          extendedWarningStartTime = currentTime;
          Serial.println((String(DEVICE_NAME) + " Multiple motions detected! Extended warning activated!").c_str());
          motionCount = 0; // Reset after triggering
        } else {
          // Play regular short warning tone
          playWarningTone();
        }
      } else {
        // Continuous motion detection logic
        unsigned long motionDuration = currentTime - motionStartTime;
        if (motionDuration >= continuousMotionThreshold) {
          // Trigger extended warning due to continuous motion
          if (!extendedWarningActive) {
            if (!notificationSent && notificationsEnabled) {
              sendPushoverNotification((String(DEVICE_NAME) + " Continuous motion detected! Extended warning activated!").c_str());
              notificationSent = true; // Set flag to true after sending notification
            }
            extendedWarningActive = true;
            extendedWarningStartTime = currentTime;
            Serial.println("Continuous motion detected for 10+ seconds! Extended warning activated!");
            motionCount = 0; // Reset motion count
          }
        }
      }
    } else if (pirValue == LOW && motionDetected) {
      // Motion stopped
      Serial.println("Ready...");
      notificationSent = false; // Reset notification flag
      motionDetected = false; // Reset flag when motion stops
    }
  }

  // --- Full Alarm Logic ---
  if (alarmActive) {
    if (currentTime - alarmStartTime < alarmDuration) {
      if(soundActivated) playAlarmSound();
      
      // Only turn on lights once at the beginning of the alarm
      if(lightsActivated && !lightsCurrentlyOn) {
        setLights(true);
        Serial.println("Lights Activated for Alarm!");
      }
      
      if (!notificationSent && notificationsEnabled) {
        sendPushoverNotification("Alarm is sounding!");
        notificationSent = true; // Set flag to true after sending notification
      }
    } else {
      if(lightsCurrentlyOn) {
        setLights(false);
      }
      stopAlarmSound();
      alarmActive = false; // Deactivate the alarm after the duration
      notificationSent = false; // Reset notification flag
    }
  }
}

void connectToWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}


// --- Web Server Handlers ---

void handleRoot() {
  String html = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Trespass Warning and Access Threat System</title>
    </head>
    <body>
      <h3>Trespass Warning and </br>Access Threat System</h3>
      <p>System Status: )";
      html += systemArmed ? "Armed" : "Disarmed";
      html += R"(<p>Motion Counter: )";
      html += String(motionCount) + " of " + String(motionCountThreshold);
      
      // Add continuous motion status
      if (motionDetected) {
        unsigned long continuousDuration = (millis() - motionStartTime);
        html += R"(</p><p>Continuous Motion: )";
        html += String(continuousDuration / 1000) + " seconds";
      }
      
      html += R"(</p>
      <p><a href="/alarmOn">Activate Full Alarm</a></p>
      <p><a href="/stopalarm">Stop Alarm</a></p>
      <p><a href="/)";
      html += systemArmed ? "disarm\">Disarm System</a></p>" : "arm\">Arm System</a></p>";
      html += R"(
    </body>
    </html>
  )";
  server.send(200, "text/html", html);
}

void handleAlarmOn() {
  unsigned long duration = alarmDuration; // Default duration

  if (server.hasArg("dur")) {
    String durStr = server.arg("dur");
    unsigned long durVal = durStr.substring(0, durStr.length() - 1).toInt();
    char unit = durStr.charAt(durStr.length() - 1);

    switch (unit) {
      case 's':
        duration = durVal * 1000;      // Convert seconds to milliseconds
        break;
      case 'm':
        duration = durVal * 60 * 1000; // Convert minutes to milliseconds
        break;
      default:
        Serial.println("Invalid duration unit. Using default.");
        break;
    }
    alarmDuration = duration;
    Serial.print("Duration set to: ");
    Serial.print(duration);
    Serial.println(" ms");
  } else if (alarmActive) {
    Serial.println("Alarm was already active, resetting timer.");
    alarmStartTime = millis();
  } else {
    Serial.println("No duration specified, using default. ");
  }
  
  alarmActive = true;
  alarmStartTime = millis();
  Serial.print("Alarm Sounding for ");
  Serial.print(duration);
  Serial.println(" ms");
  // Return JSON instead of HTML
  String jsonResp = String("{\"status\":\"success\",\"message\":\"Alarm Activated!\",\"duration\":") + duration + "}";
  server.send(200, "application/json", jsonResp);
}

void handleEnableLights() {
  toggleLightsEnabled();
  
  // Return JSON response
  String jsonResp = String("{\"status\":\"success\",\"message\":\"Lights ") + 
                   (lightsActivated ? "Activated!" : "Deactivated!") + 
                   "\",\"lightsActivated\":" + 
                   (lightsActivated ? "true" : "false") + "}";
  server.send(200, "application/json", jsonResp);
}

void handleEnableSound() {
  soundActivated = !soundActivated;
  // Return JSON response
  if (soundActivated) {
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Sound Activated!\",\"soundActivated\":true}");
  } else {
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Sound Deactivated!\",\"soundActivated\":false}");
  }
}

void handleNotFound() {
  server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"404: Not Found\"}");
}

// --- Alarm Sound Functions ---

void playWarningTone(unsigned int dur) {
  // Play a short warning tone sequence
  digitalWrite(piezoPin, HIGH);
  delay(dur);
  digitalWrite(piezoPin, LOW);
}


void playAlarmSound() {
  //  loud, continuous alarm tone
  digitalWrite(piezoPin, HIGH);
  Serial.print(".");
  delay(1000);
}

void stopAlarmSound() {
  digitalWrite(piezoPin, LOW);
  Serial.println();
  Serial.println("Alarm Stopped!");
}

void setLights(bool on) {
  digitalWrite(lightRelayPin, on ? HIGH : LOW);
  lightsCurrentlyOn = on;
  Serial.println(on ? "Lights turned ON" : "Lights turned OFF");
}

void toggleLightsEnabled() {
  lightsActivated = !lightsActivated;
  Serial.println(lightsActivated ? "Lights feature enabled" : "Lights feature disabled");
  
  // If lights are disabled, make sure they're turned off
  if (!lightsActivated && lightsCurrentlyOn) {
    setLights(false);
  }
}

// --- Arm/Disarm Handlers ---
void handleArm() {
  systemArmed = true;
  Serial.println("System Armed");
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"System Armed!\"}");
}

void handleDisarm() {
  systemArmed = false;
  Serial.println("System Disarmed");
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"System Disarmed!\"}");
}

// --- Extended Warning Function ---
void playExtendedWarning() {
  digitalWrite(piezoPin, HIGH);
}

void sendPushoverNotification(const char* message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // Skip SSL certificate verification
    http.begin(client, "https://api.pushover.net/1/messages.json");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "token=" + String(pushoverApiToken) + "&user=" + String(pushoverUserKey) + "&message=" + String(message);

    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Pushover Response: " + response);
    } else {
      Serial.println("Error sending Pushover notification");
    }

    http.end();
  } else {
    Serial.println("WiFi not connected. Cannot send Pushover notification.");
  }
}

void handleStopAlarm() {
  // Check if an alarm or extended warning is currently active
  if (alarmActive || extendedWarningActive) {
    alarmActive = false;
    extendedWarningActive = false;
    stopAlarmSound();
    if (lightsCurrentlyOn) {
      setLights(false);
    }
    notificationSent = false; // Reset notification flag
    Serial.println("Alarm manually stopped!");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Alarm Stopped!\"}");
  } else {
    server.send(200, "application/json", "{\"status\":\"info\",\"message\":\"No Alarm is Active.\"}");
  }
}
  void getStatus() {
    JsonDocument doc;

    // System State
    JsonObject system = doc["system"].to<JsonObject>();
    system["armed"] = systemArmed;
    system["alarm_active"] = alarmActive;
    
    // Motion Information
    JsonObject motion = doc["motion"].to<JsonObject>();
    motion["count"] = motionCount;
    motion["detected"] = motionDetected;
    motion["continuous"] = motionDetected && (millis() - motionStartTime >= continuousMotionThreshold);
    motion["duration"] = (motionDetected) ? (millis() - motionStartTime) / 1000 : 0;
    
    // Alarm Settings
    JsonObject settings = doc["settings"].to<JsonObject>();
    settings["lights"] = lightsActivated;
    settings["sound"] = soundActivated;
    
    // Warning Status
    JsonObject warning = doc["warning"].to<JsonObject>();
    warning["active"] = extendedWarningActive;
    warning["duration"] = extendedWarningActive ? (millis() - extendedWarningStartTime) / 1000 : 0;
    warning["notification_sent"] = notificationSent;
    
    // Alarm Timing
    JsonObject timing = doc["timing"].to<JsonObject>();
    timing["duration"] = alarmActive ? (millis() - alarmStartTime) / 1000 : 0;
    timing["start_time"] = alarmStartTime;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
    Serial.println("Status requested");
  }
