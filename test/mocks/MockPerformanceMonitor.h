#ifndef MOCK_PERFORMANCE_MONITOR_H
#define MOCK_PERFORMANCE_MONITOR_H

#include "IPerformanceMonitor.h"
#include "PerformanceMonitor.h"
#include <vector>

/**
 * @brief Mock implementation of IPerformanceMonitor for testing
 *
 * This mock allows tests to verify that performance monitoring methods
 * are called correctly without actually measuring real performance.
 *
 * Features:
 * - Records all method calls for verification
 * - Returns predictable test values
 * - Allows assertion of call counts and parameters
 */
class MockPerformanceMonitor : public IPerformanceMonitor {
public:
    MockPerformanceMonitor() :
        _startTimingCallCount(0),
        _endTimingCallCount(0),
        _recordEventCallCount(0),
        _updateMemoryCallCount(0),
        _getMetricsCallCount(0),
        _resetCallCount(0),
        _printMetricsCallCount(0),
        _lastStartTime(0),
        _successEventCount(0),
        _failEventCount(0)
    {}

    // ==================== IPerformanceMonitor Implementation ====================

    uint64_t startCommandTiming() override {
        _startTimingCallCount++;
        _lastStartTime = 123456; // Predictable test value
        return _lastStartTime;
    }

    void endCommandTiming(uint64_t startTime) override {
        _endTimingCallCount++;
        _endTimingStartTimes.push_back(startTime);
    }

    void recordEvent(bool success) override {
        _recordEventCallCount++;
        _recordedEvents.push_back(success);
        if (success) {
            _successEventCount++;
        } else {
            _failEventCount++;
        }
    }

    void updateMemoryStats() override {
        _updateMemoryCallCount++;
    }

    void getMetrics(PerformanceMetrics& metrics) override {
        _getMetricsCallCount++;
        // Return predictable test metrics
        metrics.commands_processed = 42;
        metrics.avg_latency_us = 150;
        metrics.min_latency_us = 50;
        metrics.max_latency_us = 500;
        metrics.events_published = 20;
        metrics.events_failed = 1;
        metrics.free_heap = 200000;
        metrics.min_free_heap = 180000;
        metrics.uptime_ms = 60000;
    }

    void reset() override {
        _resetCallCount++;
        clearCallHistory();
    }

    void printMetrics() override {
        _printMetricsCallCount++;
    }

    // ==================== Test Helper Methods ====================

    // Call count getters
    int getStartTimingCallCount() const { return _startTimingCallCount; }
    int getEndTimingCallCount() const { return _endTimingCallCount; }
    int getRecordEventCallCount() const { return _recordEventCallCount; }
    int getUpdateMemoryCallCount() const { return _updateMemoryCallCount; }
    int getGetMetricsCallCount() const { return _getMetricsCallCount; }
    int getResetCallCount() const { return _resetCallCount; }
    int getPrintMetricsCallCount() const { return _printMetricsCallCount; }

    // Event recording getters
    int getSuccessEventCount() const { return _successEventCount; }
    int getFailEventCount() const { return _failEventCount; }
    const std::vector<bool>& getRecordedEvents() const { return _recordedEvents; }
    const std::vector<uint64_t>& getEndTimingStartTimes() const { return _endTimingStartTimes; }

    // Verification helpers
    bool wasStartTimingCalled() const { return _startTimingCallCount > 0; }
    bool wasEndTimingCalled() const { return _endTimingCallCount > 0; }
    bool wasRecordEventCalled() const { return _recordEventCallCount > 0; }
    bool wasGetMetricsCalled() const { return _getMetricsCallCount > 0; }

    // Reset for reuse in multiple tests
    void clearCallHistory() {
        _startTimingCallCount = 0;
        _endTimingCallCount = 0;
        _recordEventCallCount = 0;
        _updateMemoryCallCount = 0;
        _getMetricsCallCount = 0;
        _resetCallCount = 0;
        _printMetricsCallCount = 0;
        _successEventCount = 0;
        _failEventCount = 0;
        _recordedEvents.clear();
        _endTimingStartTimes.clear();
    }

private:
    // Call counters
    int _startTimingCallCount;
    int _endTimingCallCount;
    int _recordEventCallCount;
    int _updateMemoryCallCount;
    int _getMetricsCallCount;
    int _resetCallCount;
    int _printMetricsCallCount;

    // Recorded values
    uint64_t _lastStartTime;
    int _successEventCount;
    int _failEventCount;
    std::vector<bool> _recordedEvents;
    std::vector<uint64_t> _endTimingStartTimes;
};

#endif // MOCK_PERFORMANCE_MONITOR_H
