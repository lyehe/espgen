// js/api.js - Thin wrapper for REST API calls using fetch

const API_TIMEOUT_MS = 5000; // 5 second timeout for API requests

/**
 * Makes a request to the ESP32's API.
 * @param {string} endpoint - The API endpoint (e.g., '/api/v1/status').
 * @param {string} method - HTTP method ('GET', 'POST', etc.). Defaults to 'GET'.
 * @param {object|null} body - The JSON body for POST/PUT requests. Defaults to null.
 * @returns {Promise<object>} - A promise that resolves with the JSON response or rejects on error.
 */
async function apiRequest(endpoint, method = 'GET', body = null) {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT_MS);

    const options = {
        method: method,
        signal: controller.signal,
        headers: {
            'Content-Type': 'application/json',
            // Add other headers like Authorization if needed later
        },
    };

    if (body && (method === 'POST' || method === 'PUT')) {
        options.body = JSON.stringify(body);
    }

    try {
        console.log(`API Request: ${method} ${endpoint}`, body ? JSON.stringify(body) : '');
        const response = await fetch(endpoint, options);
        clearTimeout(timeoutId);

        if (!response.ok) {
            // Try to get error message from response body
            let errorMsg = `HTTP error ${response.status}`; 
            try {
                const errorData = await response.json();
                errorMsg = errorData.error || JSON.stringify(errorData);
            } catch (e) {
                // Ignore if response body isn't JSON or empty
            }
            throw new Error(errorMsg);
        }

        // Handle empty response body for status codes like 200 OK on POST
        const contentType = response.headers.get("content-type");
        if (response.status === 200 && contentType && contentType.indexOf("application/json") !== -1) {
             return await response.json();
        } else if (response.status === 200) {
            // If 200 OK but no JSON body, return a success indicator
            return { status: 'success' }; // Or just resolve()
        } else {
             // Should have been caught by !response.ok, but as a fallback
            return { status: `unexpected_${response.status}` };
        }

    } catch (error) {
        clearTimeout(timeoutId);
        if (error.name === 'AbortError') {
            console.error(`API Request timed out: ${method} ${endpoint}`);
            throw new Error('Request timed out');
        } else {
            console.error(`API Request failed: ${method} ${endpoint}`, error);
            throw error; // Re-throw the original error (could be network error or error thrown above)
        }
    }
} 