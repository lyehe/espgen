# Serial CLI Quick Reference

**Baud Rate**: 115200

## Quick Start

1. Connect via USB serial (115200 baud)
2. Press Enter to see the `>` prompt
3. Type `help` to see all commands
4. Type `status` to see current state

## Command Categories

### Basic Signal Control
```
start                          # Start signal with current/default parameters
stop                           # Stop signal generation
update <freq> <duty>           # Update frequency (Hz) and duty (0.0-1.0)
freq <hz>                      # Set frequency only
duty <0.0-1.0>                 # Set duty cycle only
```

### Exact Parameters (Precision Control)
```
period <us>                    # Set period in microseconds
pulsewidth <us>                # Set pulse width in microseconds
pulsecount <n>                 # Set exact pulse count (0=infinite)
```

### Multi-Channel Configuration
```
channel <id> <pin> <phase> <en>  # Configure slave channel
  - id: 1-5 (slave channels)
  - pin: GPIO pin (0-33)
  - phase: Phase offset in degrees (0-360)
  - en: Enable flag (0=disabled, 1=enabled)

sync                           # Trigger multi-channel sync
```

### Advanced Settings
```
polarity <h|l>                 # Set polarity (h=high, l=low)
setpin <gpio>                  # Set master output pin (0-33)
indicator <gpio>               # Set status indicator pin (0=disabled)
status                         # Show current status
```

## Common Use Cases

### 1. Generate 1kHz, 50% Duty, 10 Second Burst
```
> freq 1000
> duty 0.5
> start
[wait 10 seconds]
> stop
```

### 2. Generate Exact 100 Pulses
```
> freq 1000
> duty 0.5
> pulsecount 100
> start
```

### 3. Quadrature Signals (A/B with 90° offset)
```
> freq 1000
> duty 0.5
> setpin 18
> start
> channel 1 19 90 1
> sync
```

### 4. Three-Phase Signals (120° offset)
```
> freq 60
> duty 0.5
> setpin 18
> start
> channel 1 19 120 1
> channel 2 21 240 1
> sync
```

### 5. Precision Camera Trigger (Active-Low, Exact Timing)
```
> period 50000
> pulsewidth 1000
> pulsecount 100
> polarity l
> indicator 27
> start
```

### 6. Check System Status
```
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
```

## Parameter Ranges

| Parameter | Min | Max | Unit |
|-----------|-----|-----|------|
| Frequency | 1 | 8,000,000 | Hz |
| Period | 25 | 1,000,000,000 | microseconds |
| Duty Cycle | 0.0 | 1.0 | - |
| Pulse Width | 0 | 1,000,000 | microseconds |
| Pulse Count | 0 | 999,999,999 | pulses |
| Phase Offset | 0 | 360 | degrees |
| GPIO Pin | 0 | 33 | - |

**Note**: 0 for pulse count/duration means infinite (continuous).

## Tips

1. **Use exact parameters** for precise timing requirements (period/pulsewidth instead of freq/duty)
2. **Use pulse count** for automated test sequences
3. **Always sync** after configuring multiple channels for best alignment
4. **Set indicator pin** to monitor signal activity with an LED
5. **Check status** frequently when testing to verify configuration

## Error Messages

- `Error: Invalid format` - Command syntax error, check `help`
- `Error: Invalid pin` - GPIO pin out of range (0-33)
- `Error: Channel ID must be 1-5` - Only slave channels can be configured
- `Error: Command queue full` - System busy, retry in a moment
- `Error: Unknown command` - Command not recognized, type `help`

## Examples with Comments

```bash
# Configure high-frequency test signal
> freq 1000000          # 1 MHz
> duty 0.5              # 50% duty cycle
> setpin 18             # Use GPIO 18
> start                 # Begin generation

# Add slave channel with 180° inversion
> channel 1 19 180 1    # Channel 1, pin 19, 180° offset, enabled
> sync                  # Align channels

# Switch to exact timing mode
> period 1000           # 1000 μs = 1 ms = 1 kHz
> pulsewidth 500        # 500 μs pulse width

# Generate exactly 1000 pulses then stop
> pulsecount 1000
> start

# Monitor with indicator LED on GPIO 27
> indicator 27

# Use active-low polarity (for some camera triggers)
> polarity l

# Check everything is configured correctly
> status
```

## Comparison: Frequency vs Period

**Frequency Mode** (easier for most users):
```
> freq 1000             # 1 kHz
> duty 0.5              # 50% duty
```

**Period Mode** (for exact timing):
```
> period 1000           # 1000 μs period
> pulsewidth 500        # 500 μs pulse width
```

Both produce the same 1kHz, 50% duty cycle signal, but period mode gives microsecond-level precision.

## Comparison: Duration vs Pulse Count

**Duration Mode** (time-based):
```
> freq 1000
> start                 # Runs indefinitely
# or
> duration 10           # Runs for 10 seconds (not implemented in CLI yet)
```

**Pulse Count Mode** (exact count):
```
> freq 1000
> pulsecount 10000      # Generates exactly 10,000 pulses then stops
> start
```

For 1kHz, 10,000 pulses = 10 seconds, but pulse count mode guarantees exact count regardless of frequency changes.
