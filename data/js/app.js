// Main application logic - Add later
console.log("app.js loaded");

// --- UI Elements ---
const statusDisplay = document.getElementById('status');
const wsStatusDisplay = document.getElementById('ws-status'); // Add this element to index.html
const freqDisplay = document.getElementById('freq-display'); // Add this element
const dutyDisplay = document.getElementById('duty-display'); // Add this element
const tickDisplay = document.getElementById('tick-display'); // Add this element

// Controls
const freqInput = document.getElementById('freq-input');
const dutyInput = document.getElementById('duty-input');
const updateButton = document.getElementById('update-button');
const startButton = document.getElementById('start-button');
const stopButton = document.getElementById('stop-button');

// --- State for Tick Estimation ---
const estimationState = {
    lastServerTicks: 0,
    lastBrowserTime: 0,
    currentFrequency: 0,
    intervalId: null
};

// --- WebSocket Message Handler ---
function handleWebSocketMessage(message) {
    console.log("App received WS message:", message);
    if (!message) return;

    // Extract common data
    const frequency = message.frequency?.toFixed(2) || 'N/A';
    const dutyCycle = message.duty_cycle?.toFixed(3) || 'N/A';
    const ticks = message.ticks !== undefined ? message.ticks.toString() : 'N/A'; // Handle potential undefined or large number

    // Update common displays
    if (freqDisplay) freqDisplay.textContent = frequency;
    if (dutyDisplay) dutyDisplay.textContent = dutyCycle;
    if (tickDisplay) tickDisplay.textContent = ticks; // Update tick display

    // Update state-specific displays and controls
    if (message.type === 'update') {
        statusDisplay.textContent = 'Running';
        statusDisplay.style.color = 'green';
        // Update input controls as well
        if (freqInput) freqInput.value = message.frequency?.toFixed(2) || '';
        if (dutyInput) dutyInput.value = message.duty_cycle?.toFixed(3) || '';

        // Start/Reset Tick Estimation
        estimationState.currentFrequency = message.frequency || 0;
        estimationState.lastServerTicks = message.ticks || 0;
        estimationState.lastBrowserTime = performance.now();
        if (estimationState.intervalId) {
            clearInterval(estimationState.intervalId);
        }
        // Start new interval only if frequency is positive
        if (estimationState.currentFrequency > 0) {
             estimationState.intervalId = setInterval(updateEstimatedTicks, 100); // Update display every 100ms
        }
        // Update tick display immediately with server value
        if (tickDisplay) tickDisplay.textContent = estimationState.lastServerTicks.toString();

    } else if (message.type === 'stopped') {
        statusDisplay.textContent = 'Stopped';
        statusDisplay.style.color = 'red';
        // Update input controls as well (keeping last values)
        if (freqInput) freqInput.value = message.frequency?.toFixed(2) || '';
        if (dutyInput) dutyInput.value = message.duty_cycle?.toFixed(3) || '';

        // Stop Tick Estimation
        if (estimationState.intervalId) {
            console.log("Clearing estimation interval ID:", estimationState.intervalId);
            clearInterval(estimationState.intervalId);
            estimationState.intervalId = null;
        }
        estimationState.currentFrequency = 0;
        estimationState.lastServerTicks = message.ticks !== undefined ? message.ticks : 0; // Store final server value
        // Update tick display with final server value (should be 0)
        if (tickDisplay) tickDisplay.textContent = estimationState.lastServerTicks.toString();

    } else {
        console.log("Unknown WS message type:", message.type);
    }
}

// --- Function to update tick display based on estimation ---
function updateEstimatedTicks() {
    // Extra checks to prevent running after stop
    if (!estimationState.intervalId || estimationState.currentFrequency <= 0 || statusDisplay.textContent === 'Stopped') {
        if (estimationState.intervalId) {
             // If interval still exists but shouldn't be running, clear it definitively
             console.warn("updateEstimatedTicks called unexpectedly after stop/clear. Clearing interval again.", estimationState.intervalId);
             clearInterval(estimationState.intervalId);
             estimationState.intervalId = null;
        }
        return;
    }

    const now = performance.now();
    const elapsedMillis = now - estimationState.lastBrowserTime;
    const additionalTicks = (elapsedMillis / 1000.0) * estimationState.currentFrequency;
    const estimatedTotalTicks = estimationState.lastServerTicks + Math.floor(additionalTicks);
    // console.log(`Estimating ticks: Base=${estimationState.lastServerTicks}, ElapsedMs=${elapsedMillis.toFixed(0)}, Freq=${estimationState.currentFrequency}, Additional=${additionalTicks.toFixed(0)}, Total=${estimatedTotalTicks}`); // Verbose log

    if (tickDisplay) {
        tickDisplay.textContent = estimatedTotalTicks.toString();
    }
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
    // Set initial UI state
     if (wsStatusDisplay) wsStatusDisplay.textContent = 'Connecting...';
     if (statusDisplay) statusDisplay.textContent = 'Unknown';
     if (freqDisplay) freqDisplay.textContent = '---';
     if (dutyDisplay) dutyDisplay.textContent = '---';
     if (tickDisplay) tickDisplay.textContent = '---'; // Initialize tick display

    // Setup Button Listeners
    setupControlListeners();

    // Connect WebSocket, passing the handler function
    connectWebSocket(handleWebSocketMessage);

    // Optional: Add API interaction setup here if needed (e.g., for sending commands)
    // setupApiInteraction();
});

// --- Control Event Listeners ---
function setupControlListeners() {
    if (!updateButton || !startButton || !stopButton || !freqInput || !dutyInput) {
        console.error("One or more control elements not found!");
        return;
    }

    updateButton.addEventListener('click', () => {
        const freq = parseFloat(freqInput.value);
        const duty = parseFloat(dutyInput.value);
        if (isNaN(freq) || isNaN(duty)) {
            alert('Invalid frequency or duty cycle value.');
            return;
        }
        console.log(`Update button clicked: F=${freq}, D=${duty}`);
        updateSignal(freq, duty); // Call function from api.js
    });

    startButton.addEventListener('click', () => {
        const freq = parseFloat(freqInput.value);
        const duty = parseFloat(dutyInput.value);
        if (isNaN(freq) || isNaN(duty)) {
            alert('Invalid frequency or duty cycle value.');
            return;
        }
        console.log(`Start button clicked: F=${freq}, D=${duty}`);
        startSignal(freq, duty); // Call function from api.js
    });

    stopButton.addEventListener('click', () => {
        console.log('Stop button clicked');
        stopSignal(); // Call function from api.js
    });
}
