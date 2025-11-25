
# Display Shape Detection System

## Overview

The system automatically detects whether a display is **circular** (round) or **rectangular** and adjusts calculations accordingly. You can also **manually override** the detection using the `shape` configuration parameter.

## Configuration Override

**New in v1.96:** Manual control via `config.ini`

```ini
[display]
# Display shape override (auto, rectangular, circular)
# - auto: Auto-detection based on device database (default)
# - rectangular: Force inscribe_factor=1.0 (full width)
# - circular: Force inscribe_factor=0.7071 (inscribed circle)
shape = auto
```

### Priority System

1. **`shape` config parameter** (highest priority - manual override)
2. **`--force-display-circular` CLI flag** (deprecated, kept for compatibility)
3. **Automatic detection** (default behavior)

**Examples:**
```bash
# Config file takes precedence
coolerdash  # Uses shape from config.ini

# CLI flag overrides auto-detection only (not config)
coolerdash --force-display-circular  # Only if shape=auto in config
```

## Automatic Detection

### 1. NZXT Kraken Device Logic (Resolution-Based)

**Special handling for NZXT Kraken devices - resolution determines shape:**

```c
// NZXT Kraken display shape rules:
// - 240x240 or smaller = RECTANGULAR (no inscribe factor)
// - Larger than 240x240 (e.g., 320x320) = CIRCULAR (with inscribe factor)

if (is_kraken) {
    const int is_large_display = (screen_width > 240 || screen_height > 240);
    return is_large_display ? 1 : 0;  // 1 = circular, 0 = rectangular
}
```

**Examples:**
- NZXT Kraken 2023 (240×240) → **Rectangular** (inscribe factor = 1.0)
- NZXT Kraken Z (320×320) → **Circular** (inscribe factor = 0.7071)

### 2. Device Database for Non-Kraken Devices

For other brands, the function `is_circular_display_device()` uses a database:

```c
// Database of non-Kraken devices with CIRCULAR displays
const char *circular_devices[] = {
    // Add other brands with circular displays here
    // Example: "Corsair LCD Circular Model"
};
```

### 3. Automatic Detection Priority (when shape=auto)

1. **NZXT Kraken devices**: Resolution-based detection (≤240×240 = rectangular, >240×240 = circular)
2. **Other devices**: Check device name database
3. **Default**: Rectangular (safer fallback)

**Note:** Manual override via `shape` config parameter takes precedence over all automatic detection methods.

## Math Explanation

### Why 0.7071 (1/√2)?

**Problem**: Prevent clipping on circular displays

```
Circular Display (Radius = R)
┌─────────────────────┐
│         ╱───╲       │
│       ╱       ╲     │
│      │         │    │  ← Circular viewport
│      │ [TEXT]  │    │  ← Rectangular content must fit inside
│       ╲       ╱     │
│         ╰───╯       │
└─────────────────────┘
```

**Solution**: Largest square that fits in a circle

```
If circle radius = R:
- Diameter = 2R
- Inscribed square diagonal = 2R
- Inscribed square side = 2R / √2 = R√2
- Safe area ratio = (R√2) / (2R) = √2/2 = 1/√2 ≈ 0.7071
```

**Example** (240×240 circular display):
- Without factor: Bars would be 240px wide → **CLIPPED at edges**
- With factor: Bars are 240 × 0.7071 = 169.7px wide → **Fully visible**

## Calculation Examples

### NZXT Kraken 2023 (240×240 - Rectangular)

**Circular Displays:**
```
inscribe_factor = 0.7071 (reduced safe area) - NOTE: The `inscribe_factor` is configurable via `display_inscribe_factor` in `config.ini`; default is 0.70710678 (0.0 = auto)
safe_area_width = 240 × 0.7071 = ~170px
safe_bar_width = 170 × 0.98 = ~167px
margin = (240 - 170) / 2 = ~35px
```

**Rectangular Displays:**
```
inscribe_factor = 1.0 (no reduction)
safe_area_width = 240 × 1.0 = 240px
safe_bar_width = 240 × 0.98 = ~235px
margin = (240 - 235) / 2 = ~2.5px
```

### NZXT Kraken Z (320×320 - Circular)

**Circular Displays:**
```
inscribe_factor = 0.7071 (reduced safe area)
safe_area_width = 320 × 0.7071 = ~226px
safe_bar_width = 226 × 0.98 = ~221px
margin = (320 - 226) / 2 = ~47px
```

## Adding New Devices

### Adding a Circular Display (Non-Kraken)

Edit `src/coolercontrol.c`, function `is_circular_display_device()`:

```c
const char *circular_devices[] = {
    "Corsair LCD Circular",  // Example
    "Your Device Name",      // Add here
};
```

**Important:** The device name only needs to be **contained**, not exactly match!
- `"Corsair LCD"` matches: "Corsair LCD Circular 240", "Corsair LCD", etc.

### NZXT Kraken Devices

**No database entry needed!** Resolution-based detection:
- **240×240 or smaller**: Automatically rectangular
- **Larger than 240×240**: Automatically circular

### Rectangular Display

Rectangular displays do **not** need to be added to the database!

The system treats all unknown devices as **rectangular** (safer fallback).

## Detection Examples

| Device Name           | Resolution | Detection Result | Inscribe Factor | Reason                     |
|-----------------------|-----------|------------------|-----------------|----------------------------|
| NZXT Kraken 2023      | 240×240   | **Rectangular**  | 1.0             | Kraken ≤240×240            |
| NZXT Kraken Z         | 320×320   | **Circular**     | 0.7071          | Kraken >240×240            |
| Generic Device        | 240×240   | **Rectangular**  | 1.0             | Not in database, default   |
| Custom Circular LCD   | 240×240   | **Rectangular**  | 1.0             | Needs database entry       |

## Logging

The system logs the detection:

```
[INFO] Circular display detected (device: NZXT Kraken Z, inscribe factor: 0.7071)
[INFO] Rectangular display detected (device: NZXT Kraken 2023, inscribe factor: 1.0000)
```

## Visualization

### Circular Display (240×240) - NZXT Kraken Z

```
┌─────────────────────┐
│   [invisible]       │
│  ╭─────────╮       │
│ ╱           ╲      │
│ │ CPU  33°  │      │  ← safe_area_width = 170px
│ │ ████░░░░░ │      │  ← bar_width = 167px
│ │ GPU  46°  │      │
│ ╲           ╱      │
│  ╰─────────╯       │
│   [invisible]       │
└─────────────────────┘
    margin = 37px
```

### Rectangular Display (240×240) - NZXT Kraken 2023

```
┌─────────────────────┐
│ CPU  33°            │
│ ████████████████░░░ │  ← bar_width = 235px
│                     │
│ GPU  46°            │
│ ████████████████░░░ │
│                     │
└─────────────────────┘
  margin = 2.5px
```

## API Reference

### Function: `is_circular_display_device()`

**Location**: `src/coolercontrol.c`

**Signature**:
```c
int is_circular_display_device(const char *device_name, int screen_width, int screen_height)
```

**Parameters**:
- `device_name`: Device identifier string (e.g., "NZXT Kraken 2023")
- `screen_width`: Display width in pixels
- `screen_height`: Display height in pixels

**Returns**:
- `1` if circular display detected
- `0` if rectangular display detected

**Logic**:
1. Check if device name contains "Kraken"
2. If Kraken: Use resolution-based detection (>240×240 = circular)
3. If not Kraken: Check device database
4. Default: Rectangular (safer fallback)

### C Functions

```c
// In coolercontrol.h/c
int is_circular_display_device(const char *device_name, int screen_width, int screen_height);

// In display.c
static void calculate_scaling_params(
    const struct Config *config, 
    ScalingParams *params, 
    const char *device_name
);

int render_display(
    const struct Config *config, 
    const monitor_sensor_data_t *data, 
    const char *device_name
);
```

## Testing

### Manual Test

1. Compile:
   ```bash
   make
   ```

2. Install and restart service:
   ```bash
   makepkg -fsi
   systemctl daemon-reload
   systemctl restart coolerdash.service
   ```

3. Check logs:
   ```bash
   journalctl -u coolerdash.service -f
   ```

Expected output for **NZXT Kraken 2023 (240×240)**:
```
[INFO] Rectangular display detected (device: NZXT Kraken 2023, inscribe factor: 1.0000)
[INFO] Sending image to LCD: NZXT Kraken 2023 [abc123...]
```

Expected output for **NZXT Kraken Z (320×320)**:
```
[INFO] Circular display detected (device: NZXT Kraken Z, inscribe factor: 0.7071)
[INFO] Sending image to LCD: NZXT Kraken Z [abc123...]
```

## Troubleshooting

### Problem: Clipping on circular display

**Cause**: Device not detected as circular

**Solution Options**:
1. **Quick fix (recommended)**: Set `shape=circular` in `/etc/coolerdash/config.ini`
2. For NZXT Kraken: Verify resolution is >240×240
3. For other devices: Add device name to database in `cc_conf.c`

### Problem: Too much padding on rectangular display

**Cause**: Incorrectly detected as circular

**Solution Options**:
1. **Quick fix (recommended)**: Set `shape=rectangular` in `/etc/coolerdash/config.ini`
2. For NZXT Kraken: Verify resolution is ≤240×240
3. For other devices: Ensure not in circular device database

### Problem: Unknown device defaults to rectangular

**Cause**: Not in database and not NZXT Kraken

**Solution**: This is intentional (safe default). Use `shape=circular` in config if needed.

### Testing Shape Override

```bash
# Test rectangular layout
echo "shape=rectangular" >> /etc/coolerdash/config.ini
systemctl restart coolerdash.service

# Test circular layout
echo "shape=circular" >> /etc/coolerdash/config.ini
systemctl restart coolerdash.service

# Check logs for inscribe factor
journalctl -u coolerdash.service -f | grep "inscribe"
```

Expected output with manual override:
```
[INFO] Display shape forced to rectangular (inscribe factor: 1.0000)
[INFO] Display shape forced to circular (inscribe factor: 0.7071)
```


### Problem: Display Detected Incorrectly

**Solution for Circular display treated as rectangular:**
- Add device name to database in `coolercontrol.c`

**Solution for Rectangular display treated as circular:**
1. Check if device name is incorrectly in `circular_devices[]`
2. Remove from database

## Implementation Details

### Constants

```c
#define M_SQRT1_2  0.7071067811865476  // 1/√2 (inscribe factor)
#define CONTENT_SCALE_FACTOR  0.98      // Bars use 98% of safe area
#define TEMP_EDGE_MARGIN_FACTOR  0.02   // 2% margin for temperature labels
```

### Scaling Flow

1. **Device Detection**: `is_circular_display_device()` checks device name and resolution
2. **Inscribe Factor**: Set to 0.7071 (circular) or 1.0 (rectangular)
3. **Safe Area Calculation**: `safe_width = display_width × inscribe_factor`
4. **Content Scaling**: `bar_width = safe_width × CONTENT_SCALE_FACTOR`

---

**Last Updated**: After resolution-based NZXT Kraken detection implementation
