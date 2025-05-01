// Main application logic - Add later
console.log("app.js loaded");

// --- UI Elements ---
const statusDisplay = document.getElementById('status');
const wsStatusDisplay = document.getElementById('ws-status'); // Add this element to index.html
const freqDisplay = document.getElementById('freq-display'); // Add this element
const dutyDisplay = document.getElementById('duty-display'); // Add this element
const tickDisplay = document.getElementById('tick-display'); // Add this element
const timeDisplay = document.getElementById('time-display'); // Add this element
const durationDisplay = document.getElementById('duration-display'); // Add this element

// Controls
const freqInput = document.getElementById('freq-input');
const dutyInput = document.getElementById('duty-input');
// const updateButton = document.getElementById('update-button'); // REMOVED
const startButton = document.getElementById('start-button');
const stopButton = document.getElementById('stop-button');
const delayInput = document.getElementById('delay-input'); // Get delay input
const durationInput = document.getElementById('duration-input'); // Get duration input

// Configuration Controls
const pinInput = document.getElementById('pin-input');
const setPinButton = document.getElementById('set-pin-button');

// --- State for Tick Estimation ---
const estimationState = {
    lastServerTicks: 0,
    lastTickUpdateTime: 0, // Browser time when lastServerTicks was updated
    currentFrequency: 0,
    tickIntervalId: null, // RE-ADD tick interval ID
    runTimeIntervalId: null,
    isSessionActive: false,
    sessionStartTime: 0 // Browser time when the session initially started
};

// --- WebSocket Message Handler ---
function handleWebSocketMessage(message) {
    console.log("App received WS message:", message);
    if (!message) return;

    // --- Update Controls and Core Display using shared function ---
    // Create a status object similar to the /api/status response format
    // Note: is_running state needs to be inferred from the message type
    const isRunning = (message.type === 'started' || message.type === 'update');
    const statusForUI = {
        frequency: message.frequency,
        duty_cycle: message.duty_cycle,
        duration_sec: message.duration_sec,
        is_running: isRunning,
        output_pin: message.output_pin
    };
    updateControlsAndDisplay(statusForUI);

    // --- Handle WebSocket-specific updates (Ticks and Timing) ---
    const ticks = message.ticks !== undefined ? message.ticks.toString() : 'N/A';
    // Update tick display immediately with server value
    if (tickDisplay) tickDisplay.textContent = ticks;
    
    // State machine for tick/time estimation intervals
    if (isRunning) { // Corresponds to 'update' or 'started' types
        const firstStartOfSession = !estimationState.isSessionActive;
        estimationState.isSessionActive = true;

        // Update state needed for estimation
        estimationState.currentFrequency = message.frequency || 0;
        estimationState.lastServerTicks = message.ticks || 0;
        estimationState.lastTickUpdateTime = performance.now(); // Base time for estimation

        if (firstStartOfSession) {
            console.log("First start of session detected.");
            estimationState.sessionStartTime = estimationState.lastTickUpdateTime; // Set initial session start time
            
            // Clear previous intervals (safety check)
            if (estimationState.tickIntervalId) clearInterval(estimationState.tickIntervalId);
            if (estimationState.runTimeIntervalId) clearInterval(estimationState.runTimeIntervalId);
            
            // Start new intervals only if frequency is positive
            if (estimationState.currentFrequency > 0) {
                estimationState.tickIntervalId = setInterval(updateEstimatedTicks, 1000); // Run every second
                estimationState.runTimeIntervalId = setInterval(updateElapsedTimeDisplay, 100); // Runtime updates faster
                console.log("Started intervals. Tick ID:", estimationState.tickIntervalId, "Time ID:", estimationState.runTimeIntervalId);
            }
        } else {
             console.log("Signal already running. Updating params/tick display.");
             // Base ticks (lastServerTicks) and base time (lastTickUpdateTime) updated above
             
             // Ensure intervals are running if frequency > 0
             if (estimationState.currentFrequency <= 0) {
                 // Stop intervals if frequency becomes 0
                 if (estimationState.tickIntervalId) clearInterval(estimationState.tickIntervalId); estimationState.tickIntervalId = null;
                 if (estimationState.runTimeIntervalId) clearInterval(estimationState.runTimeIntervalId); estimationState.runTimeIntervalId = null;
             } else {
                 // Restart intervals if they were stopped and freq > 0
                 if (!estimationState.tickIntervalId) {
                    estimationState.tickIntervalId = setInterval(updateEstimatedTicks, 1000); // Run every second
                 }
                 if (!estimationState.runTimeIntervalId) {
                    estimationState.runTimeIntervalId = setInterval(updateElapsedTimeDisplay, 100); 
                 }
             }
        }

    } else if (message.type === 'stopped') {
        estimationState.isSessionActive = false;
        
        // Stop ALL intervals
        if (estimationState.tickIntervalId) {
            console.log("Clearing tick estimation interval ID:", estimationState.tickIntervalId);
            clearInterval(estimationState.tickIntervalId);
            estimationState.tickIntervalId = null;
        }
        if (estimationState.runTimeIntervalId) {
            console.log("Clearing runtime interval ID:", estimationState.runTimeIntervalId);
            clearInterval(estimationState.runTimeIntervalId);
            estimationState.runTimeIntervalId = null;
        }
        estimationState.currentFrequency = 0; // Reset frequency used for estimation
        estimationState.lastServerTicks = message.ticks !== undefined ? message.ticks : 0;
        // Tick display already updated with final server value above
        
        // Calculate and display final elapsed time based on session start time
        if (estimationState.sessionStartTime > 0) { // Only calculate if session was active
            const finalElapsedMillis = performance.now() - estimationState.sessionStartTime;
            if (timeDisplay) {
                timeDisplay.textContent = formatElapsedTime(finalElapsedMillis);
            }
        }
        estimationState.sessionStartTime = 0; // Reset session start time for next run

    } else {
        console.log("Unknown WS message type:", message.type);
    }
}

// --- Function to update tick display based on estimation --- 
// UNCOMMENT and use previous logic based on lastTickUpdateTime
function updateEstimatedTicks() {
    // Only estimate if the session is active and frequency is positive
    if (!estimationState.tickIntervalId || estimationState.currentFrequency <= 0 || !estimationState.isSessionActive) {
        // Safety clear if called unexpectedly
        if (estimationState.tickIntervalId) {
             console.warn("Tick estimator stopping or called unexpectedly.");
             clearInterval(estimationState.tickIntervalId);
             estimationState.tickIntervalId = null;
        }
        return;
    }

    const now = performance.now();
    // Calculate time elapsed since the LAST SERVER UPDATE
    const elapsedMillis = now - estimationState.lastTickUpdateTime; 
    const additionalTicks = (elapsedMillis / 1000.0) * estimationState.currentFrequency;
    
    // Ensure lastServerTicks is treated as a number before adding
    const baseTicks = Number(estimationState.lastServerTicks) || 0;
    // Add estimated ticks to the LAST KNOWN SERVER COUNT
    const estimatedTotalTicks = baseTicks + Math.floor(additionalTicks);

    if (tickDisplay) {
        tickDisplay.textContent = estimatedTotalTicks.toString();
    }
}

// --- Function to update elapsed time display ---
function updateElapsedTimeDisplay() {
    if (!estimationState.runTimeIntervalId || !estimationState.isSessionActive) {
         if (estimationState.runTimeIntervalId) {
             console.warn("Time display stopping.");
             clearInterval(estimationState.runTimeIntervalId);
             estimationState.runTimeIntervalId = null;
         }
        return;
    }
    const now = performance.now();
    const elapsedMillis = now - estimationState.sessionStartTime; // Use time since initial session start
    if (timeDisplay) {
        timeDisplay.textContent = formatElapsedTime(elapsedMillis);
    }
}

// --- Helper function to format milliseconds into SSSSS.ssssss ---
function formatElapsedTime(millis) {
    if (millis < 0) millis = 0;
    const totalSeconds = millis / 1000.0;
    return totalSeconds.toFixed(6);

    // --- Old HH:MM:SS.ssss logic ---
    // let totalSeconds = Math.floor(millis / 1000);
    // let hours = Math.floor(totalSeconds / 3600);
    // let minutes = Math.floor((totalSeconds % 3600) / 60);
    // let seconds = totalSeconds % 60;
    // let milliseconds = Math.floor(millis % 1000); // Get milliseconds part

    // // Pad with leading zeros
    // const hoursStr = String(hours).padStart(2, '0');
    // const minutesStr = String(minutes).padStart(2, '0');
    // const secondsStr = String(seconds).padStart(2, '0');
    // const millisStr = String(milliseconds).padStart(4, '0'); // Pad milliseconds to 4 digits

    // return `${hoursStr}:${minutesStr}:${secondsStr}.${millisStr}`;
}

// --- WebSocket Connection Status Updater ---
// This function is called by ws.js when connection status changes
function updateConnectionStatus(isConnected) {
     if (wsStatusDisplay) {
         wsStatusDisplay.textContent = isConnected ? 'Connected' : 'Disconnected';
         wsStatusDisplay.style.color = isConnected ? 'green' : 'red';
     }
     // If just connected, maybe request initial status via REST? Or wait for first WS message.
     // if (isConnected) { getInitialStatus(); }
}

// --- Initialization ---
document.addEventListener('DOMContentLoaded', (event) => {
    console.log("DOM fully loaded and parsed");
    
    // Set initial UI state (placeholders)
    if (wsStatusDisplay) wsStatusDisplay.textContent = 'Connecting...';
    if (statusDisplay) statusDisplay.textContent = 'Initializing...'; // Change initial text
    if (freqDisplay) freqDisplay.textContent = '---';
    if (dutyDisplay) dutyDisplay.textContent = '---';
    if (durationDisplay) durationDisplay.textContent = '---';
    if (tickDisplay) tickDisplay.textContent = '---';
    if (timeDisplay) timeDisplay.textContent = '---';

    // Fetch initial status from API and update controls/display
    fetchInitialStatus()
        .then(initialStatus => {
            if (initialStatus) {
                updateControlsAndDisplay(initialStatus);
            } else {
                console.warn("Failed to fetch initial status, UI might not reflect current state until first WS update.");
                // Optionally set status display to an error state
                if (statusDisplay) statusDisplay.textContent = 'Error Loading Status';
            }
        })
        .catch(error => {
            console.error("Error during initial status fetch:", error);
            if (statusDisplay) statusDisplay.textContent = 'Error Loading Status';
        });

    // Setup Button Listeners
    setupControlListeners();

    // Connect WebSocket, passing the handler function
    connectWebSocket(handleWebSocketMessage);

    // Optional: Add API interaction setup here if needed (e.g., for sending commands)
    // setupApiInteraction();
});

// --- Control Event Listeners ---
function setupControlListeners() {
    if (startButton) { // Check specifically for startButton
        startButton.addEventListener('click', () => {
            const freq = parseFloat(freqInput.value);
            const dutyPercent = parseFloat(dutyInput.value);
            const delay = parseFloat(delayInput.value);
            const duration = parseFloat(durationInput.value);

            if (isNaN(freq) || isNaN(dutyPercent) || isNaN(delay) || isNaN(duration)) {
                alert('Invalid input for Frequency, Duty Cycle, Delay, or Duration.');
                return;
            }
            const duty = dutyPercent / 100.0; // Convert percentage to fraction

            console.log(`Start requested - Freq: ${freq}, Duty: ${duty}, Delay: ${delay}s, Duration: ${duration}s`);

            if (delay > 0) {
                console.log(`Delaying start by ${delay} seconds...`);
                setTimeout(() => {
                    console.log("Executing delayed start.");
                    startSignal(freq, duty, duration)
                        .catch(err => console.error("Error sending delayed start command:", err));
                }, delay * 1000);
            } else {
                startSignal(freq, duty, duration)
                    .catch(err => console.error("Error sending start command:", err));
            }
        });
    } else {
        console.error("Start button element not found!");
    }

    if (stopButton) { // Check specifically for stopButton
        stopButton.addEventListener('click', () => {
            console.log('Stop requested');
            stopSignal().catch(err => console.error("Error sending stop command:", err));
            // Also stop any local estimation timers immediately on stop click
            if (estimationState.tickIntervalId) clearInterval(estimationState.tickIntervalId); estimationState.tickIntervalId = null;
            if (estimationState.runTimeIntervalId) clearInterval(estimationState.runTimeIntervalId); estimationState.runTimeIntervalId = null;
            estimationState.isSessionActive = false; // Assume stopped until WS confirms
             if (statusDisplay) statusDisplay.textContent = 'Stopping...';
             // Reset displays? Optional, depends on desired behavior before WS confirmation
        });
    } else {
        console.error("Stop button element not found!");
    }

    // Add listener for the Set Pin button
    if (setPinButton) { // Check specifically for setPinButton
        setPinButton.addEventListener('click', () => {
            console.log("Set Pin Button Clicked - Handler Attached!");
            if (!pinInput) {
                console.error("Pin input element not found!");
                return;
            }
            const pin = parseInt(pinInput.value, 10);
            console.log(`Set Pin button clicked. Value: ${pinInput.value}, Parsed: ${pin}`);

            // Validate the pin number
            if (isNaN(pin) || pin < 12 || pin > 19) {
                alert('Invalid Output Pin number. Please enter a value between 12 and 19.');
                return;
            }

            console.log(`Sending command to set output pin to ${pin}...`);
            setOutputPin(pin)
                .catch(err => console.error("Error sending set output pin command:", err));
        });
    } else {
        console.error("Set Pin button element not found!");
    }

    // REMOVED UPDATE BUTTON LISTENER
    // if (updateButton) { updateButton.addEventListener(...) }
}

// --- Function to update UI elements from status object ---
function updateControlsAndDisplay(statusData) {
    if (!statusData) {
        console.warn("updateControlsAndDisplay called with null data.");
        return;
    }

    console.log("Updating UI with status:", statusData);

    // Extract data, providing defaults
    const frequency = statusData.frequency?.toFixed(2) || '---';
    // Duty cycle from API/Status is 0-1 fraction
    const dutyCycleFraction = statusData.duty_cycle;
    const dutyCyclePercent = dutyCycleFraction !== undefined ? (dutyCycleFraction * 100).toFixed(1) : '---';
    // Duration from API/Status is duration_sec or lastAppliedDurationSec (check ApiRouter.cpp if needed)
    // Assuming the key is duration_sec based on previous changes
    const duration = statusData.duration_sec !== undefined ? statusData.duration_sec.toFixed(2) : '---';
    const isRunning = statusData.is_running === true; // Explicitly check for true
    const outputPin = statusData.output_pin; // Get the output pin

    // Update Display Elements
    if (freqDisplay) freqDisplay.textContent = frequency;
    if (dutyDisplay) dutyDisplay.textContent = dutyCyclePercent; // Display percentage
    if (durationDisplay) durationDisplay.textContent = duration === '0.00' ? 'Infinite' : duration;
    if (statusDisplay) {
        statusDisplay.textContent = isRunning ? 'Running' : 'Stopped';
        statusDisplay.style.color = isRunning ? 'green' : 'red';
    }

    // Update Input Controls
    if (freqInput) freqInput.value = frequency !== '---' ? frequency : ''; // Use number or empty
    if (dutyInput) dutyInput.value = dutyCyclePercent !== '---' ? dutyCyclePercent : ''; // Use percentage or empty
    if (durationInput) durationInput.value = duration !== '---' ? duration : '0'; // Use duration or 0
    
    // Update Pin Configuration Input
    if (pinInput && outputPin !== undefined) {
        console.log(`Setting pin input field to: ${outputPin}`);
        pinInput.value = outputPin;
    }
    
    // Update tick estimation state if needed (only frequency is used by estimator)
    // This might be better handled only by WebSocket updates? 
    // For now, only update if the source is clearly indicating a running state.
    // if (isRunning) {
    //     estimationState.currentFrequency = statusData.frequency || 0;
    // }
}
 