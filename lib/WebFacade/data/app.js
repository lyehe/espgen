// app.js - Main controller for the web UI

document.addEventListener('DOMContentLoaded', () => {
    console.log('DOM fully loaded and parsed');

    // --- Elements ---
    const wsStatus = document.getElementById('ws-status');
    const ipAddress = document.getElementById('ip-address');
    const wifiMode = document.getElementById('wifi-mode');
    const signalRunning = document.getElementById('signal-running');
    const frequencyInput = document.getElementById('frequency');
    const dutySlider = document.getElementById('duty');
    const dutyValueSpan = document.getElementById('duty-value');
    const startButton = document.getElementById('start-button');
    const stopButton = document.getElementById('stop-button');
    const updateButton = document.getElementById('update-button');
    const logOutput = document.getElementById('log-output');

    let currentStatus = {}; // Store last known status

    // --- Logging Helper ---
    function logMessage(message) {
        const timestamp = new Date().toLocaleTimeString();
        logOutput.textContent += `[${timestamp}] ${message}\n`;
        logOutput.scrollTop = logOutput.scrollHeight; // Scroll to bottom
    }

    // --- Update UI ---
    function updateUI(statusData) {
        currentStatus = statusData;
        ipAddress.textContent = statusData.ip_address || 'N/A';
        wifiMode.textContent = statusData.wifi_mode || 'N/A';
        signalRunning.textContent = statusData.is_running ? 'Yes' : 'No';
        frequencyInput.value = statusData.frequency || 1000;
        dutySlider.value = statusData.duty_cycle || 0.5;
        dutyValueSpan.textContent = parseFloat(dutySlider.value).toFixed(2);
        logMessage(`UI updated: Freq=${statusData.frequency}, Duty=${statusData.duty_cycle}, Running=${statusData.is_running}`);
    }

    // --- WebSocket Handling ---
    const wsCallbacks = {
        onOpen: () => {
            wsStatus.textContent = 'Connected';
            wsStatus.style.color = 'green';
            logMessage('WebSocket connected.');
            // Request initial status after WS connects
            fetchStatus(); 
        },
        onClose: () => {
            wsStatus.textContent = 'Disconnected';
            wsStatus.style.color = 'red';
            logMessage('WebSocket disconnected. Attempting reconnect...');
        },
        onError: (error) => {
            wsStatus.textContent = 'Error';
            wsStatus.style.color = 'orange';
            logMessage(`WebSocket error: ${error}`);
        },
        onMessage: (event) => {
            logMessage(`WS Received: ${event.data}`);
            try {
                const message = JSON.parse(event.data);
                // Update UI based on WebSocket event messages (Phase 7)
                 if (message.event === 'started' || message.event === 'params_changed' || message.event === 'stopped') {
                    // Re-fetch full status or update partial based on message content
                    fetchStatus(); // Simple approach: always fetch full status on event
                } else if (message.status === 'connected') {
                    // Initial connection message, maybe fetch status
                    fetchStatus();
                }
            } catch (e) {
                logMessage(`Error parsing WS message: ${e}`);
            }
        }
    };
    // Initialize WebSocket connection (from js/ws.js)
    wsConnect(wsCallbacks);

    // --- REST API Handling (js/api.js) ---
    function fetchStatus() {
        apiRequest('/api/v1/status')
            .then(data => {
                logMessage('Status fetched successfully.');
                updateUI(data);
            })
            .catch(error => {
                logMessage(`Error fetching status: ${error}`);
            });
    }

    function sendCommand(endpoint, body) {
        apiRequest(endpoint, 'POST', body)
            .then(data => {
                logMessage(`Command ${endpoint} successful: ${JSON.stringify(data)}`);
                 // Optionally fetch status again after command 
                 // setTimeout(fetchStatus, 500); // Delay slightly
            })
            .catch(error => {
                logMessage(`Error sending command ${endpoint}: ${error}`);
            });
    }

    // --- Event Listeners ---
    dutySlider.addEventListener('input', () => {
        dutyValueSpan.textContent = parseFloat(dutySlider.value).toFixed(2);
        // Optional: Send update command on slider change (real-time update)
        // sendCommand('/api/v1/trigger/update', { channel: 0, duty_cycle: parseFloat(dutySlider.value) });
    });

    startButton.addEventListener('click', () => {
        const freq = parseInt(frequencyInput.value, 10);
        const duty = parseFloat(dutySlider.value);
        logMessage(`Sending START: Freq=${freq}, Duty=${duty}`);
        sendCommand('/api/v1/trigger/start', { channel: 0, frequency: freq, duty_cycle: duty });
    });

    stopButton.addEventListener('click', () => {
        logMessage('Sending STOP');
        sendCommand('/api/v1/trigger/stop', { channel: 0 });
    });

    updateButton.addEventListener('click', () => {
         const duty = parseFloat(dutySlider.value);
         logMessage(`Sending UPDATE: Duty=${duty}`);
         // Assuming an update command exists, or reuse start?
         // For now, let's simulate using 'start' to update params
         const freq = parseInt(frequencyInput.value, 10); 
         sendCommand('/api/v1/trigger/start', { channel: 0, frequency: freq, duty_cycle: duty }); 
         // Or: sendCommand('/api/v1/trigger/update', { channel: 0, duty_cycle: duty });
    });

    // --- Initial Load ---
    logMessage('UI Initialized.');
    // Initial status fetch might happen via WebSocket onOpen, or uncomment below
    // fetchStatus(); 

    // Fallback polling if WebSocket is down (or as primary method)
    // setInterval(fetchStatus, 5000); // Poll every 5 seconds
}); 