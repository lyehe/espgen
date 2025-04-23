#include "LedcDriver.h"
#include <driver/ledc.h>
#include <Arduino.h>

// LEDC Driver implementation placeholder

LedcDriver::LedcDriver(int pin, int channel, double freq, uint8_t resolution) :
    _pin(pin),
    _channel(channel),
    _frequency(freq),
    _resolution(resolution),
    _maxDutyValue((1 << resolution) - 1)
{
}

void LedcDriver::begin() {
    ledcSetup(_channel, _frequency, _resolution);
    ledcAttachPin(_pin, _channel);
    Serial.printf("LEDC: Channel %d setup on Pin %d, Freq: %.2f Hz, Res: %d bits\n", _channel, _pin, _frequency, _resolution);
}

void LedcDriver::setDuty(double dutyCycle) {
    if (dutyCycle < 0.0) dutyCycle = 0.0;
    if (dutyCycle > 1.0) dutyCycle = 1.0;
    uint32_t duty = (uint32_t)(dutyCycle * _maxDutyValue);
    ledcWrite(_channel, duty);
    // Serial.printf("LEDC: Channel %d set duty to %d (%.2f%%)\n", _channel, duty, dutyCycle * 100.0); // Optional: verbose logging
}

void LedcDriver::setFrequency(double freq) {
    if (freq <= 0) return; // Prevent invalid frequency
    _frequency = freq;
    ledcWriteTone(_channel, _frequency); // Updates frequency and potentially duty resolution
    // Note: ledcWriteTone might implicitly change the duty cycle if resolution changes.
    // Re-apply duty cycle if necessary, though often handled adequately by ESP-IDF.
    Serial.printf("LEDC: Channel %d set frequency to %.2f Hz\n", _channel, _frequency);
}

void LedcDriver::stop() {
    ledcWrite(_channel, 0); // Set duty to 0
    // ledcDetachPin(_pin); // Detach pin - Keep pin attached, just set duty to 0
    Serial.printf("LEDC: Channel %d set duty to 0 (stopped)\n", _channel);
}

// Heartbeat function - to be called periodically from the main loop or a task
void ledc_heartbeat_task(void *pvParameters) {
    (void)pvParameters; // Unused parameter
    while (1) {
        Serial.println("alive");
        vTaskDelay(pdMS_TO_TICKS(5000)); // Print "alive" every 5 seconds
    }
} 