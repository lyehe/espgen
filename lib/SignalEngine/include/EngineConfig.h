#ifndef ENGINE_CONFIG_H
#define ENGINE_CONFIG_H

// Configuration for the Signal Engine
// This might overlap with common/build_opts.h initially
// but can hold more specific engine tuning parameters.

#define ENGINE_TASK_STACK_SIZE 4096
#define ENGINE_TASK_PRIORITY   5

// Define hardware resources (which timer, channels etc.)
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL_0     LEDC_CHANNEL_0
#define LEDC_RESOLUTION    LEDC_TIMER_10_BIT // Example resolution

#endif // ENGINE_CONFIG_H 