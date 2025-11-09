# Frontend-Backend Integration Audit

## Executive Summary

**Date**: 2025-11-09
**Status**: ⚠️ **SIGNIFICANT GAPS IDENTIFIED**

The backend (SignalEngine + PulseGenerator) has extensive capabilities, but only **basic features** are exposed through the API and frontend. The system supports advanced multi-channel pulse generation with exact timing control, but the web interface only provides simple start/stop functionality.

**Coverage**: ~30% of backend features accessible via frontend

---

## Backend Capabilities (Complete Feature List)

### ✅ Core Signal Control
| Feature | Backend Support | API Support | Frontend Support |
|---------|----------------|-------------|------------------|
| Start signal | ✅ Yes | ✅ Yes | ✅ Yes |
| Stop signal | ✅ Yes | ✅ Yes | ✅ Yes |
| Update parameters | ✅ Yes | ✅ Yes | ⚠️ Partial |

### ⚠️ Timing Specifications (Dual Mode)
| Feature | Backend Support | API Support | Frontend Support |
|---------|----------------|-------------|------------------|
| **Frequency (Hz)** | ✅ Yes (1Hz - 8MHz) | ✅ Yes | ✅ Yes |
| **Period (μs)** | ✅ Yes (25μs - 1000s) | ❌ No | ❌ No |
| **Duty Cycle (0-1)** | ✅ Yes | ✅ Yes | ✅ Yes (%) |
| **Pulse Width (μs)** | ✅ Yes | ❌ No | ❌ No |

### ⚠️ Duration Control (Dual Mode)
| Feature | Backend Support | API Support | Frontend Support |
|---------|----------------|-------------|------------------|
| **Duration (seconds)** | ✅ Yes | ✅ Yes | ✅ Yes |
| **Pulse Count (exact)** | ✅ Yes | ❌ No | ❌ No |
| Infinite mode | ✅ Yes (duration=0) | ✅ Yes | ✅ Yes |

### ❌ Multi-Channel Support (6 Channels)
| Feature | Backend Support | API Support | Frontend Support |
|---------|----------------|-------------|------------------|
| Master channel (ch 0) | ✅ Yes (always on) | ⚠️ Implicit only | ❌ No |
| Slave channels (ch 1-5) | ✅ Yes | ❌ No | ❌ No |
| Configure slave | ✅ Yes | ❌ No | ❌ No |
| Enable/disable channel | ✅ Yes | ❌ No | ❌ No |
| Phase offset (degrees) | ✅ Yes (0-360°) | ❌ No | ❌ No |
| Phase delay (μs) | ✅ Yes | ❌ No | ❌ No |
| Get channel status | ✅ Yes | ⚠️ Partial | ❌ No |

### ❌ Advanced Features
| Feature | Backend Support | API Support | Frontend Support |
|---------|----------------|-------------|------------------|
| **Signal polarity** | ✅ Yes (active-high/low) | ❌ No | ❌ No |
| **Status indicator pin** | ✅ Yes | ✅ Yes | ❌ No |
| **Sync trigger** | ✅ Yes | ❌ No | ❌ No |
| Pulse count tracking | ✅ Yes | ⚠️ Via events | ❌ No |

### ✅ Configuration
| Feature | Backend Support | API Support | Frontend Support |
|---------|----------------|-------------|------------------|
| Set output pin | ✅ Yes (GPIO 0-33) | ✅ Yes (12-19) | ✅ Yes (12-19) |
| Get output pin | ✅ Yes | ✅ Yes | ⚠️ Read-only |
| Set indicator pin | ✅ Yes | ✅ Yes | ❌ No |
| Get indicator pin | ✅ Yes | ❌ No | ❌ No |

### ✅ Status & Monitoring
| Feature | Backend Support | API Support | Frontend Support |
|---------|----------------|-------------|------------------|
| Get status | ✅ Yes | ✅ Yes | ✅ Yes |
| Get frequency | ✅ Yes | ✅ Yes | ✅ Yes (display) |
| Get duty cycle | ✅ Yes | ✅ Yes | ✅ Yes (display) |
| Get running state | ✅ Yes | ✅ Yes | ✅ Yes |
| Get duration | ✅ Yes | ✅ Yes | ✅ Yes (display) |
| Get cycle count | ✅ Yes | ⚠️ Via events | ⚠️ Via WebSocket |
| Discovery | ✅ Yes | ✅ Yes | ❌ Not used |

---

## API Endpoints Analysis

### Existing Endpoints

#### ✅ POST /api/trigger
**Coverage**: Basic control only
**Supported commands**:
- `start` - Start with frequency, duty_cycle, duration_sec
- `stop` - Stop signal
- `update` - Update frequency and duty_cycle

**Missing features**:
- ❌ Pulse count mode (instead of duration)
- ❌ Period/pulse width (exact parameters)
- ❌ Phase offset configuration
- ❌ Polarity control
- ❌ Channel specification
- ❌ Multiple channel configuration

#### ✅ GET /api/status
**Coverage**: Basic status only
**Returns**:
- channel (hardcoded to 0)
- frequency, duty_cycle
- is_running, status_text
- duration_sec, output_pin

**Missing**:
- ❌ Multi-channel status
- ❌ Period/pulse width values
- ❌ Phase offset values
- ❌ Polarity setting
- ❌ Pulse count mode info
- ❌ Indicator pin status
- ❌ Enabled channels list

#### ✅ POST /api/v1/config/output_pin
**Coverage**: Complete for master pin
**Working**: Sets master channel output pin (12-19)

#### ✅ POST /api/setindicator
**Coverage**: Complete for indicator
**Working**: Sets status indicator pin

**Missing frontend integration**: ❌ No UI control

#### ✅ GET /api/discovery
**Coverage**: Complete for discovery
**Returns**: device_type, hostname

**Missing frontend integration**: ❌ Not used by UI

### Missing API Endpoints

#### ❌ POST /api/channel (Multi-channel configuration)
**Needed for**:
- Configure slave channels (1-5)
- Set phase offsets
- Enable/disable individual channels

**Suggested payload**:
```json
{
  "channel": 1,
  "pin": 19,
  "phase_offset": 90.0,
  "enabled": true
}
```

#### ❌ POST /api/params (Exact parameters)
**Needed for**:
- Period/pulse width mode
- Pulse count mode
- Polarity control

**Suggested payload**:
```json
{
  "param_mode": "exact",
  "period_us": 10000,
  "pulse_width_us": 5000,
  "pulse_count": 100,
  "polarity": "active_low"
}
```

#### ❌ POST /api/sync
**Needed for**:
- Manual sync trigger for multi-channel alignment

**Suggested payload**:
```json
{
  "command": "sync"
}
```

#### ❌ GET /api/channels
**Needed for**:
- Get all channel configurations
- Get enabled/disabled status
- Get phase offsets

**Suggested response**:
```json
{
  "channels": [
    {"id": 0, "pin": 18, "enabled": true, "phase": 0},
    {"id": 1, "pin": 19, "enabled": true, "phase": 90},
    {"id": 2, "pin": 21, "enabled": false, "phase": 180}
  ]
}
```

---

## Frontend Controls Analysis

### Existing Controls (index.html + api.js)

#### ✅ Basic Controls
- **Frequency input**: 1-100000 Hz (step 10)
- **Duty cycle input**: 0-100% (step 1)
- **Duration input**: 0+ seconds (step 0.1)
- **Start delay input**: 0+ seconds (step 0.1) - ⚠️ NOT IMPLEMENTED IN BACKEND
- **Start button**: Triggers start command
- **Stop button**: Triggers stop command
- **Output pin input**: 12-19 (step 1)
- **Set pin button**: Changes output pin

#### ✅ Status Display
- WebSocket status
- Signal status (running/stopped)
- Frequency display
- Duty cycle display
- Duration display
- Ticks since start (via WebSocket)
- Running time display

### Missing Frontend Controls

#### ❌ Mode Selection
- No radio buttons for: Frequency vs Period mode
- No radio buttons for: Duty Cycle vs Pulse Width mode
- No radio buttons for: Duration vs Pulse Count mode

#### ❌ Exact Parameter Inputs
- No Period (μs) input field
- No Pulse Width (μs) input field
- No Pulse Count input field
- No Phase Offset input fields

#### ❌ Multi-Channel Interface
- No channel selector (0-5)
- No slave channel configuration UI
- No phase offset controls per channel
- No enable/disable checkboxes per channel
- No visual representation of channels
- No multi-channel status display

#### ❌ Advanced Controls
- No polarity selector (active-high/low)
- No indicator pin configuration
- No sync trigger button
- No channel sync status display

---

## Critical Integration Gaps

### 🔴 Priority 1: Multi-Channel Support
**Impact**: HIGH - Core feature completely inaccessible
**Backend**: Fully implemented (6 channels, phase offsets)
**API**: Missing entirely
**Frontend**: Missing entirely

**Required work**:
1. Add API endpoint: `POST /api/channel` for slave configuration
2. Add API endpoint: `GET /api/channels` for status
3. Add frontend: Channel selector UI
4. Add frontend: Phase offset controls
5. Add frontend: Channel enable/disable checkboxes

### 🔴 Priority 2: Exact Parameter Modes
**Impact**: HIGH - Precision control inaccessible
**Backend**: Fully implemented (period, pulse width, pulse count)
**API**: Missing for pulse count, period, pulse width
**Frontend**: Missing entirely

**Required work**:
1. Extend `POST /api/trigger` to support paramMode flags
2. Add support for period_us, pulse_width_us, pulse_count
3. Add frontend: Mode selector radio buttons
4. Add frontend: Conditional input fields based on mode
5. Add unit conversion helpers in frontend

### 🟡 Priority 3: Polarity & Advanced Features
**Impact**: MEDIUM - Nice-to-have for specialized use cases
**Backend**: Fully implemented (polarity, indicator, sync)
**API**: Partial (indicator endpoint exists)
**Frontend**: Missing

**Required work**:
1. Extend `POST /api/trigger` to support polarity field
2. Add `POST /api/sync` endpoint
3. Add frontend: Polarity selector dropdown
4. Add frontend: Indicator pin config UI
5. Add frontend: Sync trigger button

### 🟢 Priority 4: Enhanced Status Display
**Impact**: LOW - Informational improvements
**Backend**: All data available
**API**: Extend `/api/status` response
**Frontend**: Add display elements

**Required work**:
1. Extend `/api/status` to include all parameters
2. Add frontend: Display period, pulse width values
3. Add frontend: Display polarity, indicator pin
4. Add frontend: Display per-channel status

---

## Recommendations

### Phase 1: Essential Features (Recommended for MVP)
1. ✅ Keep existing basic controls (frequency, duty, duration, start/stop)
2. ✅ Keep existing pin configuration
3. ✅ Keep existing status display

### Phase 2: Multi-Channel Support (High Value)
1. **Add multi-channel API** (`POST /api/channel`, `GET /api/channels`)
2. **Add multi-channel frontend UI**:
   - Channel tabs or accordion
   - Phase offset sliders per channel
   - Enable/disable toggles
3. **Estimated effort**: 6-8 hours

### Phase 3: Exact Parameter Modes (Power User Feature)
1. **Extend `/api/trigger` with paramMode support**
2. **Add mode selector UI**:
   - Toggle between Hz/Period
   - Toggle between Duty/PulseWidth
   - Toggle between Duration/PulseCount
3. **Estimated effort**: 4-6 hours

### Phase 4: Advanced Controls (Optional)
1. **Add polarity control** (API + UI)
2. **Add indicator pin UI**
3. **Add sync trigger button**
4. **Estimated effort**: 2-3 hours

---

## Action Items

### Immediate (Fix critical gaps)
- [ ] Implement `POST /api/channel` endpoint
- [ ] Implement `GET /api/channels` endpoint
- [ ] Extend `POST /api/trigger` to support paramMode flags
- [ ] Add pulse_count, period_us, pulse_width_us to API

### Short-term (Add multi-channel UI)
- [ ] Create channel configuration section in HTML
- [ ] Add JavaScript handlers for channel config
- [ ] Add phase offset sliders
- [ ] Add channel status display

### Medium-term (Add mode selectors)
- [ ] Add radio buttons for parameter modes
- [ ] Add conditional input fields
- [ ] Add unit conversion utilities
- [ ] Update API calls to use paramMode

### Long-term (Polish)
- [ ] Add polarity selector
- [ ] Add indicator pin config UI
- [ ] Add sync trigger button
- [ ] Add comprehensive status display
- [ ] Add preset save/load functionality

---

## Summary

**Current state**: Basic single-channel control with frequency/duty/duration
**Backend capability**: Advanced 6-channel synchronized pulse generation
**Gap**: ~70% of features not accessible via frontend

**Recommendation**: Prioritize multi-channel API and UI (Phase 2) to unlock the core value proposition of the MCPWM-based implementation.
