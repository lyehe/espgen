# Migration from LEDC to MCPWM-based Pulse Generator

## Overview
This update replaces the LEDC-based PWM driver with a new MCPWM-based multi-channel pulse generator, enabling synchronized trigger signals with phase offset and skip capabilities.

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

### Multi-Channel Support
Configure up to 6 independent pulse outputs:
```cpp
PulseChannelConfig_t ch0_config = {
    .gpio_pin = 18,
    .phase_offset_deg = 0.0,    // No offset
    .enabled = true,
    .skip_count = 0
};

PulseChannelConfig_t ch1_config = {
    .gpio_pin = 19,
    .phase_offset_deg = 90.0,   // 90° phase offset from channel 0
    .enabled = true,
    .skip_count = 0
};

pulseGen.configureChannel(0, ch0_config);
pulseGen.configureChannel(1, ch1_config);
```

### Hardware Synchronization
All channels start simultaneously with precise phase relationships maintained by the MCPWM hardware.

### New Command API
```cpp
// Configure a channel with phase offset
SignalCmd configCmd = {
    .type = SIG_CMD_CONFIG_CHANNEL,
    .channel = 1,
    .pin = 19,
    .phaseOffset = 90.0,
    .enabled = true
};

// Enable/disable specific channels
SignalCmd enableCmd = {
    .type = SIG_CMD_ENABLE_CHANNEL,
    .channel = 2,
    .enabled = false  // Skip this channel
};

// Trigger software sync to realign all channels
SignalCmd syncCmd = {
    .type = SIG_CMD_SYNC
};
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
