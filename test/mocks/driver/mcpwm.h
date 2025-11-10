#ifndef DRIVER_MCPWM_H_MOCK
#define DRIVER_MCPWM_H_MOCK

// This is a mock driver/mcpwm.h for native testing
// Include the main mocks header which provides all implementations
#include "../mocks.h"

// Mock MCPWM types and constants (minimal definitions needed)
typedef enum {
    MCPWM_UNIT_0,
    MCPWM_UNIT_1,
    MCPWM_UNIT_MAX
} mcpwm_unit_t;

typedef enum {
    MCPWM_TIMER_0,
    MCPWM_TIMER_1,
    MCPWM_TIMER_2,
    MCPWM_TIMER_MAX
} mcpwm_timer_t;

typedef enum {
    MCPWM_OPR_A,
    MCPWM_OPR_B,
    MCPWM_OPR_MAX
} mcpwm_operator_t;

typedef enum {
    MCPWM_UP_COUNTER,
    MCPWM_DOWN_COUNTER,
    MCPWM_UP_DOWN_COUNTER
} mcpwm_counter_type_t;

typedef enum {
    MCPWM_DUTY_MODE_0,
    MCPWM_DUTY_MODE_1,
    MCPWM_DUTY_MODE_MAX
} mcpwm_duty_type_t;

typedef struct {
    uint32_t frequency;
    float cmpr_a;
    float cmpr_b;
    mcpwm_duty_type_t duty_mode;
    mcpwm_counter_type_t counter_mode;
} mcpwm_config_t;

// Mock MCPWM functions (inline no-ops for testing)
inline esp_err_t mcpwm_gpio_init(mcpwm_unit_t unit, int signal, int pin) { return ESP_OK; }
inline esp_err_t mcpwm_init(mcpwm_unit_t unit, mcpwm_timer_t timer, const mcpwm_config_t *config) { return ESP_OK; }
inline esp_err_t mcpwm_set_frequency(mcpwm_unit_t unit, mcpwm_timer_t timer, uint32_t freq) { return ESP_OK; }
inline esp_err_t mcpwm_set_duty(mcpwm_unit_t unit, mcpwm_timer_t timer, mcpwm_operator_t op, float duty) { return ESP_OK; }
inline esp_err_t mcpwm_start(mcpwm_unit_t unit, mcpwm_timer_t timer) { return ESP_OK; }
inline esp_err_t mcpwm_stop(mcpwm_unit_t unit, mcpwm_timer_t timer) { return ESP_OK; }

#endif // DRIVER_MCPWM_H_MOCK
