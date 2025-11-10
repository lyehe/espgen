#ifndef SIGNAL_EVENT_PUBLISHER_H
#define SIGNAL_EVENT_PUBLISHER_H

#include <Arduino.h>
#include "signal_iface.h"
#include "SignalState.h"
#include "esp_event.h"

/**
 * @brief Manages ESP event publishing for SignalEngine state changes.
 *
 * This class encapsulates all ESP event system operations, providing
 * a clean interface for publishing signal events to subscribers
 * (WebSocket clients, monitoring systems, etc.).
 *
 * Responsibilities:
 * - Publish STARTED events when signal starts
 * - Publish STOPPED events when signal stops
 * - Publish PARAMS_CHANGED events when parameters change
 * - Populate event data from SignalState
 * - Handle event posting errors and timeouts
 *
 * Design Pattern: Single Responsibility Principle
 * Thread Safety: Works with SignalState which handles mutex protection
 */

class SignalEventPublisher {
public:
    SignalEventPublisher();
    ~SignalEventPublisher();

    /**
     * @brief Publish STARTED event
     *
     * Populates event data from current state and posts to ESP event loop.
     *
     * @param state Reference to SignalState for reading current state
     * @return true if event posted successfully, false on error
     */
    bool publishStarted(const SignalState& state);

    /**
     * @brief Publish STOPPED event
     *
     * Populates event data from current state and posts to ESP event loop.
     * Typically called with final accumulated tick count.
     *
     * @param state Reference to SignalState for reading current state
     * @param finalTicks Final accumulated tick count (before reset)
     * @return true if event posted successfully, false on error
     */
    bool publishStopped(const SignalState& state, uint64_t finalTicks);

    /**
     * @brief Publish PARAMS_CHANGED event
     *
     * Populates event data from current state and posts to ESP event loop.
     *
     * @param state Reference to SignalState for reading current state
     * @return true if event posted successfully, false on error
     */
    bool publishParamsChanged(const SignalState& state);

    /**
     * @brief Publish custom event with pre-populated data
     *
     * Allows publishing any event type with manually populated data.
     * Useful for special cases not covered by the convenience methods.
     *
     * @param eventId Event ID (SIG_EVT_STARTED, SIG_EVT_STOPPED, etc.)
     * @param eventData Pre-populated event data structure
     * @return true if event posted successfully, false on error
     */
    bool publishEvent(SigEvtId eventId, const SignalEvtData& eventData);

private:
    /**
     * @brief Populate event data from SignalState
     *
     * Reads current state values and populates the event data structure.
     * Uses atomicUpdate to ensure consistent multi-value reads.
     *
     * @param eventData Reference to event data structure to populate
     * @param state Reference to SignalState for reading current state
     */
    void populateEventData(SignalEvtData& eventData, const SignalState& state);

    /**
     * @brief Post event to ESP event loop with timeout
     *
     * @param eventId Event ID
     * @param eventData Event data structure
     * @return true if posted successfully, false on error/timeout
     */
    bool postEvent(SigEvtId eventId, const SignalEvtData& eventData);

    // Event posting timeout (milliseconds)
    static const TickType_t EVENT_POST_TIMEOUT_MS = 100;
};

#endif // SIGNAL_EVENT_PUBLISHER_H
