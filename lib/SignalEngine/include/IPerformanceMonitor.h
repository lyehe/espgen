#ifndef IPERFORMANCE_MONITOR_H
#define IPERFORMANCE_MONITOR_H

#include <stdint.h>

// Forward declaration
struct PerformanceMetrics;

/**
 * @brief Performance Monitor Interface (SOLID: Dependency Inversion)
 *
 * Abstract interface for performance monitoring.
 * Enables dependency injection and testing with mock implementations.
 *
 * Design Pattern: Interface Segregation + Dependency Inversion
 */
class IPerformanceMonitor {
public:
    virtual ~IPerformanceMonitor() = default;

    /**
     * @brief Start timing a command
     *
     * Call this at the beginning of command processing.
     *
     * @return Start timestamp in microseconds
     */
    virtual uint64_t startCommandTiming() = 0;

    /**
     * @brief End timing a command and update statistics
     *
     * Call this at the end of command processing.
     *
     * @param startTime Start timestamp from startCommandTiming()
     */
    virtual void endCommandTiming(uint64_t startTime) = 0;

    /**
     * @brief Record an event publication
     *
     * @param success true if event was published successfully
     */
    virtual void recordEvent(bool success) = 0;

    /**
     * @brief Update memory usage statistics
     *
     * Samples current heap usage and tracks minimum.
     */
    virtual void updateMemoryStats() = 0;

    /**
     * @brief Get current performance metrics
     *
     * @param metrics Output parameter to receive metrics
     */
    virtual void getMetrics(PerformanceMetrics& metrics) = 0;

    /**
     * @brief Reset all statistics
     */
    virtual void reset() = 0;

    /**
     * @brief Print performance metrics to Serial
     */
    virtual void printMetrics() = 0;
};

#endif // IPERFORMANCE_MONITOR_H
