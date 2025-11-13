#include "PerformanceMonitor.h"

PerformanceMonitor::PerformanceMonitor() :
    _metrics(),
    _totalLatencyUs(0),
    _startTimeMs(millis())
{
}

uint64_t PerformanceMonitor::startCommandTiming() {
    return esp_timer_get_time();
}

void PerformanceMonitor::endCommandTiming(uint64_t startTime) {
    uint64_t endTime = esp_timer_get_time();
    uint64_t latency = endTime - startTime;

    // Update statistics
    _metrics.totalCommands++;
    _totalLatencyUs += latency;
    _metrics.avgCommandLatencyUs = _totalLatencyUs / _metrics.totalCommands;

    if (latency > _metrics.maxCommandLatencyUs) {
        _metrics.maxCommandLatencyUs = latency;
    }

    if (latency < _metrics.minCommandLatencyUs) {
        _metrics.minCommandLatencyUs = latency;
    }
}

void PerformanceMonitor::recordEvent(bool success) {
    _metrics.totalEvents++;
    if (!success) {
        _metrics.failedEvents++;
    }
}

void PerformanceMonitor::updateMemoryStats() {
    uint32_t freeHeap = ESP.getFreeHeap();
    _metrics.freeHeap = freeHeap;

    if (freeHeap < _metrics.minFreeHeap) {
        _metrics.minFreeHeap = freeHeap;
    }
}

void PerformanceMonitor::getMetrics(PerformanceMetrics& metrics) {
    // Update uptime
    _metrics.uptimeMs = millis() - _startTimeMs;

    // Update memory stats
    updateMemoryStats();

    // Copy to output
    metrics = _metrics;
}

void PerformanceMonitor::reset() {
    _metrics = PerformanceMetrics();
    _totalLatencyUs = 0;
    _startTimeMs = millis();
}

void PerformanceMonitor::printMetrics() {
    // Update before printing
    _metrics.uptimeMs = millis() - _startTimeMs;
    updateMemoryStats();

    Serial.println("========== Performance Metrics ==========");
    Serial.printf("Uptime: %llu ms (%.2f hours)\n",
                  _metrics.uptimeMs,
                  _metrics.uptimeMs / 3600000.0);
    Serial.println();

    Serial.println("Commands:");
    Serial.printf("  Total: %llu\n", _metrics.totalCommands);
    if (_metrics.totalCommands > 0) {
        Serial.printf("  Avg Latency: %llu us (%.2f ms)\n",
                      _metrics.avgCommandLatencyUs,
                      _metrics.avgCommandLatencyUs / 1000.0);
        Serial.printf("  Min Latency: %llu us (%.2f ms)\n",
                      _metrics.minCommandLatencyUs,
                      _metrics.minCommandLatencyUs / 1000.0);
        Serial.printf("  Max Latency: %llu us (%.2f ms)\n",
                      _metrics.maxCommandLatencyUs,
                      _metrics.maxCommandLatencyUs / 1000.0);
    }
    Serial.println();

    Serial.println("Events:");
    Serial.printf("  Total: %llu\n", _metrics.totalEvents);
    Serial.printf("  Failed: %llu\n", _metrics.failedEvents);
    if (_metrics.totalEvents > 0) {
        Serial.printf("  Success Rate: %.1f%%\n",
                      ((_metrics.totalEvents - _metrics.failedEvents) * 100.0) / _metrics.totalEvents);
    }
    Serial.println();

    Serial.println("Memory:");
    Serial.printf("  Free Heap: %u bytes (%.2f KB)\n",
                  _metrics.freeHeap,
                  _metrics.freeHeap / 1024.0);
    Serial.printf("  Min Free Heap: %u bytes (%.2f KB)\n",
                  _metrics.minFreeHeap,
                  _metrics.minFreeHeap / 1024.0);
    Serial.println("========================================");
}
