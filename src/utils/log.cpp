#include "log.h"

void logEvent(const char* message, bool force) {
    static unsigned long lastLogTime = 0;
    unsigned long now = millis();
    if (force || (now - lastLogTime >= 1000)) {
        Serial.println(message);
        lastLogTime = now;
    }
}
