// Functions for interacting with the REST API - Add later
console.log("api.js loaded");

const API_ENDPOINT = '/api/trigger';

/**
 * Sends a command to the signal generator API.
 * @param {object} commandData - The command payload (e.g., { command: 'start', frequency: 1000, duty_cycle: 0.5 })
 * @returns {Promise<Response>} A promise that resolves with the fetch Response object.
 */
async function sendSignalCommand(commandData) {
    console.log('Sending command:', commandData);
    try {
        const response = await fetch(API_ENDPOINT, {
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
 */
function startSignal(frequency, dutyCycle) {
    return sendSignalCommand({ command: 'start', frequency: frequency, duty_cycle: dutyCycle });
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