#ifndef DRIVER_GPIO_H_MOCK
#define DRIVER_GPIO_H_MOCK

// This is a mock driver/gpio.h for native testing
#include "../mocks.h"

// Mock GPIO types and functions
typedef enum {
    GPIO_MODE_DISABLE,
    GPIO_MODE_INPUT,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_OUTPUT_OD,
    GPIO_MODE_INPUT_OUTPUT_OD,
    GPIO_MODE_INPUT_OUTPUT
} gpio_mode_t;

typedef enum {
    GPIO_PULLUP_DISABLE,
    GPIO_PULLUP_ENABLE
} gpio_pullup_t;

typedef enum {
    GPIO_PULLDOWN_DISABLE,
    GPIO_PULLDOWN_ENABLE
} gpio_pulldown_t;

// Mock GPIO functions
inline esp_err_t gpio_set_direction(int gpio_num, gpio_mode_t mode) { return ESP_OK; }
inline esp_err_t gpio_set_level(int gpio_num, int level) { return ESP_OK; }
inline esp_err_t gpio_set_pull_mode(int gpio_num, gpio_pullup_t pull_up, gpio_pulldown_t pull_down) { return ESP_OK; }

#endif // DRIVER_GPIO_H_MOCK
