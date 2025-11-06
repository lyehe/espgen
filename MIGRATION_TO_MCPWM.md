# Migration from LEDC to MCPWM-based Pulse Generator

## Overview
This update replaces the LEDC-based PWM driver with a new MCPWM-based multi-channel pulse generator, enabling synchronized trigger signals with phase offset and skip capabilities.

## Master/Slave Architecture

The pulse generator implements a **master/slave architecture**:
- **Master Channel (Channel 0)**: Always enabled, controls the primary output
- **Slave Channels (Channels 1-5)**: Follow the master, can be individually disabled

This architecture ensures:
- Master channel cannot be accidentally disabled
- All enabled slaves are hardware-synchronized to the master
- Individual slave channels can be enabled/disabled without affecting others
- Frequency changes apply to all channels simultaneously

## What Changed

### Removed Components
- **LedcDriver** (lib/SignalEngine/include/LedcDriver.h, lib/SignalEngine/src/LedcDriver.cpp)
  - Single-channel LEDC peripheral implementation
  - Limited to LED PWM use cases

- **RmtDriver** (stub implementation, never completed)
  - Placeholder files removed

- **CmdDispatcher.cpp** (old example code, not used)

### Added Components
- **PulseGenerator** (lib/SignalEngine/include/PulseGenerator.h, lib/SignalEngine/src/PulseGenerator.cpp)
  - Multi-channel pulse generator using ESP32 MCPWM peripheral
  - Supports up to 6 synchronized output channels
  - Hardware-based synchronization for precise phase alignment
  - Configurable phase offset per channel (0-360 degrees)
  - Individual channel enable/disable (skip functionality)
  - Nanosecond-level timing precision

### Modified Components
- **signal_iface.h**
  - Added new command types: `SIG_CMD_CONFIG_CHANNEL`, `SIG_CMD_ENABLE_CHANNEL`, `SIG_CMD_SYNC`
  - Extended `SignalCmd` structure with multi-channel support:
    - `channel` field for channel selection (0-5)
    - `phaseOffset` field for phase configuration
    - `enabled` flag for channel enable/disable

- **SignalEngine.h / SignalEngine.cpp**
  - Replaced `LedcDriver ledcChannel0` with `PulseGenerator pulseGen`
  - Updated command dispatcher to handle new multi-channel commands
  - Backward compatible with existing single-channel API

## Key Features

### Multi-Channel Support (Master/Slave Architecture)

Configure the master and slave channels:

```cpp
// Configure master channel (always enabled)
pulseGen.configureMaster(18);  // Master on GPIO 18

// Configure slave channels with phase offsets
pulseGen.configureSlave(1, 19, 0.0,   true);   // Slave 1: GPIO 19, 0° offset, enabled
pulseGen.configureSlave(2, 21, 90.0,  true);   // Slave 2: GPIO 21, 90° offset, enabled
pulseGen.configureSlave(3, 22, 180.0, true);   // Slave 3: GPIO 22, 180° offset, enabled
pulseGen.configureSlave(4, 23, 270.0, false);  // Slave 4: GPIO 23, 270° offset, DISABLED

// Set global frequency (applies to all channels)
pulseGen.setFrequency(1000);  // 1 kHz

// Set individual duty cycles
pulseGen.setDutyCycle(0, 0.5);  // Master: 50% duty
pulseGen.setDutyCycle(1, 0.25); // Slave 1: 25% duty
pulseGen.setDutyCycle(2, 0.75); // Slave 2: 75% duty

// Start synchronized pulse generation
pulseGen.start();

// Later: Enable/disable slaves without stopping master
pulseGen.enableSlave(4, true);   // Enable slave 4
pulseGen.disableAllSlaves();     // Disable all slaves (master still runs)
pulseGen.enableAllSlaves();      // Re-enable all configured slaves
```

### Hardware Synchronization
All channels start simultaneously with precise phase relationships maintained by the MCPWM hardware.

### New Command API

```cpp
// Configure master channel (channel 0)
SignalCmd setMasterPinCmd = {
    .type = SIG_CMD_SET_PIN,
    .channel = 0,
    .pin = 18
};

// Configure slave channel with phase offset
SignalCmd configSlaveCmd = {
    .type = SIG_CMD_CONFIG_CHANNEL,
    .channel = 1,           // Slave channel 1
    .pin = 19,
    .phaseOffset = 90.0,    // 90° offset from master
    .enabled = true
};

// Enable/disable specific slave channels
SignalCmd enableSlaveCmd = {
    .type = SIG_CMD_ENABLE_CHANNEL,
    .channel = 2,           // Slave channel 2
    .enabled = false        // Disable (skip) this slave
};
// Note: Attempting to disable channel 0 (master) will fail

// Trigger software sync to realign all channels
SignalCmd syncCmd = {
    .type = SIG_CMD_SYNC
};
```

### Master/Slave Channel Control

The master/slave architecture provides clear control semantics:

```cpp
// Master channel (0) methods
pulseGen.configureMaster(gpio_pin);              // Configure master
pulseGen.isMasterChannel(channel_id);            // Check if channel is master
pulseGen.getMasterDutyCycle();                   // Get master duty cycle

// Slave channel (1-5) methods
pulseGen.configureSlave(id, pin, phase, enabled); // Configure slave
pulseGen.enableSlave(id, enabled);                // Enable/disable slave
pulseGen.disableAllSlaves();                      // Disable all slaves
pulseGen.enableAllSlaves();                       // Enable all slaves
pulseGen.getEnabledSlaveCount();                  // Count enabled slaves

// Protection
// - Master (channel 0) cannot be disabled
// - enableChannel(0, false) returns false
// - Master always remains active during pulse generation
```

## Backward Compatibility

The implementation maintains backward compatibility:
- Existing single-channel commands work with channel 0
- `SIG_CMD_START`, `SIG_CMD_STOP`, `SIG_CMD_UPDATE_*` continue to function
- Pin configuration through `SIG_CMD_SET_PIN` still supported for channel 0

## Hardware Requirements

- ESP32 (MCPWM peripheral available on most ESP32 variants)
- GPIO pins for output channels (recommended: 12-33)

## Benefits Over LEDC

1. **True Multi-Channel Sync**: Hardware-level synchronization vs software coordination
2. **Phase Control**: Precise phase offsets for trigger timing
3. **Better Precision**: MCPWM provides better timing accuracy
4. **Flexible Configuration**: Per-channel enable/disable without stopping all outputs
5. **Future Ready**: Foundation for advanced features (burst mode, pattern generation, etc.)

## Migration Path

Existing code using the old single-channel API requires no changes. To use multi-channel features:

1. Configure additional channels using `SIG_CMD_CONFIG_CHANNEL`
2. Set individual duty cycles per channel
3. Use `SIG_CMD_START` to start all enabled channels simultaneously
4. Optionally use `SIG_CMD_ENABLE_CHANNEL` to skip specific channels
5. Use `SIG_CMD_SYNC` to realign phases if needed

## Testing Notes

Since this is a hardware-level change:
- Test on actual ESP32 hardware
- Verify GPIO pin assignments match your board
- Use an oscilloscope to verify phase relationships
- Check timing precision with high-frequency signals

## Future Enhancements

The MCPWM-based architecture enables:
- Dead-time insertion (for motor control)
- Fault detection and protection
- Capture mode (for frequency/duty measurement)
- Carrier wave modulation
- Burst mode and pattern generation
