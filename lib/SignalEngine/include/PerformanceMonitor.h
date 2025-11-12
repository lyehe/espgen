#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <Arduino.h>
#include "esp_timer.h"

/**
 * @brief Simple performance monitoring for signal engine
 *
 * Tracks command processing times, memory usage, and other metrics.
 * Useful for diagnostics and performance optimization.
 *
 * Design Pattern: Singleton (optional) or standalone utility
 */

// Performance metrics structure
struct PerformanceMetrics {
    // Command processing
    uint64_t totalCommands;
    uint64_t avgCommandLatencyUs;
    uint64_t maxCommandLatencyUs;
    uint64_t minCommandLatencyUs;

    // Memory usage
    uint32_t freeHeap;
    uint32_t minFreeHeap;

    // Uptime
    uint64_t uptimeMs;

    // Event publishing
    uint64_t totalEvents;
    uint64_t failedEvents;

    // Constructor with defaults
    PerformanceMetrics() :
        totalCommands(0),
        avgCommandLatencyUs(0),
        maxCommandLatencyUs(0),
        minCommandLatencyUs(UINT64_MAX),
        freeHeap(0),
        minFreeHeap(UINT32_MAX),
        uptimeMs(0),
        totalEvents(0),
        failedEvents(0)
    {}
};

class PerformanceMonitor {
public:
    PerformanceMonitor();

    /**
     * @brief Start timing a command
     *
     * Call this at the beginning of command processing.
     *
     * @return Start timestamp in microseconds
     */
    uint64_t startCommandTiming();

    /**
     * @brief End timing a command and update statistics
     *
     * Call this at the end of command processing.
     *
     * @param startTime Start timestamp from startCommandTiming()
     */
    void endCommandTiming(uint64_t startTime);

    /**
     * @brief Record an event publication
     *
     * @param success true if event was published successfully
     */
    void recordEvent(bool success);

    /**
     * @brief Update memory usage statistics
     *
     * Samples current heap usage and tracks minimum.
     */
    void updateMemoryStats();

    /**
     * @brief Get current performance metrics
     *
     * @param metrics Output parameter to receive metrics
     */
    void getMetrics(PerformanceMetrics& metrics);

    /**
     * @brief Reset all statistics
     */
    void reset();

    /**
     * @brief Print performance metrics to Serial
     */
    void printMetrics();

private:
    PerformanceMetrics _metrics;
    uint64_t _totalLatencyUs;  // Running sum for average calculation
    uint64_t _startTimeMs;     // Boot time for uptime calculation
};

#endif // PERFORMANCE_MONITOR_H
