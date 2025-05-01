#ifndef LEDC_DRIVER_H
#define LEDC_DRIVER_H

#include <stdint.h> // Include for uint8_t etc.

// LEDC Driver class definition
class LedcDriver {
public:
    // Constructor
    LedcDriver(int pin, int channel, double freq, uint8_t resolution);

    // Initialize LEDC
    void begin();

    // Set duty cycle (0.0 to 1.0)
    void setDuty(double dutyCycle);

    // Set frequency (Hz)
    void setFrequency(double freq);

    // Stop output (set duty to 0)
    void stop();

    // Detach old pin and attach new pin
    void reAttachPin(int newPin);

    // Getters (optional)
    int getChannel() const { return _channel; }
    uint8_t getResolution() const { return _resolution; }
    // Add other getters if needed

private:
    int _pin;
    int _channel;
    double _frequency;
    uint8_t _resolution;
    // uint32_t _maxDutyValue; // Removed - Calculate in setDuty based on _resolution
};

#endif // LEDC_DRIVER_H 