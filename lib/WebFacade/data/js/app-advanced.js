// app-advanced.js - Advanced multi-channel pulse generator UI controller

let logOutput;

// Logging helper
function log(message) {
    const timestamp = new Date().toLocaleTimeString();
    if (logOutput) {
        logOutput.textContent += `[${timestamp}] ${message}\n`;
        logOutput.scrollTop = logOutput.scrollHeight;
    }
    console.log(`[${timestamp}] ${message}`);
}

// Mode switching functions
function setupModeListeners() {
    // Frequency mode switcher
    document.querySelectorAll('input[name="freq-mode"]').forEach(radio => {
        radio.addEventListener('change', (e) => {
            if (e.target.value === 'hz') {
                document.getElementById('freq-hz-input').classList.remove('hidden');
                document.getElementById('period-us-input').classList.add('hidden');
            } else {
                document.getElementById('freq-hz-input').classList.add('hidden');
                document.getElementById('period-us-input').classList.remove('hidden');
            }
        });
    });

    // Pulse mode switcher
    document.querySelectorAll('input[name="pulse-mode"]').forEach(radio => {
        radio.addEventListener('change', (e) => {
            if (e.target.value === 'duty') {
                document.getElementById('duty-pct-input').classList.remove('hidden');
                document.getElementById('width-us-input').classList.add('hidden');
            } else {
                document.getElementById('duty-pct-input').classList.add('hidden');
                document.getElementById('width-us-input').classList.remove('hidden');
            }
        });
    });

    // Duration mode switcher
    document.querySelectorAll('input[name="duration-mode"]').forEach(radio => {
        radio.addEventListener('change', (e) => {
            if (e.target.value === 'time') {
                document.getElementById('duration-sec-input').classList.remove('hidden');
                document.getElementById('count-input').classList.add('hidden');
            } else {
                document.getElementById('duration-sec-input').classList.add('hidden');
                document.getElementById('count-input').classList.remove('hidden');
            }
        });
    });
}

// Tab switching for slave channels
function showChannel(channelId) {
    // Hide all tab contents
    document.querySelectorAll('.tab-content').forEach(content => {
        content.classList.remove('active');
    });
    // Remove active class from all tabs
    document.querySelectorAll('.tab').forEach(tab => {
        tab.classList.remove('active');
    });

    // Show selected channel
    document.getElementById(`channel-${channelId}`).classList.add('active');
    // Highlight selected tab
    event.target.classList.add('active');

    log(`Switched to Channel ${channelId} configuration`);
}

// Start signal with current parameters
function startSignal() {
    const payload = {
        command: 'start',
        channel: 0 // Master channel
    };

    // Get frequency mode
    const freqMode = document.querySelector('input[name="freq-mode"]:checked').value;
    if (freqMode === 'hz') {
        payload.frequency = parseFloat(document.getElementById('frequency').value);
    } else {
        payload.period_us = parseInt(document.getElementById('period').value);
    }

    // Get pulse mode
    const pulseMode = document.querySelector('input[name="pulse-mode"]:checked').value;
    if (pulseMode === 'duty') {
        const dutyPct = parseFloat(document.getElementById('duty-cycle').value);
        payload.duty_cycle = dutyPct / 100.0; // Convert percentage to 0-1
    } else {
        payload.pulse_width_us = parseInt(document.getElementById('pulse-width').value);
    }

    // Get duration mode
    const durationMode = document.querySelector('input[name="duration-mode"]:checked').value;
    if (durationMode === 'time') {
        payload.duration_sec = parseFloat(document.getElementById('duration').value);
    } else {
        payload.pulse_count = parseInt(document.getElementById('pulse-count').value);
    }

    // Get polarity
    payload.polarity = document.getElementById('polarity').value;

    log(`Starting signal with: ${JSON.stringify(payload)}`);

    apiRequest('/api/trigger', 'POST', payload)
        .then(data => {
            log(`Start command successful: ${JSON.stringify(data)}`);
            setTimeout(fetchStatus, 300);
        })
        .catch(error => {
            log(`ERROR starting signal: ${error}`);
        });
}

// Stop signal
function stopSignal() {
    const payload = {
        command: 'stop',
        channel: 0
    };

    log('Stopping signal...');

    apiRequest('/api/trigger', 'POST', payload)
        .then(data => {
            log(`Stop command successful: ${JSON.stringify(data)}`);
            setTimeout(fetchStatus, 300);
        })
        .catch(error => {
            log(`ERROR stopping signal: ${error}`);
        });
}

// Update signal parameters
function updateSignal() {
    const payload = {
        command: 'update',
        channel: 0
    };

    // Get frequency mode
    const freqMode = document.querySelector('input[name="freq-mode"]:checked').value;
    if (freqMode === 'hz') {
        payload.frequency = parseFloat(document.getElementById('frequency').value);
    } else {
        payload.period_us = parseInt(document.getElementById('period').value);
    }

    // Get pulse mode
    const pulseMode = document.querySelector('input[name="pulse-mode"]:checked').value;
    if (pulseMode === 'duty') {
        const dutyPct = parseFloat(document.getElementById('duty-cycle').value);
        payload.duty_cycle = dutyPct / 100.0;
    } else {
        payload.pulse_width_us = parseInt(document.getElementById('pulse-width').value);
    }

    // Get duration mode
    const durationMode = document.querySelector('input[name="duration-mode"]:checked').value;
    if (durationMode === 'time') {
        payload.duration_sec = parseFloat(document.getElementById('duration').value);
    } else {
        payload.pulse_count = parseInt(document.getElementById('pulse-count').value);
    }

    log(`Updating signal parameters: ${JSON.stringify(payload)}`);

    apiRequest('/api/trigger', 'POST', payload)
        .then(data => {
            log(`Update command successful: ${JSON.stringify(data)}`);
            setTimeout(fetchStatus, 300);
        })
        .catch(error => {
            log(`ERROR updating signal: ${error}`);
        });
}

// Configure slave channel
function configureChannel(channelId) {
    const payload = {
        channel: channelId,
        pin: parseInt(document.getElementById(`ch${channelId}-pin`).value),
        phase_offset: parseFloat(document.getElementById(`ch${channelId}-phase`).value),
        enabled: document.getElementById(`ch${channelId}-enabled`).checked
    };

    log(`Configuring Channel ${channelId}: ${JSON.stringify(payload)}`);

    apiRequest('/api/channel', 'POST', payload)
        .then(data => {
            log(`Channel ${channelId} configuration successful: ${JSON.stringify(data)}`);
        })
        .catch(error => {
            log(`ERROR configuring channel ${channelId}: ${error}`);
        });
}

// Set master output pin
function setMasterPin() {
    const pin = parseInt(document.getElementById('master-pin-input').value);
    const payload = { pin: pin };

    log(`Setting master output pin to GPIO ${pin}...`);

    apiRequest('/api/v1/config/output_pin', 'POST', payload)
        .then(data => {
            log(`Master pin set successfully: ${JSON.stringify(data)}`);
            setTimeout(fetchStatus, 300);
        })
        .catch(error => {
            log(`ERROR setting master pin: ${error}`);
        });
}

// Set indicator pin
function setIndicatorPin() {
    const pin = parseInt(document.getElementById('indicator-pin-input').value);
    const payload = { pin: pin };

    log(`Setting indicator pin to GPIO ${pin}...`);

    apiRequest('/api/setindicator', 'POST', payload)
        .then(data => {
            log(`Indicator pin set successfully: ${JSON.stringify(data)}`);
            document.getElementById('indicator-pin').textContent = pin === 0 ? 'Disabled' : `GPIO ${pin}`;
        })
        .catch(error => {
            log(`ERROR setting indicator pin: ${error}`);
        });
}

// Trigger sync for all channels
function triggerSync() {
    log('Triggering multi-channel sync...');

    apiRequest('/api/sync', 'POST', {})
        .then(data => {
            log(`Sync triggered successfully: ${JSON.stringify(data)}`);
        })
        .catch(error => {
            log(`ERROR triggering sync: ${error}`);
        });
}

// Fetch current status from API
function fetchStatus() {
    apiRequest('/api/status', 'GET')
        .then(data => {
            log('Status fetched successfully');
            updateStatusDisplay(data);
        })
        .catch(error => {
            log(`ERROR fetching status: ${error}`);
        });
}

// Update status display
function updateStatusDisplay(status) {
    // Update signal state
    const signalState = document.getElementById('signal-state');
    if (status.is_running) {
        signalState.textContent = 'Running';
        signalState.style.color = '#4CAF50';
    } else {
        signalState.textContent = 'Stopped';
        signalState.style.color = '#666';
    }

    // Update status indicators
    if (status.frequency) {
        document.getElementById('status-frequency').textContent = `${status.frequency.toFixed(2)} Hz`;
    }
    if (status.duty_cycle !== undefined) {
        document.getElementById('status-duty').textContent = `${(status.duty_cycle * 100).toFixed(1)}%`;
    }
    if (status.periodUs) {
        document.getElementById('status-period').textContent = `${status.periodUs} μs`;
    } else if (status.frequency) {
        // Calculate period from frequency
        const periodUs = Math.round(1000000 / status.frequency);
        document.getElementById('status-period').textContent = `${periodUs} μs`;
    }
    if (status.pulseWidthUs) {
        document.getElementById('status-pulse-width').textContent = `${status.pulseWidthUs} μs`;
    } else if (status.frequency && status.duty_cycle) {
        // Calculate pulse width from frequency and duty
        const periodUs = 1000000 / status.frequency;
        const pulseWidthUs = Math.round(periodUs * status.duty_cycle);
        document.getElementById('status-pulse-width').textContent = `${pulseWidthUs} μs`;
    }

    // Update master pin
    document.getElementById('master-pin').textContent = `GPIO ${status.output_pin}`;
    document.getElementById('master-pin-input').value = status.output_pin;

    // Update input fields to match status (for user convenience)
    if (status.frequency) {
        document.getElementById('frequency').value = Math.round(status.frequency);
    }
    if (status.duty_cycle !== undefined) {
        document.getElementById('duty-cycle').value = (status.duty_cycle * 100).toFixed(1);
    }
    if (status.duration_sec !== undefined) {
        document.getElementById('duration').value = status.duration_sec;
    }
}

// WebSocket callbacks
const wsCallbacks = {
    onOpen: () => {
        const wsStatus = document.getElementById('ws-status');
        wsStatus.textContent = 'Connected';
        wsStatus.className = 'connected';
        log('WebSocket connected');
        fetchStatus();
    },
    onClose: () => {
        const wsStatus = document.getElementById('ws-status');
        wsStatus.textContent = 'Disconnected';
        wsStatus.className = 'disconnected';
        log('WebSocket disconnected - attempting reconnect...');
    },
    onError: (error) => {
        log(`WebSocket error: ${error}`);
    },
    onMessage: (event) => {
        try {
            const message = JSON.parse(event.data);
            log(`WS Event: ${message.event || 'unknown'}`);

            // Refresh status on signal events
            if (message.event === 'started' || message.event === 'stopped' || message.event === 'params_changed') {
                fetchStatus();
            }
        } catch (e) {
            log(`Error parsing WebSocket message: ${e}`);
        }
    }
};

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    logOutput = document.getElementById('log-output');
    log('ESP32 Multi-Channel Pulse Generator UI initialized');

    // Setup mode switching listeners
    setupModeListeners();

    // Connect WebSocket
    wsConnect(wsCallbacks);

    // Fetch initial status
    fetchStatus();

    // Optional: Poll status periodically as fallback
    setInterval(fetchStatus, 5000);
});
