#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <Arduino.h>

/**
 * @brief System health monitoring utilities
 *
 * Provides methods to check critical system resources like heap memory
 * and stack usage. Call these periodically to detect resource exhaustion
 * before it causes system failures.
 */
class SystemHealth {
public:
    /**
     * @brief Check heap memory and warn if low
     * @param threshold Minimum free heap in bytes (default 10KB)
     * @return true if heap is healthy, false if below threshold
     */
    static bool checkHeapHealth(size_t threshold = 10000) {
        size_t freeHeap = ESP.getFreeHeap();

        if (freeHeap < threshold) {
            Serial.printf("WARNING: Low heap memory: %u bytes free (threshold: %u)\n",
                         freeHeap, threshold);
            return false;
        }

        return true;
    }

    /**
     * @brief Check current task stack usage
     * @param threshold Minimum free stack in bytes (default 512)
     * @return true if stack is healthy, false if below threshold
     */
    static bool checkStackHealth(size_t threshold = 512) {
        UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL); // Returns WORDS
        size_t highWaterMarkBytes = highWaterMark * sizeof(StackType_t); // Convert to bytes

        if (highWaterMarkBytes < threshold) {
            Serial.printf("WARNING: Low stack space: %zu bytes remaining (threshold: %zu)\n",
                         highWaterMarkBytes, threshold);
            return false;
        }

        return true;
    }

    /**
     * @brief Check all system health metrics
     * @return true if all metrics are healthy
     */
    static bool checkAllMetrics() {
        bool heapOk = checkHeapHealth();
        bool stackOk = checkStackHealth();

        return heapOk && stackOk;
    }

    /**
     * @brief Print detailed system health report to Serial
     */
    static void printHealthReport() {
        Serial.println("=== System Health Report ===");

        // Heap information
        size_t freeHeap = ESP.getFreeHeap();
        size_t minFreeHeap = ESP.getMinFreeHeap();
        size_t heapSize = ESP.getHeapSize();

        Serial.printf("Heap: %u bytes free (min: %u, total: %u)\n",
                     freeHeap, minFreeHeap, heapSize);

        // Stack information
        UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL); // Returns WORDS
        size_t stackRemainingBytes = stackRemaining * sizeof(StackType_t); // Convert to bytes
        Serial.printf("Stack: %zu bytes remaining\n", stackRemainingBytes);

        // Task count
        UBaseType_t taskCount = uxTaskGetNumberOfTasks();
        Serial.printf("Tasks: %u running\n", taskCount);

        // Uptime
        unsigned long uptime = millis();
        unsigned long uptimeSeconds = uptime / 1000;
        unsigned long uptimeMinutes = uptimeSeconds / 60;
        unsigned long uptimeHours = uptimeMinutes / 60;

        Serial.printf("Uptime: %lu hours, %lu minutes, %lu seconds\n",
                     uptimeHours,
                     uptimeMinutes % 60,
                     uptimeSeconds % 60);

        Serial.println("============================");
    }

    /**
     * @brief Log critical warnings if system health is poor
     * Call this periodically from the main loop
     */
    static void logWarnings() {
        checkHeapHealth();
        checkStackHealth();
    }
};

#endif // SYSTEM_HEALTH_H
