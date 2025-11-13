#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <Arduino.h>
#include "esp_timer.h"
#include "IPerformanceMonitor.h"

/**
 * @brief Concrete performance monitoring implementation
 *
 * Tracks command processing times, memory usage, and other metrics.
 * Useful for diagnostics and performance optimization.
 *
 * Design Pattern: Implements IPerformanceMonitor interface for dependency injection
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

class PerformanceMonitor : public IPerformanceMonitor {
public:
    PerformanceMonitor();

    // IPerformanceMonitor interface implementation
    uint64_t startCommandTiming() override;
    void endCommandTiming(uint64_t startTime) override;
    void recordEvent(bool success) override;
    void updateMemoryStats() override;
    void getMetrics(PerformanceMetrics& metrics) override;
    void reset() override;
    void printMetrics() override;

private:
    PerformanceMetrics _metrics;
    uint64_t _totalLatencyUs;  // Running sum for average calculation
    uint64_t _startTimeMs;     // Boot time for uptime calculation
};

#endif // PERFORMANCE_MONITOR_H
