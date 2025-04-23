#ifndef LEDC_DRIVER_H
#define LEDC_DRIVER_H

#include <Arduino.h>
#include <driver/ledc.h>
#include <stdint.h> // Include for uint8_t if not implicitly included

class LedcDriver {
public:
    LedcDriver(int pin, int channel = 0, double freq = 1000.0, uint8_t resolution = 8);
    void begin();
    void setDuty(double dutyCycle); // Duty cycle 0.0 to 1.0
    void setFrequency(double freq);
    void stop();
    int getChannel() const { return _channel; } // Getter for channel number

private:
    int _pin;
    int _channel;
    double _frequency;
    uint8_t _resolution;
    uint32_t _maxDutyValue;
};

#endif // LEDC_DRIVER_H 