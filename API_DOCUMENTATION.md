# ESP32 Multi-Channel Pulse Generator - API Documentation

**Version**: 2.0
**Date**: 2025-11-09

## Overview

This document describes the complete REST API and Serial CLI for the ESP32 Multi-Channel Pulse Generator. The system supports:

- **6 synchronized channels** (1 master + 5 slaves)
- **Exact parameter control** (period, pulse width, pulse count)
- **Phase-offset control** for multi-channel synchronization
- **Polarity control** (active-high/active-low)
- **Dual interface**: REST API + Serial CLI

---

## REST API Endpoints

### Base URL
```
http://<device-ip>/api
```
or
```
http://trigger.local/api
```

---

## 1. Signal Control

### POST /api/trigger

Control signal generation with comprehensive parameter support.

**Request Body:**
```json
{
  "command": "start|stop|update",
  "channel": 0,

  // Frequency OR Period (choose one)
  "frequency": 1000,        // Hz (1 - 8000000)
  "period_us": 1000,        // microseconds (alternative to frequency)

  // Duty Cycle OR Pulse Width (choose one)
  "duty_cycle": 0.5,        // 0.0 - 1.0
  "pulse_width_us": 500,    // microseconds (alternative to duty_cycle)

  // Duration OR Pulse Count (choose one)
  "duration_sec": 5.0,      // seconds (0 = infinite)
  "pulse_count": 10000,     // exact pulse count (alternative to duration)

  // Advanced
  "polarity": "active_high" // or "active_low"
}
```

**Examples:**

1. **Start with Frequency and Duty Cycle:**
```json
{
  "command": "start",
  "frequency": 1000,
  "duty_cycle": 0.5,
  "duration_sec": 10
}
```

2. **Start with Exact Period and Pulse Width:**
```json
{
  "command": "start",
  "period_us": 10000,
  "pulse_width_us": 5000,
  "pulse_count": 1000
}
```

3. **Start with Active-Low Polarity:**
```json
{
  "command": "start",
  "frequency": 2000,
  "duty_cycle": 0.25,
  "polarity": "active_low",
  "duration_sec": 0
}
```

4. **Stop Signal:**
```json
{
  "command": "stop"
}
```

5. **Update Parameters (while running):**
```json
{
  "command": "update",
  "frequency": 5000,
  "duty_cycle": 0.75
}
```

**Response:**
```json
{
  "status": "queued"
}
```

---

## 2. Multi-Channel Control

### POST /api/channel

Configure slave channels (1-5) with phase offset control.

**Request Body:**
```json
{
  "channel": 1,           // Slave channel ID (1-5)
  "pin": 19,              // GPIO pin (0-33)
  "phase_offset": 90.0,   // Phase offset in degrees (0-360)
  "enabled": true         // Enable/disable channel
}
```

**Examples:**

1. **Configure Channel 1 with 90° Phase Offset:**
```json
{
  "channel": 1,
  "pin": 19,
  "phase_offset": 90,
  "enabled": true
}
```

2. **Configure Channel 2 with Time Delay:**
```json
{
  "channel": 2,
  "pin": 21,
  "phase_delay_us": 2500,
  "enabled": true
}
```

3. **Disable Channel 3:**
```json
{
  "channel": 3,
  "enabled": false
}
```

**Response:**
```json
{
  "status": "channel config queued"
}
```

---

### GET /api/channels

Get configuration and status of all channels.

**Response:**
```json
{
  "channels": [
    {
      "id": 0,
      "type": "master",
      "pin": 18,
      "enabled": true,
      "phase_offset": 0
    },
    {
      "id": 1,
      "type": "slave",
      "pin": 19,
      "enabled": true,
      "phase_offset": 90
    },
    {
      "id": 2,
      "type": "slave",
      "pin": 21,
      "enabled": true,
      "phase_offset": 180
    },
    {
      "id": 3,
      "type": "slave",
      "pin": 22,
      "enabled": false,
      "phase_offset": 0
    }
  ]
}
```

---

### POST /api/sync

Trigger manual synchronization of all channels.

**Request:** Empty body or `{}`

**Response:**
```json
{
  "status": "sync triggered"
}
```

---

## 3. Status and Configuration

### GET /api/status

Get current signal status.

**Response:**
```json
{
  "channel": 0,
  "frequency": 1000,
  "duty_cycle": 0.5,
  "is_running": true,
  "duration_sec": 10.0,
  "output_pin": 18,
  "status_text": "running"
}
```

---

### POST /api/v1/config/output_pin

Set the master channel output pin.

**Request:**
```json
{
  "pin": 18
}
```

**Response:**
```json
{
  "status": "output pin update queued"
}
```

---

### POST /api/setindicator

Set the status indicator pin.

**Request:**
```json
{
  "pin": 27
}
```
Use `"pin": 0` to disable the indicator.

**Response:**
```json
{
  "status": "indicator pin update queued"
}
```

---

### GET /api/discovery

Device discovery and identification.

**Response:**
```json
{
  "status": "ok",
  "device_type": "ESP32_Signal_Generator",
  "hostname": "trigger"
}
```

---

## Serial CLI Commands

Connect via USB serial at **115200 baud**.

### Basic Control

| Command | Syntax | Description | Example |
|---------|--------|-------------|---------|
| **start** | `start` | Start signal with last parameters | `start` |
| **stop** | `stop` | Stop signal generation | `stop` |
| **update** | `update <freq> <duty>` | Update frequency (Hz) and duty (0.0-1.0) | `update 2000 0.5` |
| **freq** | `freq <hz>` | Set frequency only | `freq 5000` |
| **duty** | `duty <0.0-1.0>` | Set duty cycle only | `duty 0.75` |

### Exact Parameters

| Command | Syntax | Description | Example |
|---------|--------|-------------|---------|
| **period** | `period <us>` | Set period in microseconds | `period 10000` |
| **pulsewidth** | `pulsewidth <us>` | Set pulse width in microseconds | `pulsewidth 5000` |
| **pulsecount** | `pulsecount <n>` | Set exact pulse count (0=infinite) | `pulsecount 1000` |

### Multi-Channel

| Command | Syntax | Description | Example |
|---------|--------|-------------|---------|
| **channel** | `channel <id> <pin> <phase> <en>` | Configure slave channel (1-5) | `channel 1 19 90 1` |
| **sync** | `sync` | Trigger multi-channel synchronization | `sync` |

**Channel Command Parameters:**
- `id`: Channel ID (1-5)
- `pin`: GPIO pin (0-33)
- `phase`: Phase offset in degrees (0-360)
- `en`: Enable flag (0=disabled, 1=enabled)

### Advanced

| Command | Syntax | Description | Example |
|---------|--------|-------------|---------|
| **polarity** | `polarity <h\|l>` | Set polarity (h=high, l=low) | `polarity l` |
| **setpin** | `setpin <gpio>` | Set master output pin | `setpin 18` |
| **indicator** | `indicator <gpio>` | Set status indicator pin (0=disabled) | `indicator 27` |
| **status** | `status` | Show current status | `status` |
| **help** | `help` | Show all available commands | `help` |

---

## Serial CLI Example Session

```
> help
Available Commands:
Basic Control:
  start              - Start signal generation (uses last/default params)
  stop               - Stop signal generation
  ...

> freq 1000
Setting frequency to 1000 Hz
OK: Command sent to engine.

> duty 0.5
Setting duty cycle to 0.50
OK: Command sent to engine.

> start
OK: Command sent to engine.

> channel 1 19 90 1
Configuring channel 1: pin=19, phase=90 deg, enabled=yes
OK: Command sent to engine.

> sync
Triggering multi-channel synchronization
OK: Command sent to engine.

> status
=== Current Status ===
  Running: YES
  Frequency: 1000.00 Hz
  Duty Cycle: 50.00%
  Period: 1000 us
  Pulse Width: 500 us
  Duration: 0.00 s
  Output Pin: 18
======================

> stop
OK: Command sent to engine.
```

---

## Use Case Examples

### Example 1: Quadrature Encoder Simulation

Generate two channels with 90° phase offset for quadrature signals:

**REST API:**
```bash
# Configure master channel
curl -X POST http://trigger.local/api/trigger \
  -H "Content-Type: application/json" \
  -d '{"command":"start","frequency":1000,"duty_cycle":0.5,"duration_sec":0}'

# Configure Channel 1 with 90° offset
curl -X POST http://trigger.local/api/channel \
  -H "Content-Type: application/json" \
  -d '{"channel":1,"pin":19,"phase_offset":90,"enabled":true}'
```

**Serial CLI:**
```
> freq 1000
> duty 0.5
> start
> channel 1 19 90 1
> sync
```

---

### Example 2: Three-Phase Power Simulation

Generate three channels with 120° phase offsets:

```bash
# Start master at 60 Hz
curl -X POST http://trigger.local/api/trigger \
  -d '{"command":"start","frequency":60,"duty_cycle":0.5,"duration_sec":0}'

# Configure Channel 1 at 120°
curl -X POST http://trigger.local/api/channel \
  -d '{"channel":1,"pin":19,"phase_offset":120,"enabled":true}'

# Configure Channel 2 at 240°
curl -X POST http://trigger.local/api/channel \
  -d '{"channel":2,"pin":21,"phase_offset":240,"enabled":true}'
```

---

### Example 3: Precision Camera Trigger

Generate exact 100 pulses with specific timing:

**REST API:**
```bash
curl -X POST http://trigger.local/api/trigger \
  -d '{
    "command":"start",
    "period_us":50000,
    "pulse_width_us":1000,
    "pulse_count":100,
    "polarity":"active_low"
  }'
```

**Serial CLI:**
```
> period 50000
> pulsewidth 1000
> pulsecount 100
> polarity l
> start
```

---

## Error Responses

All endpoints may return the following errors:

**400 Bad Request:**
```json
{
  "error": "Missing or invalid 'command' field"
}
```

**503 Service Unavailable:**
```json
{
  "error": "Command queue full"
}
```

---

## Notes

1. **Parameter Modes**: When using exact parameters (period, pulse_width, pulse_count), do not mix with their alternatives (frequency, duty_cycle, duration_sec) in the same request.

2. **Master Channel**: Channel 0 is the master and controls the frequency for all enabled slave channels. It cannot be disabled.

3. **Phase Offset**: Slave channels can specify phase offset either in degrees (0-360) or as a time delay in microseconds.

4. **Pulse Count**: When pulse_count is specified, the signal will stop automatically after generating the exact number of pulses.

5. **WebSocket**: The device also provides WebSocket updates at `ws://<device-ip>/ws` for real-time status events.

---

## Changelog

**Version 2.0** (2025-11-09):
- Added multi-channel support (6 channels with phase control)
- Added exact parameter modes (period, pulse width, pulse count)
- Added polarity control (active-high/active-low)
- Added sync trigger for multi-channel alignment
- Enhanced Serial CLI with all advanced features
- Updated frontend UI with comprehensive controls

**Version 1.0** (Original):
- Basic single-channel control
- Frequency and duty cycle control
- REST API and Serial CLI
- Web UI
