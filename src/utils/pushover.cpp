#include "pushover.h"
#include "../utils/log.h"

const char* Pushover::_userKey = nullptr;
const char* Pushover::_apiToken = nullptr;

void Pushover::initialize(const char* userKey, const char* apiToken) {
    _userKey = userKey;
    _apiToken = apiToken;
}

void Pushover::sendNotification(const char* message) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();
        http.begin(client, "https://api.pushover.net/1/messages.json");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String postData = "token=" + String(_apiToken) + "&user=" + String(_userKey) + "&message=" + String(message);
        int httpResponseCode = http.POST(postData);
        if (httpResponseCode > 0) {
            logEvent(("Pushover Response: " + http.getString()).c_str(), true);
        } else {
            logEvent("Error sending Pushover notification", true);
        }
        http.end();
    } else {
        logEvent("WiFi not connected. Cannot send notification.", true);
    }
}
