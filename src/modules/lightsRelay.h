#ifndef LIGHTS_RELAY_H
#define LIGHTS_RELAY_H

class LightsRelay {
public:
    LightsRelay(int pin);
    void begin();
    void setEnabled(bool enabled);
    void setState(bool on);
    bool isEnabled() const;
    bool isOn() const;

private:
    const int _pin;
    bool _enabled;
    bool _currentlyOn;
};

#endif
