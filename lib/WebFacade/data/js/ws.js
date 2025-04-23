// js/ws.js - WebSocket connection management with auto-reconnect

let websocket = null;
let reconnectInterval = 1000; // Start with 1 second
const maxReconnectInterval = 30000; // Max 30 seconds
let reconnectTimer = null;
let wsCallbacks = {}; // Store callbacks provided by app.js

function getWebSocketURL() {
    // Construct WebSocket URL (ws:// or wss://)
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.hostname;
    const port = window.location.port ? `:${window.location.port}` : ''; // Include port if specified
    return `${protocol}//${host}${port}/ws`; // Standard WebSocket endpoint
}

function connect() {
    const wsURL = getWebSocketURL();
    console.log(`Attempting to connect WebSocket to ${wsURL}...`);
    websocket = new WebSocket(wsURL);

    websocket.onopen = () => {
        console.log('WebSocket connection established.');
        reconnectInterval = 1000; // Reset reconnect interval on success
        if (wsCallbacks.onOpen) wsCallbacks.onOpen();
        clearTimeout(reconnectTimer); // Clear any pending reconnect timer
    };

    websocket.onclose = (event) => {
        console.log(`WebSocket connection closed. Code: ${event.code}, Reason: ${event.reason}`);
        if (wsCallbacks.onClose) wsCallbacks.onClose();
        // Schedule reconnect attempt with exponential backoff
        scheduleReconnect();
    };

    websocket.onerror = (error) => {
        console.error('WebSocket error:', error);
        if (wsCallbacks.onError) wsCallbacks.onError(error);
        // Error often precedes close, reconnect is handled in onclose
    };

    websocket.onmessage = (event) => {
        // Pass message handling to the main application logic
        if (wsCallbacks.onMessage) wsCallbacks.onMessage(event);
    };
}

function scheduleReconnect() {
    if (reconnectTimer) clearTimeout(reconnectTimer); // Clear existing timer

    console.log(`Scheduling WebSocket reconnect in ${reconnectInterval / 1000} seconds...`);
    reconnectTimer = setTimeout(() => {
        // Increase interval for next time, up to max
        reconnectInterval = Math.min(reconnectInterval * 2, maxReconnectInterval);
        connect(); // Attempt to reconnect
    }, reconnectInterval);
}

// Public function for app.js to start the connection process
function wsConnect(callbacks) {
    wsCallbacks = callbacks;
    connect(); // Initial connection attempt
}

// Optional: Function to send data through WebSocket (if needed)
// function wsSend(data) {
//     if (websocket && websocket.readyState === WebSocket.OPEN) {
//         websocket.send(JSON.stringify(data));
//     } else {
//         console.error('WebSocket is not open. Cannot send data.');
//     }
// } 