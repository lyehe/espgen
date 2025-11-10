#include "SignalEventPublisher.h"
#include "TimingController.h"

SignalEventPublisher::SignalEventPublisher() {
    // Constructor body (if needed)
}

SignalEventPublisher::~SignalEventPublisher() {
    // Destructor body (if needed)
}

// ========== Public Methods ==========

bool SignalEventPublisher::publishStarted(const SignalState& state) {
    SignalEvtData eventData;
    populateEventData(eventData, state);
    return postEvent(SIG_EVT_STARTED, eventData);
}

bool SignalEventPublisher::publishStopped(const SignalState& state, uint64_t finalTicks) {
    SignalEvtData eventData;
    populateEventData(eventData, state);

    // Override ticks with final value (before reset)
    eventData.current_ticks = finalTicks;

    return postEvent(SIG_EVT_STOPPED, eventData);
}

bool SignalEventPublisher::publishParamsChanged(const SignalState& state) {
    SignalEvtData eventData;
    populateEventData(eventData, state);
    return postEvent(SIG_EVT_PARAMS_CHANGED, eventData);
}

bool SignalEventPublisher::publishEvent(SigEvtId eventId, const SignalEvtData& eventData) {
    return postEvent(eventId, eventData);
}

// ========== Private Helper Methods ==========

void SignalEventPublisher::populateEventData(SignalEvtData& eventData, const SignalState& state) {
    // Use atomicUpdate to read multiple state values consistently
    // Note: We need to cast away const since atomicUpdate modifies mutex
    // This is safe because we're only reading state inside the lambda
    const_cast<SignalState&>(state).atomicUpdate([&eventData, &state]() {
        eventData.channel = 0; // Hardcode channel 0 for now (master channel)
        eventData.current_freq = (uint32_t)state.getCurrentFrequencyHz_nolock();
        eventData.current_duty = state.getCurrentDutyCycle_nolock();
        eventData.duration_sec = state.getLastAppliedDurationSec_nolock();
        eventData.output_pin = state.getOutputPin_nolock();

        // Calculate current ticks using TimingController logic
        // (This is a read-only operation, safe inside const method)
        TimingController tempController;
        if (state.isRunning_nolock() && state.getStartTimeMicros_nolock() > 0) {
            uint64_t currentCycles = tempController.calculateCycles(
                state.getStartTimeMicros_nolock(),
                esp_timer_get_time(),
                state.getCurrentFrequencyHz_nolock()
            );
            eventData.current_ticks = state.getAccumulatedTicks_nolock() + currentCycles;
        } else {
            eventData.current_ticks = state.getAccumulatedTicks_nolock();
        }
    });
}

bool SignalEventPublisher::postEvent(SigEvtId eventId, const SignalEvtData& eventData) {
    // Post event to ESP event loop with timeout
    esp_err_t post_err = esp_event_post(
        SIGNAL_EVENTS,
        eventId,
        &eventData,
        sizeof(eventData),
        pdMS_TO_TICKS(EVENT_POST_TIMEOUT_MS)
    );

    if (post_err == ESP_ERR_TIMEOUT) {
        Serial.printf("SignalEventPublisher: WARNING - Event queue full, event %d dropped\n", eventId);
        return false;
    } else if (post_err != ESP_OK) {
        Serial.printf("SignalEventPublisher: ERROR - Failed to post event %d: %s\n",
                     eventId, esp_err_to_name(post_err));
        return false;
    }

    Serial.printf("SignalEventPublisher: Posted event: Base=SIGNAL_EVENTS, ID=%d\n", eventId);
    return true;
}
