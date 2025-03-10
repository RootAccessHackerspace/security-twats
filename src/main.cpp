#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

#include "secrets.h" // Include your secrets.h file for secret variables

// --- Configuration ---
const baudRate = 9600; // Serial baud rate
const int port = 80; // Web server port
const char* ssid = WIFI_SSID;  // Use WiFi SSID from secrets.h
const char* password = WIFI_PASSWORD; // Use WiFi password from secrets.h

const int pirPin = D7;       // PIR sensor connected to digital pin D1 (GPIO 5)
const int piezoPin = D2;     // Piezo buzzer connected to digital pin D2 (GPIO 4)
const int lightRelayPin = D1; // Relay for lights connected to digital pin D1 (GPIO 5) -- Change this as needed (JA)

// --- Pushover Configuration ---
const char* pushoverUserKey = PUSHOVER_USER_KEY; // Use Pushover User Key from secrets.h
const char* pushoverApiToken = PUSHOVER_API_TOKEN; // Use Pushover API Token from secrets.h

// --- Constants ---
const bool notificationsEnabled = true;
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
bool soundActivated = true; // Flag to indicate if sound is activated
bool lightsActivated = true; // Flag to indicate if lights are activated

// --- Motion Detection Counter Variables ---
int motionCount = 0;                     // Counter for motion events in the window
unsigned long lastMotionTime = 0;        // Time of the last detected motion
bool extendedWarningActive = false;      // Flag for extended warning mode
unsigned long extendedWarningStartTime = 0;   // When extended warning started

bool notificationSent = false; // Flag to indicate if the notification has been sent

// --- Web Server ---
ESP8266WebServer server(port);

// --- Function Prototypes ---
void handleRoot();
void handleAlarmOn();
void handleEnableLights();
void handleEnableSound();
void handleNotFound();
void playWarningTone(unsigned int dur = 800);
void playAlarmSound();
void stopAlarmSound();
void connectToWiFi();
void handleArm();
void handleDisarm();
void playExtendedWarning();
void sendPushoverNotification(const char* message);

void setup() {
  Serial.begin(baudRate);
  pinMode(pirPin, INPUT);
  pinMode(piezoPin, OUTPUT);
  pinMode(lightRelayPin, OUTPUT);
  digitalWrite(piezoPin, LOW); // Ensure piezo is off initially
  digitalWrite(lightRelayPin, LOW); // Ensure lights are off initially

  connectToWiFi();

  // --- Web Server Route Setup ---
  server.on("/", handleRoot);
  server.on("/alarmOn", handleAlarmOn);
  server.on("/enableLights", handleEnableLights);
  server.on("/enableSound", handleEnableSound);
  server.on("/arm", handleArm);
  server.on("/disarm", handleDisarm);
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
            sendPushoverNotification("Multiple motion events! Extended warning activated!");
          }
          extendedWarningActive = true;
          extendedWarningStartTime = currentTime;
          Serial.println("Multiple motions detected! Extended warning activated!");
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
              sendPushoverNotification("Continuous motion detected! Extended warning activated!");
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
      if(lightsActivated) activateLights();
      if (!notificationSent && notificationsEnabled) {
        sendPushoverNotification("Alarm is sounding!");
        notificationSent = true; // Set flag to true after sending notification
      }
    } else {
      deactivateLights();
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
      <h1>Trespass Warning and Access Threat System</h1>
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
  server.send(200, "text/html", "<h1>Alarm Activated!</h1>");
}

void handleEnableLights() {
  lightsActivated = !lightsActivated;
  if (lightsActivated) {
    server.send(200, "text/html", "<h1>Lights Activated!</h1>");
  } else {
    server.send(200, "text/html", "<h1>Lights Deactivated!</h1>");
  }
}

void handleEnableSound() {
  soundActivated = !soundActivated;
  if (soundActivated) {
    server.send(200, "text/html", "<h1>Sound Activated!</h1>");
  } else {
    server.send(200, "text/html", "<h1>Sound Deactivated!</h1>");
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
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

void activateLights() {
  digitalWrite(lightRelayPin, HIGH);
  Serial.println();
  Serial.println("Lights Activated!");
}

void deactivateLights() {
  digitalWrite(lightRelayPin, LOW);
  Serial.println();
  Serial.println("Lights Deactivated!");
}

// --- Arm/Disarm Handlers ---
void handleArm() {
  systemArmed = true;
  Serial.println("System Armed");
  server.send(200, "text/html", "<h1>System Armed!</h1>");
}

void handleDisarm() {
  systemArmed = false;
  Serial.println("System Disarmed");
  server.send(200, "text/html", "<h1>System Disarmed!</h1>");
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
