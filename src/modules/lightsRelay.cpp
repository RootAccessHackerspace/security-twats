#include "lightsRelay.h"
#include <Arduino.h>
#include "../utils/log.h"

LightsRelay::LightsRelay(int pin) : _pin(pin), _enabled(false), _currentlyOn(false) {}

void LightsRelay::begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
}

void LightsRelay::setEnabled(bool enabled) {
    _enabled = enabled;
    logEvent(_enabled ? "Lights Enabled" : "Lights Disabled", true);
    if (!_enabled && _currentlyOn) {
        setState(false);
    }
}

void LightsRelay::setState(bool on) {
    digitalWrite(_pin, on ? HIGH : LOW);
    _currentlyOn = on;
    logEvent(on ? "Lights ON" : "Lights OFF", true);
}

bool LightsRelay::isEnabled() const {
    return _enabled;
}

bool LightsRelay::isOn() const {
    return _currentlyOn;
}
