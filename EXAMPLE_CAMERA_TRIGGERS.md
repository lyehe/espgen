# Example Use Case: Camera Trigger System

This example demonstrates how to use the master/slave pulse generator for a multi-camera synchronized trigger system.

## Scenario

You have 4 cameras that need synchronized triggers with different timing offsets:
- **Camera 0** (Master): Main camera, triggers first
- **Camera 1**: Triggers 10ms after master (captures motion blur)
- **Camera 2**: Triggers 20ms after master (captures different angle)
- **Camera 3**: Optional backup camera, can be disabled

## Hardware Setup

```
ESP32 GPIO Pins:
- GPIO 18 → Camera 0 trigger (Master)
- GPIO 19 → Camera 1 trigger (Slave, 10ms offset)
- GPIO 21 → Camera 2 trigger (Slave, 20ms offset)
- GPIO 22 → Camera 3 trigger (Slave, can be disabled)
```

## Code Implementation

```cpp
#include "SignalEngine.h"

SignalEngine engine;

void setup() {
    Serial.begin(115200);

    // Initialize the signal engine
    engine.begin();

    // Configure master camera trigger (Camera 0)
    // Master is always active and cannot be disabled
    SignalCmd setMasterCmd = {
        .type = SIG_CMD_SET_PIN,
        .channel = 0,
        .pin = 18
    };
    engine.sendCommand(setMasterCmd);

    // Configure Camera 1 with 10ms delay
    // At 100 Hz (10ms period), 10ms = 360° phase offset
    SignalCmd configCam1Cmd = {
        .type = SIG_CMD_CONFIG_CHANNEL,
        .channel = 1,
        .pin = 19,
        .phaseOffset = 360.0,  // Full cycle = 10ms delay
        .enabled = true
    };
    engine.sendCommand(configCam1Cmd);

    // Configure Camera 2 with 20ms delay
    // At 100 Hz, 20ms = 720° (2 full cycles)
    SignalCmd configCam2Cmd = {
        .type = SIG_CMD_CONFIG_CHANNEL,
        .channel = 2,
        .pin = 21,
        .phaseOffset = 720.0,  // 2 full cycles = 20ms delay
        .enabled = true
    };
    engine.sendCommand(configCam2Cmd);

    // Configure Camera 3 (backup, initially disabled)
    SignalCmd configCam3Cmd = {
        .type = SIG_CMD_CONFIG_CHANNEL,
        .channel = 3,
        .pin = 22,
        .phaseOffset = 0.0,     // Same timing as master
        .enabled = false        // Start disabled
    };
    engine.sendCommand(configCam3Cmd);

    // Start synchronized triggering at 100 Hz
    // Trigger pulse: 1ms width (10% duty cycle)
    SignalCmd startCmd = {
        .type = SIG_CMD_START,
        .channel = 0,
        .frequencyHz = 100.0,   // 100 triggers per second
        .dutyCycle = 0.1,       // 1ms pulse (10% of 10ms period)
        .durationSec = 0.0      // Continuous (infinite duration)
    };
    engine.sendCommand(startCmd);

    Serial.println("Camera trigger system started!");
    Serial.println("- Camera 0 (Master): Active on GPIO 18");
    Serial.println("- Camera 1: Active on GPIO 19 (10ms offset)");
    Serial.println("- Camera 2: Active on GPIO 21 (20ms offset)");
    Serial.println("- Camera 3: Disabled (can be enabled via command)");
}

void loop() {
    // Process engine events
    engine.loop();

    // Example: Enable backup camera after 10 seconds
    static unsigned long lastTime = 0;
    static bool backupEnabled = false;

    if (!backupEnabled && millis() - lastTime > 10000) {
        Serial.println("Enabling backup camera (Camera 3)...");

        SignalCmd enableCam3Cmd = {
            .type = SIG_CMD_ENABLE_CHANNEL,
            .channel = 3,
            .enabled = true
        };
        engine.sendCommand(enableCam3Cmd);

        backupEnabled = true;
    }

    delay(10);
}
```

## Serial Commands

You can also control the cameras via serial CLI:

```bash
# Change trigger frequency to 60 Hz
freq 60

# Update trigger pulse width to 2ms (at 60Hz = 16.67ms period, 2ms ≈ 12% duty)
duty 0.12

# Disable Camera 1 (slave channel 1)
# (Master Camera 0 cannot be disabled)
# Use web API or add CLI command for this

# Stop all triggers
stop

# Resume triggers
start
```

## Timing Diagram

```
Time:     0ms    10ms   20ms   30ms   40ms   ...
          |      |      |      |      |
Camera 0: █      █      █      █      █      (Master - always active)
Camera 1:        █      █      █      █      (10ms offset)
Camera 2:               █      █      █      (20ms offset)
Camera 3: (disabled, or same timing as Camera 0 when enabled)

█ = 1ms trigger pulse (10% duty cycle at 100 Hz)
```

## Key Advantages of Master/Slave Architecture

1. **Master Protection**: Camera 0 (master) cannot be accidentally disabled
2. **Flexible Slave Control**: Cameras 1-3 can be individually enabled/disabled
3. **Hardware Sync**: All active cameras maintain precise timing relationships
4. **Easy Reconfiguration**: Change offsets or enable/disable slaves without stopping the master

## Extending the Example

### Change Trigger Rate Dynamically

```cpp
SignalCmd changeFreqCmd = {
    .type = SIG_CMD_UPDATE_FREQ,
    .frequencyHz = 30.0  // Slow motion: 30 fps
};
engine.sendCommand(changeFreqCmd);
```

### Disable All Slave Cameras (Master Continues)

```cpp
// Useful for testing or single-camera mode
// Note: This would require adding a command type or calling directly:
engine.pulseGen.disableAllSlaves();
// Master (Camera 0) continues triggering alone
```

### Change Phase Offset at Runtime

```cpp
// Reconfigure Camera 1 with different offset
SignalCmd reconfigCam1Cmd = {
    .type = SIG_CMD_CONFIG_CHANNEL,
    .channel = 1,
    .pin = 19,
    .phaseOffset = 180.0,  // Now triggers at 5ms offset (half period at 100Hz)
    .enabled = true
};
engine.sendCommand(reconfigCam1Cmd);
```

## Hardware Verification

Use an oscilloscope to verify:
1. All enabled cameras trigger with correct phase relationships
2. Trigger pulses are precise (1ms width at 100 Hz)
3. Frequency changes affect all channels simultaneously
4. Disabling slaves doesn't affect master timing
5. Phase offsets are accurate (10ms, 20ms delays)

## Real-World Applications

This pattern works for:
- **Multi-camera systems** (as shown above)
- **Multi-flash photography** (staggered flash timing)
- **Stepper motor control** (multiple axes with coordinated timing)
- **Data acquisition** (synchronized sensor sampling)
- **Test equipment** (multi-channel pulse generation)
- **Industrial automation** (coordinated trigger signals)
