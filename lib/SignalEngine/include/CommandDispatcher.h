#ifndef COMMAND_DISPATCHER_H
#define COMMAND_DISPATCHER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "signal_iface.h"
#include "IPerformanceMonitor.h"

// Forward declarations
class IPulseGenerator;
class SignalState;
class SignalPersistence;
class TimingController;
class SignalEventPublisher;
struct PerformanceMetrics;

/**
 * @brief Manages command queue and dispatches commands to appropriate handlers.
 *
 * This class encapsulates the command dispatcher task and all command handling
 * logic, providing a clean interface for queuing and processing commands.
 *
 * Responsibilities:
 * - Create and manage FreeRTOS command queue
 * - Create and manage command dispatcher task
 * - Receive commands from queue
 * - Validate command parameters
 * - Dispatch commands to appropriate handlers
 * - Coordinate state, persistence, timing, and event subsystems
 *
 * Design Pattern: Single Responsibility Principle + Dependency Injection
 * Design Pattern: Dependency Inversion - Depends on IPulseGenerator interface
 * Thread Safety: FreeRTOS queue and task-based command processing
 */

class CommandDispatcher {
public:
    /**
     * @brief Constructor with dependency injection
     *
     * All dependencies are injected to maintain clean architecture and
     * enable testability. Uses interfaces for DIP compliance.
     *
     * @param pulseGen Reference to IPulseGenerator for hardware control
     * @param state Reference to SignalState for state management
     * @param persistence Reference to SignalPersistence for NVS operations
     * @param timingController Reference to TimingController for timing operations
     * @param eventPublisher Reference to SignalEventPublisher for event publishing
     * @param perfMonitor Reference to IPerformanceMonitor for metrics tracking
     */
    CommandDispatcher(
        IPulseGenerator& pulseGen,
        SignalState& state,
        SignalPersistence& persistence,
        TimingController& timingController,
        SignalEventPublisher& eventPublisher,
        IPerformanceMonitor& perfMonitor
    );

    ~CommandDispatcher();

    /**
     * @brief Initialize command dispatcher
     *
     * Creates command queue and dispatcher task.
     *
     * @return true if initialized successfully, false on error
     */
    bool begin();

    /**
     * @brief Send command to dispatcher queue
     *
     * Thread-safe command submission. Command is validated and queued
     * for processing by dispatcher task.
     *
     * @param cmd Command to send
     * @return true if queued successfully, false if queue full
     */
    bool sendCommand(const SignalCmd& cmd);

    /**
     * @brief Check if dispatcher is initialized
     *
     * @return true if begin() was called successfully, false otherwise
     */
    bool isInitialized() const;

    /**
     * @brief Get performance metrics
     *
     * @param metrics Output parameter to receive current metrics
     */
    void getPerformanceMetrics(PerformanceMetrics& metrics);

private:
    // Injected dependencies (using interfaces for DIP compliance)
    IPulseGenerator& _pulseGen;
    SignalState& _state;
    SignalPersistence& _persistence;
    TimingController& _timingController;
    SignalEventPublisher& _eventPublisher;
    IPerformanceMonitor& _perfMonitor;  // Interface for testability

    // FreeRTOS resources
    QueueHandle_t _commandQueue;
    TaskHandle_t _taskHandle;
    bool _initialized;

    // Task function
    static void dispatcherTask(void* pvParameters);

    // Command handlers
    void handleStart(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId);
    void handleStop(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId, uint64_t& finalTicks);
    void handleUpdateFreq(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId);
    void handleUpdateDuty(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId);
    void handleUpdateAll(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId);
    void handleSetPin(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId);
    void handleSetIndicator(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId);
    void handleConfigChannel(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId);
    void handleEnableChannel(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId);
    void handleSync(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId);
    void handleSweep(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId);
};

#endif // COMMAND_DISPATCHER_H
