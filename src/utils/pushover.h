#pragma once
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

class Pushover {
public:
    static void initialize(const char* userKey, const char* apiToken);
    static void sendNotification(const char* message);
private:
    static const char* _userKey;
    static const char* _apiToken;
};
