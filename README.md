# ESP32 PWM Signal Generator (`esp32-trigger`)

This project implements a flexible PWM signal generator on an ESP32 microcontroller, controllable via a web interface, a serial command-line interface (CLI), and a REST API. It features persistent settings, WiFi provisioning, and OTA updates.

## Features

*   **PWM Signal Generation:** Uses the ESP32's LEDC peripheral.
*   **Configurable Parameters:**
    *   Frequency (Hz)
    *   Duty Cycle (0-100%)
    *   Signal Duration (seconds, 0 for infinite)
    *   Start Delay (seconds, web UI only)
    *   Output Pin (GPIO 12-19, persistent)
*   **Control Interfaces:**
    *   **Web UI:** Real-time status display (frequency, duty, ticks, running time, pin), control inputs, configuration, WebSocket updates. Accessible via mDNS (`trigger.local`) or IP address.
    *   **Serial CLI:** Interactive command-line control over a serial connection (115200 baud). Type `help` for commands.
    *   **REST API:** Programmatic control via HTTP requests (endpoints for status, trigger commands, pin configuration).
*   **Connectivity & Setup:**
    *   **WiFi Provisioning:** Uses WiFiManager library to provide a captive portal (`TRIGGER-SETUP`) for easy WiFi network configuration on first boot or connection failure.
    *   **mDNS:** Advertises itself as `trigger.local` on the network.
    *   **OTA Updates:** Allows wireless firmware updates via a web page at `/update` (using AsyncElegantOTA).
*   **Persistence:** Saves the last used frequency, duty cycle, duration, and the configured output pin to Non-Volatile Storage (NVS), restoring them on reboot.
*   **Modular Design:** Separated into libraries (`SignalEngine`, `WebFacade`, `SerialCLI`, `OTABridge`) and applications for easier testing and development.

## Project Structure

```
esp32-trigger/
│
├─ platformio.ini        # PlatformIO project configuration (environments, libs)
├─ common/
│   ├─ signal_iface.h    # Shared command/event structs and definitions
│   ├─ build_opts.h      # Global build flags/options
│   └─ preferences_keys.h # NVS key definitions
│
├─ lib/
│   ├─ SignalEngine/     # Core PWM generation logic, command handling, state
│   ├─ SerialCLI/        # Serial command parsing and interaction
│   ├─ WebFacade/        # Web server, REST API, WebSocket hub, WiFiManager wrapper
│   │   └─ data/         # Root directory for LittleFS web files (HTML, JS, CSS)
│   └─ OTABridge/        # Wrapper for AsyncElegantOTA library
│
├─ apps/
│   ├─ signal_cli/       # Simple app for testing SignalEngine + SerialCLI
│   ├─ web_mock/         # Simple app for testing WebFacade (with mock engine)
│   └─ production_fw/    # Main application integrating all libraries
│
├─ docs/                 # Design documents, Doxygen config, etc.
└─ README.md             # This file
```

## Building and Flashing

This project uses [PlatformIO](https://platformio.org/).

1.  **Install PlatformIO:** Follow the instructions for your IDE (e.g., VSCode with PlatformIO extension).
2.  **Clone Repository:** `git clone <repository-url>`
3.  **Open Project:** Open the cloned folder in VSCode (or your PlatformIO-compatible IDE).
4.  **Build:**
    *   Compile default environment: `pio run`
    *   Compile production firmware: `pio run -e prod`
5.  **Upload Firmware:**
    *   `pio run -e prod -t upload`
6.  **Upload Web Filesystem:**
    *   `pio run -e prod -t uploadfs`
    *   *Note: You must run this command whenever you change files in the `lib/WebFacade/data/` directory.*
7.  **Monitor Serial Output:**
    *   `pio run -e prod -t monitor` (Baud rate: 115200)

*(Replace `-e prod` with other environment names like `-e signal_cli` if needed)*

## Usage

1.  **First Boot / WiFi Setup:**
    *   On the first boot, or if the ESP32 cannot connect to the previously saved WiFi network, it will create an Access Point named `TRIGGER-SETUP`.
    *   Connect to this network with your phone or computer.
    *   A captive portal page should automatically open (or navigate to `192.168.4.1`).
    *   Select your home WiFi network (SSID), enter the password, and save.
    *   The ESP32 will reboot and attempt to connect to your network.
2.  **Web Interface:**
    *   Once connected to your network, access the UI by navigating to `http://trigger.local` or the ESP32's IP address in your web browser.
    *   Use the controls to set frequency, duty cycle, delay, and duration, then click "Start Signal".
    *   Use "Stop Signal" to halt the output.
    *   Use the "Configuration" section to set the desired output pin (12-19) and click "Set Pin". This setting is saved persistently.
    *   Status information is updated via WebSockets.
3.  **Serial CLI:**
    *   Connect to the ESP32 via a USB-to-Serial adapter.
    *   Open a serial monitor (like the PlatformIO monitor) at 115200 baud.
    *   Press Enter to see the `>` prompt.
    *   Type `help` to see available commands. Key commands include:
        *   `start`: Start signal with last used parameters.
        *   `stop`: Stop the signal.
        *   `update <freq> <duty>`: Update frequency (Hz) and duty cycle (0.0-1.0).
        *   `freq <hz>`: Update frequency only.
        *   `duty <0.0-1.0>`: Update duty cycle only.
        *   `setpin <gpio>`: Set the output pin (12-19, saved persistently).
4.  **REST API:**
    *   Use tools like `curl` or Postman to interact with the API endpoints:
        *   `GET /api/status`: Get current signal status and configuration (including output pin).
        *   `POST /api/trigger`: Send commands (e.g., `{"command": "start", "frequency": 1000, "duty_cycle": 0.25, "duration_sec": 5.0}`).
        *   `POST /api/v1/config/output_pin`: Set the output pin (e.g., `{"pin": 18}`).
5.  **OTA Update:**
    *   Navigate to `/update` in your web browser (e.g., `http://trigger.local/update`).
    *   Select the new firmware `.bin` file and click "Update".

## Dependencies

This project relies on several libraries managed by PlatformIO (see `platformio.ini` for specific versions):

*   `ESPAsyncWebServer`: For the asynchronous web server.
*   `AsyncTCP`: Required by ESPAsyncWebServer.
*   `ArduinoJson`: For parsing and generating JSON data (REST API).
*   `WiFiManager`: For the captive portal WiFi configuration.
*   `AsyncElegantOTA`: For web-based OTA updates.
*   `ESP32TimerInterrupt`: (If still used, verify dependency).
*   `Preferences`: For accessing Non-Volatile Storage (NVS).

## License

(Placeholder) Consider adding a license, e.g., MIT License.

```
MIT License

Copyright (c) [year] [copyright holder]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
``` 