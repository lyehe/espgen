#include "LedcDriver.h"
#include <driver/ledc.h>
#include <Arduino.h>

// LEDC Driver implementation placeholder

LedcDriver::LedcDriver(int pin, int channel, double freq, uint8_t resolution) :
    _pin(pin),
    _channel(channel),
    _frequency(freq),
    _resolution(resolution)
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

    uint32_t max_duty_for_res = (1 << _resolution);
    uint32_t duty;
    if (dutyCycle >= 1.0) {
        // For 100% duty cycle, explicitly set duty to 2^resolution 
        duty = max_duty_for_res;
    } else {
        // Calculate duty based on current resolution: duty = fraction * (2^resolution - 1)
        duty = (uint32_t)(dutyCycle * (max_duty_for_res - 1));
    }
    
    // Uncomment the line below for debugging duty value
    Serial.printf("LEDC: Setting Ch %d duty raw: %lu (Input: %.3f, Res: %d, MaxRaw: %lu)\n", 
                  _channel, duty, dutyCycle, _resolution, max_duty_for_res - 1);

    ledcWrite(_channel, duty);
    // Serial.printf("LEDC: Channel %d set duty to %d (%.2f%%)\n", _channel, duty, dutyCycle * 100.0); // Optional: verbose logging
}

void LedcDriver::setFrequency(double freq) {
    if (freq <= 0) return; // Prevent invalid frequency
    _frequency = freq;

    // Use ledc_set_freq instead of ledcWriteTone to avoid changing resolution
    // Assuming high speed mode and Timer 0 for channel 0 (verify if channel changes)
    esp_err_t err = ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, _frequency);
    if (err != ESP_OK) {
        Serial.printf("LEDC: Error setting frequency: %s\n", esp_err_to_name(err));
    }
    // Note: ledc_set_freq should preserve the duty cycle, but re-applying might be safer 
    //       if exact duty cycle preservation across frequency changes is critical.
    // ledcUpdateDuty(_channel); // Might need this if using IDF directly

    Serial.printf("LEDC: Channel %d set frequency to %.2f Hz using ledc_set_freq\n", _channel, _frequency);
}

void LedcDriver::stop() {
    ledcWrite(_channel, 0); // Set duty to 0
    // ledcDetachPin(_pin); // Detach pin - Keep pin attached, just set duty to 0
    Serial.printf("LEDC: Channel %d set duty to 0 (stopped)\n", _channel);
}

void LedcDriver::reAttachPin(int newPin) {
    // Detach the current pin first
    ledcDetachPin(_pin);
    Serial.printf("LEDC: Detached Pin %d from Channel %d\n", _pin, _channel);

    // Update the internal pin number
    _pin = newPin;

    // Attach the new pin
    ledcAttachPin(_pin, _channel);
    Serial.printf("LEDC: Attached Pin %d to Channel %d\n", _pin, _channel);
}

// Heartbeat function - to be called periodically from the main loop or a task
void ledc_heartbeat_task(void *pvParameters) {
    (void)pvParameters; // Unused parameter
    while (1) {
        Serial.println("alive");
        vTaskDelay(pdMS_TO_TICKS(5000)); // Print "alive" every 5 seconds
    }
} 