#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// --- Configuration ---
const char* ssid = "RootAccess";  // Replace with WiFi SSID
const char* password = "HACKTHEPLANET"; // Replace with WiFi password

const int pirPin = D7;       // PIR sensor connected to digital pin D1 (GPIO 5)
const int piezoPin = D2;     // Piezo buzzer connected to digital pin D2 (GPIO 4)

// --- Web Server ---
ESP8266WebServer server(80);

// --- Global Variables ---
bool alarmActive = false;     // Flag to indicate if the full alarm is active
unsigned long alarmStartTime = 0;  // Timestamp of when the full alarm started
const unsigned long alarmDuration = 3000; // Alarm duration in milliseconds (2 seconds)
bool motionDetected = false; // Flag to prevent multiple short alarms.
bool systemArmed = false;    // Flag to indicate if the system is armed

// --- Function Prototypes ---
void handleRoot();
void handleAlarmOn();
void handleNotFound();
void playWarningTone();
void playAlarmSound();
void stopAlarmSound();
void connectToWiFi();
void handleArm();
void handleDisarm();


void setup() {
  Serial.begin(9600);
  pinMode(pirPin, INPUT);
  pinMode(piezoPin, OUTPUT);
  digitalWrite(piezoPin, LOW); // Ensure piezo is off initially

  connectToWiFi();

  // --- Web Server Route Setup ---
  server.on("/", handleRoot);
  server.on("/alarmOn", handleAlarmOn);
  server.on("/arm", handleArm);
  server.on("/disarm", handleDisarm);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // --- Motion Detection ---
  if (systemArmed) {
    int pirValue = digitalRead(pirPin);
    if (pirValue == HIGH && !motionDetected) {
      Serial.println("Motion detected!");
      motionDetected = true;
      playWarningTone();
    } else if (pirValue == LOW && motionDetected) {
      Serial.println("Ready...");
      motionDetected = false; // Reset flag when motion stops
    }
  }

  // --- Full Alarm Logic ---
  if (alarmActive) {
    if (millis() - alarmStartTime < alarmDuration) {
      playAlarmSound();
    } else {
      stopAlarmSound();
      alarmActive = false; // Deactivate the alarm after the duration
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
  alarmActive = true;
  alarmStartTime = millis();
  server.send(200, "text/html", "<h1>Alarm Activated!</h1>");
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

// --- Alarm Sound Functions ---

void playWarningTone() {
  // Play a short warning tone sequence
  digitalWrite(piezoPin, HIGH);
  delay(800);
  digitalWrite(piezoPin, LOW);
}


void playAlarmSound() {
  //  loud, continuous alarm tone
  Serial.println("Alarm Sounding!");
  digitalWrite(piezoPin, HIGH);
}

void stopAlarmSound() {
  digitalWrite(piezoPin, LOW);
  Serial.println("Alarm Stopped!");
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