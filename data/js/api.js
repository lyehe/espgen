// Functions for interacting with the REST API - Add later
console.log("api.js loaded");

const API_BASE_URL = ''; // Use relative paths

/**
 * Sends a command to the signal generator API.
 * @param {object} commandData - The command payload (e.g., { command: 'start', frequency: 1000, duty_cycle: 0.5 })
 * @returns {Promise<Response>} A promise that resolves with the fetch Response object.
 */
async function sendSignalCommand(commandData) {
    console.log('Sending command:', commandData);
    try {
        const response = await fetch(API_BASE_URL + '/api/trigger', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(commandData),
        });

        if (!response.ok) {
            console.error(`API Error: ${response.status} ${response.statusText}`);
            const errorBody = await response.text();
            console.error('Error Body:', errorBody);
            alert(`Failed to send command: ${response.statusText} - ${errorBody}`);
        } else {
            console.log('Command successful:', response.status);
            // Optionally parse response JSON if needed: const result = await response.json();
        }
        return response;
    } catch (error) {
        console.error('Fetch API Error:', error);
        alert(`Network error or failed to send command: ${error.message}`);
        throw error; // Re-throw if higher-level handling is needed
    }
}

/**
 * Sends a 'start' command.
 * @param {number} frequency
 * @param {number} dutyCycle
 * @param {number} durationSeconds (Optional duration, 0 or undefined for infinite)
 */
function startSignal(frequency, dutyCycle, durationSeconds) {
    const payload = {
        command: 'start',
        frequency: frequency,
        duty_cycle: dutyCycle
    };
    // Only add duration_sec if it's positive
    if (durationSeconds && durationSeconds > 0) {
        payload.duration_sec = durationSeconds;
    }
    return sendSignalCommand(payload);
}

/**
 * Sends a 'stop' command.
 */
function stopSignal() {
    return sendSignalCommand({ command: 'stop' });
}

/**
 * Sends an 'update' command (updates both frequency and duty cycle).
 * @param {number} frequency
 * @param {number} dutyCycle
 */
function updateSignal(frequency, dutyCycle) {
    return sendSignalCommand({ command: 'update', frequency: frequency, duty_cycle: dutyCycle });
}

/**
 * Sends a command to set the output pin.
 * @param {number} pin - The GPIO pin number (12-19).
 * @returns {Promise<Response>} A promise that resolves with the fetch Response object.
 */
async function setOutputPin(pin) {
    console.log(`Setting output pin to: ${pin}`);
    const url = API_BASE_URL + '/api/v1/config/output_pin';
    const payload = { pin: pin };

    try {
        const response = await fetch(url, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(payload),
        });

        if (!response.ok) {
            console.error(`API Error setting pin: ${response.status} ${response.statusText}`);
            const errorBody = await response.text();
            console.error('Error Body:', errorBody);
            alert(`Failed to set output pin: ${response.statusText} - ${errorBody}`);
        } else {
            console.log('Set pin command successful:', response.status);
            alert(`Output pin set request sent for pin ${pin}.`); // Provide feedback
            // Optionally update UI if status included pin info
        }
        return response;
    } catch (error) {
        console.error('Fetch API Error setting pin:', error);
        alert(`Network error or failed to set pin: ${error.message}`);
        throw error;
    }
}

// Function to fetch the initial status via GET /api/status
async function fetchInitialStatus() {
    const url = API_BASE_URL + '/api/status';
    console.log("Fetching initial status from:", url);
    try {
        const response = await fetch(url);
        if (!response.ok) {
            console.error('API Error fetching status:', response.status);
            // Don't alert here, handle error gracefully in the caller
            return null; 
        }
        const statusData = await response.json();
        console.log('Initial status received:', statusData);
        return statusData;
    } catch (error) {
        console.error('Network error fetching status:', error);
        // Don't alert here, handle error gracefully in the caller
        return null;
    }
}

// --- Example Usage (can be removed or kept for testing) ---
// async function testApi() {
//     console.log("Testing API...");
//     // Example start
//     await startSignal(1000, 0.5); 
//     await new Promise(resolve => setTimeout(resolve, 2000)); // Wait 2s
//     // Example stop
//     await stopSignal();
//     console.log("API Test complete.");
// }

// // Uncomment to run test on script load
// // testApi(); 