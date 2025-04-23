// Functions for WebSocket communication - Add later
console.log("ws.js loaded");

let ws = null;
let wsConnected = false;
let onWsMessageCallback = null; // Function to call when a message is received

function connectWebSocket(onMessage) {
    onWsMessageCallback = onMessage; // Store the callback function

    // Use window.location.host to dynamically get the host
    const wsHost = window.location.host;
    const wsUrl = `ws://${wsHost}/ws`;
    console.log(`Attempting to connect WebSocket to: ${wsUrl}`);

    ws = new WebSocket(wsUrl);

    ws.onopen = function(event) {
        console.log("WebSocket connection opened");
        wsConnected = true;
        // You could update a status indicator in the UI here
        if (typeof updateConnectionStatus === 'function') {
             updateConnectionStatus(true);
        }
        // Optional: Send a ping or initial message if needed
        // ws.send("Hello Server!");
    };

    ws.onmessage = function(event) {
        console.log("WebSocket message received:", event.data);
        if (onWsMessageCallback) {
            try {
                const messageData = JSON.parse(event.data);
                onWsMessageCallback(messageData); // Pass parsed data to the callback
            } catch (e) {
                console.error("Failed to parse WebSocket message JSON:", e);
            }
        }
    };

    ws.onerror = function(event) {
        console.error("WebSocket error observed:", event);
        wsConnected = false;
         if (typeof updateConnectionStatus === 'function') {
             updateConnectionStatus(false);
        }
    };

    ws.onclose = function(event) {
        console.log("WebSocket connection closed:", event.code, event.reason);
        wsConnected = false;
        if (typeof updateConnectionStatus === 'function') {
             updateConnectionStatus(false);
        }
        // Optional: Implement automatic reconnection logic here
        // setTimeout(connectWebSocket, 5000); // Try to reconnect after 5 seconds
    };
}

// Optional: Function to send data (if needed later)
// function sendWebSocketMessage(message) {
//     if (ws && wsConnected) {
//         ws.send(JSON.stringify(message));
//         console.log("WebSocket message sent:", message);
//     } else {
//         console.error("WebSocket not connected. Cannot send message.");
//     }
// } 