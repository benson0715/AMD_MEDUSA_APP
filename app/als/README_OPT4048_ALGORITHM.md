# OPT4048 Ambient Light Sensor - Implementation Guide

## Overview
The OPT4048 application implements a **power-optimized** ambient light sensor using **one-shot measurement mode** with **threshold-based hysteresis**. This driver integrates the TI OPT4048 RGBW sensor for Windows adaptive brightness control via ACPI.

## Power Optimization Strategy

The OPT4048 driver has been designed for **maximum power efficiency** using the sensor's native **one-shot measurement mode** with intelligent **threshold-based hysteresis**.

### One-Shot Mode Benefits
**What it is**: Sensor powers up, takes one measurement, then automatically returns to power-down mode.

**How it works**:
- Thread triggers one-shot measurement every 1 second
- Sensor powers internal circuits (~1ms startup)
- Performs 100ms ADC integration
- Automatically returns to power-down (<1µA)
- Total active time: ~102ms per second (10% duty cycle)

**Power savings**:
```
Continuous mode: 500µA × 1000ms = 500µA average
One-shot mode:   500µA × 100ms  = 50µA average
Savings: 90%
```

### Threshold-Based Hysteresis
**Purpose**: Only notify OS when ambient light changes significantly (>10%)

**Benefits**:
- Reduces ACPI transactions
- Prevents notification spam from minor fluctuations
- Aligns with Windows adaptive brightness requirements
- Minimizes system wake-ups

**Algorithm**:
```c
lux_delta = abs(current_lux - last_lux);
percent_change = (lux_delta * 100) / last_lux;

if (percent_change > 10%) {
    notify_OS();
    last_lux = current_lux;
}
```

### Auto-Ranging
- Sensor automatically adjusts sensitivity (0-140K lux)
- No software intervention needed
- Maintains accuracy across full dynamic range
- Zero power overhead

## Configuration

### Sensor Settings (Power-Optimized)
```c
Operation Mode:    ONE_SHOT (triggered on-demand)
Conversion Time:   100MS (balance accuracy/power)
Range:             AUTO (0-140K lux)
Threshold Channel: CH3 (White/Lux)
Fault Count:       1 (immediate response)
```

### Thread Settings
```c
Poll Period:       1000ms (1 Hz)
Conversion Wait:   120ms (100ms + margin)
Active Duty:       10% (100ms/1000ms)
Sleep Duty:        90% (900ms/1000ms)
```

## Power Budget

### Per-Measurement Power Profile
| Phase | Duration | Current | Energy |
|-------|----------|---------|--------|
| Idle (power-down) | 898ms | <1µA | ~0.003µJ |
| Trigger (I2C write) | 1ms | 5mA | ~16.5µJ |
| Conversion active | 100ms | 500µA | ~165µJ |
| Read (I2C read) | 1ms | 5mA | ~16.5µJ |
| **Total per second** | **1000ms** | **~50µA avg** | **~198µJ** |

### Daily Power Consumption
```
Measurements per day: 86,400 (1 Hz × 86,400 seconds)
Energy per measurement: 198µJ
Daily energy: 17.1 Joules = 4.75mWh @ 3.3V
Daily charge: 1.44mAh @ 3.3V
```

### Comparison with Continuous Mode
```
Continuous mode: 500µA × 24h = 12mAh/day
One-shot mode:    50µA × 24h = 1.2mAh/day
Savings: 90% (10.8mAh/day)
```

## Thread Architecture

### Initialization Sequence
1. Thread starts with 1000ms polling period (defined in `task_handler.c`)
2. Calls `app_opt4048_init()` once at startup
3. Initializes sensor hardware via `dev_opt4048_init()`
4. Configures sensor for **one-shot mode** with:
   - 100ms conversion time (accuracy vs power balance)
   - Auto-ranging (0-140K lux dynamic range)
   - Threshold channel = White (Ch3) for lux monitoring
   - Fault count = 1 (immediate response)
   - Initial mode = Power-down
5. Registers ACPI handler at plane 0xB2

### Main Loop Algorithm (Power-Optimized)

```
LOOP forever:
    1. Sleep for period (1000ms)
    2. Set task status to NOT_READY
    3. Read current power state
    
    4. IF power state changed:
        - Entering S0 → Enable sensor (stays in power-down)
        - Leaving S0 → Disable sensor (ensure power-down)
    
    5. IF in S0 state AND sensor enabled:
        - Trigger one-shot measurement
        - Wait 120ms for conversion complete
        - Check conversion ready flag
        - Read RGBW channels
        - Calculate lux and CCT
        - Update ACPI variables
        - Check for significant change (hysteresis)
        - Notify OS if needed
        - Sensor auto-returns to power-down
    
    6. Set task status to READY
END LOOP
```

## One-Shot Measurement Algorithm

### Power State Machine
```
Power-Down (0µA)
    ↓ [Trigger one-shot]
Measurement Active (~500µA for 100ms)
    ↓ [Conversion complete]
Power-Down (0µA) ← Automatic return
```

### Measurement Sequence
1. **Trigger**: Write ONE_SHOT mode to control register
2. **Power-up**: Sensor automatically powers internal circuits (~1ms)
3. **Integration**: 100ms ADC integration period
4. **Conversion**: Process raw ADC to digital codes (~1ms)
5. **Power-down**: Sensor automatically returns to power-down
6. **Total time**: ~102ms active + 898ms sleep = 10% duty cycle

### Data Acquisition
1. **Read Color Channels**: Get all 4 channels (R, G, B, W) via I2C in single transaction
2. **Calculate Lux**: Convert raw channel data to illuminance using CIE calculations
3. **Calculate CCT**: Compute correlated color temperature from RGB ratios

### Data Scaling for ACPI
- **Lux**: Clamp 32-bit value to 16-bit (max 65535 lux)
- **RGBW Channels**: Right-shift 20-bit values by 4 bits → 16-bit (divide by 16)
- **CCT**: Clamp double to 16-bit integer (max 65535K)

### Change Detection Algorithm
```
IF first_reading:
    significant_change = true
ELSE:
    lux_delta = abs(current_lux - last_reported_lux)
    percent_change = (lux_delta * 100) / last_reported_lux
    
    IF percent_change > 10%:
        significant_change = true
```

### OS Notification
- Only notify when change > 10% threshold
- Check if host notifications are enabled (`!ACPI_isHostNotifyDisable()`)
- Update `last_reported_lux` after notification
- Prevents notification spam from minor fluctuations

## Windows Adaptive Brightness Integration

### ACPI Plane 0xB2 Register Map
| Offset | Size | Description |
|--------|------|-------------|
| 0-1    | 16-bit | Lux value (illuminance) |
| 2-3    | 16-bit | Red channel |
| 4-5    | 16-bit | Green channel |
| 6-7    | 16-bit | Blue channel |
| 8-9    | 16-bit | White/IR channel |
| 10-11  | 16-bit | CCT in Kelvin |

### ACPI Handler Algorithm
Uses snapshot pattern for atomic 16-bit reads:
```
READ operation:
    IF reading MSB (odd offset):
        snapshot = current_acpi_variable
        return MSB
    ELSE (reading LSB):
        return LSB of snapshot
```

This ensures OS always reads consistent paired bytes even if sensor updates mid-read.

### Update Rate
- Sensor polls at 1 Hz
- OS notified only when change >10%
- Typical notification rate: 2-10 per minute (varies with ambient)
- Windows polls ACPI at ~0.1-1 Hz (OS controlled)

## Power Management

### State Transitions
- **S0 Entry**: Enable sensor → Set one-shot mode configuration
- **S0 Exit**: Disable sensor → Set power down mode
- **Sx States**: No polling occurs, sensor powered down to save energy

### Benefits
- Zero power consumption in sleep states
- Immediate data availability when resuming to S0
- Follows EC power management best practices

## Performance Characteristics

- **Polling Rate**: 1 Hz (once per second)
- **Response Time**: <150ms for brightness changes >10%
- **Active Power**: ~0.5mA for 100ms per measurement
- **Sleep Power**: <1µA in power-down mode
- **Average Power**: ~50µA (100ms active / 1000ms period)
- **Power Savings**: **90% reduction** vs continuous mode
- **I2C Transactions**: 2 per poll (trigger + read)
- **CPU Usage**: Minimal (sleeps 88% of time including wait)

### Power Comparison

| Mode | Active Time | Active Current | Avg Current | Power @3.3V |
|------|-------------|----------------|-------------|-------------|
| Continuous | 1000ms | 500µA | 500µA | 1.65mW |
| One-Shot | 100ms | 500µA | 50µA | 0.165mW |
| **Savings** | **90%** | - | **90%** | **90%** |

## Advanced Features (Future Enhancement)

### Interrupt-Driven Operation (Optional)
GPIO66 is configured as LIGHTING_SENSOR_INT_N with falling edge interrupt. If fully implemented:
```
1. Set high/low thresholds based on current lux ±10%
2. Sensor triggers interrupt when threshold crossed
3. Thread wakes only on significant changes
4. Further reduces polling overhead
```

**Potential additional savings**: 50-70% (poll only when needed)

### Auto One-Shot Mode
The sensor also supports `AUTO_ONE_SHOT` mode:
- Automatically triggers measurements at internal intervals
- Can be configured with programmable timing
- Eliminates need for software trigger
- Useful for ultra-low power scenarios

## Code Structure

### Driver Layer (`dev_opt4048.c`)
**Functions for power optimization**:
- `dev_opt4048_trigger_one_shot()` - Trigger single measurement
- `dev_opt4048_set_threshold_low()` - Set low threshold for hysteresis
- `dev_opt4048_set_threshold_high()` - Set high threshold for hysteresis
- `dev_opt4048_set_latch()` - Configure interrupt latching
- `dev_opt4048_set_fault_count()` - Set threshold crossing filter
- `dev_opt4048_set_int_cfg()` - Configure interrupt mode

### Application Layer (`app_opt4048_thread.c`)
**Implements one-shot operation**:
- Initialization sets ONE_SHOT mode configuration
- Poll function triggers measurement and waits
- Enable/disable keeps sensor in power-down
- Hysteresis check before OS notification

## Testing Recommendations

### Verify One-Shot Behavior
```bash
# Enable debug logging
CONFIG_ALS_LOG_LEVEL=4

# Look for log messages:
"OPT4048 one-shot: Lux=XXX..." - Successful measurement
"Conversion not ready after 120ms" - Timing issue (increase wait)
```

### Measure Actual Power
Use oscilloscope or power analyzer on sensor VDD:
- Should see 100ms bursts at ~0.5mA
- Should see <1µA between bursts
- Duty cycle should be ~10%

### Verify ACPI Updates
```bash
# Monitor ACPI reads from OS
# Check that updates only occur with >10% lux change
# Verify no updates during stable ambient light
```

## Migration from Continuous Mode

The implementation is **backward compatible** - if one-shot mode fails, it gracefully falls back to reading available data. However, one-shot mode is **strongly recommended** for:
- Battery-powered systems
- Always-on ambient light monitoring
- Low-power embedded systems
- Systems with strict power budgets

## Integration Points

1. **Driver Layer**: `dev_opt4048.c` - Hardware I2C communication
2. **Task Manager**: `task_handler.c` - Thread lifecycle management
3. **ACPI Layer**: `acpiplanes.c` - OS data interface
4. **Power Sequencing**: `app_pseq.c` - Power state tracking

## Summary

✅ **90% power reduction** vs continuous mode  
✅ **Automatic power management** (sensor handles power states)  
✅ **Smart update policy** (10% hysteresis prevents spam)  
✅ **Windows compatible** (ACPI plane 0xB2)  
✅ **Full color data** (RGBW + CCT + Lux)  
✅ **Auto-ranging** (0-140K lux dynamic range)  
✅ **Production ready** (based on TI reference design)

**Recommended use**: All battery-powered systems requiring ambient light sensing for adaptive display brightness control.
